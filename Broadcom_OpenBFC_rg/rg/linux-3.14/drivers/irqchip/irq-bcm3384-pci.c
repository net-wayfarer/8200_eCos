/*
 * Broadcom BCM3384 style Level 2 single word interrupt
 *
 * Copyright (C) 2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#define IRQS_PER_WORD		9
#define INTR_STATUS_OFFSET	0
#define INTR_MASK_STATUS_OFFSET	4
#define INTR_MASK_SET_OFFSET	8
#define INTR_MASK_CLEAR_OFFSET	12

struct bcm3384_pci_data {
	void __iomem		*base;
	struct irq_domain	*domain;
};

static void bcm3384_pci_irq_handle(unsigned int irq, struct irq_desc *desc)
{
	u32 pending, status, mask_status;
	int i;
	struct bcm3384_pci_data *data = irq_desc_get_handler_data(desc);
	struct irq_chip *chip = irq_desc_get_chip(desc);
	struct irq_chip_generic *gc =
		irq_get_domain_generic_chip(data->domain, 0);


	chained_irq_enter(chip, desc);

	status = irq_reg_readl(gc, INTR_STATUS_OFFSET);
	mask_status = irq_reg_readl(gc, INTR_MASK_STATUS_OFFSET);
	pending = status & ~mask_status;
	while (pending) {
		i = ffs(pending) - 1;
		pending &= ~(1 << i);
		generic_handle_irq(irq_find_mapping(data->domain, i));
	}
	chained_irq_exit(chip, desc);
}

static void bcm3384_pci_mask(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	irq_reg_writel(gc, (1 << d->hwirq), INTR_MASK_SET_OFFSET);
}

static void bcm3384_pci_unmask(struct irq_data *d)
{
	struct irq_chip_generic *gc = irq_data_get_irq_chip_data(d);
	irq_reg_writel(gc, (1 << d->hwirq), INTR_MASK_CLEAR_OFFSET);
}

int __init bcm3384_pci_of_init(struct device_node *dn,
			       struct device_node *parent)
{
	unsigned int clr = IRQ_NOREQUEST | IRQ_NOPROBE | IRQ_NOAUTOEN;
	struct bcm3384_pci_data *data;
	struct irq_chip_generic *gc;
	struct irq_chip_type *ct;
	int ret, parent_irq;
	unsigned int flags;

	data = kzalloc(sizeof(*data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->base = of_iomap(dn, 0);

	parent_irq = irq_of_parse_and_map(dn, 0);
	if (parent_irq < 0) {
		pr_err("failed to map interrupt\n");
		ret = parent_irq;
		goto out_unmap;
	}
	irq_set_handler_data(parent_irq, data);
	irq_set_chained_handler(parent_irq, bcm3384_pci_irq_handle);
	data->domain = irq_domain_add_linear(dn, IRQS_PER_WORD,
					     &irq_generic_chip_ops, NULL);
	if (!data->domain) {
		ret = -ENOMEM;
		goto out_unmap;
	}
	/* MIPS chips strapped for BE will automagically configure the
	 * peripheral registers for CPU-native byte order.
	 */
	flags = 0;
	if (IS_ENABLED(CONFIG_MIPS) && IS_ENABLED(CONFIG_CPU_BIG_ENDIAN))
		flags |= IRQ_GC_BE_IO;

	ret = irq_alloc_domain_generic_chips(data->domain, IRQS_PER_WORD, 1,
					     dn->full_name, handle_level_irq,
					     clr, 0, flags);
	if (ret) {
		pr_err("failed to allocate generic irq chip\n");
		goto out_free_domain;
	}
	gc = irq_get_domain_generic_chip(data->domain, 0);
	gc->unused = 0;
	gc->reg_base = data->base;
	gc->private = data;
	ct = gc->chip_types;

	ct->regs.mask = 0;
	ct->chip.irq_mask = bcm3384_pci_mask;
	ct->chip.irq_unmask = bcm3384_pci_unmask;
	ct->chip.irq_ack = irq_gc_noop;

	/* mask off all ints */
	irq_reg_writel(gc, (1 << IRQS_PER_WORD) - 1, INTR_MASK_SET_OFFSET);

	pr_info("registered pci intc (mem: 0x%p, num IRQ(s): %d)\n",
		data->base, IRQS_PER_WORD);

	return 0;

  out_free_domain:
	irq_domain_remove(data->domain);
  out_unmap:
	iounmap(data->base);
	kfree(data);
	return ret;
}

IRQCHIP_DECLARE(bcm3384_pci, "brcm,bcm3384-pci-intc", bcm3384_pci_of_init);
