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
 * Broadcom BCM3384 Gigabit Ethernet transceiver.
 *
 * Author: Tim Ross <tross@broadcom.com>
 *
 *****************************************************************************/

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/brcmphy.h>
#include <linux/delay.h>
#include <linux/bcm_media_gw/bpcm.h>
#include <linux/brcmstb/brcmstb.h>


static void *ephy_testcntrl;
static struct bpcm_device *egphy;
static struct bpcm_device *chip_clkrst;

static int bcm3384_config_init(struct phy_device *phydev)
{
	bpcm_pwr_off(egphy);
	bpcm_pwr_on(egphy);
	bpcm_write(chip_clkrst, ZONE_CFG1(0), (1 << ZONE_CFG1_RESET_OFF_SHIFT)
		   & ZONE_CFG1_RESET_OFF_MASK);
	udelay(5);
	bpcm_assert_soft_reset(egphy);
	udelay(1);
	/* Magic #'s below are from legacy eCOS and CM bootloader code. */
	__raw_writel(0xa, ephy_testcntrl);
	udelay(1);
	__raw_writel(0x0, ephy_testcntrl);
	udelay(1);
	bpcm_deassert_soft_reset(egphy);
	/* Magic #'s below are from legacy eCOS and CM bootloader code. */
	phy_write(phydev, MII_BCM54XX_EXP_SEL, 0x6032);
	phy_write(phydev, MII_BCM54XX_EXP_DATA, 0x0014);
	return 0;
}

static int bcm3384_suspend(struct phy_device *phydev)
{
	bpcm_pwr_off(egphy);
	return 0;
}

static int bcm3384_resume(struct phy_device *phydev)
{
	bcm3384_config_init(phydev);
	return 0;
}

static int bcm3384_probe(struct phy_device *phydev)
{
	int status;
	struct device *dev = &phydev->dev;

	status = bpcm_get_device("bpcm-egphy", &egphy);
	if (status) {
		dev_err(dev, "Failed to get egphy BPCM.\n");
		goto done;
	}
	status = bpcm_get_device("bpcm-chip-clkrst", &chip_clkrst);
	if (status) {
		dev_err(dev, "Failed to get chip-clkrst BPCM.\n");
		goto done;
	}
	ephy_testcntrl = ioremap(BCHP_TIMER_PER_EPHY_TESTCNTRL, 4);
	if (!ephy_testcntrl) {
		dev_err(dev, "Failure mapping ephy testcntrl reg\n");
		status = -ENOMEM;
		goto done;
	}

done:
	return status;
}

static void bcm3384_remove(struct phy_device *phydev)
{
	iounmap(ephy_testcntrl);
}

static struct phy_driver bcm3384_drivers[] = {
	{
		.phy_id 	= PHY_ID_BCM3384,
		.phy_id_mask	= 0xfffffff0,
		.name		= "Broadcom BCM3384",
		.features	= PHY_GBIT_FEATURES |
				  SUPPORTED_Pause |
				  SUPPORTED_Asym_Pause,
		.flags		= PHY_IS_INTERNAL,
		.probe		= bcm3384_probe,
		.remove		= bcm3384_remove,
		.config_init	= bcm3384_config_init,
		.config_aneg	= genphy_config_aneg,
		.read_status	= genphy_read_status,
		.suspend	= bcm3384_suspend,
		.resume		= bcm3384_resume,
		.driver		= { .owner = THIS_MODULE },
	}
};

static int __init bcm3384_init(void)
{
	return phy_drivers_register(bcm3384_drivers,
		ARRAY_SIZE(bcm3384_drivers));
}

static void __exit bcm3384_exit(void)
{
	phy_drivers_unregister(bcm3384_drivers,
		ARRAY_SIZE(bcm3384_drivers));
}

module_init(bcm3384_init);
module_exit(bcm3384_exit);

static struct mdio_device_id __maybe_unused bcm3384_tbl[] = {
	{ PHY_ID_BCM3384, 0xfffffff0 },
	{ }
};

MODULE_DEVICE_TABLE(mdio, bcm3384_tbl);
MODULE_DESCRIPTION("Broadcom BCM3384 Internal PHY Driver");
MODULE_AUTHOR("Tim Ross");
MODULE_LICENSE("GPL");
