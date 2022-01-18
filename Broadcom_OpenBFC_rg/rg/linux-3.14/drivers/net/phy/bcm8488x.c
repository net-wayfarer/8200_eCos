 /****************************************************************************
 *
 * Copyright (c) 2017 Broadcom Corporation
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
 * Broadcom BCM8488x Multi-Gigabit Ethernet external transceivers.
 *
 * Author: Ravi Patel <ravi.patel@broadcom.com>
 ****************************************************************************/

#include <linux/module.h>
#include <linux/phy.h>
#include <linux/delay.h>
#include <linux/mdio.h>
#include <linux/brcmphy.h>
#include <linux/firmware.h>

/* AN 10GBASE-T control register. */
#define MDIO_AN_10GBT_CTRL_ADV5G	0x0100	/* Advertise 5GBASE-T */
#define MDIO_AN_10GBT_CTRL_ADV2P5G	0x0080	/* Advertise 2.5GBASE-T */
#define MDIO_AN_10GBT_CTRL_ADV		(MDIO_AN_10GBT_CTRL_ADV10G | \
									MDIO_AN_10GBT_CTRL_ADV5G | \
									MDIO_AN_10GBT_CTRL_ADV2P5G)

/* AN 10GBASE-T status register. */
#define MDIO_AN_10GBT_STAT_LP5G		0x0040	/* LP is 5GBT capable */
#define MDIO_AN_10GBT_STAT_LP2P5G	0x0020	/* LP is 2.5GBT capable */

/* MDIO_MMD_PMAPMD registers */
#define PROCTL			0xA817
#define PROCTL_CLEAR	(0x00)
#define PROCTL_WRITE	(0x09)
#define PROCTL_DOWNLOAD	(0x38)

#define ADDR_L			0xA819
#define ADDR_H			0xA81A
#define DATA_L			0xA81B
#define DATA_H			0xA81C

/* MDIO_MMD_AN registers */
#define MDIO_MII_BASE		(0xFFE0)
#define MDIO_MII_BMCR		(MDIO_MII_BASE + MII_BMCR)
#define MDIO_MII_BMSR		(MDIO_MII_BASE + MII_BMSR)
#define MDIO_MII_ADVERTISE	(MDIO_MII_BASE + MII_ADVERTISE)
#define MDIO_MII_LPA		(MDIO_MII_BASE + MII_LPA)
#define MDIO_MII_CTRL1000	(MDIO_MII_BASE + MII_CTRL1000)
#define MDIO_MII_STAT1000	(MDIO_MII_BASE + MII_STAT1000)
#define MDIO_MII_ESTATUS	(MDIO_MII_BASE + MII_ESTATUS)

/* MDIO_MMD_VEND1 registers */
#define STATUS			0x400D
#define MAC_LINK			(1 << 13)
#define LINESIDE_XFI_PHY	(1 << 11)
#define COPPER_LINK			(1 << 5)
#define COPPER_SPEED_SHIFT	(2)
#define COPPER_SPEED_MASK	(7 << COPPER_SPEED_SHIFT)
#define COPPER_SPEED_2P5G	(1 << COPPER_SPEED_SHIFT)
#define COPPER_SPEED_100M	(2 << COPPER_SPEED_SHIFT)
#define COPPER_SPEED_5G		(3 << COPPER_SPEED_SHIFT)
#define COPPER_SPEED_1G		(4 << COPPER_SPEED_SHIFT)
#define COPPER_SPEED_10G	(6 << COPPER_SPEED_SHIFT)
#define COPPER_DETECTED		(1 << 1)

#define FW_REV			0x400F
#define FW_BUILD_VER_SHIFT	(12)
#define FW_BUILD_VER_MASK	(0xF << FW_BUILD_VER_SHIFT)
#define FW_MAIN_VER_SHIFT	(7)
#define FW_MAIN_VER_MASK	(0x1F << FW_MAIN_VER_SHIFT)
#define FW_BRANCH_VER_MASK	(0x7F)

#define FW_DATE			0x4010
#define FW_YEAR4_SHIFT	(13)
#define FW_YEAR4_MASK	(1 << FW_YEAR4_SHIFT)
#define FW_MONTH_SHIFT	(9)
#define FW_MONTH_MASK	(0xF << FW_MONTH_SHIFT)
#define FW_DAY_SHIFT	(4)
#define FW_DAY_MASK		(0x1F << FW_DAY_SHIFT)
#define FW_YEAR_MASK	(0xF)

/* BCM8488 Model IDs */
enum bcm8488x_model_ids {
	BCM84880_A0 = 0x18,
	BCM84880_B0 = 0x19,
};

static struct bcm8488x_phy {
	u32 phy_id;
	const char *model;
	const char *fw_name;
} bcm8488x_phys[] = {
	{
		.phy_id  = PHY_ID_BCM8488X_OUI | BCM84880_A0,
		.model   = "BCM84880 A0",
		.fw_name = "brcm/BCM8488-A0-TCM.bin",
	},
	{
		.phy_id  = PHY_ID_BCM8488X_OUI | BCM84880_B0,
		.model   = "BCM84880 B0",
		.fw_name = "brcm/BCM8488-B0-TCM.bin",
	},
};

static unsigned short bcm8488x_halt_seq1[] = {
	0x4188, 0x48f0,
	0x4186, 0x8000,
	0x4181, 0x017c,
	0x4181, 0x0040,
};

static unsigned short bcm8488x_halt_seq2[] = {
	/* ADDR_L, ADDR_H, DATA_L, DATA_H and then PROCTL_WRITE */
	0x0000, 0xc300,
	0x0010, 0x0000,
	0x0000,	0xffff,
	0x1018,	0xe59f,
	0x0004,	0xffff,
	0x1f11,	0xee09,
	0x0008,	0xffff,
	0x0000,	0xe3a0,
	0x000c,	0xffff,
	0x1806,	0xe3a0,
	0x0010,	0xffff,
	0x0002,	0xe8a0,
	0x0014,	0xffff,
	0x0001,	0xe150,
	0x0018,	0xffff,
	0xfffc,	0x3aff,
	0x001c,	0xffff,
	0xfffe,	0xeaff,
	0x0020,	0xffff,
	0x0021,	0x0004,
};

static unsigned short bcm8488x_halt_seq3[] = {
	0x4181, 0x0000,
};

static struct bcm8488x_phy *bcm8488x_phy_id_match(u32 phy_id)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(bcm8488x_phys); i++)
		if (bcm8488x_phys[i].phy_id == phy_id)
			break;

	return i < ARRAY_SIZE(bcm8488x_phys) ? &bcm8488x_phys[i] : NULL;
}

static void bcm8488x_processor_halt(struct phy_device *phydev)
{
	int i;

	/* Start loading seq1 in DEVAD 30 */
	for (i = 0; i < ARRAY_SIZE(bcm8488x_halt_seq1); i+=2)
		phy_write_mmd(phydev, MDIO_MMD_VEND1, bcm8488x_halt_seq1[i],
				bcm8488x_halt_seq1[i + 1]);

	/* Start loading seq2 in DEVAD 1 */
	for (i = 0; i < ARRAY_SIZE(bcm8488x_halt_seq2); i+=4) {
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_L,
				bcm8488x_halt_seq2[i]);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_H,
				bcm8488x_halt_seq2[i + 1]);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_L,
				bcm8488x_halt_seq2[i + 2]);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_H,
				bcm8488x_halt_seq2[i + 3]);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, PROCTL,
				PROCTL_WRITE);
	}

	/* Start loading seq3 in DEVAD 30 */
	for (i = 0; i < ARRAY_SIZE(bcm8488x_halt_seq3); i+=2)
		phy_write_mmd(phydev, MDIO_MMD_VEND1, bcm8488x_halt_seq3[i],
				bcm8488x_halt_seq3[i + 1]);
}

static int bcm8488x_firmware_load(struct phy_device *phydev, const char *name)
{
	int ret, i;
	const struct firmware *fw;
	const u8 *fw_data;
	unsigned short data_low, data_high;
	unsigned int fw_cksum = 0;

	/* Request BCM8488x firmware from user space */
	ret = request_firmware(&fw, name, &phydev->dev);
	if (ret) {
		dev_err(&phydev->dev, "Request firmware %s failed\n", name);
		return ret;
	}

	/* Start loading firmware into BCM8488x device OCM */
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_H, 0);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_L, 0);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, PROCTL, PROCTL_DOWNLOAD);

	fw_data = fw->data;
	for (i = 0; i < fw->size; i+=4) {
		data_low = (fw_data[i + 1] << 8) | fw_data[i];
		data_high = (fw_data[i + 3] << 8) | fw_data[i + 2];
		fw_cksum += ((data_high << 16) | data_low);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_H, data_high);
		phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_L, data_low);
	}

	dev_info(&phydev->dev, "PHY Firmware Size %d:0x%x bytes\n", i, i);
	dev_info(&phydev->dev, "PHY Firmware Simple Checksum: 0x%08x\n", fw_cksum);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, PROCTL, PROCTL_CLEAR);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_L, 0x0000);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, ADDR_H, 0xc300);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_L, 0x0000);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, DATA_H, 0x0000);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, PROCTL, PROCTL_WRITE);

	return 0;
}

static void bcm8488x_processor_start(struct phy_device *phydev)
{
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, 0xa008, 0x0000);
	phy_write_mmd(phydev, MDIO_MMD_VEND1, 0x8004, 0x5555);
	phy_write_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1, BMCR_RESET);
}

static int bcm8488x_verify_loadcrc(struct phy_device *phydev)
{
	int ret, timeout;

	timeout = 1000;
	do {
		ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_CTRL1);
		if (ret < 0) {
			dev_err(&phydev->dev, "Error verifying firmware is loaded\n");
			return ret;
		}

		usleep_range(1000, 2000);
	} while (ret != 0x2040 && --timeout);

	if (!timeout) {
		dev_err(&phydev->dev, "Timeout verifying firmware is loaded\n");
		return -ETIMEDOUT;
	}

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, STATUS);
	if (ret < 0) {
		dev_err(&phydev->dev, "Error verifying firmware crc\n");
		return ret;
	}

	if (!(ret & 0x4000)) {
		dev_err(&phydev->dev, "Firmware loaded has bad crc\n");
		return -EIO;
	}

	dev_info(&phydev->dev, "PHY Firmware is loaded with Good CRC.\n");

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, FW_REV);
	if (ret < 0) {
		dev_err(&phydev->dev, "Error reading firmware ver\n");
		return ret;
	}

	dev_info(&phydev->dev, "PHY Firmware Version(Main.Branch.Build): "
			"%d.%02d.%02d\n",
			(ret & FW_MAIN_VER_MASK) >> FW_MAIN_VER_SHIFT,
			(ret & FW_BRANCH_VER_MASK),
			(ret & FW_BUILD_VER_MASK) >> FW_BUILD_VER_SHIFT);

	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, FW_DATE);
	if (ret < 0) {
		dev_err(&phydev->dev, "\nError reading firmware date\n");
		return ret;
	}

	dev_info(&phydev->dev, "PHY Firmware Date(MM/DD/YYYY): "
			"%02d/%02d/%04d\n",
			(ret & FW_MONTH_MASK) >> FW_MONTH_SHIFT,
			(ret & FW_DAY_MASK) >> FW_DAY_SHIFT,
			((ret & FW_YEAR4_MASK) >> (FW_YEAR4_SHIFT - FW_DAY_SHIFT)) +
			(ret & FW_YEAR_MASK) + 2000);

	return 0;
}

static int bcm8488x_init_supported(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	unsigned long *supported = phyxcvr->phymode.supported;
	int ret;

	brcmphy_zero_phymode(supported);
	brcmphy_add_phymode(supported, Pause);
	brcmphy_add_phymode(supported, Asym_Pause);
	ret = phy_read_mmd(phydev, MDIO_MMD_VEND1, STATUS);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading vendor1 status\n", ret);
		return ret;
	}

	if (ret & LINESIDE_XFI_PHY)
		brcmphy_add_phymode(supported, FIBRE);
	else
		brcmphy_add_phymode(supported, TP);

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_DEVS1);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading devices in package\n", ret);
		return ret;
	}

	if (ret & 0x0001)
		brcmphy_add_phymode(supported, MII);

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading basic mode status\n", ret);
		return ret;
	}

	if (ret & MDIO_AN_STAT1_ABLE)
		brcmphy_add_phymode(supported, Autoneg);

	ret = phy_read_mmd(phydev, MDIO_MMD_PMAPMD, MDIO_SPEED);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading speed ability\n", ret);
		return ret;
	}

	if (ret & MDIO_SPEED_10G)
		brcmphy_add_phymode(supported, 10000baseT_Full);

	if (ret & 0x4000)
		brcmphy_add_phymode(supported, 5000baseT_Full);

	if (ret & 0x2000)
		brcmphy_add_phymode(supported, 2500baseT_Full);

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMSR);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading basic mode status\n", ret);
		return ret;
	}

	if (ret & BMSR_ANEGCAPABLE)
		brcmphy_add_phymode(supported, Autoneg);

	if (ret & BMSR_100FULL)
		brcmphy_add_phymode(supported, 100baseT_Full);

	if (ret & BMSR_100HALF)
		brcmphy_add_phymode(supported, 100baseT_Half);

	if (ret & BMSR_ESTATEN) {
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_ESTATUS);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d reading extended status\n", ret);
			return ret;
		}

		if (ret & ESTATUS_1000_TFULL)
			brcmphy_add_phymode(supported, 1000baseT_Full);

		if (ret & ESTATUS_1000_THALF)
			brcmphy_add_phymode(supported, 1000baseT_Half);
	}

	ethtool_convert_link_mode_to_legacy_u32(&phydev->supported, supported);

	return 0;
}

static int bcm8488x_restart_aneg(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	unsigned long *advertising = phyxcvr->phymode.advertising;
	u32 features = phydev->advertising;
	int ret = -1;

	if (brcmphy_test_phymode(advertising, 10000baseT_Full) ||
			brcmphy_test_phymode(advertising, 5000baseT_Full) ||
			brcmphy_test_phymode(advertising, 2500baseT_Full)) {
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d reading AN basic mode control\n", ret);
			return ret;
		}

		ret |= (BMCR_ANENABLE | BMCR_ANRESTART);
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_CTRL1, ret);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d writing AN basic mode control\n", ret);
			return ret;
		}

	} else if (features & (PHY_1000BT_FEATURES | PHY_100BT_FEATURES)) {
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMCR);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d reading basic mode control\n", ret);
			return ret;
		}

		ret |= (BMCR_ANENABLE | BMCR_ANRESTART);
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMCR, ret);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d writing basic mode control\n", ret);
			return ret;
		}
	}

	return ret;
}

static int bcm8488x_config_advert(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	struct brcmphy_core *phycore = phyxcvr->phycore;
	unsigned long *advertising = phyxcvr->phymode.advertising;
	int ret, adv, changed = 0;

	if (brcmphy_empty_phymode(advertising))
		brcmphy_copy_phymode(advertising, phyxcvr->phymode.supported);
	else
		brcmphy_and_phymode(advertising, phyxcvr->phymode.supported);

	if (phycore)
		brcmphy_and_phymode(advertising, phycore->phymode.supported);

	ethtool_convert_link_mode_to_legacy_u32(&phydev->advertising, advertising);
	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading 10GBASE-T auto-negotiation control\n", ret);
		return ret;
	}

	adv = ret;
	if (brcmphy_test_phymode(advertising, 10000baseT_Full))
		adv |= (MDIO_AN_10GBT_CTRL_ADV10G);
	else
		adv &= ~(MDIO_AN_10GBT_CTRL_ADV10G);

	if (brcmphy_test_phymode(advertising, 5000baseT_Full))
		adv |= (MDIO_AN_10GBT_CTRL_ADV5G);
	else
		adv &= ~(MDIO_AN_10GBT_CTRL_ADV5G);

	if (brcmphy_test_phymode(advertising, 2500baseT_Full))
		adv |= (MDIO_AN_10GBT_CTRL_ADV2P5G);
	else
		adv &= ~(MDIO_AN_10GBT_CTRL_ADV2P5G);

	if (ret != adv) {
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL, adv);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d writing 10GBASE-T auto-negotiation control\n", ret);
			return ret;
		}

		changed = 1;
	}

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_CTRL1000);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading 1000BASE-T control\n", ret);
		return ret;
	}

	adv = ret;
	if (brcmphy_test_phymode(advertising, 1000baseT_Full))
		adv |= (ADVERTISE_1000FULL);
	else
		adv &= ~(ADVERTISE_1000FULL);

	if (brcmphy_test_phymode(advertising, 1000baseT_Half))
		adv |= (ADVERTISE_1000HALF);
	else
		adv &= ~(ADVERTISE_1000HALF);

	if (ret != adv) {
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_MII_CTRL1000, adv);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d writing 1000BASE-T control\n", ret);
			return ret;
		}

		changed = 1;
	}

	ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_ADVERTISE);
	if (ret < 0) {
		dev_err(&phydev->dev,
				"Error %d reading Advertisement control\n", ret);
		return ret;
	}

	adv = ret;
	if (brcmphy_test_phymode(advertising, Asym_Pause))
		adv |= (ADVERTISE_PAUSE_ASYM);
	else
		adv &= ~(ADVERTISE_PAUSE_ASYM);

	if (brcmphy_test_phymode(advertising, Pause))
		adv |= (ADVERTISE_PAUSE_CAP);
	else
		adv &= ~(ADVERTISE_PAUSE_CAP);

	if (brcmphy_test_phymode(advertising, 100baseT_Full))
		adv |= (ADVERTISE_100FULL);
	else
		adv &= ~(ADVERTISE_100FULL);

	if (brcmphy_test_phymode(advertising, 100baseT_Half))
		adv |= (ADVERTISE_100HALF);
	else
		adv &= ~(ADVERTISE_100HALF);

	if (ret != adv) {
		ret = phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_MII_ADVERTISE, adv);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d writing Advertisement control\n", ret);
			return ret;
		}

		changed = 1;
	}

	return changed;
}

static int bcm8488x_config_init(struct phy_device *phydev)
{
	struct bcm8488x_phy *phy;
	int ret;

	phy = bcm8488x_phy_id_match(phydev->phy_id);
	if (!phy)
		return -ENODEV;

	dev_info(&phydev->dev, "%s: initializing\n", phy->model);
	bcm8488x_processor_halt(phydev);
	ret = bcm8488x_firmware_load(phydev, phy->fw_name);
	if (ret < 0)
		return ret;

	bcm8488x_processor_start(phydev);

	ret = bcm8488x_verify_loadcrc(phydev);
	if (ret < 0)
		return ret;

	ret = bcm8488x_init_supported(phydev);
	if (ret < 0)
		return ret;

	ret = bcm8488x_config_advert(phydev);
	if (ret < 0)
		return ret;

	dev_info(&phydev->dev, "%s: initialized\n", phy->model);

	return 0;
}

static int bcm8488x_config_forced(struct phy_device *phydev)
{
	int ctl = 0;

	if (phydev->speed == SPEED_1000)
		ctl |= BMCR_SPEED1000;
	else if (phydev->speed == SPEED_100)
		ctl |= BMCR_SPEED100;
	else {
		dev_err(&phydev->dev, "Unsupported %d Mbps forced link speed\n",
				phydev->speed);
		return -EINVAL;
	}

	if (phydev->duplex == DUPLEX_FULL)
		ctl |= BMCR_FULLDPLX;

	phydev->pause = 0;
	phydev->asym_pause = 0;

	return phy_write_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMCR, ctl);
}

static int bcm8488x_config_aneg(struct phy_device *phydev)
{
	int result;

	if (AUTONEG_ENABLE != phydev->autoneg)
		return bcm8488x_config_forced(phydev);

	result = bcm8488x_config_advert(phydev);
	if (result < 0) /* error */
		return result;
	if (result == 0) {
		/* Advertisement hasn't changed, but maybe aneg was never on to
		 * begin with?  Or maybe phy was isolated?
		 */
		int ctl = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMCR);

		if (ctl < 0)
			return ctl;

		if (!(ctl & BMCR_ANENABLE) || (ctl & BMCR_ISOLATE))
			result = 1; /* do restart aneg */
	}

	/* Only restart aneg if we are advertising something different
	 * than we were before.
	 */
	if (result > 0)
		result = bcm8488x_restart_aneg(phydev);

	return result;
}

static int bcm8488x_aneg_done(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	unsigned long *advertising = phyxcvr->phymode.advertising;
	u32 features = phydev->advertising;
	int ret = -1;

	if (brcmphy_test_phymode(advertising, 10000baseT_Full) ||
			brcmphy_test_phymode(advertising, 5000baseT_Full) ||
			brcmphy_test_phymode(advertising, 2500baseT_Full)) {
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_STAT1);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d reading AN basic mode status\n", ret);
			return ret;
		}

		ret &= BMSR_ANEGCOMPLETE;
	} else if (features & (PHY_1000BT_FEATURES | PHY_100BT_FEATURES)) {
		ret = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMSR);
		if (ret < 0) {
			dev_err(&phydev->dev,
					"Error %d reading MII basic mode control\n", ret);
			return ret;
		}

		ret &= BMSR_ANEGCOMPLETE;
	}

	return ret;
}

static int bcm8488x_update_link(struct phy_device *phydev)
{
	int status;

	/* Read status */
	status = phy_read_mmd(phydev, MDIO_MMD_VEND1, STATUS);
	if (status < 0)
		return status;

	if (status & COPPER_LINK) {
		if (phydev->link == 0)
			dev_info(&phydev->dev, "Link up status 0x%04X\n", status);
		phydev->link = 1;
	} else {
		if (phydev->link == 1)
			dev_info(&phydev->dev, "Link down status 0x%04X\n", status);
		phydev->link = 0;
	}

	return 0;
}

static int bcm8488x_read_status(struct phy_device *phydev)
{
	__ETHTOOL_DECLARE_LINK_MODE_MASK(lp);
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	struct brcmphy_core *phycore = phyxcvr->phycore;
	unsigned long *supported = phyxcvr->phymode.supported;
	unsigned long *lp_advertising = phyxcvr->phymode.lp_advertising;
	int speed[2];
	int duplex[2];
	int adv;
	int lpa;
	int common_adv_mb;
	int common_adv_gb = 0;
	int common_adv_mgb = 0;
	u32 result;

	speed[0] = phydev->speed * phydev->link;
	duplex[0] = phydev->duplex * phydev->link;

	/* Update the link, but return if there was an error */
	lpa = bcm8488x_update_link(phydev);
	if (lpa)
		return lpa;

	brcmphy_zero_phymode(lp_advertising);
	phydev->lp_advertising = 0;

	if (AUTONEG_ENABLE == phydev->autoneg) {
		if (brcmphy_test_phymode(supported, 10000baseT_Full) ||
				brcmphy_test_phymode(supported, 5000baseT_Full) ||
				brcmphy_test_phymode(supported, 2500baseT_Full)) {
			lpa = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_STAT);
			if (lpa < 0)
				return lpa;

			adv = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_AN_10GBT_CTRL);
			if (adv < 0)
				return adv;

			adv &= (MDIO_AN_10GBT_CTRL_ADV);
			if (adv & MDIO_AN_10GBT_CTRL_ADV10G)
				adv = (adv & ~MDIO_AN_10GBT_CTRL_ADV10G) |
					(MDIO_AN_10GBT_CTRL_ADV10G << 1);

			common_adv_mgb = lpa & adv >> 2;
			if (lpa & MDIO_AN_10GBT_STAT_LP10G)
				brcmphy_add_phymode(lp_advertising, 10000baseT_Full);

			if (lpa & MDIO_AN_10GBT_STAT_LP5G)
				brcmphy_add_phymode(lp_advertising, 5000baseT_Full);

			if (lpa & MDIO_AN_10GBT_STAT_LP2P5G)
				brcmphy_add_phymode(lp_advertising, 2500baseT_Full);

			ethtool_convert_link_mode_to_legacy_u32(&phydev->lp_advertising,
					lp_advertising);
		}

		if (brcmphy_test_phymode(supported, 1000baseT_Full) ||
				brcmphy_test_phymode(supported, 1000baseT_Half)) {
			lpa = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_STAT1000);
			if (lpa < 0)
				return lpa;

			adv = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_CTRL1000);
			if (adv < 0)
				return adv;

			common_adv_gb = lpa & adv << 2;
			result = mii_stat1000_to_ethtool_lpa_t(lpa);
			ethtool_convert_legacy_u32_to_link_mode(lp, result);
			brcmphy_or_phymode(lp_advertising, lp);
			ethtool_convert_link_mode_to_legacy_u32(&phydev->lp_advertising,
					lp_advertising);
		}

		lpa = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_LPA);
		if (lpa < 0)
			return lpa;

		adv = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_ADVERTISE);
		if (adv < 0)
			return adv;

		common_adv_mb = lpa & adv;
		result = mii_lpa_to_ethtool_lpa_t(lpa);
		ethtool_convert_legacy_u32_to_link_mode(lp, result);
		brcmphy_or_phymode(lp_advertising, lp);
		ethtool_convert_link_mode_to_legacy_u32(&phydev->lp_advertising,
				lp_advertising);

		phydev->speed = SPEED_100;
		phydev->duplex = DUPLEX_HALF;
		phydev->pause = 0;
		phydev->asym_pause = 0;

		if (common_adv_mgb & MDIO_AN_10GBT_STAT_LP10G) {
			phydev->speed = SPEED_10000;
			phydev->duplex = DUPLEX_FULL;

		} else if (common_adv_mgb & MDIO_AN_10GBT_STAT_LP5G) {
			phydev->speed = SPEED_5000;
			phydev->duplex = DUPLEX_FULL;

		} else if (common_adv_mgb & MDIO_AN_10GBT_STAT_LP2P5G) {
			phydev->speed = SPEED_2500;
			phydev->duplex = DUPLEX_FULL;

		} else if (common_adv_gb & (LPA_1000FULL | LPA_1000HALF)) {
			phydev->speed = SPEED_1000;
			if (common_adv_gb & LPA_1000FULL)
				phydev->duplex = DUPLEX_FULL;

		} else if (common_adv_mb & LPA_100FULL)
				phydev->duplex = DUPLEX_FULL;

		if (phydev->duplex == DUPLEX_FULL) {
			phydev->pause = lpa & LPA_PAUSE_CAP ? 1 : 0;
			phydev->asym_pause = lpa & LPA_PAUSE_ASYM ? 1 : 0;
		}
	} else {
		int bmcr = phy_read_mmd(phydev, MDIO_MMD_AN, MDIO_MII_BMCR);

		if (bmcr < 0)
			return bmcr;

		if (bmcr & BMCR_FULLDPLX)
			phydev->duplex = DUPLEX_FULL;
		else
			phydev->duplex = DUPLEX_HALF;

		if (bmcr & BMCR_SPEED1000)
			phydev->speed = SPEED_1000;
		else
			phydev->speed = SPEED_100;

		phydev->pause = 0;
		phydev->asym_pause = 0;
	}

	if (phycore && phycore->media_speed) {
		speed[1] = phydev->speed * phydev->link;
		duplex[1] = phydev->duplex * phydev->link;
		if (phydev->link && ((speed[0] != speed[1]) ||
				(duplex[0] != duplex[1]))) {
			if (brcmphy_test_phymode(supported, FIBRE))
				lpa = PORT_FIBRE;
			else
				lpa = PORT_TP;

			phycore->media_speed(phycore, lpa, speed[1], duplex[1]);
		}
	}

	return 0;
}

static int bcm8488x_match_phy_device(struct phy_device *phydev)
{
	u32 phy_id = phydev->c45_ids.device_ids[3];
	int match = 0;

	if (((phy_id ^ PHY_ID_BCM8488X_OUI) & PHY_ID_BCM8488X_MASK) == 0) {
		if (!bcm8488x_phy_id_match(phy_id))
			dev_info(&phydev->dev, "phy_id 0x%08x un-supported\n", phy_id);
		else
			match = 1;
	}

	if (match)
		phydev->phy_id = phy_id;

	return match;
}

static void bcm8488x_pma_pcs_mii_update(struct phy_device *phydev,
		u16 reg, u16 mask, u16 val)
{
	u16 devad[3] = {MDIO_MMD_PMAPMD, MDIO_MMD_PCS, MDIO_MMD_AN};
	u16 regad[3] = {reg, reg, MDIO_MII_BASE | reg};
	int i, data;

	for (i = 0; i < 3; i++) {
		data = phy_read_mmd(phydev, devad[i], regad[i]);
		data = (data & ~mask) | val;
		phy_write_mmd(phydev, devad[i], regad[i], data);
	}
}

static int bcm8488x_suspend(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	struct brcmphy_core *phycore = phyxcvr->phycore;

	mutex_lock(&phydev->lock);
	bcm8488x_pma_pcs_mii_update(phydev, MDIO_CTRL1, BMCR_PDOWN, BMCR_PDOWN);
	mutex_unlock(&phydev->lock);
	if (phycore && phycore->suspend)
		phycore->suspend(phycore);

	return 0;
}

int bcm8488x_resume(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr = dev_get_drvdata(&phydev->dev);
	struct brcmphy_core *phycore = phyxcvr->phycore;

	if (phycore && phycore->resume)
		phycore->resume(phycore);

	mutex_lock(&phydev->lock);
	bcm8488x_pma_pcs_mii_update(phydev, MDIO_CTRL1, BMCR_PDOWN, 0);
	mutex_unlock(&phydev->lock);

	return 0;
}

static int bcm8488x_probe(struct phy_device *phydev)
{
	struct brcmphy_xcvr *phyxcvr;

	phyxcvr = devm_kzalloc(&phydev->dev, sizeof(*phyxcvr), GFP_KERNEL);
	if (!phyxcvr)
		return -ENOMEM;

	dev_set_drvdata(&phydev->dev, phyxcvr);
	dev_info(&phydev->dev, "probed\n");

	return 0;
}

static void bcm8488x_remove(struct phy_device *phydev)
{
	bcm8488x_suspend(phydev);
	bcm8488x_processor_halt(phydev);
	dev_info(&phydev->dev, "removed\n");
}

static void bcm8488x_shutdown(struct device *dev)
{
	struct phy_device *phydev = to_phy_device(dev);

	bcm8488x_remove(phydev);
	dev_info(dev, "shutdown\n");
}

#define BCM8488X_GPHY(_oui, _name)				\
{												\
	.phy_id		= (_oui),						\
	.name		= _name,						\
	.phy_id_mask	= PHY_ID_BCM8488X_MASK,		\
	.soft_reset		= genphy_no_soft_reset,		\
	.config_init	= bcm8488x_config_init,		\
	.probe			= bcm8488x_probe,			\
	.suspend		= bcm8488x_suspend,			\
	.resume			= bcm8488x_resume,			\
	.config_aneg	= bcm8488x_config_aneg,		\
	.aneg_done		= bcm8488x_aneg_done,		\
	.update_link	= bcm8488x_update_link,		\
	.read_status	= bcm8488x_read_status,		\
	.remove			= bcm8488x_remove,			\
	.match_phy_device = bcm8488x_match_phy_device,		\
	.driver		= { .owner = THIS_MODULE,		\
				    .shutdown = bcm8488x_shutdown },		\
}

static struct phy_driver bcm8488x_driver[] = {
	BCM8488X_GPHY(PHY_ID_BCM8488X_OUI, "Broadcom BCM8488X"),
};

static struct mdio_device_id __maybe_unused bcm8488x_tbl[] = {
	{ PHY_ID_BCM8488X_OUI, PHY_ID_BCM8488X_MASK, },
	{ }
};

static int __init bcm8488x_phy_init(void)
{
	return phy_drivers_register(bcm8488x_driver,
			ARRAY_SIZE(bcm8488x_driver));
}

static void __exit bcm8488x_phy_exit(void)
{
	phy_drivers_unregister(bcm8488x_driver,
			ARRAY_SIZE(bcm8488x_driver));
}

module_init(bcm8488x_phy_init);
module_exit(bcm8488x_phy_exit);

MODULE_DEVICE_TABLE(mdio, bcm8488x_tbl);

MODULE_DESCRIPTION("Broadcom BCM8488x external PHY driver");
MODULE_AUTHOR("Ravi Patel <ravi.patel@broadcom.com>");
MODULE_LICENSE("GPL");
