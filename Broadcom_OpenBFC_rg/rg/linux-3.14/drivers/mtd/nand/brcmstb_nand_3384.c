/*
 * Copyright © 2010-2015 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/completion.h>
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/bug.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/mm.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/of.h>
#include <linux/of_mtd.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/log2.h>

/*
 * SWLINUX-1818: this flag controls if WP stays on between erase/write
 * commands to mitigate flash corruption due to power glitches. Values:
 * 0: NAND_WP is not used or not available
 * 1: NAND_WP is set by default, cleared for erase/write operations
 * 2: NAND_WP is always cleared
 */
static int wp_on = 1;
module_param(wp_on, int, 0444);

/***********************************************************************
 * Definitions
 ***********************************************************************/

#define DRV_NAME			"brcmnand"

#define CMD_NULL			0x00
#define CMD_PAGE_READ			0x01
#define CMD_SPARE_AREA_READ		0x02
#define CMD_STATUS_READ			0x03
#define CMD_PROGRAM_PAGE		0x04
#define CMD_PROGRAM_SPARE_AREA		0x05
#define CMD_COPY_BACK			0x06
#define CMD_DEVICE_ID_READ		0x07
#define CMD_BLOCK_ERASE			0x08
#define CMD_FLASH_RESET			0x09
#define CMD_BLOCKS_LOCK			0x0a
#define CMD_BLOCKS_LOCK_DOWN		0x0b
#define CMD_BLOCKS_UNLOCK		0x0c
#define CMD_READ_BLOCKS_LOCK_STATUS	0x0d
#define CMD_PARAMETER_READ		0x0e
#define CMD_PARAMETER_CHANGE_COL	0x0f
#define CMD_LOW_LEVEL_OP		0x10

struct brcm_nand_dma_desc {
	u32 next_desc;
	u32 next_desc_ext;
	u32 cmd_irq;
	u32 dram_addr;
	u32 dram_addr_ext;
	u32 tfr_len;
	u32 total_len;
	u32 flash_addr;
	u32 flash_addr_ext;
	u32 cs;
	u32 pad2[5];
	u32 status_valid;
} __packed;

/* Bitfields for brcm_nand_dma_desc::status_valid */
#define FLASH_DMA_ECC_ERROR	(1 << 8)
#define FLASH_DMA_CORR_ERROR	(1 << 9)

/* 512B flash cache in the NAND controller HW */
#define FC_SHIFT		9U
#define FC_BYTES		512U
#define FC_WORDS		(FC_BYTES >> 2)

#define BRCMNAND_MIN_PAGESIZE	512
#define BRCMNAND_MIN_BLOCKSIZE	(8 * 1024)
#define BRCMNAND_MIN_DEVSIZE	(4ULL * 1024 * 1024)

struct brcmstb_nand_controller {
	struct nand_hw_control	controller;
	void __iomem		*nand_base;
	void __iomem		*nand_fc; /* flash cache */
	void __iomem		*flash_dma_base;
	unsigned int		irq;
	unsigned int		dma_irq;
	int			nand_version;
	const u16		*reg_offsets;
	unsigned int		reg_spacing; /* between CS1, CS2, ... regs */

	int			cmd_pending;
	bool			dma_pending;
	struct completion	done;
	struct completion	dma_done;

	/* List of NAND hosts (one for each chip-select) */
	struct list_head host_list;

	struct brcm_nand_dma_desc *dma_desc;
	dma_addr_t		dma_pa;

	/* in-memory cache of the FLASH_CACHE, used only for some commands */
	u32			flash_cache[FC_WORDS];

	u32			nand_cs_nand_select;
	u32			nand_cs_nand_xor;
	u32			corr_stat_threshold;
	u32			flash_dma_mode;

	unsigned int		max_block_size;
	const unsigned int	*block_sizes;
	unsigned int		max_page_size;
	const unsigned int	*page_sizes;
	unsigned int		max_oob;

	u32			semagic;
	u32			lock_cnt;
};

struct brcmstb_nand_cfg {
	u64			device_size;
	unsigned int		block_size;
	unsigned int		page_size;
	unsigned int		spare_area_size;
	unsigned int		device_width;
	unsigned int		col_adr_bytes;
	unsigned int		blk_adr_bytes;
	unsigned int		ful_adr_bytes;
	unsigned int		sector_size_1k;
	unsigned int		ecc_level;
	/* use for low-power standby/resume only */
	u32			acc_control;
	u32			config;
	u32			config_ext;
	u32			timing_1;
	u32			timing_2;
};

struct brcmstb_nand_host {
	struct list_head	node;
	struct device_node	*of_node;

	struct nand_chip	chip;
	struct mtd_info		mtd;
	struct platform_device	*pdev;
	int			cs;

	unsigned int		last_cmd;
	unsigned int		last_byte;
	u64			last_addr;
	struct brcmstb_nand_cfg	hwcfg;
	struct brcmstb_nand_controller *ctrl;
};

enum brcmnand_reg {
	BRCMNAND_CMD_START = 0,
	BRCMNAND_CMD_EXT_ADDRESS,
	BRCMNAND_CMD_ADDRESS,
	BRCMNAND_INTFC_STATUS,
	BRCMNAND_CS_SELECT,
	BRCMNAND_CS_XOR,
	BRCMNAND_LL_OP,
	BRCMNAND_CS0_BASE,
	BRCMNAND_CS1_BASE,		/* CS1 regs, if non-contiguous */
	BRCMNAND_CORR_THRESHOLD,
	BRCMNAND_CORR_THRESHOLD_EXT,
	BRCMNAND_UNCORR_COUNT,
	BRCMNAND_CORR_COUNT,
	BRCMNAND_CORR_EXT_ADDR,
	BRCMNAND_CORR_ADDR,
	BRCMNAND_UNCORR_EXT_ADDR,
	BRCMNAND_UNCORR_ADDR,
	BRCMNAND_SEMAPHORE,
	BRCMNAND_ID,
	BRCMNAND_ID_EXT,
	BRCMNAND_LL_RDATA,
	BRCMNAND_OOB_READ_BASE,
	BRCMNAND_OOB_READ_10_BASE,	/* offset 0x10, if non-contiguous */
	BRCMNAND_OOB_WRITE_BASE,
	BRCMNAND_OOB_WRITE_10_BASE,	/* offset 0x10, if non-contiguous */
	BRCMNAND_FC_BASE,
};

/* BRCMNAND v4.0 */
static const u16 brcmnand_regs_v40[] = {
	[BRCMNAND_CMD_START]		=  0x04,
	[BRCMNAND_CMD_EXT_ADDRESS]	=  0x08,
	[BRCMNAND_CMD_ADDRESS]		=  0x0c,
	[BRCMNAND_INTFC_STATUS]		=  0x6c,
	[BRCMNAND_CS_SELECT]		=  0x14,
	[BRCMNAND_CS_XOR]		=  0x18,
	[BRCMNAND_LL_OP]		= 0x178,
	[BRCMNAND_CS0_BASE]		=  0x40,
	[BRCMNAND_CS1_BASE]		=  0xd0,
	[BRCMNAND_CORR_THRESHOLD]	=  0x84,
	[BRCMNAND_CORR_THRESHOLD_EXT]	=     0,
	[BRCMNAND_UNCORR_COUNT]		=     0,
	[BRCMNAND_CORR_COUNT]		=     0,
	[BRCMNAND_CORR_EXT_ADDR]	=  0x70,
	[BRCMNAND_CORR_ADDR]		=  0x74,
	[BRCMNAND_UNCORR_EXT_ADDR]	=  0x78,
	[BRCMNAND_UNCORR_ADDR]		=  0x7c,
	[BRCMNAND_SEMAPHORE]		=  0x58,
	[BRCMNAND_ID]			=  0x60,
	[BRCMNAND_ID_EXT]		=  0x64,
	[BRCMNAND_LL_RDATA]		= 0x17c,
	[BRCMNAND_OOB_READ_BASE]	=  0x20,
	[BRCMNAND_OOB_READ_10_BASE]	= 0x130,
	[BRCMNAND_OOB_WRITE_BASE]	=  0x30,
	[BRCMNAND_OOB_WRITE_10_BASE]	=     0,
	[BRCMNAND_FC_BASE]		= 0x200,
};

/* BRCMNAND v5.0 */
static const u16 brcmnand_regs_v50[] = {
	[BRCMNAND_CMD_START]		=  0x04,
	[BRCMNAND_CMD_EXT_ADDRESS]	=  0x08,
	[BRCMNAND_CMD_ADDRESS]		=  0x0c,
	[BRCMNAND_INTFC_STATUS]		=  0x6c,
	[BRCMNAND_CS_SELECT]		=  0x14,
	[BRCMNAND_CS_XOR]		=  0x18,
	[BRCMNAND_LL_OP]		= 0x178,
	[BRCMNAND_CS0_BASE]		=  0x40,
	[BRCMNAND_CS1_BASE]		=  0xd0,
	[BRCMNAND_CORR_THRESHOLD]	=  0x84,
	[BRCMNAND_CORR_THRESHOLD_EXT]	=     0,
	[BRCMNAND_UNCORR_COUNT]		=     0,
	[BRCMNAND_CORR_COUNT]		=     0,
	[BRCMNAND_CORR_EXT_ADDR]	=  0x70,
	[BRCMNAND_CORR_ADDR]		=  0x74,
	[BRCMNAND_UNCORR_EXT_ADDR]	=  0x78,
	[BRCMNAND_UNCORR_ADDR]		=  0x7c,
	[BRCMNAND_SEMAPHORE]		=  0x58,
	[BRCMNAND_ID]			=  0x60,
	[BRCMNAND_ID_EXT]		=  0x64,
	[BRCMNAND_LL_RDATA]		= 0x17c,
	[BRCMNAND_OOB_READ_BASE]	=  0x20,
	[BRCMNAND_OOB_READ_10_BASE]	= 0x130,
	[BRCMNAND_OOB_WRITE_BASE]	=  0x30,
	[BRCMNAND_OOB_WRITE_10_BASE]	= 0x140,
	[BRCMNAND_FC_BASE]		= 0x400,
};

/* BRCMNAND v6.0 - v7.1 */
static const u16 brcmnand_regs_v60[] = {
	[BRCMNAND_CMD_START]		=  0x04,
	[BRCMNAND_CMD_EXT_ADDRESS]	=  0x08,
	[BRCMNAND_CMD_ADDRESS]		=  0x0c,
	[BRCMNAND_INTFC_STATUS]		=  0x14,
	[BRCMNAND_CS_SELECT]		=  0x18,
	[BRCMNAND_CS_XOR]		=  0x1c,
	[BRCMNAND_LL_OP]		=  0x20,
	[BRCMNAND_CS0_BASE]		=  0x50,
	[BRCMNAND_CS1_BASE]		=     0,
	[BRCMNAND_CORR_THRESHOLD]	=  0xc0,
	[BRCMNAND_CORR_THRESHOLD_EXT]	=  0xc4,
	[BRCMNAND_UNCORR_COUNT]		=  0xfc,
	[BRCMNAND_CORR_COUNT]		= 0x100,
	[BRCMNAND_CORR_EXT_ADDR]	= 0x10c,
	[BRCMNAND_CORR_ADDR]		= 0x110,
	[BRCMNAND_UNCORR_EXT_ADDR]	= 0x114,
	[BRCMNAND_UNCORR_ADDR]		= 0x118,
	[BRCMNAND_SEMAPHORE]		= 0x150,
	[BRCMNAND_ID]			= 0x194,
	[BRCMNAND_ID_EXT]		= 0x198,
	[BRCMNAND_LL_RDATA]		= 0x19c,
	[BRCMNAND_OOB_READ_BASE]	= 0x200,
	[BRCMNAND_OOB_READ_10_BASE]	=     0,
	[BRCMNAND_OOB_WRITE_BASE]	= 0x280,
	[BRCMNAND_OOB_WRITE_10_BASE]	=     0,
	[BRCMNAND_FC_BASE]		= 0x400,
};

enum brcmnand_cs_reg {
	BRCMNAND_CS_CFG_EXT = 0,
	BRCMNAND_CS_CFG,
	BRCMNAND_CS_ACC_CONTROL,
	BRCMNAND_CS_TIMING1,
	BRCMNAND_CS_TIMING2,
};

/* Per chip-select offsets for v7.1 */
static const u8 brcmnand_cs_offsets_v71[] = {
	[BRCMNAND_CS_ACC_CONTROL]	= 0x00,
	[BRCMNAND_CS_CFG_EXT]		= 0x04,
	[BRCMNAND_CS_CFG]		= 0x08,
	[BRCMNAND_CS_TIMING1]		= 0x0c,
	[BRCMNAND_CS_TIMING2]		= 0x10,
};

/* Per chip-select offsets for pre v7.1, except CS0 on v4.0 */
static const u8 brcmnand_cs_offsets[] = {
	[BRCMNAND_CS_ACC_CONTROL]	= 0x00,
	[BRCMNAND_CS_CFG_EXT]		= 0x04,
	[BRCMNAND_CS_CFG]		= 0x04,
	[BRCMNAND_CS_TIMING1]		= 0x08,
	[BRCMNAND_CS_TIMING2]		= 0x0c,
};

/* Per chip-select offset for v4.0/v5.0 on CS0 only */
static const u8 brcmnand_cs_offsets_v4[] = {
	[BRCMNAND_CS_ACC_CONTROL]	= 0x00,
	[BRCMNAND_CS_CFG_EXT]		= 0x08,
	[BRCMNAND_CS_CFG]		= 0x08,
	[BRCMNAND_CS_TIMING1]		= 0x10,
	[BRCMNAND_CS_TIMING2]		= 0x14,
};

/* BRCMNAND_INTFC_STATUS */
enum {
	INTFC_FLASH_STATUS		= GENMASK(7, 0),

	INTFC_ERASED			= BIT(27),
	INTFC_OOB_VALID			= BIT(28),
	INTFC_CACHE_VALID		= BIT(29),
	INTFC_FLASH_READY		= BIT(30),
	INTFC_CTLR_READY		= BIT(31),
};

static int brcmstb_nand_revision_init(struct brcmstb_nand_controller *ctrl)
{
	static const unsigned int block_sizes_v6[] = { 8, 16, 128, 256, 512, 1024, 2048, 0 };
	static const unsigned int block_sizes[] = { 16, 128, 8, 512, 256, 1024, 2048, 0 };
	static const unsigned int page_sizes[] = { 512, 2048, 4096, 8192, 0 };

	ctrl->nand_version = __raw_readl(ctrl->nand_base + 0) & 0xffff;

	/* Only support v4.0+? */
	if (ctrl->nand_version < 0x0400)
		return -ENODEV;

	/* Register offsets */
	if (ctrl->nand_version >= 0x0600)
		ctrl->reg_offsets = brcmnand_regs_v60;
	else if (ctrl->nand_version >= 0x0500)
		ctrl->reg_offsets = brcmnand_regs_v50;
	else if (ctrl->nand_version >= 0x0400)
		ctrl->reg_offsets = brcmnand_regs_v40;

	/* Chip-select stride */
	if (ctrl->nand_version >= 0x0701)
		ctrl->reg_spacing = 0x14;
	else
		ctrl->reg_spacing = 0x10;

	/* Page / block sizes */
	if (ctrl->nand_version >= 0x0701) {
		ctrl->max_page_size = 16 * 1024;
		ctrl->max_block_size = 2 * 1024 * 1024;
	} else {
		ctrl->page_sizes = page_sizes;
		if (ctrl->nand_version >= 0x0600)
			ctrl->block_sizes = block_sizes_v6;
		else
			ctrl->block_sizes = block_sizes;

		if (ctrl->nand_version < 0x0400) {
			ctrl->max_page_size = 4096;
			ctrl->max_block_size = 512 * 1024;
		}
	}

	/* Maximum spare area sector size (per 512B) */
	if (ctrl->nand_version >= 0x0600)
		ctrl->max_oob = 64;
	else if (ctrl->nand_version >= 0x0500)
		ctrl->max_oob = 32;
	else
		ctrl->max_oob = 16;

	return 0;
}

static inline u32 brcmnand_read_reg(struct brcmstb_nand_controller *ctrl,
		enum brcmnand_reg reg)
{
	u16 offs = ctrl->reg_offsets[reg];
	if (offs)
		return __raw_readl(ctrl->nand_base + offs);
	else
		return 0;
}

static inline void brcmnand_write_reg(struct brcmstb_nand_controller *ctrl,
		enum brcmnand_reg reg, u32 val)
{
	u16 offs = ctrl->reg_offsets[reg];
	if (offs)
		__raw_writel(val, ctrl->nand_base + offs);
}

static inline void brcmnand_rmw_reg(struct brcmstb_nand_controller *ctrl,
		enum brcmnand_reg reg, u32 mask, unsigned int shift, u32 val)
{
	u32 tmp = brcmnand_read_reg(ctrl, reg);
	tmp &= ~mask;
	tmp |= val << shift;
	brcmnand_write_reg(ctrl, reg, tmp);
}

static inline unsigned int brcmnand_read_fc(struct brcmstb_nand_controller *ctrl,
		int word)
{
	return __raw_readl(ctrl->nand_fc + word * 4);
}

static inline void brcmnand_write_fc(struct brcmstb_nand_controller *ctrl,
		int word, u32 val)
{
	__raw_writel(val, ctrl->nand_fc + word * 4);
}

static inline u16 brcmnand_cs_offset(struct brcmstb_nand_controller *ctrl,
		int cs, enum brcmnand_cs_reg reg)
{
	u16 offs_cs0 = ctrl->reg_offsets[BRCMNAND_CS0_BASE];
	u16 offs_cs1 = ctrl->reg_offsets[BRCMNAND_CS1_BASE];
	u8 cs_offs;

	if (ctrl->nand_version >= 0x0701)
		cs_offs = brcmnand_cs_offsets_v71[reg];
	/* NAND v4.0 and v5.0 controllers have a different CS0 offset layout */
	else if ((ctrl->nand_version == 0x0400 ||
		  ctrl->nand_version == 0x0500) && cs == 0)
		cs_offs = brcmnand_cs_offsets_v4[reg];
	else
		cs_offs = brcmnand_cs_offsets[reg];

	if (cs && offs_cs1)
		return offs_cs1 + (cs - 1) * ctrl->reg_spacing + cs_offs;

	return offs_cs0 + cs * ctrl->reg_spacing + cs_offs;
}

static inline bool brcmstb_nand_has_wp(struct brcmstb_nand_controller *ctrl)
{
	return ctrl->nand_version >= 0x0700;
}

static inline unsigned int brcmnand_count_corrected(struct brcmstb_nand_controller *ctrl)
{
	if (ctrl->nand_version < 0x0600)
		return 1;
	return brcmnand_read_reg(ctrl, BRCMNAND_CORR_COUNT);
}

static inline u32 brcmnand_ecc_level_mask(struct brcmstb_nand_controller *ctrl)
{
	return ((ctrl->nand_version >= 0x0600) ? 0x1f : 0x0f) << 16;
}

static void brcmnand_set_ecc_enabled(struct brcmstb_nand_host *host, int en)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u16 offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_ACC_CONTROL);
	u32 acc_control = __raw_readl(ctrl->nand_base + offs);

	if (en) {
		acc_control |= BIT(30) | BIT(31); /* RD/WR ECC enabled */
		acc_control |= host->hwcfg.ecc_level << 16;
	} else {
		acc_control &= ~(BIT(30) | BIT(31)); /* RD/WR ECC enabled */
		acc_control &= ~brcmnand_ecc_level_mask(ctrl);
	}

	__raw_writel(acc_control, ctrl->nand_base + offs);
}

static void brcmnand_wr_corr_thresh(struct brcmstb_nand_controller *ctrl,
		int cs, u8 val)
{
	unsigned int shift = 0, bits;
	enum brcmnand_reg reg = BRCMNAND_CORR_THRESHOLD;

	if (ctrl->nand_version >= 0x0600)
		bits = 6;
	else if (ctrl->nand_version >= 0x0500)
		bits = 5;
	else
		bits = 4;

	if (ctrl->nand_version >= 0x0600) {
		if (cs >= 5)
			reg = BRCMNAND_CORR_THRESHOLD_EXT;
		shift = (cs % 5) * bits;
	}
	brcmnand_rmw_reg(ctrl, reg, (bits - 1) << shift, shift, val);
}

static inline int brcmnand_cmd_shift(struct brcmstb_nand_controller *ctrl)
{
	if (ctrl->nand_version < 0x0700)
		return 24;
	return 0;
}

static inline int brcmnand_sector_1k_shift(struct brcmstb_nand_controller *ctrl)
{
	if (ctrl->nand_version >= 0x0600)
		return 7;
	else if (ctrl->nand_version >= 0x0500)
		return 6;
	else
		return -1;
}

static int brcmnand_get_sector_size_1k(struct brcmstb_nand_host *host)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	int shift = brcmnand_sector_1k_shift(ctrl);
	u16 acc_control_offs = brcmnand_cs_offset(ctrl, host->cs,
						  BRCMNAND_CS_ACC_CONTROL);

	if (shift < 0)
		return 0;

	return (__raw_readl(ctrl->nand_base + acc_control_offs) >> shift) & 0x1;
}

static void brcmnand_set_sector_size_1k(struct brcmstb_nand_host *host,
					       int val)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	int shift = brcmnand_sector_1k_shift(ctrl);
	u16 acc_control_offs = brcmnand_cs_offset(ctrl, host->cs,
						  BRCMNAND_CS_ACC_CONTROL);
	u32 tmp;

	if (shift < 0)
		return;

	tmp = __raw_readl(ctrl->nand_base + acc_control_offs);
	tmp &= ~(1 << shift);
	tmp |= (!!val) << shift;
	__raw_writel(tmp, ctrl->nand_base + acc_control_offs);
}

static void controller_hw_lock(struct nand_hw_control *hwcontrol)
{
	struct brcmstb_nand_controller *ctrl =
		container_of(hwcontrol, struct brcmstb_nand_controller, controller);
	int loops = 0;
	u32 semagic;

	semagic = brcmnand_read_reg(ctrl, BRCMNAND_SEMAPHORE) & 0xff;
	if (semagic == ctrl->semagic) {
		/* already locked, so just increment nest cnt */
		ctrl->lock_cnt++;
		return;
	}
	brcmnand_write_reg(ctrl, BRCMNAND_SEMAPHORE, ctrl->semagic);
	semagic = brcmnand_read_reg(ctrl, BRCMNAND_SEMAPHORE) & 0xff;
	while ((semagic != ctrl->semagic) && (loops++ < 10000)) {
		udelay(100);
		brcmnand_write_reg(ctrl, BRCMNAND_SEMAPHORE, ctrl->semagic);
		semagic = brcmnand_read_reg(ctrl, BRCMNAND_SEMAPHORE) & 0xff;
	}
	if (semagic != ctrl->semagic) {
		pr_err("NAND hw lock by force, assuming other side crashed\n");
		brcmnand_write_reg(ctrl, BRCMNAND_SEMAPHORE, 0);
		brcmnand_write_reg(ctrl, BRCMNAND_SEMAPHORE, ctrl->semagic);
	}
	ctrl->lock_cnt++;
	enable_irq(ctrl->irq);
}

static void controller_hw_unlock(struct nand_hw_control *hwcontrol)
{
	struct brcmstb_nand_controller *ctrl =
		container_of(hwcontrol, struct brcmstb_nand_controller, controller);
	BUG_ON(ctrl->lock_cnt == 0);
	ctrl->lock_cnt--;
	if (ctrl->lock_cnt == 0) {
		disable_irq(ctrl->irq);
		brcmnand_write_reg(ctrl, BRCMNAND_SEMAPHORE, 0);
	}
}

/***********************************************************************
 * Flash DMA
 ***********************************************************************/

enum flash_dma_reg {
	FLASH_DMA_REVISION		= 0x00,
	FLASH_DMA_FIRST_DESC		= 0x04,
	FLASH_DMA_FIRST_DESC_EXT	= 0x08,
	FLASH_DMA_CTRL			= 0x0c,
	FLASH_DMA_MODE			= 0x10,
	FLASH_DMA_STATUS		= 0x14,
	FLASH_DMA_INTERRUPT_DESC	= 0x18,
	FLASH_DMA_INTERRUPT_DESC_EXT	= 0x1c,
	FLASH_DMA_ERROR_STATUS		= 0x20,
	FLASH_DMA_CURRENT_DESC		= 0x24,
	FLASH_DMA_CURRENT_DESC_EXT	= 0x28,
};

static inline bool has_flash_dma(struct brcmstb_nand_controller *ctrl)
{
	return ctrl->flash_dma_base;
}

static inline bool flash_dma_buf_ok(const void *buf)
{
	return buf && !is_vmalloc_addr(buf) &&
		likely(IS_ALIGNED((uintptr_t)buf, 4));
}

static inline void flash_dma_writel(struct brcmstb_nand_controller *ctrl, u8 offs,
		u32 val)
{
	__raw_writel(val, ctrl->flash_dma_base + offs);
}

static inline u32 flash_dma_readl(struct brcmstb_nand_controller *ctrl, u8 offs)
{
	return __raw_readl(ctrl->flash_dma_base + offs);
}

/* Low-level operation types: command, address, write, or read */
enum brcmstb_nand_llop_type {
	LL_OP_CMD,
	LL_OP_ADDR,
	LL_OP_WR,
	LL_OP_RD,
};

/***********************************************************************
 * Internal support functions
 ***********************************************************************/

static inline bool is_hamming_ecc(struct brcmstb_nand_cfg *cfg)
{
	return cfg->sector_size_1k == 0 && cfg->spare_area_size == 16 &&
		cfg->ecc_level == 15;
}

/*
 * Returns a nand_ecclayout strucutre for the given layout/configuration.
 * Returns NULL on failure.
 */
static struct nand_ecclayout *brcmstb_nand_create_layout(int ecc_level,
		struct brcmstb_nand_host *host)
{
	struct brcmstb_nand_cfg *cfg = &host->hwcfg;
	int i, j;
	struct nand_ecclayout *layout;
	int req;
	int sectors;
	int sas;
	int idx1, idx2;

	layout = devm_kzalloc(&host->pdev->dev, sizeof(*layout), GFP_KERNEL);
	if (!layout)
		return NULL;

	sectors = cfg->page_size / (512 << cfg->sector_size_1k);
	sas = cfg->spare_area_size << cfg->sector_size_1k;

	/* Hamming */
	if (is_hamming_ecc(cfg)) {
		for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
			/* First sector of each page may have BBI */
			if (i == 0) {
				layout->oobfree[idx2].offset = i * sas + 1;
				/* Small-page NAND use byte 6 for BBI */
				if (cfg->page_size == 512)
					layout->oobfree[idx2].offset--;
				layout->oobfree[idx2].length = 5;
			} else {
				layout->oobfree[idx2].offset = i * sas;
				layout->oobfree[idx2].length = 6;
			}
			idx2++;
			layout->eccpos[idx1++] = i * sas + 6;
			layout->eccpos[idx1++] = i * sas + 7;
			layout->eccpos[idx1++] = i * sas + 8;
			layout->oobfree[idx2].offset = i * sas + 9;
			layout->oobfree[idx2].length = 7;
			idx2++;
			/* Leave zero-terminated entry for OOBFREE */
			if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
				    idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
				break;
		}
		goto out;
	}

	/*
	 * CONTROLLER_VERSION:
	 *   < v5.0: ECC_REQ = ceil(BCH_T * 13/8)
	 *  >= v5.0: ECC_REQ = ceil(BCH_T * 14/8)  [see SWLINUX-2038]
	 * But we will just be conservative.
	 */
	req = DIV_ROUND_UP(ecc_level * 14, 8);
	if (req >= sas) {
		dev_err(&host->pdev->dev,
			"error: ECC too large for OOB (ECC bytes %d, spare sector %d)\n",
			req, sas);
		return NULL;
	}

	layout->eccbytes = req * sectors;
	for (i = 0, idx1 = 0, idx2 = 0; i < sectors; i++) {
		for (j = sas - req; j < sas && idx1 <
				MTD_MAX_ECCPOS_ENTRIES_LARGE; j++, idx1++)
			layout->eccpos[idx1] = i * sas + j;

		/* First sector of each page may have BBI */
		if (i == 0) {
			if (cfg->page_size == 512 && (sas - req >= 6)) {
				/* Small-page NAND use byte 6 for BBI */
				layout->oobfree[idx2].offset = 0;
				layout->oobfree[idx2].length = 5;
				idx2++;
				if (sas - req > 6) {
					layout->oobfree[idx2].offset = 6;
					layout->oobfree[idx2].length =
						sas - req - 6;
					idx2++;
				}
			} else if (sas > req + 1) {
				layout->oobfree[idx2].offset = i * sas + 1;
				layout->oobfree[idx2].length = sas - req - 1;
				idx2++;
			}
		} else if (sas > req) {
			layout->oobfree[idx2].offset = i * sas;
			layout->oobfree[idx2].length = sas - req;
			idx2++;
		}
		/* Leave zero-terminated entry for OOBFREE */
		if (idx1 >= MTD_MAX_ECCPOS_ENTRIES_LARGE ||
				idx2 >= MTD_MAX_OOBFREE_ENTRIES_LARGE - 1)
			break;
	}
out:
	/* Sum available OOB */
	for (i = 0; i < MTD_MAX_OOBFREE_ENTRIES_LARGE; i++)
		layout->oobavail += layout->oobfree[i].length;
	return layout;
}

static struct nand_ecclayout *brcmstb_choose_ecc_layout(
		struct brcmstb_nand_host *host)
{
	struct nand_ecclayout *layout;
	struct brcmstb_nand_cfg *p = &host->hwcfg;
	unsigned int ecc_level = p->ecc_level;

	if (p->sector_size_1k)
		ecc_level <<= 1;

	layout = brcmstb_nand_create_layout(ecc_level, host);
	if (!layout) {
		dev_err(&host->pdev->dev,
				"no proper ecc_layout for this NAND cfg\n");
		return NULL;
	}

	return layout;
}

static void brcmstb_nand_wp(struct mtd_info *mtd, int wp)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;

	if (brcmstb_nand_has_wp(ctrl) && wp_on == 1) {
		static int old_wp = -1;
		if (old_wp != wp) {
			dev_dbg(&host->pdev->dev, "WP %s\n", wp ? "on" : "off");
			old_wp = wp;
		}
		brcmnand_rmw_reg(ctrl, BRCMNAND_CS_SELECT, 1 << 29, 29, wp);
	}
}

/* Helper functions for reading and writing OOB registers */
static inline unsigned char oob_reg_read(struct brcmstb_nand_controller *ctrl,
		int offs)
{
	u16 offset0, offset10, reg_offs;

	offset0 = ctrl->reg_offsets[BRCMNAND_OOB_READ_BASE];
	offset10 = ctrl->reg_offsets[BRCMNAND_OOB_READ_10_BASE];

	if (offs >= ctrl->max_oob)
		return 0x77;

	if (offs >= 16 && offset10)
		reg_offs = offset10 + ((offs - 0x10) & ~0x03);
	else
		reg_offs = offset0 + (offs & ~0x03);

	return __raw_readl(ctrl->nand_base + reg_offs) >>
				(24 - ((offs & 0x03) << 3));
}

static inline void oob_reg_write(struct brcmstb_nand_controller *ctrl, int offs,
		unsigned long data)
{
	u16 offset0, offset10, reg_offs;

	offset0 = ctrl->reg_offsets[BRCMNAND_OOB_WRITE_BASE];
	offset10 = ctrl->reg_offsets[BRCMNAND_OOB_WRITE_10_BASE];

	if (offs >= ctrl->max_oob)
		return;

	if (offs >= 16 && offset10)
		reg_offs = offset10 + ((offs - 0x10) & ~0x03);
	else
		reg_offs = offset0 + (offs & ~0x03);

	__raw_writel(data, ctrl->nand_base + reg_offs);
}

/*
 * read_oob_from_regs - read data from OOB registers
 * @i: sub-page sector index
 * @oob: buffer to read to
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int read_oob_from_regs(struct brcmstb_nand_controller *ctrl, int i,
		u8 *oob, int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - (int)ctrl->max_oob);
	tbytes = min_t(int, tbytes, ctrl->max_oob);

	for (j = 0; j < tbytes; j++)
		oob[j] = oob_reg_read(ctrl, j);
	return tbytes;
}

/*
 * write_oob_to_regs - write data to OOB registers
 * @i: sub-page sector index
 * @oob: buffer to write from
 * @sas: spare area sector size (i.e., OOB size per FLASH_CACHE)
 * @sector_1k: 1 for 1KiB sectors, 0 for 512B, other values are illegal
 */
static int write_oob_to_regs(struct brcmstb_nand_controller *ctrl, int i,
		const u8 *oob, int sas, int sector_1k)
{
	int tbytes = sas << sector_1k;
	int j;

	/* Adjust OOB values for 1K sector size */
	if (sector_1k && (i & 0x01))
		tbytes = max(0, tbytes - (int)ctrl->max_oob);
	tbytes = min_t(int, tbytes, ctrl->max_oob);

	for (j = 0; j < tbytes; j += 4)
		oob_reg_write(ctrl, j,
				(oob[j + 0] << 24) |
				(oob[j + 1] << 16) |
				(oob[j + 2] <<  8) |
				(oob[j + 3] <<  0));
	return tbytes;
}

static irqreturn_t brcmstb_nand_ctlrdy_irq(int irq, void *data)
{
	struct brcmstb_nand_controller *ctrl = data;

	/* Discard all NAND_CTLRDY interrupts during DMA */
	if (ctrl->dma_pending)
		return IRQ_HANDLED;

	BUG_ON(ctrl->cmd_pending == 0);
	complete(&ctrl->done);
	return IRQ_HANDLED;
}

static irqreturn_t brcmstb_nand_dma_irq(int irq, void *data)
{
	struct brcmstb_nand_controller *ctrl = data;

	complete(&ctrl->dma_done);

	return IRQ_HANDLED;
}

static void brcmstb_nand_send_cmd(struct brcmstb_nand_host *host, int cmd)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u32 intfc;

	dev_dbg(&host->pdev->dev, "send native cmd %d addr_lo 0x%x\n", cmd,
		brcmnand_read_reg(ctrl, BRCMNAND_CMD_ADDRESS));
	BUG_ON(ctrl->cmd_pending != 0);
	ctrl->cmd_pending = cmd;

	intfc = brcmnand_read_reg(ctrl, BRCMNAND_INTFC_STATUS);
	BUG_ON(!(intfc & INTFC_CTLR_READY));

	mb();
	brcmnand_write_reg(ctrl, BRCMNAND_CMD_START, cmd << brcmnand_cmd_shift(ctrl));
}

/***********************************************************************
 * NAND MTD API: read/program/erase
 ***********************************************************************/

static void brcmstb_nand_cmd_ctrl(struct mtd_info *mtd, int dat,
	unsigned int ctrl)
{
	/* intentionally left blank */
}

static int brcmstb_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned long timeo = msecs_to_jiffies(100);

	dev_dbg(&host->pdev->dev, "wait on native cmd %d\n", ctrl->cmd_pending);
	if (ctrl->cmd_pending &&
			wait_for_completion_timeout(&ctrl->done, timeo) <= 0) {
		unsigned long cmd = brcmnand_read_reg(ctrl, BRCMNAND_CMD_START)
					>> brcmnand_cmd_shift(ctrl);

		dev_err_ratelimited(&host->pdev->dev,
			"timeout waiting for command %u (%ld)\n",
			host->last_cmd, cmd);
		dev_err_ratelimited(&host->pdev->dev, "intfc status %08x\n",
			brcmnand_read_reg(ctrl, BRCMNAND_INTFC_STATUS));
	}
	ctrl->cmd_pending = 0;
	return brcmnand_read_reg(ctrl, BRCMNAND_INTFC_STATUS) &
				 INTFC_FLASH_STATUS;
}

static int brcmstb_nand_low_level_op(struct brcmstb_nand_host *host,
		enum brcmstb_nand_llop_type type, u32 data, bool last_op)
{
	struct mtd_info *mtd = &host->mtd;
	struct nand_chip *chip = &host->chip;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u32 tmp;

	tmp = data & 0xffff; /* data */
	switch (type) {
	case LL_OP_CMD:
		/* WE | CLE */
		tmp |= BIT(17) | BIT(19);
		break;
	case LL_OP_ADDR:
		/* WE | ALE */
		tmp |= BIT(17) | BIT(18);
		break;
	case LL_OP_WR:
		/* WE */
		tmp |= BIT(17);
		break;
	case LL_OP_RD:
		/* RE */
		tmp |= BIT(16);
		break;
	}
	if (last_op)
		/* RETURN_IDLE */
		tmp |= BIT(31);

	dev_dbg(&host->pdev->dev, "ll_op cmd 0x%lx\n", (unsigned long)tmp);

	brcmnand_write_reg(ctrl, BRCMNAND_LL_OP, tmp);
	(void)brcmnand_read_reg(ctrl, BRCMNAND_LL_OP);

	brcmstb_nand_send_cmd(host, CMD_LOW_LEVEL_OP);
	return brcmstb_nand_waitfunc(mtd, chip);
}

static void brcmstb_nand_cmdfunc(struct mtd_info *mtd, unsigned command,
	int column, int page_addr)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u64 addr = (u64)page_addr << chip->page_shift;
	int native_cmd = 0;

	if (command == NAND_CMD_READID || command == NAND_CMD_PARAM ||
			command == NAND_CMD_RNDOUT)
		addr = (u64)column;
	/* Avoid propagating a negative, don't-care address */
	else if (page_addr < 0)
		addr = 0;

	dev_dbg(&host->pdev->dev, "cmd 0x%x addr 0x%llx\n", command,
		(unsigned long long)addr);

	host->last_cmd = command;
	host->last_byte = 0;
	host->last_addr = addr;

	switch (command) {
	case NAND_CMD_RESET:
		native_cmd = CMD_FLASH_RESET;
		break;
	case NAND_CMD_STATUS:
		native_cmd = CMD_STATUS_READ;
		break;
	case NAND_CMD_READID:
		native_cmd = CMD_DEVICE_ID_READ;
		break;
	case NAND_CMD_READOOB:
		native_cmd = CMD_SPARE_AREA_READ;
		break;
	case NAND_CMD_ERASE1:
		native_cmd = CMD_BLOCK_ERASE;
		brcmstb_nand_wp(mtd, 0);
		break;
	case NAND_CMD_PARAM:
		native_cmd = CMD_PARAMETER_READ;
		break;
	case NAND_CMD_SET_FEATURES:
	case NAND_CMD_GET_FEATURES:
		brcmstb_nand_low_level_op(host, LL_OP_CMD, command, false);
		brcmstb_nand_low_level_op(host, LL_OP_ADDR, column, false);
		break;
	case NAND_CMD_RNDOUT:
		native_cmd = CMD_PARAMETER_CHANGE_COL;
		addr &= ~((u64)(FC_BYTES - 1));
		/*
		 * HW quirk: PARAMETER_CHANGE_COL requires SECTOR_SIZE_1K=0
		 * NB: hwcfg.sector_size_1k may not be initialized yet
		 */
		if (brcmnand_get_sector_size_1k(host)) {
			host->hwcfg.sector_size_1k =
				brcmnand_get_sector_size_1k(host);
			brcmnand_set_sector_size_1k(host, 0);
		}
		break;
	}

	if (!native_cmd)
		return;

	brcmnand_write_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS,
		(host->cs << 16) | ((addr >> 32) & 0xffff));
	(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS);
	brcmnand_write_reg(ctrl, BRCMNAND_CMD_ADDRESS, addr & 0xffffffff);
	(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_ADDRESS);

	brcmstb_nand_send_cmd(host, native_cmd);
	brcmstb_nand_waitfunc(mtd, chip);

	if (native_cmd == CMD_PARAMETER_READ ||
			native_cmd == CMD_PARAMETER_CHANGE_COL) {
		int i;
		/*
		 * Must cache the FLASH_CACHE now, since changes in
		 * SECTOR_SIZE_1K may invalidate it
		 */
		for (i = 0; i < FC_WORDS; i++)
			ctrl->flash_cache[i] = brcmnand_read_fc(ctrl, i);
		/* Cleanup from HW quirk: restore SECTOR_SIZE_1K */
		if (host->hwcfg.sector_size_1k)
			brcmnand_set_sector_size_1k(host,
						    host->hwcfg.sector_size_1k);
	}

	/* Re-enable protection is necessary only after erase */
	if (command == NAND_CMD_ERASE1)
		brcmstb_nand_wp(mtd, 1);
}

static uint8_t brcmstb_nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	uint8_t ret = 0;
	int addr, offs;

	switch (host->last_cmd) {
	case NAND_CMD_READID:
		if (host->last_byte < 4)
			ret = brcmnand_read_reg(ctrl, BRCMNAND_ID) >>
				(24 - (host->last_byte << 3));
		else if (host->last_byte < 8)
			ret = brcmnand_read_reg(ctrl, BRCMNAND_ID_EXT) >>
				(56 - (host->last_byte << 3));
		break;

	case NAND_CMD_READOOB:
		ret = oob_reg_read(ctrl, host->last_byte);
		break;

	case NAND_CMD_STATUS:
		ret = brcmnand_read_reg(ctrl, BRCMNAND_INTFC_STATUS) & 0xff;
		if (wp_on) /* SWLINUX-1818: hide WP status from MTD */
			ret |= NAND_STATUS_WP;
		break;

	case NAND_CMD_PARAM:
	case NAND_CMD_RNDOUT:
		addr = host->last_addr + host->last_byte;
		offs = addr & (FC_BYTES - 1);

		/* At FC_BYTES boundary, switch to next column */
		if (host->last_byte > 0 && offs == 0)
			chip->cmdfunc(mtd, NAND_CMD_RNDOUT, addr, -1);

		ret = ctrl->flash_cache[offs >> 2] >>
					(24 - ((offs & 0x03) << 3));
		break;
	case NAND_CMD_GET_FEATURES:
		if (host->last_byte >= ONFI_SUBFEATURE_PARAM_LEN) {
			ret = 0;
		} else {
			bool last = host->last_byte ==
				ONFI_SUBFEATURE_PARAM_LEN - 1;
			brcmstb_nand_low_level_op(host, LL_OP_RD, 0, last);
			ret = brcmnand_read_reg(ctrl, BRCMNAND_LL_RDATA) & 0xff;
		}
	}

	dev_dbg(&host->pdev->dev, "read byte = 0x%02x\n", ret);
	host->last_byte++;

	return ret;
}

static void brcmstb_nand_read_buf(struct mtd_info *mtd, uint8_t *buf, int len)
{
	int i;

	for (i = 0; i < len; i++, buf++)
		*buf = brcmstb_nand_read_byte(mtd);
}

static void brcmstb_nand_write_buf(struct mtd_info *mtd, const uint8_t *buf, int len)
{
	int i;
	struct nand_chip *chip = mtd->priv;
	struct brcmstb_nand_host *host = chip->priv;

	switch (host->last_cmd) {
	case NAND_CMD_SET_FEATURES:
		for (i = 0; i < len; i++)
			brcmstb_nand_low_level_op(host, LL_OP_WR, buf[i], (i + 1) == len);
		break;
	default:
		BUG();
		break;
	}
}

/**
 * Construct a FLASH_DMA descriptor as part of a linked list. You must know the
 * following ahead of time:
 *  - Is this descriptor the beginning or end of a linked list?
 *  - What is the (DMA) address of the next descriptor in the linked list?
 */
static int brcmstb_nand_fill_dma_desc(struct brcmstb_nand_host *host,
		struct brcm_nand_dma_desc *desc, u64 addr, dma_addr_t buf,
		u32 len, u8 dma_cmd, bool begin, bool end, dma_addr_t next_desc)
{
	memset(desc, 0, sizeof(*desc));
	/* Descriptors are written in native byte order (wordwise) */
	desc->next_desc = next_desc & 0xffffffff;
	desc->next_desc_ext = ((u64)next_desc) >> 32;
	desc->cmd_irq = (dma_cmd << 24) |
		(end ? (0x03 << 8) : 0) | /* IRQ | STOP */
		(!!begin) | ((!!end) << 1); /* head, tail */
#ifdef CONFIG_CPU_BIG_ENDIAN
	desc->cmd_irq |= 0x01 << 12;
#endif
	desc->dram_addr = buf & 0xffffffff;
	desc->dram_addr_ext = (u64)buf >> 32;
	desc->tfr_len = len;
	desc->total_len = len;
	desc->flash_addr = addr & 0xffffffff;
	desc->flash_addr_ext = addr >> 32;
	desc->cs = host->cs;
	desc->status_valid = 0x01;
	return 0;
}

/**
 * Kick the FLASH_DMA engine, with a given DMA descriptor
 */
static void brcmstb_nand_dma_run(struct brcmstb_nand_host *host, dma_addr_t desc)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned long timeo = msecs_to_jiffies(100);

	flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC, desc & 0xffffffff);
	(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC);
	flash_dma_writel(ctrl, FLASH_DMA_FIRST_DESC_EXT, (u64)desc >> 32);
	(void)flash_dma_readl(ctrl, FLASH_DMA_FIRST_DESC_EXT);

	/* Start FLASH_DMA engine */
	ctrl->dma_pending = true;
	mb();
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0x03); /* wake | run */

	if (wait_for_completion_timeout(&ctrl->dma_done, timeo) <= 0) {
		dev_err(&host->pdev->dev,
				"timeout waiting for DMA; status %#x, error status %#x\n",
				flash_dma_readl(ctrl, FLASH_DMA_STATUS),
				flash_dma_readl(ctrl, FLASH_DMA_ERROR_STATUS));
	}
	ctrl->dma_pending = false;
	flash_dma_writel(ctrl, FLASH_DMA_CTRL, 0); /* force stop */
}

static int brcmstb_nand_dma_trans(struct brcmstb_nand_host *host, u64 addr,
	u32 *buf, u32 len, u8 dma_cmd)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	dma_addr_t buf_pa;
	int dir = dma_cmd == CMD_PAGE_READ ? DMA_FROM_DEVICE : DMA_TO_DEVICE;

	buf_pa = dma_map_single(&host->pdev->dev, buf, len, dir);

	brcmstb_nand_fill_dma_desc(host, ctrl->dma_desc, addr, buf_pa, len,
				   dma_cmd, true, true, 0);

	brcmstb_nand_dma_run(host, ctrl->dma_pa);

	dma_unmap_single(&host->pdev->dev, buf_pa, len, dir);

	if (ctrl->dma_desc->status_valid & FLASH_DMA_ECC_ERROR)
		return -EBADMSG;
	else if (ctrl->dma_desc->status_valid & FLASH_DMA_CORR_ERROR)
		return -EUCLEAN;

	return 0;
}

/*
 * Assumes proper CS is already set
 */
static int brcmstb_nand_read_by_pio(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, unsigned int trans,
	u32 *buf, u8 *oob, u64 *err_addr)
{
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	int i, j, ret = 0;

	/* Clear error addresses */
	brcmnand_write_reg(ctrl, BRCMNAND_UNCORR_ADDR, 0);
	brcmnand_write_reg(ctrl, BRCMNAND_CORR_ADDR, 0);

	brcmnand_write_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS,
			(host->cs << 16) | ((addr >> 32) & 0xffff));
	(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS);

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		brcmnand_write_reg(ctrl, BRCMNAND_CMD_ADDRESS, addr & 0xffffffff);
		(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_ADDRESS);
		/* SPARE_AREA_READ does not use ECC, so just use PAGE_READ */
		brcmstb_nand_send_cmd(host, CMD_PAGE_READ);
		brcmstb_nand_waitfunc(mtd, chip);

		if (likely(buf))
			for (j = 0; j < FC_WORDS; j++, buf++)
				*buf = brcmnand_read_fc(ctrl, j);

		if (oob)
			oob += read_oob_from_regs(ctrl, i, oob,
					mtd->oobsize / trans,
					host->hwcfg.sector_size_1k);

		if (!ret) {
			*err_addr = brcmnand_read_reg(ctrl,
					BRCMNAND_UNCORR_ADDR) |
				((u64)(brcmnand_read_reg(ctrl,
						BRCMNAND_UNCORR_EXT_ADDR)
					& 0xffff) << 32);
			if (*err_addr)
				ret = -EBADMSG;
		}

		if (!ret) {
			*err_addr = brcmnand_read_reg(ctrl,
					BRCMNAND_CORR_ADDR) |
				((u64)(brcmnand_read_reg(ctrl,
						BRCMNAND_CORR_EXT_ADDR)
					& 0xffff) << 32);
			if (*err_addr)
				ret = -EUCLEAN;
		}
	}

	return ret;
}

/*
 * Check a page to see if it is erased (w/ bitflips) after an uncorrectable ECC
 * error
 *
 * Because the HW ECC signals an ECC error if an erase paged has even a single
 * bitflip, we must check each ECC error to see if it is actually an erased
 * page with bitflips, not a truly corrupted page.
 *
 * On a real error, return a negative error code (-EBADMSG for ECC error), and
 * buf will contain raw data.
 * Otherwise, fill buf with 0xff and return the maximum number of
 * bitflips-per-ECC-sector to the caller.
 *
 */
static int brcmstb_nand_verify_erased_page(struct mtd_info *mtd,
		  struct nand_chip *chip, void *buf, u64 addr)
{
	int i, sas, oob_nbits, data_nbits;
	void *oob = chip->oob_poi;
	unsigned int max_bitflips = 0;
	int page = addr >> chip->page_shift;
	int ret;

	if (!buf) {
		buf = chip->buffers->databuf;
		/* Invalidate page cache */
		chip->pagebuf = -1;
	}

	sas = mtd->oobsize / chip->ecc.steps;
	oob_nbits = sas << 3;
	data_nbits = chip->ecc.size << 3;

	/* read without ecc for verification */
	chip->cmdfunc(mtd, NAND_CMD_READ0, 0x00, page);
	ret = chip->ecc.read_page_raw(mtd, chip, buf, true, page);
	if (ret)
		return ret;

	for (i = 0; i < chip->ecc.steps; i++, oob += sas) {
		unsigned int bitflips = 0;

		bitflips += oob_nbits - bitmap_weight(oob, oob_nbits);
		bitflips += data_nbits - bitmap_weight(buf, data_nbits);

		buf += chip->ecc.size;
		addr += chip->ecc.size;

		/* Too many bitflips */
		if (bitflips > chip->ecc.strength)
			return -EBADMSG;

		max_bitflips = max(max_bitflips, bitflips);
	}

	return max_bitflips;
}

static int brcmstb_nand_read(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, unsigned int trans,
	u32 *buf, u8 *oob)
{
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u64 err_addr = 0;
	int err;

	dev_dbg(&host->pdev->dev, "read %llx -> %p\n",
		(unsigned long long)addr, buf);

	if (ctrl->nand_version >= 0x0600)
		brcmnand_write_reg(ctrl, BRCMNAND_UNCORR_COUNT, 0);

	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		err = brcmstb_nand_dma_trans(host, addr, buf, trans * FC_BYTES,
					     CMD_PAGE_READ);
		if (err) {
			if (mtd_is_bitflip_or_eccerr(err))
				err_addr = addr;
			else
				return -EIO;
		}
	} else {
		if (oob)
			memset(oob, 0x99, mtd->oobsize);

		err = brcmstb_nand_read_by_pio(mtd, chip, addr, trans, buf,
					       oob, &err_addr);
	}

	if (mtd_is_eccerr(err)) {
		int ret = brcmstb_nand_verify_erased_page(mtd, chip, buf, addr);
		if (ret < 0) {
			dev_dbg(&host->pdev->dev,
					"uncorrectable error at 0x%llx\n",
					(unsigned long long)err_addr);
			mtd->ecc_stats.failed++;
			/* NAND layer expects zero on ECC errors */
			return 0;
		} else {
			if (buf)
				memset(buf, 0xff, FC_BYTES * trans);
			if (oob)
				memset(oob, 0xff, mtd->oobsize);

			dev_info(&host->pdev->dev,
					"corrected %d bitflips in blank page at 0x%llx\n",
					ret, (unsigned long long)addr);
			return ret;
		}
	}

	if (mtd_is_bitflip(err)) {
		unsigned int corrected = brcmnand_count_corrected(ctrl);
		dev_dbg(&host->pdev->dev, "corrected error at 0x%llx\n",
			(unsigned long long)err_addr);
		mtd->ecc_stats.corrected += corrected;
		/* Always exceed the software-imposed threshold */
		return max(mtd->bitflip_threshold, corrected);
	}

	return 0;
}

static int brcmstb_nand_read_page(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;

	return brcmstb_nand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
}

static int brcmstb_nand_read_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, uint8_t *buf, int oob_required, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	u8 *oob = oob_required ? (u8 *)chip->oob_poi : NULL;
	int ret;

	brcmnand_set_ecc_enabled(host, 0);
	ret = brcmstb_nand_read(mtd, chip, host->last_addr,
			mtd->writesize >> FC_SHIFT, (u32 *)buf, oob);
	brcmnand_set_ecc_enabled(host, 1);
	return ret;
}

static int brcmstb_nand_read_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	return brcmstb_nand_read(mtd, chip, (u64)page << chip->page_shift,
			mtd->writesize >> FC_SHIFT,
			NULL, (u8 *)chip->oob_poi);
}

static int brcmstb_nand_read_oob_raw(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	struct brcmstb_nand_host *host = chip->priv;

	brcmnand_set_ecc_enabled(host, 0);
	brcmstb_nand_read(mtd, chip, (u64)page << chip->page_shift,
		mtd->writesize >> FC_SHIFT,
		NULL, (u8 *)chip->oob_poi);
	brcmnand_set_ecc_enabled(host, 1);
	return 0;
}

static int brcmstb_nand_read_subpage(struct mtd_info *mtd,
	struct nand_chip *chip, uint32_t data_offs, uint32_t readlen,
	uint8_t *bufpoi)
{
	struct brcmstb_nand_host *host = chip->priv;

	return brcmstb_nand_read(mtd, chip, host->last_addr + data_offs,
			readlen >> FC_SHIFT, (u32 *)bufpoi, NULL);
}

static int brcmstb_nand_write(struct mtd_info *mtd,
	struct nand_chip *chip, u64 addr, const u32 *buf, u8 *oob)
{
	struct brcmstb_nand_host *host = chip->priv;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	unsigned int i, j, trans = mtd->writesize >> FC_SHIFT;
	int status, ret = 0;

	dev_dbg(&host->pdev->dev, "write %llx <- %p\n",
		(unsigned long long)addr, buf);

	if (unlikely((u32)buf & 0x03)) {
		dev_warn(&host->pdev->dev, "unaligned buffer: %p\n", buf);
		buf = (u32 *)((u32)buf & ~0x03);
	}

	brcmstb_nand_wp(mtd, 0);

	for (i = 0; i < ctrl->max_oob; i += 4)
		oob_reg_write(ctrl, i, 0xffffffff);

	if (has_flash_dma(ctrl) && !oob && flash_dma_buf_ok(buf)) {
		if (brcmstb_nand_dma_trans(host, addr, (u32 *)buf,
					mtd->writesize, CMD_PROGRAM_PAGE))
			ret = -EIO;
		goto out;
	}

	brcmnand_write_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS,
			(host->cs << 16) | ((addr >> 32) & 0xffff));
	(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_EXT_ADDRESS);

	for (i = 0; i < trans; i++, addr += FC_BYTES) {
		/* full address MUST be set before populating FC */
		brcmnand_write_reg(ctrl, BRCMNAND_CMD_ADDRESS,
				addr & 0xffffffff);
		(void)brcmnand_read_reg(ctrl, BRCMNAND_CMD_ADDRESS);

		if (buf)
			for (j = 0; j < FC_WORDS; j++, buf++)
				brcmnand_write_fc(ctrl, j, *buf);
		else if (oob)
			for (j = 0; j < FC_WORDS; j++)
				brcmnand_write_fc(ctrl, j, 0xffffffff);

		if (oob) {
			oob += write_oob_to_regs(ctrl, i, oob,
					mtd->oobsize / trans,
					host->hwcfg.sector_size_1k);
		}

		/* we cannot use SPARE_AREA_PROGRAM when PARTIAL_PAGE_EN=0 */
		brcmstb_nand_send_cmd(host, CMD_PROGRAM_PAGE);
		status = brcmstb_nand_waitfunc(mtd, chip);

		if (status & NAND_STATUS_FAIL) {
			dev_info(&host->pdev->dev, "program failed at %llx\n",
				(unsigned long long)addr);
			ret = -EIO;
			goto out;
		}
	}
out:
	brcmstb_nand_wp(mtd, 1);
	return ret;
}

static int brcmstb_nand_write_page(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct brcmstb_nand_host *host = chip->priv;
	void *oob = oob_required ? chip->oob_poi : NULL;

	brcmstb_nand_write(mtd, chip, host->last_addr, (const u32 *)buf, oob);
	return 0;
}

static int brcmstb_nand_write_page_raw(struct mtd_info *mtd,
	struct nand_chip *chip, const uint8_t *buf, int oob_required)
{
	struct brcmstb_nand_host *host = chip->priv;
	void *oob = oob_required ? chip->oob_poi : NULL;

	brcmnand_set_ecc_enabled(host, 0);
	brcmstb_nand_write(mtd, chip, host->last_addr, (const u32 *)buf, oob);
	brcmnand_set_ecc_enabled(host, 1);
	return 0;
}

static int brcmstb_nand_write_oob(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	return brcmstb_nand_write(mtd, chip, (u64)page << chip->page_shift,
				  NULL, chip->oob_poi);
}

static int brcmstb_nand_write_oob_raw(struct mtd_info *mtd,
	struct nand_chip *chip, int page)
{
	struct brcmstb_nand_host *host = chip->priv;
	int ret;

	brcmnand_set_ecc_enabled(host, 0);
	ret = brcmstb_nand_write(mtd, chip, (u64)page << chip->page_shift, NULL,
		(u8 *)chip->oob_poi);
	brcmnand_set_ecc_enabled(host, 1);

	return ret;
}

/***********************************************************************
 * Per-CS setup (1 NAND device)
 ***********************************************************************/

static void brcmstb_nand_set_cfg(struct brcmstb_nand_host *host,
	struct brcmstb_nand_cfg *cfg)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u16 cfg_offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_CFG);
	u16 cfg_ext_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_CFG_EXT);
	u16 acc_control_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_ACC_CONTROL);
	u8 block_size = 0, page_size = 0, device_size = 0;
	u32 tmp;

	if (ctrl->block_sizes) {
		int i, found;

		for (i = 0, found = 0; ctrl->block_sizes[i]; i++)
			if (ctrl->block_sizes[i] * 1024 == cfg->block_size) {
				block_size = i;
				found = 1;
			}
		if (!found)
			dev_warn(&host->pdev->dev, "invalid block size %u\n",
					cfg->block_size);
	} else {
		block_size = ffs(cfg->block_size) - ffs(BRCMNAND_MIN_BLOCKSIZE);
	}

	if (cfg->block_size < BRCMNAND_MIN_BLOCKSIZE || (ctrl->max_block_size &&
				cfg->block_size > ctrl->max_block_size)) {
		dev_warn(&host->pdev->dev, "invalid block size %u\n",
				cfg->block_size);
		block_size = 0;
	}

	if (ctrl->page_sizes) {
		int i, found;

		for (i = 0, found = 0; ctrl->page_sizes[i]; i++)
			if (ctrl->page_sizes[i] == cfg->page_size) {
				page_size = i;
				found = 1;
			}
		if (!found)
			dev_warn(&host->pdev->dev, "invalid page size %u\n",
					cfg->page_size);
	} else {
		page_size = ffs(cfg->page_size) - ffs(BRCMNAND_MIN_PAGESIZE);
	}

	if (cfg->page_size < BRCMNAND_MIN_PAGESIZE || (ctrl->max_page_size &&
				cfg->page_size > ctrl->max_page_size)) {
		dev_warn(&host->pdev->dev, "invalid page size %u\n",
				cfg->page_size);
		page_size = 0;
	}

	if (fls64(cfg->device_size) < fls64(BRCMNAND_MIN_DEVSIZE))
		dev_warn(&host->pdev->dev, "invalid device size 0x%llx\n",
			(unsigned long long)cfg->device_size);
	else
		device_size = fls64(cfg->device_size) - fls64(BRCMNAND_MIN_DEVSIZE);

	tmp = (cfg->blk_adr_bytes << 8) |
		(cfg->col_adr_bytes << 12) |
		(cfg->ful_adr_bytes << 16) |
		(!!(cfg->device_width == 16) << 23) |
		(device_size << 24);
	if (cfg_offs == cfg_ext_offs) {
		tmp |= (page_size << 20) | (block_size << 28);
		__raw_writel(tmp, ctrl->nand_base + cfg_offs);
	} else {
		__raw_writel(tmp, ctrl->nand_base + cfg_offs);
		tmp = page_size | (block_size << 4);
		__raw_writel(tmp, ctrl->nand_base + cfg_ext_offs);
	}

	/* Configure ACC_CONTROL */
	tmp = __raw_readl(ctrl->nand_base + acc_control_offs);
	tmp &= ~brcmnand_ecc_level_mask(ctrl);
	tmp |= cfg->ecc_level << 16;
	tmp &= ~(1 << 7); /* FIXME: some controllers only use bits[5:0] */
	tmp |= cfg->spare_area_size;
	if (ctrl->nand_version >= 0x0500)
		tmp |= !!cfg->sector_size_1k << brcmnand_sector_1k_shift(ctrl);
	__raw_writel(tmp, ctrl->nand_base + acc_control_offs);

	/* threshold = ceil(BCH-level * 0.75) */
	brcmnand_wr_corr_thresh(ctrl, host->cs,
			((cfg->ecc_level << cfg->sector_size_1k) * 3 + 2) / 4);
}

static void brcmstb_nand_get_cfg(struct brcmstb_nand_host *host,
	struct brcmstb_nand_cfg *cfg)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u16 cfg_offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_CFG);
	u16 cfg_ext_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_CFG_EXT);
	u16 acc_control_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_ACC_CONTROL);
	u32 config, config_ext, acc_control;

	config = __raw_readl(ctrl->nand_base + cfg_offs);
	if (cfg_offs != cfg_ext_offs)
		config_ext = __raw_readl(ctrl->nand_base + cfg_ext_offs);
	else
		config_ext = 0;
	acc_control = __raw_readl(ctrl->nand_base + acc_control_offs);

	if (ctrl->block_sizes) {
		int i , idx;

		idx = (config >> 28) & 0x07;

		for (i = 0; ctrl->block_sizes[i]; i++)
			if (i == idx)
				break;
		if (i == idx)
			cfg->block_size = ctrl->block_sizes[idx] * 1024;
		else
			cfg->block_size = 128 * 1024;
	} else {
		cfg->block_size = BRCMNAND_MIN_BLOCKSIZE <<
			((config_ext >> 4) & 0xff);
	}

	if (ctrl->page_sizes) {
		int i, idx;

		idx = (config >> 20) & 0x03;

		for (i = 0; ctrl->page_sizes[i]; i++)
			if (i == idx)
				break;
		if (i == idx)
			cfg->page_size = ctrl->page_sizes[idx];
		else
			cfg->page_size = 2048;
	} else {
		cfg->page_size = BRCMNAND_MIN_PAGESIZE << (config_ext & 0x0f);
	}

	cfg->device_size = BRCMNAND_MIN_DEVSIZE << ((config >> 24) & 0x0f);
	cfg->device_width = (config & BIT(23)) ? 16 : 8;
	cfg->col_adr_bytes = (config >> 12) & 0x07;
	cfg->blk_adr_bytes = (config >> 8) & 0x07;
	cfg->ful_adr_bytes = (config >> 16) & 0x07;

	if (ctrl->nand_version >= 0x0600)
		cfg->spare_area_size = acc_control & 0x7f;
	else
		cfg->spare_area_size = acc_control & 0x3f;

	cfg->sector_size_1k = brcmnand_get_sector_size_1k(host);

	cfg->ecc_level = (acc_control & brcmnand_ecc_level_mask(ctrl)) >> 16;
}

static void brcmstb_nand_print_cfg(char *buf, struct brcmstb_nand_cfg *cfg)
{
	buf += sprintf(buf,
		"%lluMiB total, %uKiB blocks, %u%s pages, %uB OOB, %u-bit",
		(unsigned long long)cfg->device_size >> 20,
		cfg->block_size >> 10,
		cfg->page_size >= 1024 ? cfg->page_size >> 10 : cfg->page_size,
		cfg->page_size >= 1024 ? "KiB" : "B",
		cfg->spare_area_size, cfg->device_width);

	/* Account for Hamming ECC and for BCH 512B vs 1KiB sectors */
	if (is_hamming_ecc(cfg))
		sprintf(buf, ", Hamming ECC");
	else if (cfg->sector_size_1k)
		sprintf(buf, ", BCH-%u (1KiB sector)", cfg->ecc_level << 1);
	else
		sprintf(buf, ", BCH-%u\n", cfg->ecc_level);
}

/*
 * Return true if the two configurations are basically identical. Note that we
 * allow certain variations in spare area size.
 */
static bool brcmstb_nand_config_match(struct brcmstb_nand_cfg *orig,
		struct brcmstb_nand_cfg *new)
{
	/* Negative matches */
	if (orig->device_size != new->device_size)
		return false;
	if (orig->block_size != new->block_size)
		return false;
	if (orig->page_size != new->page_size)
		return false;
	if (orig->device_width != new->device_width)
		return false;
	if (orig->col_adr_bytes != new->col_adr_bytes)
		return false;
	/* blk_adr_bytes can be larger than expected, but not smaller */
	if (orig->blk_adr_bytes < new->blk_adr_bytes)
		return false;
	if (orig->ful_adr_bytes < new->ful_adr_bytes)
		return false;

	/* Positive matches */
	if (orig->spare_area_size == new->spare_area_size)
		return true;
	return orig->spare_area_size >= 27 &&
	       orig->spare_area_size <= new->spare_area_size;
}

/*
 * Minimum number of bytes to address a page. Calculated as:
 *     roundup(log2(size / page-size) / 8)
 *
 * NB: the following does not "round up" for non-power-of-2 'size'; but this is
 *     OK because many other things will break if 'size' is irregular...
 */
static inline int get_blk_adr_bytes(u64 size, u32 writesize)
{
	return ALIGN(ilog2(size) - ilog2(writesize), 8) >> 3;
}

static int brcmstb_nand_setup_dev(struct brcmstb_nand_host *host)
{
	struct mtd_info *mtd = &host->mtd;
	struct nand_chip *chip = &host->chip;
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	struct brcmstb_nand_cfg orig_cfg, new_cfg;
	char msg[128];
	u32 mask, offs, tmp;

	brcmstb_nand_get_cfg(host, &orig_cfg);
	host->hwcfg = orig_cfg;

	memset(&new_cfg, 0, sizeof(new_cfg));
	new_cfg.device_size = mtd->size;
	new_cfg.block_size = mtd->erasesize;
	new_cfg.page_size = mtd->writesize;
	new_cfg.spare_area_size = mtd->oobsize / (mtd->writesize >> FC_SHIFT);
	new_cfg.device_width = (chip->options & NAND_BUSWIDTH_16) ? 16 : 8;
	new_cfg.col_adr_bytes = 2;
	new_cfg.blk_adr_bytes = get_blk_adr_bytes(mtd->size, mtd->writesize);

	new_cfg.ful_adr_bytes = new_cfg.blk_adr_bytes;
	if (mtd->writesize > 512)
		new_cfg.ful_adr_bytes += new_cfg.col_adr_bytes;
	else
		new_cfg.ful_adr_bytes += 1;

	if (new_cfg.spare_area_size > ctrl->max_oob)
		new_cfg.spare_area_size = ctrl->max_oob;

	if (!brcmstb_nand_config_match(&orig_cfg, &new_cfg)) {
		if (ctrl->nand_version >= 0x0500)
			/* default to 1K sector size (if page is large enough) */
			new_cfg.sector_size_1k = (new_cfg.page_size >= 1024) ? 1 : 0;

		if (new_cfg.spare_area_size >= 36 && new_cfg.sector_size_1k)
			new_cfg.ecc_level = 20;
		else if (new_cfg.spare_area_size >= 22)
			new_cfg.ecc_level = 12;
		else if (chip->badblockpos == NAND_SMALL_BADBLOCK_POS)
			new_cfg.ecc_level = 5;
		else
			new_cfg.ecc_level = 8;

		brcmstb_nand_set_cfg(host, &new_cfg);
		host->hwcfg = new_cfg;

		brcmnand_set_ecc_enabled(host, 1);

		if (brcmnand_read_reg(ctrl, BRCMNAND_CS_SELECT) &
				(0x100 << host->cs)) {
			/* bootloader activated this CS */
			dev_warn(&host->pdev->dev, "overriding bootloader "
				"settings on CS%d\n", host->cs);
			brcmstb_nand_print_cfg(msg, &orig_cfg);
			dev_warn(&host->pdev->dev, "was: %s\n", msg);
			brcmstb_nand_print_cfg(msg, &new_cfg);
			dev_warn(&host->pdev->dev, "now: %s\n", msg);
		} else {
			/*
			 * nandcs= argument activated this CS; assume that
			 * nobody even tried to set the device configuration
			 */
			brcmstb_nand_print_cfg(msg, &new_cfg);
			dev_info(&host->pdev->dev, "detected %s\n", msg);
		}
	} else {
		/*
		 * Set oobsize to be consistent with controller's
		 * spare_area_size. This helps nandwrite testing.
		 */
		mtd->oobsize = orig_cfg.spare_area_size *
			       (mtd->writesize >> FC_SHIFT);

		brcmstb_nand_print_cfg(msg, &orig_cfg);
		dev_info(&host->pdev->dev, "%s\n", msg);
	}

	/* PARTIAL_PAGE_EN, RD_ERASED_ECC_EN, FAST_PGM_RDIN */
	mask = BIT(26) | BIT(27) | BIT(28);
	if (ctrl->nand_version >= 0x0600)
		mask |= BIT(23); /* PREFETCH_EN */

	offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_ACC_CONTROL);
	tmp = __raw_readl(ctrl->nand_base + offs);
	tmp &= ~mask;
	__raw_writel(tmp, ctrl->nand_base + offs);

	mb();

	return 0;
}

static int brcmstb_nand_init_cs(struct brcmstb_nand_host *host)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	struct device_node *dn = host->of_node;
	struct platform_device *pdev = host->pdev;
	struct mtd_info *mtd;
	struct nand_chip *chip;
	int ret = 0;
	struct mtd_part_parser_data ppdata = { .of_node = dn };

	ret = of_property_read_u32(dn, "reg", &host->cs);
	if (ret) {
		dev_err(&pdev->dev, "can't get chip-select\n");
		return -ENXIO;
	}

	mtd = &host->mtd;
	chip = &host->chip;

	chip->priv = host;
	mtd->priv = chip;
	mtd->name = dev_name(&pdev->dev);
	mtd->owner = THIS_MODULE;
	mtd->dev.parent = &pdev->dev;

	chip->IO_ADDR_R = (void *)0xdeadbeef;
	chip->IO_ADDR_W = (void *)0xdeadbeef;

	chip->cmd_ctrl = brcmstb_nand_cmd_ctrl;
	chip->cmdfunc = brcmstb_nand_cmdfunc;
	chip->waitfunc = brcmstb_nand_waitfunc;
	chip->read_byte = brcmstb_nand_read_byte;
	chip->read_buf = brcmstb_nand_read_buf;
	chip->write_buf = brcmstb_nand_write_buf;

	chip->ecc.mode = NAND_ECC_HW;
	chip->ecc.read_page = brcmstb_nand_read_page;
	chip->ecc.read_subpage = brcmstb_nand_read_subpage;
	chip->ecc.write_page = brcmstb_nand_write_page;
	chip->ecc.read_page_raw = brcmstb_nand_read_page_raw;
	chip->ecc.write_page_raw = brcmstb_nand_write_page_raw;
	chip->ecc.write_oob_raw = brcmstb_nand_write_oob_raw;
	chip->ecc.read_oob_raw = brcmstb_nand_read_oob_raw;
	chip->ecc.read_oob = brcmstb_nand_read_oob;
	chip->ecc.write_oob = brcmstb_nand_write_oob;

	chip->controller = &ctrl->controller;

	if (nand_scan_ident(mtd, 1, NULL))
		return -ENXIO;

	chip->options |= NAND_NO_SUBPAGE_WRITE;
	/*
	 * NAND_USE_BOUNCE_BUFFER option prevents us from getting
	 * passed kmapped buffer that we cannot DMA.
	 * When option is set nand_base passes preallocated poi
	 * buffer that is used as bounce buffer for DMA
	 */
	chip->options |= NAND_USE_BOUNCE_BUFFER;

	if (of_get_nand_on_flash_bbt(dn))
		chip->bbt_options |= NAND_BBT_USE_FLASH | NAND_BBT_NO_OOB;

	if (brcmstb_nand_setup_dev(host))
		return -ENXIO;

	/* nand_scan_tail() needs this to be set up */
	if (is_hamming_ecc(&host->hwcfg))
		chip->ecc.strength = 1;
	else
		chip->ecc.strength = host->hwcfg.ecc_level
				<< host->hwcfg.sector_size_1k;
	chip->ecc.size = host->hwcfg.sector_size_1k ? 1024 : 512;
	/* only use our internal HW threshold */
	mtd->bitflip_threshold = 1;

	chip->ecc.layout = brcmstb_choose_ecc_layout(host);
	if (!chip->ecc.layout)
		return -ENXIO;

	if (nand_scan_tail(mtd))
		return -ENXIO;

	return mtd_device_parse_register(mtd, NULL, &ppdata, NULL, 0);
}

static void brcmnand_save_restore_cs_config(struct brcmstb_nand_host *host,
		int restore)
{
	struct brcmstb_nand_controller *ctrl = host->ctrl;
	u16 cfg_offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_CFG);
	u16 cfg_ext_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_CFG_EXT);
	u16 acc_control_offs = brcmnand_cs_offset(ctrl, host->cs,
			BRCMNAND_CS_ACC_CONTROL);
	u16 t1_offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_TIMING1);
	u16 t2_offs = brcmnand_cs_offset(ctrl, host->cs, BRCMNAND_CS_TIMING2);

	if (restore) {
		__raw_writel(host->hwcfg.config, ctrl->nand_base + cfg_offs);
		if (cfg_offs != cfg_ext_offs)
			__raw_writel(host->hwcfg.config_ext,
					ctrl->nand_base + cfg_ext_offs);
		__raw_writel(host->hwcfg.acc_control,
				ctrl->nand_base + acc_control_offs);
		__raw_writel(host->hwcfg.timing_1, ctrl->nand_base + t1_offs);
		__raw_writel(host->hwcfg.timing_2, ctrl->nand_base + t2_offs);
	} else {
		host->hwcfg.config = __raw_readl(ctrl->nand_base + cfg_offs);
		if (cfg_offs != cfg_ext_offs)
			host->hwcfg.config_ext =
				__raw_readl(ctrl->nand_base + cfg_ext_offs);
		host->hwcfg.acc_control =
			__raw_readl(ctrl->nand_base + acc_control_offs);
		host->hwcfg.timing_1 = __raw_readl(ctrl->nand_base + t1_offs);
		host->hwcfg.timing_2 = __raw_readl(ctrl->nand_base + t2_offs);
	}
}

static int brcmstb_nand_suspend(struct device *dev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(dev);
	struct brcmstb_nand_host *host;

	dev_dbg(dev, "Save state for S3 suspend\n");

	list_for_each_entry(host, &ctrl->host_list, node)
		brcmnand_save_restore_cs_config(host, 0);

	ctrl->nand_cs_nand_select = brcmnand_read_reg(ctrl, BRCMNAND_CS_SELECT);
	ctrl->nand_cs_nand_xor = brcmnand_read_reg(ctrl, BRCMNAND_CS_XOR);
	ctrl->corr_stat_threshold =
		brcmnand_read_reg(ctrl, BRCMNAND_CORR_THRESHOLD);

	if (has_flash_dma(ctrl))
		ctrl->flash_dma_mode = flash_dma_readl(ctrl, FLASH_DMA_MODE);

	return 0;
}

static int brcmstb_nand_resume(struct device *dev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(dev);
	struct brcmstb_nand_host *host;

	dev_dbg(dev, "Restore state after S3 suspend\n");

	if (has_flash_dma(ctrl)) {
		flash_dma_writel(ctrl, FLASH_DMA_MODE, ctrl->flash_dma_mode);
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);
	}

	brcmnand_write_reg(ctrl, BRCMNAND_CS_SELECT, ctrl->nand_cs_nand_select);
	brcmnand_write_reg(ctrl, BRCMNAND_CS_XOR, ctrl->nand_cs_nand_xor);
	brcmnand_write_reg(ctrl, BRCMNAND_CORR_THRESHOLD,
			ctrl->corr_stat_threshold);

	list_for_each_entry(host, &ctrl->host_list, node) {
		struct mtd_info *mtd = &host->mtd;
		struct nand_chip *chip = mtd->priv;

		brcmnand_save_restore_cs_config(host, 1);

		/* Reset the chip, required by some chips after power-up */
		chip->cmdfunc(mtd, NAND_CMD_RESET, -1, -1);
	}

	return 0;
}

static const struct dev_pm_ops brcmstb_nand_pm_ops = {
	.suspend		= brcmstb_nand_suspend,
	.resume			= brcmstb_nand_resume,
};

/***********************************************************************
 * Platform driver setup (per controller)
 ***********************************************************************/

static int brcmstb_nand_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *dn = dev->of_node, *child;
	static struct brcmstb_nand_controller *ctrl;
	struct resource *res;
	int ret;

	/* We only support device-tree instantiation */
	if (!dn)
		return -ENODEV;

	ctrl = devm_kzalloc(dev, sizeof(*ctrl), GFP_KERNEL);
	if (!ctrl)
		return -ENOMEM;

	dev_set_drvdata(dev, ctrl);

	init_completion(&ctrl->done);
	init_completion(&ctrl->dma_done);
	spin_lock_init(&ctrl->controller.lock);
	init_waitqueue_head(&ctrl->controller.wq);
	INIT_LIST_HEAD(&ctrl->host_list);

	/*
	 * NAND
	 * FIXME: eventually, should get resource by name ("nand"), but the DT
	 * binding may be in flux for a while (8/16/13)
	 */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(dev, "can't get NAND register range\n");
		return -ENODEV;
	}

	ctrl->nand_base = devm_request_and_ioremap(dev, res);
	if (!ctrl->nand_base)
		return -ENODEV;

	/* Initialize NAND revision */
	ret = brcmstb_nand_revision_init(ctrl);
	if (ret)
		return ret;

	/* Locate NAND flash cache */
	ctrl->nand_fc = ctrl->nand_base + ctrl->reg_offsets[BRCMNAND_FC_BASE];

	/* FLASH_DMA */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "flash-dma");
	if (res) {
		ctrl->flash_dma_base = devm_request_and_ioremap(dev, res);
		if (!ctrl->flash_dma_base)
			return -ENODEV;

		flash_dma_writel(ctrl, FLASH_DMA_MODE, 1); /* linked-list */
		flash_dma_writel(ctrl, FLASH_DMA_ERROR_STATUS, 0);

		/* Allocate descriptor(s) */
		ctrl->dma_desc = dmam_alloc_coherent(dev,
						     sizeof(*ctrl->dma_desc),
						     &ctrl->dma_pa, GFP_KERNEL);
		if (!ctrl->dma_desc)
			return -ENOMEM;

		ctrl->dma_irq = platform_get_irq(pdev, 1);
		if ((int)ctrl->dma_irq < 0) {
			dev_err(dev, "missing FLASH_DMA IRQ\n");
			return -ENODEV;
		}

		ret = devm_request_irq(dev, ctrl->dma_irq,
				brcmstb_nand_dma_irq, 0, DRV_NAME,
				ctrl);
		if (ret < 0) {
			dev_err(dev, "can't allocate IRQ %d: error %d\n",
					ctrl->dma_irq, ret);
			return ret;
		}

		dev_info(dev, "enabling FLASH_DMA\n");
	}
	/* Disable automatic device ID config, direct addressing, and XOR */
	brcmnand_rmw_reg(ctrl, BRCMNAND_CS_SELECT, (1 << 30) | 0xff, 0, 0);
	brcmnand_rmw_reg(ctrl, BRCMNAND_CS_XOR, 0xff, 0, 0);

	if (brcmstb_nand_has_wp(ctrl)) {
		if (wp_on == 2) /* SWLINUX-1818: Permanently remove write-protection */
			brcmnand_rmw_reg(ctrl, BRCMNAND_CS_SELECT,
					1 << 29, 29, 0);
	} else {
		wp_on = 0;
	}

	/* IRQ */
	ctrl->irq = platform_get_irq(pdev, 0);
	if ((int)ctrl->irq < 0) {
		dev_err(dev, "no IRQ defined\n");
		return -ENODEV;
	}
	ret = devm_request_irq(dev, ctrl->irq, brcmstb_nand_ctlrdy_irq, 0,
			DRV_NAME, ctrl);
	if (ret < 0) {
		dev_err(dev, "can't allocate IRQ %d: error %d\n",
			ctrl->irq, ret);
		return ret;
	}
	ret = of_property_read_u32(dn, "semaphore-magic", &ctrl->semagic);
	if (ret) {
		ctrl->controller.hw_lock = NULL;
		ctrl->controller.hw_unlock = NULL;
	}
	else {
		ctrl->controller.hw_lock = controller_hw_lock;
		ctrl->controller.hw_unlock = controller_hw_unlock;
		disable_irq(ctrl->irq);
		controller_hw_lock(&ctrl->controller);
	}

	for_each_available_child_of_node(dn, child) {
		if (of_device_is_compatible(child, "brcm,nandcs")) {
			struct brcmstb_nand_host *host;

			host = devm_kzalloc(dev, sizeof(*host), GFP_KERNEL);
			if (!host)
				return -ENOMEM;
			host->pdev = pdev;
			host->ctrl = ctrl;
			host->of_node = child;

			ret = brcmstb_nand_init_cs(host);
			if (ret)
				continue; /* Try all chip-selects */

			list_add_tail(&host->node, &ctrl->host_list);
		}
	}
	if (ctrl->controller.hw_unlock)
		controller_hw_unlock(&ctrl->controller);
	/* No chip-selects could initialize properly */
	if (list_empty(&ctrl->host_list))
		return -ENODEV;

	return 0;
}

static int brcmstb_nand_remove(struct platform_device *pdev)
{
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(&pdev->dev);
	struct brcmstb_nand_host *host;

	list_for_each_entry(host, &ctrl->host_list, node)
		nand_release(&host->mtd);

	dev_set_drvdata(&pdev->dev, NULL);

	return 0;
}

static void brcmstb_nand_shutdown(struct platform_device *pdev)
{
	int n = 0;
	struct brcmstb_nand_controller *ctrl = dev_get_drvdata(&pdev->dev);
	spin_lock(&ctrl->controller.lock);
	if (ctrl->lock_cnt && ctrl->controller.hw_unlock)
		controller_hw_unlock(&ctrl->controller);
}

static const struct of_device_id brcmstb_nand_of_match[] = {
	{ .compatible = "brcm,brcmnand-v4.0" },
	{ .compatible = "brcm,brcmnand-v5.0" },
	{ .compatible = "brcm,brcmnand-v6.0" },
	{ .compatible = "brcm,brcmnand-v7.0" },
	{ .compatible = "brcm,brcmnand-v7.1" },
	{},
};

static struct platform_driver brcmstb_nand_driver = {
	.probe			= brcmstb_nand_probe,
	.remove			= brcmstb_nand_remove,
	.shutdown       	= brcmstb_nand_shutdown,
	.driver = {
		.name		= DRV_NAME,
		.pm		= &brcmstb_nand_pm_ops,
		.of_match_table = of_match_ptr(brcmstb_nand_of_match),
	}
};
module_platform_driver(brcmstb_nand_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("NAND driver for STB chips");
MODULE_ALIAS("platform:brcmnand");
