/*
 * Copyright (C) 2016 Broadcom
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/bitops.h>
#include <linux/gpio/driver.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/module.h>
#include <linux/basic_mmio_gpio.h>
#include <linux/irqdomain.h>
#include <linux/irqchip/chained_irq.h>
#include <linux/interrupt.h>
#include <linux/reboot.h>

#define GIO_BANK_SIZE           0x20
#define GIO_ODEN(bank)          (((bank) * GIO_BANK_SIZE) + 0x00)
#define GIO_DATA(bank)          (((bank) * GIO_BANK_SIZE) + 0x04)
#define GIO_IODIR(bank)         (((bank) * GIO_BANK_SIZE) + 0x08)
#define GIO_EC(bank)            (((bank) * GIO_BANK_SIZE) + 0x0c)
#define GIO_EI(bank)            (((bank) * GIO_BANK_SIZE) + 0x10)
#define GIO_MASK(bank)          (((bank) * GIO_BANK_SIZE) + 0x14)
#define GIO_LEVEL(bank)         (((bank) * GIO_BANK_SIZE) + 0x18)
#define GIO_STAT(bank)          (((bank) * GIO_BANK_SIZE) + 0x1c)
#define GIO_BANK_OFF(bank, off)	(((bank) * GIO_BANK_SIZE) + (off * sizeof(u32)))

struct brcmstb_gpio_bank {
	struct list_head node;
	int id;
	struct bgpio_chip bgc;
	struct brcmstb_gpio_priv *parent_priv;
	u32 width;
	u32 wake_active;
	u32 regs[GIO_BANK_SIZE / sizeof(u32)];
};

struct brcmstb_gpio_priv {
	struct list_head bank_list;
	void __iomem *reg_base;
	int num_gpios;
	struct platform_device *pdev;
	struct irq_chip irq_chip;
	struct irq_domain *irq_domain;
	int parent_irq;
	int gpio_base;
	bool can_wake;
	int parent_wake_irq;
	struct notifier_block reboot_notifier;
};

#define MAX_GPIO_PER_BANK       32
#define GPIO_BANK(gpio)         ((gpio) >> 5)
/* assumes MAX_GPIO_PER_BANK is a multiple of 2 */
#define GPIO_BIT(gpio)          ((gpio) & (MAX_GPIO_PER_BANK - 1))

static inline struct brcmstb_gpio_bank *
brcmstb_gpio_gc_to_bank(struct gpio_chip *gc)
{
	struct bgpio_chip *bgc = to_bgpio_chip(gc);
	return container_of(bgc, struct brcmstb_gpio_bank, bgc);
}

static inline struct brcmstb_gpio_priv *
brcmstb_gpio_gc_to_priv(struct gpio_chip *gc)
{
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	return bank->parent_priv;
}

static void brcmstb_gpio_set_imask(struct brcmstb_gpio_bank *bank,
		unsigned int offset, bool enable)
{
	struct bgpio_chip *bgc = &bank->bgc;
	struct brcmstb_gpio_priv *priv = bank->parent_priv;
	u32 mask = bgc->pin2mask(bgc, offset);
	u32 imask;
	unsigned long flags;

	spin_lock_irqsave(&bgc->lock, flags);
	imask = bgc->read_reg(priv->reg_base + GIO_MASK(bank->id));
	if (enable)
		imask |= mask;
	else
		imask &= ~mask;
	bgc->write_reg(priv->reg_base + GIO_MASK(bank->id), imask);
	spin_unlock_irqrestore(&bgc->lock, flags);
}

static int brcmstb_gpio_to_irq(struct gpio_chip *gc, unsigned gc_offset)
{
	struct brcmstb_gpio_priv *priv = brcmstb_gpio_gc_to_priv(gc);
	/* gc_offset is relative to this gpio_chip; want real offset */
	int offset = gc_offset + (gc->base - priv->gpio_base);

	if (offset >= priv->num_gpios)
		return -ENXIO;
	return irq_create_mapping(priv->irq_domain, offset);
}

/* -------------------- IRQ chip functions -------------------- */

static int brcmstb_gpio_hwirq_to_offset(irq_hw_number_t hwirq,
		struct brcmstb_gpio_bank *bank)
{
	return hwirq - (bank->bgc.gc.base - bank->parent_priv->gpio_base);
}

static void brcmstb_gpio_irq_mask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	int offset = brcmstb_gpio_hwirq_to_offset(d->hwirq, bank);

	brcmstb_gpio_set_imask(bank, offset, false);
}

static void brcmstb_gpio_irq_unmask(struct irq_data *d)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	int offset = brcmstb_gpio_hwirq_to_offset(d->hwirq, bank);

	brcmstb_gpio_set_imask(bank, offset, true);
}

static int brcmstb_gpio_irq_set_type(struct irq_data *d, unsigned int type)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	struct brcmstb_gpio_priv *priv = bank->parent_priv;
	u32 mask = BIT(brcmstb_gpio_hwirq_to_offset(d->hwirq, bank));
	u32 edge_insensitive, iedge_insensitive;
	u32 edge_config, iedge_config;
	u32 level, ilevel;
	unsigned long flags;

	switch (type) {
	case IRQ_TYPE_LEVEL_LOW:
		level = mask;
		edge_config = 0;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		level = mask;
		edge_config = mask;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		level = 0;
		edge_config = 0;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_RISING:
		level = 0;
		edge_config = mask;
		edge_insensitive = 0;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		level = 0;
		edge_config = 0;  /* don't care, but want known value */
		edge_insensitive = mask;
		break;
	default:
		return -EINVAL;
	}

	spin_lock_irqsave(&bank->bgc.lock, flags);

	iedge_config = bank->bgc.read_reg(priv->reg_base +
			GIO_EC(bank->id)) & ~mask;
	iedge_insensitive = bank->bgc.read_reg(priv->reg_base +
			GIO_EI(bank->id)) & ~mask;
	ilevel = bank->bgc.read_reg(priv->reg_base +
			GIO_LEVEL(bank->id)) & ~mask;

	bank->bgc.write_reg(priv->reg_base + GIO_EC(bank->id),
			iedge_config | edge_config);
	bank->bgc.write_reg(priv->reg_base + GIO_EI(bank->id),
			iedge_insensitive | edge_insensitive);
	bank->bgc.write_reg(priv->reg_base + GIO_LEVEL(bank->id),
			ilevel | level);

	spin_unlock_irqrestore(&bank->bgc.lock, flags);
	return 0;
}

static int __brcmstb_gpio_irq_set_wake(struct brcmstb_gpio_priv *priv,
		unsigned int enable)
{
	int ret = 0;

	/*
	 * Only enable wake IRQ once for however many hwirqs can wake
	 * since they all use the same wake IRQ.  Mask will be set
	 * up appropriately thanks to IRQCHIP_MASK_ON_SUSPEND flag.
	 */
	if (enable)
		ret = enable_irq_wake(priv->parent_wake_irq);
	else
		ret = disable_irq_wake(priv->parent_wake_irq);
	if (ret)
		dev_err(&priv->pdev->dev, "failed to %s wake-up interrupt\n",
				enable ? "enable" : "disable");
	return ret;
}

static int brcmstb_gpio_irq_set_wake(struct irq_data *d, unsigned int enable)
{
	struct gpio_chip *gc = irq_data_get_irq_chip_data(d);
	struct brcmstb_gpio_priv *priv = brcmstb_gpio_gc_to_priv(gc);
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	u32 mask = BIT(brcmstb_gpio_hwirq_to_offset(d->hwirq, bank));

	/* Do not do anything specific for now, suspend/resume callbacks will
	 * configure the interrupt mask appropriately
	 */
	if (enable)
		bank->wake_active |= mask;
	else
		bank->wake_active &= ~mask;

	return __brcmstb_gpio_irq_set_wake(priv, enable);
}

static irqreturn_t brcmstb_gpio_wake_irq_handler(int irq, void *data)
{
	struct brcmstb_gpio_priv *priv = data;

	if (!priv || irq != priv->parent_wake_irq)
		return IRQ_NONE;
	pm_wakeup_event(&priv->pdev->dev, 0);
	return IRQ_HANDLED;
}

static void brcmstb_gpio_irq_bank_handler(int irq,
		struct brcmstb_gpio_bank *bank)
{
	struct brcmstb_gpio_priv *priv = bank->parent_priv;
	struct irq_domain *domain = priv->irq_domain;
	int hwbase = bank->bgc.gc.base - priv->gpio_base;
	void __iomem *reg_base = priv->reg_base;
	unsigned long status;
	unsigned long flags;

	spin_lock_irqsave(&bank->bgc.lock, flags);
	while ((status = bank->bgc.read_reg(reg_base + GIO_STAT(bank->id)) &
			 bank->bgc.read_reg(reg_base + GIO_MASK(bank->id)))) {
		int bit;

		/* Ack the status bits we are about to service */
		bank->bgc.write_reg(reg_base + GIO_STAT(bank->id),
				    status);
		spin_unlock_irqrestore(&bank->bgc.lock, flags);

		for_each_set_bit(bit, &status, 32) {
			int hwirq = hwbase + bit;
			if (bit >= bank->width)
				dev_warn(&priv->pdev->dev,
					 "IRQ for invalid GPIO (bank=%d, offset=%d)\n",
					 bank->id, bit);
			generic_handle_irq(irq_find_mapping(domain, hwirq));
		}

		spin_lock_irqsave(&bank->bgc.lock, flags);
	}
	spin_unlock_irqrestore(&bank->bgc.lock, flags);
}

/* Each UPG GIO block has one IRQ for all banks */
static void brcmstb_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct brcmstb_gpio_priv *priv = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct list_head *pos;

	/* Interrupts weren't properly cleared during probe */
	BUG_ON(!priv || !chip);

	chained_irq_enter(chip, desc);
	list_for_each(pos, &priv->bank_list) {
		struct brcmstb_gpio_bank *bank =
			list_entry(pos, struct brcmstb_gpio_bank, node);
		brcmstb_gpio_irq_bank_handler(irq, bank);
	}
	chained_irq_exit(chip, desc);
}

static int brcmstb_gpio_reboot(struct notifier_block *nb,
		unsigned long action, void *data)
{
	struct brcmstb_gpio_priv *priv =
		container_of(nb, struct brcmstb_gpio_priv, reboot_notifier);

	/* Enable GPIO for S5 cold boot */
	if (action == SYS_POWER_OFF)
		__brcmstb_gpio_irq_set_wake(priv, 1);

	return NOTIFY_DONE;
}

static struct brcmstb_gpio_bank *brcmstb_gpio_hwirq_to_bank(
		struct brcmstb_gpio_priv *priv, irq_hw_number_t hwirq)
{
	struct list_head *pos;
	int i = 0;

	/* banks are in descending order */
	list_for_each_prev(pos, &priv->bank_list) {
		struct brcmstb_gpio_bank *bank =
			list_entry(pos, struct brcmstb_gpio_bank, node);
		i += bank->bgc.gc.ngpio;
		if (hwirq < i)
			return bank;
	}
	return NULL;
}

/*
 * This lock class tells lockdep that GPIO irqs are in a different
 * category than their parents, so it won't report false recursion.
 */
static struct lock_class_key brcmstb_gpio_irq_lock_class;


static int brcmstb_gpio_irq_map(struct irq_domain *d, unsigned int irq,
		irq_hw_number_t hwirq)
{
	struct brcmstb_gpio_priv *priv = d->host_data;
	struct brcmstb_gpio_bank *bank =
		brcmstb_gpio_hwirq_to_bank(priv, hwirq);
	struct platform_device *pdev = priv->pdev;
	int ret;

	if (!bank)
		return -EINVAL;

	dev_dbg(&pdev->dev, "Mapping irq %d for gpio line %d (bank %d)\n",
		irq, (int)hwirq, bank->id);
	ret = irq_set_chip_data(irq, &bank->bgc.gc);
	if (ret < 0)
		return ret;
	irq_set_lockdep_class(irq, &brcmstb_gpio_irq_lock_class);
	irq_set_chip_and_handler(irq, &priv->irq_chip, handle_simple_irq);
#ifdef CONFIG_ARM
	set_irq_flags(irq, IRQF_VALID);
#else
	irq_set_noprobe(irq);
#endif
	return 0;
}

static void brcmstb_gpio_irq_unmap(struct irq_domain *d, unsigned int irq)
{
	irq_set_chip_and_handler(irq, NULL, NULL);
	irq_set_chip_data(irq, NULL);
}

static struct irq_domain_ops brcmstb_gpio_irq_domain_ops = {
	.map = brcmstb_gpio_irq_map,
	.unmap = brcmstb_gpio_irq_unmap,
	.xlate = irq_domain_xlate_twocell,
};

/* Make sure that the number of banks matches up between properties */
static int brcmstb_gpio_sanity_check_banks(struct device *dev,
		struct device_node *np, struct resource *res)
{
	int res_num_banks = resource_size(res) / GIO_BANK_SIZE;
	int num_banks = of_property_count_u32_elems(np, "brcm,gpio-bank-widths");

	if (res_num_banks != num_banks) {
		dev_err(dev, "Mismatch in banks: res had %d, bank-widths had %d\n",
				res_num_banks, num_banks);
		return -EINVAL;
	} else {
		return 0;
	}
}

static int brcmstb_gpio_remove(struct platform_device *pdev)
{
	struct brcmstb_gpio_priv *priv = platform_get_drvdata(pdev);
	struct list_head *pos;
	struct brcmstb_gpio_bank *bank;
	int ret = 0;

	if (!priv) {
		dev_err(&pdev->dev, "called %s without drvdata!\n", __func__);
		return -EFAULT;
	}

	/*
	 * You can lose return values below, but we report all errors, and it's
	 * more important to actually perform all of the steps.
	 */
	list_for_each(pos, &priv->bank_list) {
		bank = list_entry(pos, struct brcmstb_gpio_bank, node);
		ret = bgpio_remove(&bank->bgc);
		if (ret)
			dev_err(&pdev->dev, "bgpio_remove fail in cleanup\n");
	}
	if (priv->reboot_notifier.notifier_call) {
		ret = unregister_reboot_notifier(&priv->reboot_notifier);
		if (ret)
			dev_err(&pdev->dev,
				"failed to unregister reboot notifier\n");
	}
	return ret;
}

static int brcmstb_gpio_of_xlate(struct gpio_chip *gc,
		const struct of_phandle_args *gpiospec, u32 *flags)
{
	struct brcmstb_gpio_priv *priv = brcmstb_gpio_gc_to_priv(gc);
	struct brcmstb_gpio_bank *bank = brcmstb_gpio_gc_to_bank(gc);
	int offset;

	if (gc->of_gpio_n_cells != 2) {
		WARN_ON(1);
		return -EINVAL;
	}

	if (WARN_ON(gpiospec->args_count < gc->of_gpio_n_cells))
		return -EINVAL;

	offset = gpiospec->args[0] - (gc->base - priv->gpio_base);
	if (offset >= gc->ngpio || offset < 0)
		return -EINVAL;

	if (unlikely(offset >= bank->width)) {
		dev_warn_ratelimited(&priv->pdev->dev,
			"Received request for invalid GPIO offset %d\n",
			gpiospec->args[0]);
	}

	if (flags)
		*flags = gpiospec->args[1];

	return offset;
}

/* priv->parent_irq and priv->num_gpios must be set before calling */
static int brcmstb_gpio_irq_setup(struct platform_device *pdev,
		struct brcmstb_gpio_priv *priv)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = NULL;

	priv->irq_chip.name = dev_name(dev);
	priv->irq_chip.irq_disable = brcmstb_gpio_irq_mask;
	priv->irq_chip.irq_mask = brcmstb_gpio_irq_mask;
	priv->irq_chip.irq_unmask = brcmstb_gpio_irq_unmask;
	priv->irq_chip.irq_set_type = brcmstb_gpio_irq_set_type;

	/* Ensures that interrupts are masked when changing their type  */
	priv->irq_chip.flags = IRQCHIP_SET_TYPE_MASKED;

	if (IS_ENABLED(CONFIG_PM_SLEEP) && !priv->can_wake
		&& !priv->parent_wake_irq) {
		priv->parent_wake_irq = platform_get_irq(pdev, 1);
		while (priv->parent_wake_irq < 0) {
			np = of_find_compatible_node(np, NULL,
							"brcm,brcmstb-gpio");
			if (!np)
				break;
			pdev = of_find_device_by_node(np);
			of_node_put(np);
			if (!pdev)
				continue;
			priv->parent_wake_irq = platform_get_irq(pdev, 1);
		}
		if (priv->parent_wake_irq < 0) {
			dev_warn(dev,
				"Couldn't get wake IRQ - GPIOs will not be able to wake from sleep");
		} else {
			int err;

			/*
			 * Set wakeup capability before requesting wakeup
			 * interrupt, so we can process boot-time "wakeups"
			 * (e.g., from S5 cold boot)
			 */
			device_set_wakeup_capable(dev, true);
			device_wakeup_enable(dev);
			err = devm_request_irq(dev, priv->parent_wake_irq,
					brcmstb_gpio_wake_irq_handler, IRQF_SHARED,
					"brcmstb-gpio-wake", priv);

			if (err < 0) {
				dev_err(dev, "Couldn't request wake IRQ");
				return err;
			}

			priv->reboot_notifier.notifier_call =
				brcmstb_gpio_reboot;
			register_reboot_notifier(&priv->reboot_notifier);
			priv->can_wake = true;
		}
	}

	if (priv->can_wake)
		priv->irq_chip.irq_set_wake = brcmstb_gpio_irq_set_wake;
	else
		/* Ensures that all non-wakeup IRQs are disabled at suspend */
		priv->irq_chip.flags |= IRQCHIP_MASK_ON_SUSPEND;

	/* Restore the intended value of np */
	np = dev->of_node;

	priv->irq_domain =
		irq_domain_add_linear(np, priv->num_gpios,
				      &brcmstb_gpio_irq_domain_ops,
				      priv);
	if (!priv->irq_domain) {
		dev_err(dev, "Couldn't allocate IRQ domain\n");
		return -ENXIO;
	}
	irq_set_chained_handler(priv->parent_irq,
				brcmstb_gpio_irq_handler);
	irq_set_handler_data(priv->parent_irq, priv);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static void brcmstb_gpio_bank_save(struct brcmstb_gpio_priv *priv,
				   struct brcmstb_gpio_bank *bank)
{
	struct bgpio_chip *bgc = &bank->bgc;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bank->regs); i++)
		bank->regs[i] = bgc->read_reg(priv->reg_base +
					      GIO_BANK_OFF(bank->id, i));
}

static void brcmstb_gpio_bank_restore(struct brcmstb_gpio_priv *priv,
				      struct brcmstb_gpio_bank *bank)
{
	struct bgpio_chip *bgc = &bank->bgc;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(bank->regs); i++)
		bgc->write_reg(priv->reg_base + GIO_BANK_OFF(bank->id, i),
				bank->regs[i]);
}

static int brcmstb_gpio_suspend(struct device *dev)
{
	struct brcmstb_gpio_priv *priv = dev_get_drvdata(dev);
	struct brcmstb_gpio_bank *bank;
	struct bgpio_chip *bgc;
	struct list_head *pos;
	u32 imask;

	list_for_each(pos, &priv->bank_list) {
		bank = list_entry(pos, struct brcmstb_gpio_bank, node);
		bgc = &bank->bgc;

		brcmstb_gpio_bank_save(priv, bank);

		/* Unmask GPIOs which have been flagged as wake-up sources */
		if (priv->can_wake) {
			imask = bgc->read_reg(priv->reg_base +
					      GIO_MASK(bank->id));
			imask |= bank->wake_active;
			bgc->write_reg(priv->reg_base + GIO_MASK(bank->id),
				       imask);
		}
	}

	return 0;
}

static int brcmstb_gpio_resume(struct device *dev)
{
	struct brcmstb_gpio_priv *priv = dev_get_drvdata(dev);
	struct brcmstb_gpio_bank *bank;
	struct list_head *pos;
	struct bgpio_chip *bgc;
	u32 imask;

	list_for_each(pos, &priv->bank_list) {
		bank = list_entry(pos, struct brcmstb_gpio_bank, node);
		bgc = &bank->bgc;

		brcmstb_gpio_bank_restore(priv, bank);

		/* Mask GPIOs which have been flagged as wake-up sources */
		if (priv->can_wake) {
			imask = bgc->read_reg(priv->reg_base +
					      GIO_MASK(bank->id));
			imask &= ~bank->wake_active;
			bgc->write_reg(priv->reg_base + GIO_MASK(bank->id),
				       imask);
		}
	}

	return 0;
}

#else
#define brcmstb_gpio_suspend	NULL
#define brcmstb_gpio_resume	NULL
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops brcmstb_gpio_pm_ops = {
	.suspend_noirq	= brcmstb_gpio_suspend,
	.resume_noirq = brcmstb_gpio_resume,
};

static int brcmstb_gpio_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	void __iomem *reg_base;
	struct brcmstb_gpio_priv *priv;
	struct resource *res;
	struct property *prop;
	const __be32 *p;
	u32 bank_width;
	int num_banks = 0;
	int err;
	static int gpio_base;
	struct bgpio_chip *bgc;

	priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;
	platform_set_drvdata(pdev, priv);
	INIT_LIST_HEAD(&priv->bank_list);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	reg_base = devm_ioremap_resource(dev, res);
	if (IS_ERR(reg_base))
		return PTR_ERR(reg_base);

	priv->gpio_base = gpio_base;
	priv->reg_base = reg_base;
	priv->pdev = pdev;

	if (of_property_read_bool(np, "interrupt-controller")) {
		priv->parent_irq = platform_get_irq(pdev, 0);
		if (priv->parent_irq < 0) {
#ifdef CONFIG_BCM7120_L2_IRQ
			dev_err(dev, "Couldn't get IRQ");
			return -ENOENT;
#else
			/* TODO: change back when BCM7120_L2_IRQ is default */
			dev_warn(dev, "Couldn't get IRQ. Enable CONFIG_BCM7120_L2_IRQ if you want GPIO interrupt support\n");
#endif
		}
	} else {
		priv->parent_irq = -ENOENT;
	}

	if (brcmstb_gpio_sanity_check_banks(dev, np, res))
		return -EINVAL;

	of_property_for_each_u32(np, "brcm,gpio-bank-widths", prop, p,
			bank_width) {
		struct brcmstb_gpio_bank *bank;
		struct gpio_chip *gc;

		bank = devm_kzalloc(dev, sizeof(*bank), GFP_KERNEL);
		if (!bank) {
			err = -ENOMEM;
			goto fail;
		}

		bank->parent_priv = priv;
		bank->id = num_banks;
		if (bank_width <= 0 || bank_width > MAX_GPIO_PER_BANK) {
			dev_err(dev, "Invalid bank width %d\n", bank_width);
			err = -EINVAL;
			goto fail;
		} else {
			bank->width = bank_width;
		}

		/*
		 * Regs are 4 bytes wide, have data reg, no set/clear regs,
		 * and direction bits have 0 = output and 1 = input
		 */
		bgc = &bank->bgc;
		err = bgpio_init(bgc, dev, 4,
				reg_base + GIO_DATA(bank->id),
				NULL, NULL, NULL,
				reg_base + GIO_IODIR(bank->id), 0);
		if (err) {
			dev_err(dev, "bgpio_init() failed\n");
			goto fail;
		}

		gc = &bgc->gc;
		gc->of_node = np;
		gc->owner = THIS_MODULE;
		gc->label = np->full_name;
		gc->base = gpio_base;
		gc->of_gpio_n_cells = 2;
		gc->of_xlate = brcmstb_gpio_of_xlate;
		/* not all ngpio lines are valid, will use bank width later */
		gc->ngpio = MAX_GPIO_PER_BANK;
		if (priv->parent_irq >= 0)
			gc->to_irq = brcmstb_gpio_to_irq;

		/*
		 * Mask all interrupts by default, since wakeup interrupts may
		 * be retained from S5 cold boot
		 */
		bank->bgc.write_reg(reg_base + GIO_MASK(bank->id), 0);

		err = gpiochip_add(gc);
		if (err) {
			dev_err(dev, "Could not add gpiochip for bank %d\n",
					bank->id);
			/*
			 * In premise, should go to post_gpio_init_fail but
			 * since that calls bgpio_remove() which just calls
			 * gpiochip_remove() it would crash.
			 */
			goto fail;
		}
		gpio_base += gc->ngpio;

		dev_dbg(dev, "bank=%d, base=%d, ngpio=%d, width=%d\n", bank->id,
			gc->base, gc->ngpio, bank->width);

		/* Everything looks good, so add bank to list */
		list_add(&bank->node, &priv->bank_list);

		num_banks++;
	}

	priv->num_gpios = gpio_base - priv->gpio_base;
	if (priv->parent_irq >= 0) {
		err = brcmstb_gpio_irq_setup(pdev, priv);
		if (err)
			goto fail;
	}

	dev_info(dev, "Registered %d banks (GPIO(s): %d-%d)\n",
			num_banks, priv->gpio_base, gpio_base - 1);

	return 0;

fail:
	(void) brcmstb_gpio_remove(pdev);
	return err;
}

static struct of_device_id brcmstb_gpio_of_match[] = {
	{ .compatible = "brcm,brcmstb-gpio" },
	{},
};

MODULE_DEVICE_TABLE(of, brcmstb_gpio_of_match);

static struct platform_driver brcmstb_gpio_driver = {
	.driver = {
		.name = "brcmstb-gpio",
		.of_match_table = brcmstb_gpio_of_match,
		.pm = &brcmstb_gpio_pm_ops,
	},
	.probe = brcmstb_gpio_probe,
	.remove = brcmstb_gpio_remove,
};

static int __init brcmstb_gpio_init(void)
{
	return platform_driver_register(&brcmstb_gpio_driver);
}
subsys_initcall(brcmstb_gpio_init);

static void __exit brcmstb_gpio_exit(void)
{
	platform_driver_unregister(&brcmstb_gpio_driver);
}
module_exit(brcmstb_gpio_exit);

MODULE_AUTHOR("Gregory Fong");
MODULE_DESCRIPTION("Driver for Broadcom BRCMSTB SoC UPG GPIO");
MODULE_LICENSE("GPL v2");
