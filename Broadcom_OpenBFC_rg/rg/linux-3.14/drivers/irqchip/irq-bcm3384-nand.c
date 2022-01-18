/*
 * Broadcom BCM3384 style Level 2 single word interrupt
 *
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

/*
 * Handles the unusual case of mask and status bits in a single 32 bit word
 * enable_mask, status_mask, enable_shift, status_shift define where those
 * bits are based on DT property values enable_shift, status_shift,
 * and #interrupts. It is assumed that the bits are contiguous.
 */

#define pr_fmt(fmt)	KBUILD_MODNAME	": " fmt

#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kconfig.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/io.h>
#include <linux/irqdomain.h>
#include <linux/reboot.h>
#include <linux/bitops.h>
#include <linux/irqchip/chained_irq.h>

#include "irqchip.h"

struct bcm3384_nand_data {
	void __iomem		*base;
	struct irq_domain	*domain;
	u32			enable_mask;
	u32			status_mask;
	u32			enable_shift;
	u32			status_shift;
	u32			nirqs;
};

static void bcm3384_nand_irq_handle(unsigned int irq, struct irq_desc *desc)
{
	struct bcm3384_nand_data *data = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct irq_chip_generic *gc =
		irq_get_domain_generic_chip(data->domain, 0);
	u32 pending, rval;
	int i;

	chained_irq_enter(chip, desc);

	irq_gc_lock(gc);
	rval = irq_reg_readl(gc, 0);
	irq_reg_writel(gc, rval, 0); /* ack */
	irq_gc_unlock(gc);

	pending = (rval & data->status_mask) >> data->status_shift;
	pending &= (rval & data->enable_mask) >> data->enable_shift;

	while (pending) {
		i = ffs(pending) - 1;
		pending &= ~(1 << i);
		generic_handle_irq(irq_find_mapping(data->domain, i));
	}
	chained_irq_exit(chip, desc);
}

static void irq_word_mask(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	struct bcm3384_nand_data *data = gc->private;
	u32 mask = 1 << (d->hwirq + data->enable_shift);

	irq_gc_lock(gc);
	*ct->mask_cache &= ~mask;
	irq_reg_writel(gc, *ct->mask_cache, 0);
	irq_gc_unlock(gc);
}

static void irq_word_unmask(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	struct irq_chip_type *ct = irq_data_get_chip_type(d);
	struct bcm3384_nand_data *data = gc->private;
	u32 mask = 1 << (d->hwirq + data->enable_shift);

	irq_gc_lock(gc);
	*ct->mask_cache |= mask;
	irq_reg_writel(gc, *ct->mask_cache, 0);
	irq_gc_unlock(gc);
}

int __init bcm3384_nand_of_init(struct device_node *dn,
				struct device_node *parent)
{
	unsigned int clr = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	struct bcm3384_nand_data *data;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;
	int ret, parent_irq;
	unsigned int flags;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->base = of_iomap(dn, 0);

	ret = of_property_read_u32(dn, "enable-shift", &data->enable_shift);
	if (ret) {
		pr_err("invalid enable-shift property\n");
		ret = -EINVAL;
		goto out_unmap;
	}
	ret = of_property_read_u32(dn, "status-shift", &data->status_shift);
	if (ret) {
		pr_err("invalid status-shift property\n");
		ret = -EINVAL;
		goto out_unmap;
	}
	ret = of_property_read_u32(dn, "#interrupts", &data->nirqs);
	if (ret) {
		pr_err("invalid #interrupts property\n");
		ret = -EINVAL;
		goto out_unmap;
	}
	if (((data->enable_shift + data->nirqs) > 32) ||
	    ((data->status_shift + data->nirqs) > 32)) {
		pr_err("invalid shift + #interrupts properties > 32\n");
		ret = -EINVAL;
		goto out_unmap;
	}
	data->enable_mask = ((1 << data->nirqs) - 1) << data->enable_shift;
	data->status_mask = ((1 << data->nirqs) - 1) << data->status_shift;

	/* Disable all interrupts by default */
	__raw_writel(data->status_mask, data->base);

	parent_irq = irq_of_parse_and_map(dn, 0);
	if (parent_irq < 0) {
		pr_err("failed to map interrupt\n");
		ret = parent_irq;
		goto out_unmap;
	}
	irq_set_handler_data(parent_irq, data);
	irq_set_chained_handler(parent_irq, bcm3384_nand_irq_handle);
	data->domain = irq_domain_add_linear(dn, data->nirqs,
					     &irq_generic_chip_ops, NULL);
	if (!data->domain) {
		ret = -ENOMEM;
		goto out_unmap;
	}
	/* MIPS chips strapped for BE will automagically configure the
	 * peripheral registers for CPU-native byte order.
	 */
	flags = IRQ_GC_INIT_MASK_CACHE;
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		flags |= IRQ_GC_BE_IO;

	ret = irq_alloc_domain_generic_chips(data->domain, data->nirqs, 1,
					     dn->full_name, handle_level_irq,
					     clr, 0, flags);
	if (ret) {
		pr_err("failed to allocate generic irq chip\n");
		goto out_free_domain;
	}
	gc = irq_get_domain_generic_chip(data->domain, 0);
	gc->unused = ~((1 << data->nirqs) - 1); /* misleading */
	gc->reg_base = data->base;
	gc->private = data;
	ct = gc->chip_types;

	ct->regs.mask = 0;
	ct->chip.irq_mask = irq_word_mask;
	ct->chip.irq_unmask = irq_word_unmask;
	ct->chip.irq_ack = irq_gc_noop;
	pr_info("registered irq-word intc (mem: 0x%p, num IRQ(s): %d)\n",
		data->base, data->nirqs);

	return 0;

  out_free_domain:
	irq_domain_remove(data->domain);
  out_unmap:
	iounmap(data->base);
	kfree(data);
	return ret;
}

IRQCHIP_DECLARE(bcm3384_nand, "brcm,bcm3384-nand-intc", bcm3384_nand_of_init);
