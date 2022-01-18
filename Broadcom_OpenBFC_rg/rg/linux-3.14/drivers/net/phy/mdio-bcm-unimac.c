/*
 * Broadcom UniMAC MDIO bus controller driver
 *
 * Copyright (C) 2014-2017 Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/phy.h>
#include <linux/platform_device.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/delay.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_mdio.h>

#define MDIO_CMD		0x00
#define  MDIO_START_BUSY	(1 << 29)
#define  MDIO_READ_FAIL		(1 << 28)
#define  MDIO_RD_C45	(3 << 26)
#define  MDIO_RD		(2 << 26)
#define  MDIO_WR		(1 << 26)
#define  MDIO_ADDR_C45	(0 << 26)
#define  MDIO_PMD_SHIFT		21
#define  MDIO_PMD_MASK		(0x1F << MDIO_PMD_SHIFT)
#define  MDIO_REG_SHIFT		16
#define  MDIO_REG_MASK		(0x1F << MDIO_REG_SHIFT)

#define MDIO_CFG		0x04
#define  MDIO_C22		(1 << 0)
#define  MDIO_C45		0
#define  MDIO_CLK_DIV_SHIFT	4
#define  MDIO_CLK_DIV_MASK	0x3F
#define  MDIO_SUPP_PREAMBLE	(1 << 12)

#define SELECT_ONCHIP_PHY       0x400
#define MAX_NAME_LEN	16

struct phy_access {
	int		addr;
	void	*data;
	int		(*hdlr)(void *data, bool connect);
};

struct unimac_mdio_priv {
	struct mii_bus		*mii_bus;
	void __iomem		*base;
	struct phy_access	phy_access;
	u32					cfg;
	u32					phy_c45_mask;
	u16					extoffset[PHY_MAX_ADDR];
	char				name[MAX_NAME_LEN];
};

int unimac_mdio_access(struct mii_bus *bus, int addr, void *data,
		int (*hdlr)(void *data, bool connect))
{
	struct unimac_mdio_priv *priv = bus->priv;

	mutex_lock(&bus->mdio_lock);
	priv->phy_access.addr = addr;
	priv->phy_access.data = data;
	priv->phy_access.hdlr = hdlr;
	mutex_unlock(&bus->mdio_lock);

	return 0;
}
EXPORT_SYMBOL(unimac_mdio_access);

static inline unsigned int unimac_mdio_busy(struct unimac_mdio_priv *priv)
{
	return __raw_readl(priv->base + MDIO_CMD) & MDIO_START_BUSY;
}

static inline int unimac_mdio_start(struct unimac_mdio_priv *priv, u32 cmd)
{
	int ret = 0;
	unsigned int timeout = 1000;

	cmd |= MDIO_START_BUSY;
	__raw_writel(cmd, priv->base + MDIO_CMD);

	do {
		if (!unimac_mdio_busy(priv))
			break;

		usleep_range(1, 2);
	} while (timeout--);

	if (!timeout)
		ret = -ETIMEDOUT;

	return ret;
}

static inline void unimac_mdio_clause(struct unimac_mdio_priv *priv, int reg)
{
	if (!priv->cfg)
		priv->cfg = __raw_readl(priv->base + MDIO_CFG);

	/* Configure clause if required */
	if (!(priv->cfg & MDIO_C22) && !(reg & MII_ADDR_C45)) {
		priv->cfg |= MDIO_C22;
		__raw_writel(priv->cfg, priv->base + MDIO_CFG);
	} else if (!(priv->cfg & MDIO_C45) && (reg & MII_ADDR_C45)) {
		priv->cfg &= ~MDIO_C22;
		__raw_writel(priv->cfg, priv->base + MDIO_CFG);
	}
}

static inline int unimac_mdio_address(struct unimac_mdio_priv *priv,
		int phy_id, int reg)
{
	int ret = 0;
	u32 cmd = 0;

	/* Configure register address as required */
	if (!(reg & MII_ADDR_C45)) {
		if ((priv->phy_c45_mask & (1 << phy_id)) ||
				(reg > 0x001F && reg < 0x8000) || (reg > 0xFFFF))
			return -ENXIO;

		/* Prepare MDIO C22 address extension command as required */
		reg &= 0xFFF0;
		if (reg & 0x8000) {
			if (priv->extoffset[phy_id] != reg) {
				cmd = MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
					(0x1F << MDIO_REG_SHIFT) | reg;
			}
		} else if (priv->extoffset[phy_id] &&
				(priv->extoffset[phy_id] != 0x8000)) {
			cmd = MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
				(0x1F << MDIO_REG_SHIFT) | 0x8000;
		}

		/* Start MDIO C22 address extension transaction */
		if (cmd) {
			ret = unimac_mdio_start(priv, cmd);
			if (ret == 0)
				priv->extoffset[phy_id] = reg;
		}
	} else {
		/* Prepare MDIO C45 address command */
		cmd = MDIO_ADDR_C45 | (phy_id << MDIO_PMD_SHIFT) |
			(reg & 0x1FFFFF);

		/* Start MDIO C45 address transaction */
		ret = unimac_mdio_start(priv, cmd);
	}

	return ret;
}

static int unimac_mdio_read(struct mii_bus *bus, int phy_id, int reg)
{
	struct unimac_mdio_priv *priv = bus->priv;
	int ret;
	u32 cmd;

	/* Setup clause 22 or 45 based on register address */
	unimac_mdio_clause(priv, reg);

	/* Switch on MDIO for phy_id */
	if ((phy_id == priv->phy_access.addr) && priv->phy_access.hdlr)
		 priv->phy_access.hdlr(priv->phy_access.data, 1);

	/* Perform clause 22 address extension or clause 45 address transaction */
	ret = unimac_mdio_address(priv, phy_id, reg);
	if (ret)
		goto _access;

	/* Prepare the read operation */
	if (reg & MII_ADDR_C45)
		cmd = MDIO_RD_C45 | (phy_id << MDIO_PMD_SHIFT) |
			(reg & 0x1FFFFF);
	else if (reg > 0x1F)
		cmd = MDIO_RD | (phy_id << MDIO_PMD_SHIFT) |
			((0x10 + (reg & 0xF)) << MDIO_REG_SHIFT);
	else
		cmd = MDIO_RD | (phy_id << MDIO_PMD_SHIFT) |
			(reg << MDIO_REG_SHIFT);

	/* Start MDIO transaction */
	ret = unimac_mdio_start(priv, cmd);

_access:
	/* Switch off MDIO for phy_id */
	if ((phy_id == priv->phy_access.addr) && priv->phy_access.hdlr)
		 priv->phy_access.hdlr(priv->phy_access.data, 0);

	if (ret)
		return ret;

	cmd = __raw_readl(priv->base + MDIO_CMD);

	/* Some broken devices are known not to release the line during
	 * turn-around, e.g: Broadcom BCM53125 external switches, so check for
	 * that condition here and ignore the MDIO controller read failure
	 * indication.
	 */
	if (!(bus->phy_ignore_ta_mask & 1 << phy_id) && (cmd & MDIO_READ_FAIL))
		return -EIO;

	return cmd & 0xFFFF;
}

static int unimac_mdio_write(struct mii_bus *bus, int phy_id,
			     int reg, u16 val)
{
	struct unimac_mdio_priv *priv = bus->priv;
	int ret;
	u32 cmd;

	/* Setup clause 22 or 45 based on register address */
	unimac_mdio_clause(priv, reg);

	/* Switch on MDIO for phy_id */
	if ((phy_id == priv->phy_access.addr) && priv->phy_access.hdlr)
		 priv->phy_access.hdlr(priv->phy_access.data, 1);

	/* Perform clause 22 address extension or clause 45 address transaction */
	ret = unimac_mdio_address(priv, phy_id, reg);
	if (ret)
		goto _access;

	/* Prepare the write operation */
	if (reg & MII_ADDR_C45)
		cmd = MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
			(reg & MDIO_REG_MASK) | val;
	else if (reg > 0x1F)
		cmd = MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
			((0x10 + (reg & 0xF)) << MDIO_REG_SHIFT) | val;
	else
		cmd = MDIO_WR | (phy_id << MDIO_PMD_SHIFT) |
			(reg << MDIO_REG_SHIFT) | val;

	/* Start MDIO transaction */
	ret = unimac_mdio_start(priv, cmd);

_access:
	/* Switch off MDIO for phy_id */
	if ((phy_id == priv->phy_access.addr) && priv->phy_access.hdlr)
		 priv->phy_access.hdlr(priv->phy_access.data, 0);

	return ret;
}

/* Workaround for integrated BCM7xxx Gigabit PHYs which have a problem with
 * their internal MDIO management controller making them fail to successfully
 * be read from or written to for the first transaction.  We insert a dummy
 * BMSR read here to make sure that phy_get_device() and get_phy_id() can
 * correctly read the PHY MII_PHYSID1/2 registers and successfully register a
 * PHY device for this peripheral.
 *
 * Once the PHY driver is registered, we can workaround subsequent reads from
 * there (e.g: during system-wide power management).
 *
 * bus->reset is invoked before mdiobus_scan during mdiobus_register and is
 * therefore the right location to stick that workaround. Since we do not want
 * to read from non-existing PHYs, we either use bus->phy_mask or do a manual
 * Device Tree scan to limit the search area.
 */
static int unimac_mdio_reset(struct mii_bus *bus)
{
	struct unimac_mdio_priv *priv = bus->priv;
	struct device_node *np = bus->dev.of_node;
	struct device_node *child;
	u32 read_mask = 0;
	int addr;

	if (!np) {
		read_mask = ~bus->phy_mask;
	} else {
		for_each_available_child_of_node(np, child) {
			addr = of_mdio_parse_addr(&bus->dev, child);
			if (addr < 0)
				continue;

			if (of_device_is_compatible(child,
						"ethernet-phy-ieee802.3-c45")) {
				priv->phy_c45_mask |= (1 << addr);
				continue;
			}

			read_mask |= 1 << addr;
		}
	}

	for (addr = 0; addr < PHY_MAX_ADDR; addr++) {
		if (read_mask & 1 << addr)
			mdiobus_read(bus, addr, MII_BMSR);
	}

	return 0;
}

static int unimac_mdio_probe(struct platform_device *pdev)
{
	struct unimac_mdio_priv *priv;
	struct device_node *np;
	struct mii_bus *bus;
	struct resource *r;
	void __iomem *ctrl;
	const char *str;
	int ret;
	u32 iface_ctrl_reg;
	bool use_external_phy;

	np = pdev->dev.of_node;

	priv = devm_kzalloc(&pdev->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);

	/* Just ioremap, as this MDIO block is usually integrated into an
	 * Ethernet MAC controller register range
	 */
	priv->base = devm_ioremap(&pdev->dev, r->start, resource_size(r));
	if (!priv->base) {
		dev_err(&pdev->dev, "failed to remap register\n");
		return -ENOMEM;
	}

	/*
	 * The 3384 UniMAC1 MDIO bus defaults to an on-chip phy which disconnects
	 * the MDIO pads, so we have to hit the UniMAC control reg to tell it the
	 * phy's are external.
	 */
	use_external_phy = of_property_read_bool(np, "use-external-phy");
	if (use_external_phy) {
		r = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		ctrl = devm_ioremap(&pdev->dev, r->start,
				resource_size(r));
		if (!ctrl) {
			dev_err(&pdev->dev, "failed to remap register 1\n");
			return -ENOMEM;
		}

		iface_ctrl_reg = __raw_readl(ctrl);
		iface_ctrl_reg &= ~SELECT_ONCHIP_PHY;
		__raw_writel(iface_ctrl_reg, ctrl);
		devm_iounmap(&pdev->dev, ctrl);
	}

	ret = of_property_read_string(np, "dev-name", &str);
	if (ret) {
		dev_err(&pdev->dev, "failed to read dev-name\n");
		return ret;
	}

	strncpy(priv->name, str, sizeof(priv->name));
	priv->mii_bus = mdiobus_alloc();
	if (!priv->mii_bus)
		return -ENOMEM;

	bus = priv->mii_bus;
	bus->priv = priv;
	bus->name = priv->name;
	bus->parent = &pdev->dev;
	bus->read = unimac_mdio_read;
	bus->write = unimac_mdio_write;
	bus->reset = unimac_mdio_reset;
	snprintf(bus->id, MII_BUS_ID_SIZE, "%s", pdev->name);

	bus->irq = devm_kzalloc(&pdev->dev, sizeof(int) * PHY_MAX_ADDR, GFP_KERNEL);
	if (!bus->irq)
		return -ENOMEM;

	ret = of_mdiobus_register(bus, np);
	if (ret) {
		dev_err(&pdev->dev, "MDIO bus registration failed\n");
		goto out_mdio_free;
	}

	platform_set_drvdata(pdev, priv);

	dev_info(&pdev->dev, "Broadcom %s at 0x%p\n", priv->name, priv->base);

	return 0;

out_mdio_free:
	mdiobus_free(bus);
	return ret;
}

static int unimac_mdio_remove(struct platform_device *pdev)
{
	struct unimac_mdio_priv *priv = platform_get_drvdata(pdev);

	mdiobus_unregister(priv->mii_bus);
	mdiobus_free(priv->mii_bus);

	return 0;
}

static const struct of_device_id unimac_mdio_ids[] = {
	{ .compatible = "brcm,genet-mdio-v4", },
	{ .compatible = "brcm,unimac-mdio", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, unimac_mdio_ids);

static struct platform_driver unimac_mdio_driver = {
	.driver = {
		.name = "unimac-mdio",
		.of_match_table = unimac_mdio_ids,
	},
	.probe	= unimac_mdio_probe,
	.remove	= unimac_mdio_remove,
};
module_platform_driver(unimac_mdio_driver);

MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("Broadcom UniMAC MDIO bus controller");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:unimac-mdio");
