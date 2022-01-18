 /****************************************************************************
 *
 * Copyright (c) 2015 Broadcom Corporation
 *
 * Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to
 * you under the terms of the GNU General Public License version 2 (the
 * "GPL"), available at [http://www.broadcom.com/licenses/GPLv2.php], with
 * the following added to such license:
 *
 * As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy
 * and distribute the resulting executable under terms of your choice,
 * provided that you also meet, for each linked independent module, the
 * terms and conditions of the license of that module. An independent
 * module is a module which is not derived from this software. The special
 * exception does not apply to any modifications of the software.
 *
 * Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a
 * license other than the GPL, without Broadcom's express prior written
 * consent.
 *
 ****************************************************************************
 * Broadcom BCM54210 Gigabit Ethernet transceivers.
 *
 * Author: Tim Ross <tross@broadcom.com>
 *	   Inspired by code written by Maciej W. Rozycki.
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/brcmphy.h>


MODULE_DESCRIPTION("Broadcom BCM54210 PHY driver");
MODULE_AUTHOR("Tim Ross");
MODULE_LICENSE("GPL");

static int bcm54210_config_init(struct phy_device *phydev)
{
	int reg, err, val;

	reg = phy_read(phydev, MII_BCM54XX_ECR);
	if (reg < 0)
		return reg;

	/* Mask interrupts globally.  */
	reg |= MII_BCM54XX_ECR_IM;
	err = phy_write(phydev, MII_BCM54XX_ECR, reg);
	if (err < 0)
		return err;

	/* Unmask events we are interested in.  */
	reg = ~(MII_BCM54XX_INT_DUPLEX |
		MII_BCM54XX_INT_SPEED |
		MII_BCM54XX_INT_LINK);
	err = phy_write(phydev, MII_BCM54XX_IMR, reg);
	if (err < 0)
		return err;

	/* Enable in-band signalling */
	val = bcm54xx_auxctl_read(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_MISC);
	val &= ~MII_BCM54XX_AUXCTL_MISC_OOBS_DIS;
	err = bcm54xx_auxctl_write(phydev, MII_BCM54XX_AUXCTL_SHDWSEL_MISC,
				   val);
	if (err < 0)
		return err;

	/* Disable GTXCLK delay */
	val = bcm54xx_shadow_read(phydev, BCM54XX_SHD_CAC);
	val &= ~BCM54XX_SHD_CAC_GTXCLK_DEL_EN;
	err = bcm54xx_shadow_write(phydev, BCM54XX_SHD_CAC, val);
	if (err < 0)
		return err;

	return 0;
}

static int bcm54210_ack_interrupt(struct phy_device *phydev)
{
	int reg;

	/* Clear pending interrupts.  */
	reg = phy_read(phydev, MII_BCM54XX_ISR);
	if (reg < 0)
		return reg;

	return 0;
}

static int bcm54210_config_intr(struct phy_device *phydev)
{
	int reg, err;

	reg = phy_read(phydev, MII_BCM54XX_ECR);
	if (reg < 0)
		return reg;

	if (phydev->interrupts == PHY_INTERRUPT_ENABLED)
		reg &= ~MII_BCM54XX_ECR_IM;
	else
		reg |= MII_BCM54XX_ECR_IM;

	err = phy_write(phydev, MII_BCM54XX_ECR, reg);
	return err;
}

static struct phy_driver bcm54210_drivers[] = {
	{
		.phy_id		= PHY_ID_BCM54210,
		.phy_id_mask	= 0xfffffff0,
		.name		= "Broadcom BCM54210",
		.features	= PHY_GBIT_FEATURES |
				  SUPPORTED_Pause | SUPPORTED_Asym_Pause,
		.flags		= PHY_HAS_MAGICANEG | PHY_HAS_INTERRUPT,
		.config_init	= bcm54210_config_init,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.ack_interrupt	= bcm54210_ack_interrupt,
		.config_intr	= bcm54210_config_intr,
		.driver		= { .owner = THIS_MODULE },
	}
};

static int __init bcm54210_init(void)
{
	return phy_drivers_register(bcm54210_drivers,
		ARRAY_SIZE(bcm54210_drivers));
}

static void __exit bcm54210_exit(void)
{
	phy_drivers_unregister(bcm54210_drivers,
		ARRAY_SIZE(bcm54210_drivers));
}

module_init(bcm54210_init);
module_exit(bcm54210_exit);

static struct mdio_device_id __maybe_unused bcm54210_tbl[] = {
	{ PHY_ID_BCM54210, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, bcm54210_tbl);
