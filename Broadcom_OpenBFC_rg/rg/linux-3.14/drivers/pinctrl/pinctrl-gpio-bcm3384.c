/*
 * Driver for Broadcom BCM3384 SoC Pin and GPIO Control
 *
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This driver is inspired by:
 * pinctrl-bcm3384.c, please see original file for copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitmap.h>
#include <linux/bug.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/pinctrl/consumer.h>
#include <linux/pinctrl/machine.h>
#include <linux/pinctrl/pinconf.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/pinctrl/pinmux.h>
#include <linux/platform_device.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/interrupt.h>
#include <linux/brcmstb/brcmstb.h>

#define MODULE_NAME	"bcm3384-pinctrl-gpio"
#define MODULE_VER	"0.1"

/* GPIO register addresses */
#define GPIO_DIR	(pc->gpio_dir_base)
#define GPIO_DATA	(pc->gpio_data_base)
#define FSEL_EN_MSB	(pc->fsel_base)
#define FSEL_EN_LSB	(pc->fsel_base + 0x04)
#define FSEL_DATA_MSB	(pc->fsel_base + 0x08)
#define FSEL_DATA_LSB	(pc->fsel_base + 0x0c)
#define FSEL_CMD	(pc->fsel_base + 0x10)
#define FSEL_READ	(pc->fsel_read_base)

#define FSEL_SET_CMD	0x21
#define FSEL_GET_CMD	0x23
#define FSEL_FUNC_SHIFT	12
#define FSEL_READ_MASK	0x7

#define GPIO_REG_OFFSET(p)	((p) >> 5)
#define GPIO_REG_SHIFT(p)	((p) & 0x1f)

#define GPIO_NAME_SIZE	8
#define FUNCS_NAME_SIZE	4

#define BCM3384_GPIO_IDX_START	0

#define BCM3384_GPIO_IRQ_SLOTS	6
#define IRQ_SLOT_UNASSIGNED	-1
#define IRQ_SLOT_UNUSABLE	-2

struct bcm3384_pinctrl {
	struct device *dev;

	void __iomem *gpio_dir_base;
	void __iomem *gpio_data_base;
	void __iomem *fsel_base;
	void __iomem *fsel_read_base;
	void __iomem *mbox_cfg;

	spinlock_t reg_lock;

	struct pinctrl_dev *pctl_dev;
	int ngpio;
	int nfuncs;
	struct pinctrl_pin_desc *gpio_pins;
	char **gpio_groups;
	char **pmx_funcs;
	struct gpio_chip gpio_chip;
	struct pinctrl_gpio_range gpio_range;

	void __iomem *gpio_irq_ctrl;
	void __iomem *gpio_irq_mux_lo;
	void __iomem *gpio_irq_mux_hi;
	void __iomem *parent_irq_status;
	u32 parent_irq_mbox_status_bit;
	int parent_irq;
	struct irq_chip irq_chip;
	struct irq_domain *irq_domain;
	int gpio_irq_slot_assignment[BCM3384_GPIO_IRQ_SLOTS];
	unsigned int gpio_irq_slot_type[BCM3384_GPIO_IRQ_SLOTS];
	u32 gpio_irq_slot_mask;
	spinlock_t slot_lock;
};

/*
 * Use MBOX CFG register as spinlock between multiple CPU's and HW blocks
 * outside the control of Linux (not SMP cores).
 * MBOX CFG REG:
 *      [7]:	lock
 *      [5]:	bus type (GISB=1,UBUS=0)
 *      [4:0]:	UBUS source ID (Viper=0, Zephyr=N/A)
 *
 *      *** NOTE ***: bus type & source ID are under SW control - they
 *      are not set automatically by HW when the lock is obtained.
 *      Setting them incorrectly will lead to Linux being unable to
 *      release the MBOX as it will then be under control of the other
 *      CPU or HW block.
 *
 *      *** NOTE ***: We delay 10us between attempts to get the lock in
 *      order to provide the other CPU's or HW block an opportunity to get
 *      their work done so they can release the lock. Without this we could
 *      flood the UBUS, GISB, etc and they wouldn't ever release the lock.
 */
#define MBOX_LOCK_SRC_GISB_MASK		0x00000020
#define MBOX_LOCK_MASK			BCHP_MBOX_GB_CFG14_MBOX_LOCK_MASK
#define MBOX_OWNER_RG			(MBOX_LOCK_MASK | MBOX_LOCK_SRC_GISB_MASK)
static inline void hw_spin_lock_irqsave(struct bcm3384_pinctrl *pc,
	unsigned long *flags)
{
	u32 owner;
	dev_dbg(pc->dev, "-->\n");
	spin_lock_irqsave(&pc->reg_lock, *flags);
	while ((owner = __raw_readl(pc->mbox_cfg)) != MBOX_OWNER_RG) {
		if (!(owner & MBOX_LOCK_MASK)) {
			__raw_writel(MBOX_OWNER_RG, pc->mbox_cfg);
			continue;
		}
		spin_unlock_irqrestore(&pc->reg_lock, *flags);
		udelay(10);
		spin_lock_irqsave(&pc->reg_lock, *flags);
	}
	dev_dbg(pc->dev, "<--\n");
}

static inline void hw_spin_unlock_irqrestore(struct bcm3384_pinctrl *pc,
	unsigned long flags)
{
	u32 owner;
	dev_dbg(pc->dev, "-->\n");
	owner = __raw_readl(pc->mbox_cfg);
	owner &= ~MBOX_LOCK_MASK;
	__raw_writel(owner, pc->mbox_cfg);
	spin_unlock_irqrestore(&pc->reg_lock, flags);
	dev_dbg(pc->dev, "<--\n");
}

/* -------------------- GPIO functions -------------------- */

static inline int bcm3384_gpio_get_gpio(struct bcm3384_pinctrl *pc,
	void __iomem *reg, unsigned gpio)
{
	int val;

	dev_dbg(pc->dev, "-->\n");
	reg += GPIO_REG_OFFSET(gpio) * 4;
	val = __raw_readl(reg);
	val >>= GPIO_REG_SHIFT(gpio);
	val &= 1;
	dev_dbg(pc->dev, "reg: %p, gpio: %d, val: %08x\n", reg, gpio,
		 val);
	dev_dbg(pc->dev, "<--\n");
	return val;
}

static inline void bcm3384_gpio_set_gpio(struct bcm3384_pinctrl *pc,
	void __iomem *reg, unsigned gpio, int value)
{
	u32 data;

	dev_dbg(pc->dev, "-->\n");
	reg += GPIO_REG_OFFSET(gpio) * 4;
	data = __raw_readl(reg);
	dev_dbg(pc->dev, "reg: %p, gpio: %d, data: %08x --> ", reg, gpio,
		 data);
	if (value)
		data |= BIT(GPIO_REG_SHIFT(gpio));
	else
		data &= ~BIT(GPIO_REG_SHIFT(gpio));
	dev_dbg(pc->dev, "%08x\n", data);
	__raw_writel(data, reg);
	dev_dbg(pc->dev, "<--\n");
}

static inline unsigned bcm3384_pinctrl_fsel_get(struct bcm3384_pinctrl *pc,
	unsigned pin)
{
	unsigned val;

	dev_dbg(pc->dev, "-->\n");
	__raw_writel(0, FSEL_CMD);
	__raw_writel(0, FSEL_EN_MSB);
	__raw_writel(0, FSEL_EN_LSB);
	__raw_writel(0, FSEL_DATA_MSB);
	__raw_writel(pin, FSEL_DATA_LSB);
	dev_dbg(pc->dev, "wrote 0x%08x to %p\n", pin, FSEL_DATA_LSB);
	__raw_writel(FSEL_GET_CMD, FSEL_CMD);
	dev_dbg(pc->dev, "wrote 0x%08x to %p\n", FSEL_GET_CMD, FSEL_CMD);
	val = __raw_readl(FSEL_READ) & FSEL_READ_MASK;
	dev_dbg(pc->dev, "read 0x%08x from %p\n", val, FSEL_READ);

	dev_dbg(pc->dev, "%s is set to %s\n", pc->gpio_groups[pin],
		pc->pmx_funcs[val]);

	dev_dbg(pc->dev, "<--\n");
	return val;
}

static inline void bcm3384_pinctrl_fsel_set(struct bcm3384_pinctrl *pc,
	unsigned pin, unsigned fsel)
{
	dev_dbg(pc->dev, "-->\n");
	__raw_writel(0, FSEL_CMD);
	__raw_writel(0, FSEL_EN_MSB);
	__raw_writel(0, FSEL_EN_LSB);
	__raw_writel(0, FSEL_DATA_MSB);
	__raw_writel((fsel << FSEL_FUNC_SHIFT) | pin, FSEL_DATA_LSB);
	dev_dbg(pc->dev, "wrote 0x%08x to %p\n",
		 (fsel << FSEL_FUNC_SHIFT) | pin, FSEL_DATA_LSB);
	__raw_writel(FSEL_SET_CMD, FSEL_CMD);
	dev_dbg(pc->dev, "wrote 0x%08x to %p\n", FSEL_SET_CMD, FSEL_CMD);
	__raw_writel(0, FSEL_CMD);

	dev_dbg(pc->dev, "setting %s to %s\n", pc->gpio_groups[pin],
		pc->pmx_funcs[fsel]);
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	int ret;
	dev_dbg(pc->dev, "-->\n");
	ret = pinctrl_request_gpio(chip->base + offset);
	dev_dbg(pc->dev, "<--\n");
	return ret;
}

static void bcm3384_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	dev_dbg(pc->dev, "-->\n");
	pinctrl_free_gpio(chip->base + offset);
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	int ret;
	dev_dbg(pc->dev, "-->\n");
	ret = pinctrl_gpio_direction_input(chip->base + offset);
	dev_dbg(pc->dev, "<--\n");
	return ret;
}

static int bcm3384_gpio_get(struct gpio_chip *chip, unsigned gpio)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	unsigned long flags;
	int val;

	dev_dbg(pc->dev, "-->\n");
	hw_spin_lock_irqsave(pc, &flags);
	val = bcm3384_gpio_get_gpio(pc, GPIO_DATA, gpio);
	hw_spin_unlock_irqrestore(pc, flags);
	dev_dbg(pc->dev, "<--\n");
	return val;
}

static int bcm3384_gpio_direction_output(struct gpio_chip *chip,
	unsigned offset, int value)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	int ret;
	dev_dbg(pc->dev, "-->\n");
	ret = pinctrl_gpio_direction_output(chip->base + offset);
	dev_dbg(pc->dev, "<--\n");
	return ret;
}

static void bcm3384_gpio_set(struct gpio_chip *chip, unsigned gpio, int value)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	unsigned long flags;

	dev_dbg(pc->dev, "-->\n");
	hw_spin_lock_irqsave(pc, &flags);
	bcm3384_gpio_set_gpio(pc, GPIO_DATA, gpio, value);
	hw_spin_unlock_irqrestore(pc, flags);
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_gpio_to_irq(struct gpio_chip *chip, unsigned gpio)
{
	struct bcm3384_pinctrl *pc = dev_get_drvdata(chip->dev);
	int err;
	dev_dbg(pc->dev, "-->\n");
	err = irq_create_mapping(pc->irq_domain, gpio);
	dev_dbg(pc->dev, "<--\n");
	return err;
}

static struct gpio_chip bcm3384_gpio_chip = {
	.label = MODULE_NAME,
	.owner = THIS_MODULE,
	.request = bcm3384_gpio_request,
	.free = bcm3384_gpio_free,
	.direction_input = bcm3384_gpio_direction_input,
	.direction_output = bcm3384_gpio_direction_output,
	.get = bcm3384_gpio_get,
	.set = bcm3384_gpio_set,
	.to_irq = bcm3384_gpio_to_irq,
	.base = BCM3384_GPIO_IDX_START,
	.can_sleep = false,
};

/* -------------------- pinctrl functions -------------------- */

static int bcm3384_pctl_get_groups_count(struct pinctrl_dev *pctldev)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	int count;

	dev_dbg(pc->dev, "-->\n");
	count = pc->ngpio;
	dev_dbg(pc->dev, "<--\n");
	return count;
}

static const char *bcm3384_pctl_get_group_name(struct pinctrl_dev *pctldev,
		unsigned selector)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	const char *name;

	dev_dbg(pc->dev, "-->\n");
	name = pc->gpio_groups[selector];
	dev_dbg(pc->dev, "<--\n");
	return name;
}

static int bcm3384_pctl_get_group_pins(struct pinctrl_dev *pctldev,
	unsigned selector,
	const unsigned **pins,
	unsigned *num_pins)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pc->dev, "-->\n");
	*pins = &pc->gpio_pins[selector].number;
	*num_pins = 1;

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static void bcm3384_pctl_pin_dbg_show(struct pinctrl_dev *pctldev,
	struct seq_file *s, unsigned gpio)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	unsigned fsel;
	const char *fname;
	int value;
	unsigned long flags;

	hw_spin_lock_irqsave(pc, &flags);
	fsel = bcm3384_pinctrl_fsel_get(pc, gpio);
	fname = pc->pmx_funcs[fsel];
	value = bcm3384_gpio_get_gpio(pc, GPIO_DATA, gpio);
	hw_spin_unlock_irqrestore(pc, flags);
	seq_printf(s, "function %s is %s",
		fname, value ? "hi" : "lo");
}

static void bcm3384_pctl_dt_free_map(struct pinctrl_dev *pctldev,
	struct pinctrl_map *maps, unsigned num_maps)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	dev_dbg(pc->dev, "-->\n");
	kfree(maps);
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_pctl_dt_node_to_map_func(struct bcm3384_pinctrl *pc,
	struct device_node *np, u32 pin, u32 fnum, struct pinctrl_map **maps)
{
	struct pinctrl_map *map = *maps;

	dev_dbg(pc->dev, "-->\n");
	if (fnum >= pc->nfuncs) {
		dev_err(pc->dev, "%s: invalid brcm,function %d\n",
			of_node_full_name(np), fnum);
		return -EINVAL;
	}

	map->type = PIN_MAP_TYPE_MUX_GROUP;
	map->data.mux.group = pc->gpio_groups[pin];
	map->data.mux.function = pc->pmx_funcs[fnum];
	(*maps)++;

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static int bcm3384_pctl_dt_node_to_map(struct pinctrl_dev *pctldev,
	struct device_node *np, struct pinctrl_map **map, unsigned *num_maps)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	struct property *pins, *funcs;
	int num_pins, num_funcs;
	struct pinctrl_map *maps, *cur_map;
	int i, err;
	u32 pin, func;

	dev_dbg(pc->dev, "-->\n");
	pins = of_find_property(np, "brcm,pins", NULL);
	if (!pins) {
		dev_err(pc->dev, "%s: missing brcm,pins property\n",
				of_node_full_name(np));
		return -EINVAL;
	}

	funcs = of_find_property(np, "brcm,function", NULL);
	if (!funcs) {
		dev_err(pc->dev,
			"%s: missing brcm,function property\n",
			of_node_full_name(np));
		return -EINVAL;
	}

	num_pins = pins->length / 4;
	num_funcs = funcs ? (funcs->length / 4) : 0;

	if (num_funcs > 1 && num_funcs != num_pins) {
		dev_err(pc->dev,
			"%s: brcm,function must have 1 or %d entries\n",
			of_node_full_name(np), num_pins);
		return -EINVAL;
	}

	cur_map = maps = kzalloc(num_pins * sizeof(*maps),
				GFP_KERNEL);
	if (!maps) {
		dev_err(pc->dev, "unable to alloc map memory\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_pins; i++) {
		err = of_property_read_u32_index(np, "brcm,pins", i, &pin);
		if (err)
			goto out;
		if (pin >= pc->ngpio) {
			dev_err(pc->dev, "%s: invalid brcm,pins value %d\n",
				of_node_full_name(np), pin);
			err = -EINVAL;
			goto out;
		}

		err = of_property_read_u32_index(np, "brcm,function",
				(num_funcs > 1) ? i : 0, &func);
		if (err)
			goto out;
		err = bcm3384_pctl_dt_node_to_map_func(pc, np, pin,
						       func, &cur_map);
		if (err)
			goto out;
	}

	*map = maps;
	*num_maps = num_pins;

	dev_dbg(pc->dev, "<--\n");
	return 0;

out:
	kfree(maps);
	dev_dbg(pc->dev, "<--\n");
	return err;
}

static const struct pinctrl_ops bcm3384_pctl_ops = {
	.get_groups_count = bcm3384_pctl_get_groups_count,
	.get_group_name = bcm3384_pctl_get_group_name,
	.get_group_pins = bcm3384_pctl_get_group_pins,
	.pin_dbg_show = bcm3384_pctl_pin_dbg_show,
	.dt_node_to_map = bcm3384_pctl_dt_node_to_map,
	.dt_free_map = bcm3384_pctl_dt_free_map,
};

static int bcm3384_pmx_get_functions_count(struct pinctrl_dev *pctldev)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	int count;

	dev_dbg(pc->dev, "-->\n");
	count = pc->nfuncs;
	dev_dbg(pc->dev, "<--\n");
	return count;
}

static const char *bcm3384_pmx_get_function_name(struct pinctrl_dev *pctldev,
	unsigned selector)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	const char *ret;

	dev_dbg(pc->dev, "-->\n");
	ret = pc->pmx_funcs[selector];
	dev_dbg(pc->dev, "<--\n");
	return ret;
}

static int bcm3384_pmx_get_function_groups(struct pinctrl_dev *pctldev,
	unsigned selector, const char * const **groups,
	unsigned * const num_groups)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);

	dev_dbg(pc->dev, "-->\n");
	/* every pin can do every function */
	*groups = (const char * const *)pc->gpio_groups;
	*num_groups = pc->ngpio;

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static int bcm3384_pmx_enable(struct pinctrl_dev *pctldev,
	unsigned func_selector, unsigned group_selector)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;

	dev_dbg(pc->dev, "-->\n");
	hw_spin_lock_irqsave(pc, &flags);
	bcm3384_pinctrl_fsel_set(pc, group_selector, func_selector);
	hw_spin_unlock_irqrestore(pc, flags);
	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static void bcm3384_pmx_disable(struct pinctrl_dev *pctldev,
	unsigned func_selector, unsigned group_selector)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	dev_dbg(pc->dev, "-->\n");
	/* leave the pin in it's current state */
	dev_dbg(pc->dev, "<--\n");
}

static void bcm3384_pmx_gpio_disable_free(struct pinctrl_dev *pctldev,
	struct pinctrl_gpio_range *range, unsigned offset)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	dev_dbg(pc->dev, "-->\n");
	/* leave the pin in it's current state */
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_pmx_gpio_set_direction(struct pinctrl_dev *pctldev,
	struct pinctrl_gpio_range *range, unsigned gpio, bool input)
{
	struct bcm3384_pinctrl *pc = pinctrl_dev_get_drvdata(pctldev);
	unsigned long flags;

	dev_dbg(pc->dev, "-->\n");
	hw_spin_lock_irqsave(pc, &flags);
	bcm3384_gpio_set_gpio(pc, GPIO_DIR, gpio, input ? 0 : 1);
	hw_spin_unlock_irqrestore(pc, flags);
	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static const struct pinmux_ops bcm3384_pmx_ops = {
	.get_functions_count = bcm3384_pmx_get_functions_count,
	.get_function_name = bcm3384_pmx_get_function_name,
	.get_function_groups = bcm3384_pmx_get_function_groups,
	.enable = bcm3384_pmx_enable,
	.disable = bcm3384_pmx_disable,
	.gpio_disable_free = bcm3384_pmx_gpio_disable_free,
	.gpio_set_direction = bcm3384_pmx_gpio_set_direction,
};

static struct pinctrl_desc bcm3384_pinctrl_desc = {
	.name = MODULE_NAME,
	.pctlops = &bcm3384_pctl_ops,
	.pmxops = &bcm3384_pmx_ops,
	.owner = THIS_MODULE,
};

static struct pinctrl_gpio_range bcm3384_pinctrl_gpio_range = {
	.name = MODULE_NAME,
};

/* -------------------- IRQ chip functions -------------------- */

static inline u32 bcm3384_get_gpio_irq_slot_status(struct bcm3384_pinctrl *pc)
{
	u32 val = __raw_readl(pc->gpio_irq_ctrl);
	dev_dbg(pc->dev, "-->\n");
	val &= BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqStatus_MASK;
	val >>= BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqStatus_SHIFT;
	dev_dbg(pc->dev, "<--\n");
	return val;
}

static inline void bcm3384_clr_gpio_irq_slot_status(struct bcm3384_pinctrl *pc,
	int slot)
{
	u32 val;
	dev_dbg(pc->dev, "-->\n");
	val = __raw_readl(pc->gpio_irq_ctrl);
	val &= ~BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqStatus_MASK;
	val |= 1 << (BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqStatus_SHIFT + slot);
	__raw_writel(val, pc->gpio_irq_ctrl);
	dev_dbg(pc->dev, "<--\n");
}

static inline void bcm3384_set_gpio_irq_slot_mask(struct bcm3384_pinctrl *pc,
	int slot)
{
	u32 val;
	dev_dbg(pc->dev, "-->\n");
	val = __raw_readl(pc->gpio_irq_ctrl);
	val |= 1 << (BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqMask_SHIFT + slot);
	__raw_writel(val, pc->gpio_irq_ctrl);
	dev_dbg(pc->dev, "<--\n");
}

static inline void bcm3384_clr_gpio_irq_slot_mask(struct bcm3384_pinctrl *pc,
	int slot)
{
	u32 val;
	dev_dbg(pc->dev, "-->\n");
	val = __raw_readl(pc->gpio_irq_ctrl);
	val &= ~(1 << (BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqMask_SHIFT + slot));
	__raw_writel(val, pc->gpio_irq_ctrl);
	dev_dbg(pc->dev, "<--\n");
}

static inline int bcm3384_gpio_hwirq_to_slot(struct bcm3384_pinctrl *pc,
	irq_hw_number_t hwirq)
{
	int i;

	dev_dbg(pc->dev, "-->\n");
	for (i = 0; i < BCM3384_GPIO_IRQ_SLOTS; i++) {
		if (pc->gpio_irq_slot_assignment[i] == hwirq)
			break;
	}
	dev_dbg(pc->dev, "<--\n");
	if (i == BCM3384_GPIO_IRQ_SLOTS)
		return -EINVAL;
	else
		return i;
}

static inline void bcm3384_set_gpio_irq_slot_assignment(
	struct bcm3384_pinctrl *pc, unsigned gpio, int slot)
{
	u32 lo;
	u32 hi;
	u32 lo_mask = BCHP_INT_PER_EXTIRQMUXSEL0_EXT_IRQ0_SEL_MASK;
	int lo_shift = get_bitmask_order(lo_mask);
	int lo_slot_shift = lo_shift * slot;
	u32 hi_mask = BCHP_Dbg_PER_EXTIRQMUXSEL0_1_EXT_IRQ0_SEL_MASK;
	int hi_shift = get_bitmask_order(hi_mask);
	int hi_slot_shift = hi_shift * slot;

	dev_dbg(pc->dev, "-->\n");
	lo = __raw_readl(pc->gpio_irq_mux_lo);
	hi = __raw_readl(pc->gpio_irq_mux_hi);
	lo &= ~(lo_mask << lo_slot_shift);
	lo |= ((u32)gpio & lo_mask) << lo_slot_shift;
	hi &= ~(hi_mask << hi_slot_shift);
	hi |= ((((u32)gpio & ~lo_mask) >> lo_shift) & hi_mask) <<
		hi_slot_shift;
	__raw_writel(lo, pc->gpio_irq_mux_lo);
	__raw_writel(hi, pc->gpio_irq_mux_hi);
	dev_dbg(pc->dev, "<--\n");
}

static void bcm3384_gpio_irq_mask(struct irq_data *d)
{
	struct bcm3384_pinctrl *pc = irq_data_get_irq_chip_data(d);
	int slot;
	unsigned long slot_flags, reg_flags;

	dev_dbg(pc->dev, "-->\n");
	spin_lock_irqsave(&pc->slot_lock, slot_flags);
	hw_spin_lock_irqsave(pc, &reg_flags);
	slot = bcm3384_gpio_hwirq_to_slot(pc, d->hwirq);
	pc->gpio_irq_slot_mask &= ~(1 << slot);
	bcm3384_clr_gpio_irq_slot_mask(pc, slot);
	hw_spin_unlock_irqrestore(pc, reg_flags);
	spin_unlock_irqrestore(&pc->slot_lock, slot_flags);
	dev_dbg(pc->dev, "<--\n");
}

static void bcm3384_gpio_irq_unmask(struct irq_data *d)
{
	struct bcm3384_pinctrl *pc = irq_data_get_irq_chip_data(d);
	int slot;
	unsigned long slot_flags, reg_flags;

	dev_dbg(pc->dev, "-->\n");
	spin_lock_irqsave(&pc->slot_lock, slot_flags);
	hw_spin_lock_irqsave(pc, &reg_flags);
	slot = bcm3384_gpio_hwirq_to_slot(pc, d->hwirq);
	pc->gpio_irq_slot_mask |= 1 << slot;
	bcm3384_set_gpio_irq_slot_mask(pc, slot);
	hw_spin_unlock_irqrestore(pc, reg_flags);
	spin_unlock_irqrestore(&pc->slot_lock, slot_flags);
	dev_dbg(pc->dev, "<--\n");
}

static int bcm3384_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct bcm3384_pinctrl *pc = irq_data_get_irq_chip_data(d);
	int slot;
	u32 level;
	u32 sense;
	u32 edge_insensitive;
	unsigned long reg_flags, slot_flags;
	u32 val;

	dev_dbg(pc->dev, "-->\n");
	switch (type) {
	case IRQ_TYPE_LEVEL_LOW:
		level = 1;
		sense = 0;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		level = 1;
		sense = 1;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		level = 0;
		sense = 0;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_RISING:
		level = 0;
		sense = 1;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		level = 0;
		sense = 0;
		edge_insensitive = 1;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&pc->slot_lock, slot_flags);
	hw_spin_lock_irqsave(pc, &reg_flags);
	slot = bcm3384_gpio_hwirq_to_slot(pc, d->hwirq);
	val = __raw_readl(pc->gpio_irq_ctrl);
	val &= ~(1 <<
		 (BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqLevelSense_SHIFT + slot));
	val &= ~(1 <<
		 (BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqSense_SHIFT + slot));
	val &= ~(1 <<
		 (BCHP_INT_PER_EXT0IRQCONTROL_EdgeInsensitive_SHIFT + slot));
	val |= (level <<
		(BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqLevelSense_SHIFT + slot));
	val |= (sense <<
		(BCHP_INT_PER_EXT0IRQCONTROL_ExtIrqSense_SHIFT + slot));
	val |= (edge_insensitive <<
		(BCHP_INT_PER_EXT0IRQCONTROL_EdgeInsensitive_SHIFT + slot));
	__raw_writel(val, pc->gpio_irq_ctrl);
	pc->gpio_irq_slot_type[slot] = type;
	hw_spin_unlock_irqrestore(pc, reg_flags);
	spin_unlock_irqrestore(&pc->slot_lock, slot_flags);

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static void bcm3384_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct bcm3384_pinctrl *pc = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	unsigned long events;
	unsigned slot;
	unsigned gpio;
	unsigned int type;
	unsigned long slot_flags, reg_flags;
	unsigned int child_irq;

	dev_dbg(pc->dev, "IRQ -->\n");
	chained_irq_enter(chip, desc);
	hw_spin_lock_irqsave(pc, &reg_flags);
	spin_lock_irqsave(&pc->slot_lock, slot_flags);
	events = bcm3384_get_gpio_irq_slot_status(pc);
	dev_dbg(pc->dev, "events: 0x%08lx, slot_mask: 0x%08x\n", events,
			pc->gpio_irq_slot_mask);
	events &= pc->gpio_irq_slot_mask;
	for_each_set_bit(slot, &events, BCM3384_GPIO_IRQ_SLOTS) {
		gpio = pc->gpio_irq_slot_assignment[slot];
		if (gpio < 0) {
			dev_err(pc->dev,
				"IRQ from GPIO slot %d but slot is %s.\n",
				slot, gpio == IRQ_SLOT_UNASSIGNED ?
				"unassigned" : "unusable");
			continue;
		}
		type = pc->gpio_irq_slot_type[slot];

		/* ack edge triggered IRQs immediately */
		if (!(type & IRQ_TYPE_LEVEL_MASK))
			bcm3384_clr_gpio_irq_slot_status(pc, slot);

		child_irq = irq_find_mapping(pc->irq_domain, gpio);
		generic_handle_irq(child_irq);

		/* ack level triggered IRQ after handling them */
		if (type & IRQ_TYPE_LEVEL_MASK)
			bcm3384_clr_gpio_irq_slot_status(pc, slot);

        /* set mask since other CPU will have cleared it */
		bcm3384_set_gpio_irq_slot_mask(pc, slot);

	}
	spin_unlock_irqrestore(&pc->slot_lock, slot_flags);
	hw_spin_unlock_irqrestore(pc, reg_flags);
	chained_irq_exit(chip, desc);

	/*
	 * TEMPORARY HACK:
	 * The 7120 L2 driver assumes status bits clear themselves when
	 * the source clears and thus it doesn't register an irq_ack handler
	 * to clear sticky status bits. So we have to ack the MBOX IRQ in
	 * the parent controller's status reg.
	 *
	 * This should be removed when the 7120 L2 driver is modified to
	 * handle this or replaced with a new driver to handle the G2U
	 * bridge interrupt status properly.
	 */
	__raw_writel(BIT(pc->parent_irq_mbox_status_bit), pc->parent_irq_status);
	dev_dbg(pc->dev, "IRQ <--\n");
}

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key bcm3384_gpio_irq_lock_class;

static int bcm3384_gpio_irq_map(struct irq_domain *d, unsigned int irq,
	irq_hw_number_t hwirq)
{
	struct bcm3384_pinctrl *pc = d->host_data;
	unsigned long slot_flags, reg_flags;
	int err;
	int slot;

	dev_dbg(pc->dev, "-->\n");
	spin_lock_irqsave(&pc->slot_lock, slot_flags);
	hw_spin_lock_irqsave(pc, &reg_flags);

	for (slot = 0; slot < BCM3384_GPIO_IRQ_SLOTS; slot++) {
		if (pc->gpio_irq_slot_assignment[slot] == IRQ_SLOT_UNASSIGNED)
			break;
	}
	if (slot == BCM3384_GPIO_IRQ_SLOTS) {
		dev_err(pc->dev, "No GPIO IRQ slots available.\n");
		err = -ENOSPC;
		goto done;
	}

	err = gpio_lock_as_irq(&pc->gpio_chip, hwirq);
	if (err)
		goto done;

	err = irq_set_chip_data(irq, pc);
	if (err < 0)
		goto done;
	irq_set_lockdep_class(irq, &bcm3384_gpio_irq_lock_class);
	irq_set_chip_and_handler(irq, &pc->irq_chip, handle_simple_irq);
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	irq_set_noprobe(irq);
#endif

	pc->gpio_irq_slot_assignment[slot] = hwirq;
	bcm3384_set_gpio_irq_slot_assignment(pc, hwirq, slot);
	dev_dbg(pc->dev, "Mapped GPIO %d to IRQ %d in slot %d.\n",
		(int)hwirq, irq, slot);

done:
	hw_spin_unlock_irqrestore(pc, reg_flags);
	spin_unlock_irqrestore(&pc->slot_lock, slot_flags);
	dev_dbg(pc->dev, "<--\n");
	return err;
}

static void bcm3384_gpio_irq_unmap(struct irq_domain *d, unsigned int irq)
{
	struct bcm3384_pinctrl *pc = d->host_data;
	int slot;
	unsigned long flags;
	struct irq_data *irqd = irq_get_irq_data(irq);
	irq_hw_number_t hwirq;

	dev_dbg(pc->dev, "-->\n");

	/*
	 * We don't set HW because there isn't any way to "remove" a GPIO
	 * assignment.
	 */
	hwirq = irqd_to_hwirq(irqd);
	spin_lock_irqsave(&pc->slot_lock, flags);
	for (slot = 0; slot < BCM3384_GPIO_IRQ_SLOTS; slot++) {
		if (pc->gpio_irq_slot_assignment[slot] == (int)hwirq) {
			pc->gpio_irq_slot_assignment[slot] =
				IRQ_SLOT_UNASSIGNED;
			break;
		}
	}
	spin_unlock_irqrestore(&pc->slot_lock, flags);
	irq_set_chip_and_handler(irq, NULL, NULL);
	irq_set_chip_data(irq, NULL);
	gpio_unlock_as_irq(&pc->gpio_chip, hwirq);
	dev_dbg(pc->dev, "<--\n");
}

static struct irq_domain_ops bcm3384_gpio_irq_domain_ops = {
	.map = bcm3384_gpio_irq_map,
	.unmap = bcm3384_gpio_irq_unmap,
	.xlate = irq_domain_xlate_twocell
};

static int bcm3384_gpio_irq_setup(struct platform_device *pdev,
	struct bcm3384_pinctrl *pc)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;

	dev_dbg(pc->dev, "-->\n");
	pc->irq_chip.name = np->full_name;
	pc->irq_chip.irq_mask = bcm3384_gpio_irq_mask;
	pc->irq_chip.irq_unmask = bcm3384_gpio_irq_unmask;
	pc->irq_chip.irq_set_type = bcm3384_gpio_irq_set_type;

	/* Ensures that all non-wakeup IRQs are disabled at suspend */
	pc->irq_chip.flags = IRQCHIP_MASK_ON_SUSPEND;

	pc->irq_domain = irq_domain_add_linear(np, pc->ngpio,
					       &bcm3384_gpio_irq_domain_ops,
					       pc);
	if (!pc->irq_domain) {
		dev_err(dev, "Couldn't allocate IRQ domain\n");
		return -ENXIO;
	}
	irq_set_chained_handler(pc->parent_irq,
				bcm3384_gpio_irq_handler);
	irq_set_handler_data(pc->parent_irq, pc);

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static int bcm3384_pinctrl_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct bcm3384_pinctrl *pc;
	struct resource iomem;
	int err;
	int i;
	struct property *prop;
	const __be32 *p;
	u32 u;

	dev_dbg(dev, "-->\n");
	pc = devm_kzalloc(dev, sizeof(*pc), GFP_KERNEL);
	if (!pc) {
		dev_err(dev, "unable to allocate private data\n");
		return -ENOMEM;
	}

	platform_set_drvdata(pdev, pc);
	pc->dev = dev;

	spin_lock_init(&pc->reg_lock);
	spin_lock_init(&pc->slot_lock);

	err = of_address_to_resource(np, 0, &iomem);
	if (err) {
		dev_err(dev, "could not get GPIO_DIR base address\n");
		return err;
	}
	pc->gpio_dir_base = devm_ioremap_resource(dev, &iomem);
	if (IS_ERR(pc->gpio_dir_base)) {
		dev_err(dev, "unable to map gpio_dir_base\n");
		return PTR_ERR(pc->gpio_dir_base);
	}
	dev_dbg(dev, "gpio_dir_base: %p\n", pc->gpio_dir_base);
	err = of_address_to_resource(np, 1, &iomem);
	if (err) {
		dev_err(dev, "could not get GPIO_DATA base address\n");
		return err;
	}
	pc->gpio_data_base = devm_ioremap_resource(dev, &iomem);
	if (IS_ERR(pc->gpio_data_base)) {
		dev_err(dev, "unable to map gpio_data_base\n");
		return PTR_ERR(pc->gpio_data_base);
	}
	dev_dbg(dev, "gpio_data_base: %p\n", pc->gpio_data_base);
	err = of_address_to_resource(np, 2, &iomem);
	if (err) {
		dev_err(dev, "could not get FSEL base address\n");
		return err;
	}
	pc->fsel_base = devm_ioremap_resource(dev, &iomem);
	if (IS_ERR(pc->fsel_base)) {
		dev_err(dev, "unable to map fsel_base\n");
		return PTR_ERR(pc->fsel_base);
	}
	dev_dbg(dev, "fsel_base: %p\n", pc->fsel_base);
	err = of_address_to_resource(np, 3, &iomem);
	if (err) {
		dev_err(dev, "could not get FSEL_READ base address\n");
		return err;
	}
	pc->fsel_read_base = devm_ioremap_resource(dev, &iomem);
	if (IS_ERR(pc->fsel_read_base)) {
		dev_err(dev, "unable to map fsel_read_base\n");
		return PTR_ERR(pc->fsel_read_base);
	}
	dev_dbg(dev, "fsel_read_base: %p\n", pc->fsel_read_base);

	err = of_property_read_u32(np, "brcm,num-gpio-pins", &pc->ngpio);
	if (err) {
		dev_err(dev, "could not get number of gpio pins\n");
		return err;
	}
	pc->gpio_pins = devm_kzalloc(dev,
		sizeof(*pc->gpio_pins) * pc->ngpio, GFP_KERNEL);
	if (!pc->gpio_pins) {
		dev_err(dev, "unable to alloc gpio_pins array\n");
		return -ENOMEM;
	}
	pc->gpio_groups = devm_kzalloc(dev,
		sizeof(*pc->gpio_groups) * pc->ngpio, GFP_KERNEL);
	if (!pc->gpio_groups) {
		dev_err(dev, "unable to alloc gpio_groups array\n");
		return -ENOMEM;
	}
	for (i = 0; i < pc->ngpio; i++) {
		/* pins are just named gpio0..gpioN */
		pc->gpio_pins[i].number = i;
		pc->gpio_pins[i].name = devm_kzalloc(dev, GPIO_NAME_SIZE,
						     GFP_KERNEL);
		if (!pc->gpio_pins[i].name) {
			dev_err(dev, "unable to alloc gpio_pins.name string\n");
			return -ENOMEM;
		}
		snprintf((char *)pc->gpio_pins[i].name, GPIO_NAME_SIZE,
			 "gpio%d", i);

		/* one pin per group */
		pc->gpio_groups[i] = devm_kzalloc(dev, GPIO_NAME_SIZE,
						  GFP_KERNEL);
		if (!pc->gpio_groups[i]) {
			dev_err(dev, "unable to alloc gpio_groups string\n");
			return -ENOMEM;
		}
		snprintf(pc->gpio_groups[i], GPIO_NAME_SIZE, "gpio%d", i);
	}
	err = of_property_read_u32(np, "brcm,num-pinmux-funcs", &pc->nfuncs);
	if (err) {
		dev_err(dev, "could not get number of pinmux functions\n");
		return err;
	}
	pc->pmx_funcs = devm_kzalloc(dev,
		sizeof(*pc->pmx_funcs) * pc->nfuncs, GFP_KERNEL);
	if (!pc->pmx_funcs) {
		dev_err(dev, "unable to alloc pmx_funcs array\n");
		return -ENOMEM;
	}
	for (i = 0; i < pc->nfuncs; i++) {
		/* funcs are just named f0..fN */
		pc->pmx_funcs[i] = devm_kzalloc(dev, FUNCS_NAME_SIZE,
						  GFP_KERNEL);
		if (!pc->pmx_funcs[i]) {
			dev_err(dev, "unable to alloc pinmux funcs string\n");
			return -ENOMEM;
		}
		snprintf(pc->pmx_funcs[i], FUNCS_NAME_SIZE, "f%d", i);
	}

	if (of_find_property(np, "interrupt-controller", NULL)) {
		pc->parent_irq = platform_get_irq(pdev, 0);
		if (pc->parent_irq < 0) {
			dev_err(dev, "Couldn't get IRQ");
			return -ENOENT;
		}

		err = of_address_to_resource(np, 4, &iomem);
		if (err) {
			dev_err(dev, "could not get MBOX_CFG base address\n");
			return err;
		}
		pc->mbox_cfg = devm_ioremap_resource(dev, &iomem);
		if (IS_ERR(pc->mbox_cfg)) {
			dev_err(dev, "unable to map mbox_cfg\n");
			return PTR_ERR(pc->mbox_cfg);
		}
		dev_dbg(dev, "mbox_cfg: %p\n", pc->mbox_cfg);

		err = of_address_to_resource(np, 5, &iomem);
		if (err) {
			dev_err(dev, "could not get GPIO_IRQ_CTRL base address\n");
			return err;
		}
		pc->gpio_irq_ctrl = devm_ioremap_resource(dev, &iomem);
		if (IS_ERR(pc->gpio_irq_ctrl)) {
			dev_err(dev, "unable to map gpio_irq_ctrl\n");
			return PTR_ERR(pc->gpio_irq_ctrl);
		}
		dev_dbg(dev, "gpio_irq_ctrl: %p\n", pc->gpio_irq_ctrl);

		err = of_address_to_resource(np, 6, &iomem);
		if (err) {
			dev_err(dev, "could not get GPIO_IRQ_MUX_LO base address\n");
			return err;
		}
		pc->gpio_irq_mux_lo = devm_ioremap_resource(dev, &iomem);
		if (IS_ERR(pc->gpio_irq_mux_lo)) {
			dev_err(dev, "unable to map gpio_irq_mux_lo\n");
			return PTR_ERR(pc->gpio_irq_mux_lo);
		}
		dev_dbg(dev, "gpio_irq_mux_lo: %p\n", pc->gpio_irq_mux_lo);

		err = of_address_to_resource(np, 7, &iomem);
		if (err) {
			dev_err(dev, "could not get GPIO_IRQ_MUX_HI base address\n");
			return err;
		}
		pc->gpio_irq_mux_hi = devm_ioremap_resource(dev, &iomem);
		if (IS_ERR(pc->gpio_irq_mux_hi)) {
			dev_err(dev, "unable to map gpio_irq_mux_hi\n");
			return PTR_ERR(pc->gpio_irq_mux_lo);
		}
		dev_dbg(dev, "gpio_irq_mux_hi: %p\n", pc->gpio_irq_mux_hi);

		err = of_address_to_resource(np, 8, &iomem);
		if (err) {
			dev_err(dev, "could not get parent_irq_status base address\n");
			return err;
		}
		pc->parent_irq_status = devm_ioremap_resource(dev, &iomem);
		if (IS_ERR(pc->parent_irq_status)) {
			dev_err(dev, "unable to map parent_irq_status\n");
			return PTR_ERR(pc->parent_irq_status);
		}
		dev_dbg(dev, "parent_irq_status: %p\n", pc->parent_irq_status);

		err = of_property_read_u32(np, "brcm,parent-irq-status-bit",
					   &pc->parent_irq_mbox_status_bit);
		if (err) {
			dev_err(dev, "could not get parent IRQ status bit\n");
			return err;
		}

		for (i = 0; i < BCM3384_GPIO_IRQ_SLOTS; i++)
			pc->gpio_irq_slot_assignment[i] = IRQ_SLOT_UNUSABLE;
		err = of_property_count_u32_elems(np,
				"brcm,gpio_irq_slot_assignments");
		if (err <= 0) {
			dev_err(dev,
				"brcm,gpio_irq_slot_assignments is malformed\n");
			return -EINVAL;
		}
		of_property_for_each_u32(np, "brcm,gpio_irq_slot_assignments",
					 prop, p, u)
			pc->gpio_irq_slot_assignment[u] = IRQ_SLOT_UNASSIGNED;
	} else {
		pc->parent_irq = -ENOENT;
	}

	pc->gpio_chip = bcm3384_gpio_chip;
	pc->gpio_chip.dev = dev;
	pc->gpio_chip.of_node = np;
	pc->gpio_chip.ngpio = pc->ngpio;
	err = gpiochip_add(&pc->gpio_chip);
	if (err) {
		dev_err(dev, "could not add GPIO chip\n");
		return err;
	}

	bcm3384_pinctrl_desc.pins = pc->gpio_pins;
	bcm3384_pinctrl_desc.npins = pc->ngpio;
	pc->pctl_dev = pinctrl_register(&bcm3384_pinctrl_desc, dev, pc);
	if (!pc->pctl_dev) {
		err = gpiochip_remove(&pc->gpio_chip);
		return -EINVAL;
	}

	bcm3384_pinctrl_gpio_range.npins = pc->ngpio;
	pc->gpio_range = bcm3384_pinctrl_gpio_range;
	pc->gpio_range.base = pc->gpio_chip.base;
	pc->gpio_range.gc = &pc->gpio_chip;
	pinctrl_add_gpio_range(pc->pctl_dev, &pc->gpio_range);

	if (pc->parent_irq >= 0) {
		err = bcm3384_gpio_irq_setup(pdev, pc);
		if (err)
			return err;
	}

	dev_info(dev, "%d GPIO's, each with %d functions\n",
		pc->ngpio, pc->nfuncs);

	dev_dbg(dev, "<--\n");
	return 0;
}

static int bcm3384_pinctrl_remove(struct platform_device *pdev)
{
	struct bcm3384_pinctrl *pc = platform_get_drvdata(pdev);
	int res;

	dev_dbg(pc->dev, "-->\n");
	pinctrl_unregister(pc->pctl_dev);
	res = gpiochip_remove(&pc->gpio_chip);

	dev_dbg(pc->dev, "<--\n");
	return 0;
}

static struct of_device_id bcm3384_pinctrl_match[] = {
	{ .compatible = "brcm,bcm3384-pinctrl-gpio" },
	{}
};
MODULE_DEVICE_TABLE(of, bcm3384_pinctrl_match);

static struct platform_driver bcm3384_pinctrl_driver = {
	.probe = bcm3384_pinctrl_probe,
	.remove = bcm3384_pinctrl_remove,
	.driver = {
		.name = MODULE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = bcm3384_pinctrl_match,
	},
};

static int __init bcm3384_pinctrl_init(void)
{
	pr_debug("%s driver v%s\n", MODULE_NAME, MODULE_VER);
	return platform_driver_register(&bcm3384_pinctrl_driver);
}
subsys_initcall(bcm3384_pinctrl_init);

static void __exit bcm3384_pinctrl_exit(void)
{
	platform_driver_unregister(&bcm3384_pinctrl_driver);
}
module_exit(bcm3384_pinctrl_exit);


MODULE_AUTHOR("Tim Ross");
MODULE_DESCRIPTION("Broadcom BCM3384 SoC Pin & GPIO Control Driver");
MODULE_LICENSE("GPL v2");
