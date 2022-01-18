/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 *  Copyright (C) 2015 Jim Quinlan Broadcom Corporation
 */
#include <linux/init.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <linux/printk.h>
#include <linux/syscore_ops.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/module.h>
#include <linux/of_pci.h>
#include <linux/spinlock.h>
#include <linux/regulator/consumer.h>

/* Broadcom PCIE Offsets */
#define PCIE_RC_CFG_PCIE_LINK_CAPABILITY		0x00b8
#define PCIE_RC_CFG_PCIE_LINK_STATUS_CONTROL		0x00bc
#define PCIE_RC_CFG_PCIE_ROOT_CAP_CONTROL		0x00c8
#define PCIE_RC_CFG_PCIE_LINK_STATUS_CONTROL_2		0x00dc
#define PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1		0x0188
#define PCIE_RC_CFG_PRIV1_ID_VAL3			0x043c
#define PCIE_RC_DL_MDIO_ADDR				0x1100
#define PCIE_RC_DL_MDIO_WR_DATA				0x1104
#define PCIE_RC_DL_MDIO_RD_DATA				0x1108
#define PCIE_MISC_MISC_CTRL				0x4008
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LO		0x400c
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_HI		0x4010
#define PCIE_MISC_RC_BAR1_CONFIG_LO			0x402c
#define PCIE_MISC_RC_BAR1_CONFIG_HI			0x4030
#define PCIE_MISC_RC_BAR2_CONFIG_LO			0x4034
#define PCIE_MISC_RC_BAR2_CONFIG_HI			0x4038
#define PCIE_MISC_RC_BAR3_CONFIG_LO			0x403c
#define PCIE_MISC_RC_BAR3_CONFIG_HI			0x4040
#define PCIE_MISC_MSI_BAR_CONFIG_LO			0x4044
#define PCIE_MISC_MSI_BAR_CONFIG_HI			0x4048
#define PCIE_MISC_PCIE_STATUS				0x4068
#define PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT	0x4070
#define PCIE_MISC_UBUS_BAR2_CONFIG_REMAP		0x408c
#define PCIE_MISC_HARD_PCIE_HARD_DEBUG			0x4204
#define PCIE_INTR2_CPU_CLEAR				0x4308
#define PCIE_INTR2_CPU_MASK_SET				0x4310
#define PCIE_INTR2_CPU_MASK_CLEAR			0x4314
#if CONFIG_BCM3384
#define PCIE_EXT_CFG_PCIE_EXT_CFG_INDEX			0x0
#define PCIE_EXT_CFG_PCIE_EXT_CFG_DATA			0x4
#else
#define PCIE_EXT_CFG_PCIE_EXT_CFG_INDEX			0x8400
#define PCIE_EXT_CFG_PCIE_EXT_CFG_DATA			0x8404
#endif
#define PCIE_RGR1_SW_INIT_1				0x9210

#define INT_PER_PCIE_SOFTRESETB_LO			0x0

#define BRCM_NUM_PCI_OUT_WINS		0x4
#define BRCM_MAX_PCI_CONTROLLERS	4

#define PCI_BUSNUM_SHIFT		20
#define PCI_SLOT_SHIFT			15
#define PCI_FUNC_SHIFT			12

/*
 * Note: our PCIe core does not support IO BARs at all.  MEM only.
 * The Linux PCI code insists on having IO resources and io_map_base for
 * all PCI controllers, so those values are totally bogus.
 */
#define IO_ADDR_PCIE		0x400
#define PCIE_IO_SIZE		0x400
#define BOGUS_IO_MAP_BASE	1

#define IDX_ADDR(base)		((base) + PCIE_EXT_CFG_PCIE_EXT_CFG_INDEX)
#define DATA_ADDR(base)		((base) + PCIE_EXT_CFG_PCIE_EXT_CFG_DATA)

static int brcm_pci_read_config(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *data);
static int brcm_pci_write_config(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 data);

static struct pci_ops brcm_pci_ops = {
	.read = brcm_pci_read_config,
	.write = brcm_pci_write_config,
};

#if CONFIG_BCM3384
static struct brcm_perst {
	void __iomem		*base;
	spinlock_t		lock;
	struct device		*dev;
} brcm_perst;
#endif

struct brcm_window {
	u64 pci_addr;
	u64 size;
	u64 cpu_addr;
	u32 info;
};

struct brcm_dev_pwr_supply {
	struct list_head node;
	char name[32];
	struct regulator *regulator;
};

/* Internal Bus Controller Information.*/
static struct brcm_pcie {
	struct pci_controller	controller;
	void __iomem		*base;
	void __iomem		*base_cfg;
	char			name[8];
	bool			suspended;
	struct clk		*clk;
	int			num_out_wins;
	struct resource		mem_res;
	struct resource		io_res;
	bool			ssc;
	int			gen;
	struct brcm_window	out_wins[BRCM_NUM_PCI_OUT_WINS];
	int			id;
	struct device		*dev;
	struct list_head	pwr_supplies;
} *brcm_pcie[BRCM_MAX_PCI_CONTROLLERS];

static int brcm_num_pci_controllers;
static int num_memc;
static void turn_off(struct brcm_pcie *);

#if defined(__BIG_ENDIAN)
#define	DATA_ENDIAN			2	/* PCI->DDR inbound accesses */
#define MMIO_ENDIAN			2	/* CPU->PCI outbound accesses */
#else
#define	DATA_ENDIAN			0
#define MMIO_ENDIAN			0
#endif

/* negative return value indicates error */
static int mdio_read(void __iomem *base, u8 phyad, u8 regad)
{
	u32 data = ((phyad & 0xf) << 16)
		| (regad & 0x1f)
		| 0x100000;

	__raw_writel(data, base + PCIE_RC_DL_MDIO_ADDR);
	__raw_readl(base + PCIE_RC_DL_MDIO_ADDR);

	data = __raw_readl(base + PCIE_RC_DL_MDIO_RD_DATA);
	if (!(data & 0x80000000)) {
		mdelay(1);
		data = __raw_readl(base + PCIE_RC_DL_MDIO_RD_DATA);
	}
	return (data & 0x80000000) ? (data & 0xffff) : -EIO;
}


/* negative return value indicates error */
static int mdio_write(void __iomem *base, u8 phyad, u8 regad, u16 wrdata)
{
	u32 data = ((phyad & 0xf) << 16) | (regad & 0x1f);

	__raw_writel(data, base + PCIE_RC_DL_MDIO_ADDR);
	__raw_readl(base + PCIE_RC_DL_MDIO_ADDR);

	__raw_writel(0x80000000 | wrdata, base + PCIE_RC_DL_MDIO_WR_DATA);
	data = __raw_readl(base + PCIE_RC_DL_MDIO_WR_DATA);
	if (!(data & 0x80000000)) {
		mdelay(1);
		data = __raw_readl(base + PCIE_RC_DL_MDIO_WR_DATA);
	}
	return (data & 0x80000000) ? 0 : -EIO;
}


static void wr_fld(void __iomem *p, u32 mask, int shift, u32 val)
{
	u32 reg = __raw_readl(p);
	reg = (reg & ~mask) | (val << shift);
	__raw_writel(reg, p);
}


static void wr_fld_rb(void __iomem *p, u32 mask, int shift, u32 val)
{
	wr_fld(p, mask, shift, val);
	(void) __raw_readl(p);
}


/* configures device for ssc mode; negative return value indicates error */
static int set_ssc(void __iomem *base)
{
	int tmp;
	u16 wrdata;

	tmp = mdio_write(base, 0, 0x1f, 0x1100);
	if (tmp < 0)
		return tmp;

	tmp = mdio_read(base, 0, 2);
	if (tmp < 0)
		return tmp;

	wrdata = ((u16)tmp & 0x3fff) | 0xc000;
	tmp = mdio_write(base, 0, 2, wrdata);
	if (tmp < 0)
		return tmp;

	mdelay(1);
	tmp = mdio_read(base, 0, 1);
	if (tmp < 0)
		return tmp;

	return 0;
}


/* returns 0 if in ssc mode, 1 if not, <0 on error */
static int is_ssc(void __iomem *base)
{
	int tmp = mdio_write(base, 0, 0x1f, 0x1100);
	if (tmp < 0)
		return tmp;
	tmp = mdio_read(base, 0, 1);
	if (tmp < 0)
		return tmp;
	return (tmp & 0xc00) == 0xc00 ? 0 : 1;
}


/* limits operation to a specific generation (1, 2, or 3) */
static void set_gen(void __iomem *base, int gen)
{
	wr_fld(base + PCIE_RC_CFG_PCIE_LINK_CAPABILITY, 0xf, 0, gen);
	wr_fld(base + PCIE_RC_CFG_PCIE_LINK_STATUS_CONTROL_2, 0xf, 0, gen);
}


static void set_pcie_outbound_win(void __iomem *base, unsigned win, u64 start,
				  u64 len)
{
	u32 tmp;

	__raw_writel((u32)(start) + MMIO_ENDIAN,
		     base + PCIE_MISC_CPU_2_PCIE_MEM_WIN0_LO+(win*8));
	__raw_writel((u32)(start >> 32),
		     base + PCIE_MISC_CPU_2_PCIE_MEM_WIN0_HI+(win*8));
	tmp = ((((u32)start) >> 20) << 4)
		| (((((u32)start) + ((u32)len) - 1) >> 20) << 20);
	__raw_writel(tmp,
		     base + PCIE_MISC_CPU_2_PCIE_MEM_WIN0_BASE_LIMIT+(win*4));
}


static int is_pcie_link_up(struct brcm_pcie *pcie)
{
	void __iomem *base = pcie->base;
	u32 val = __raw_readl(base + PCIE_MISC_PCIE_STATUS);
	return  ((val & 0x30) == 0x30) ? 1 : 0;
}


static void pcie_reset(struct brcm_pcie *pcie, bool assert)
{
	int val;

	if (IS_ENABLED(CONFIG_BCM3384)) {
		unsigned long flags;

		val = assert ? 0 : 1;
		spin_lock_irqsave(&brcm_perst.lock, flags);
		wr_fld_rb(brcm_perst.base + INT_PER_PCIE_SOFTRESETB_LO,
			  1 << pcie->id, pcie->id, val);
		spin_unlock_irqrestore(&brcm_perst.lock, flags);
	} else {
		/* field: PCIE_SW_PERST = assert */
		val = assert ? 1 : 0;
		wr_fld_rb(pcie->base + PCIE_RGR1_SW_INIT_1, 0x00000001, 0, val);
	}
}


static void bridge_reset(struct brcm_pcie *pcie, bool assert)
{
	int val = assert ? 1 : 0;
	if (!IS_ENABLED(CONFIG_BCM3384))
		/* field: PCIE_BRIDGE_SW_INIT = 1 */
		wr_fld_rb(pcie->base + PCIE_RGR1_SW_INIT_1, 0x00000002, 1, val);
}


static void brcm_pcie_setup_early(struct brcm_pcie *pcie)
{
	void __iomem *base = pcie->base;
	u64 total_out_win_size = 0;
	int i;

	bridge_reset(pcie, true);
	pcie_reset(pcie, true);

	/* delay 100us */
	udelay(100);
	bridge_reset(pcie, false);

	/* enable SCB_MAX_BURST_SIZE | CSR_READ_UR_MODE | SCB_ACCESS_EN */
	__raw_writel(0x81e03000, base + PCIE_MISC_MISC_CTRL);

	for (i = 0; i < pcie->num_out_wins; i++) {
		struct brcm_window *w = &pcie->out_wins[i];
		set_pcie_outbound_win(base, i, w->cpu_addr, w->size);
		total_out_win_size += w->size;
	}

	pcie->mem_res.name = "External PCIe Mem";
	pcie->mem_res.start = pcie->out_wins[0].cpu_addr;
	pcie->mem_res.end = pcie->out_wins[0].cpu_addr + total_out_win_size - 1;
	pcie->mem_res.flags = IORESOURCE_MEM;
	pcie->controller.mem_resource = &pcie->mem_res;

	pcie->io_res.name = "External PCIe IO (unavailable)";
	pcie->io_res.start = IO_ADDR_PCIE * (pcie->id + 1);
	pcie->io_res.end = pcie->io_res.start + PCIE_IO_SIZE - 1;
	pcie->io_res.flags = IORESOURCE_MEM;
	pcie->controller.io_resource = &pcie->io_res;

	pcie->controller.io_map_base = BOGUS_IO_MAP_BASE,
	pcie->controller.pci_ops = &brcm_pci_ops;

	/* set up 4GB PCIE->SCB memory window on BAR2 */
	__raw_writel(0x00000011, base + PCIE_MISC_RC_BAR2_CONFIG_LO);
	__raw_writel(0x00000000, base + PCIE_MISC_RC_BAR2_CONFIG_HI);

	/* field: SCB0_SIZE = 4/1 GB */
	if (IS_ENABLED(CONFIG_BCM3384))
		wr_fld(base + PCIE_MISC_MISC_CTRL, 0xf8000000, 27, 0x11);
	else
		wr_fld(base + PCIE_MISC_MISC_CTRL, 0xf8000000, 27, 0x0f);
	/* field: SCB1_SIZE = 1 GB */
	if (num_memc > 1)
		wr_fld(base + PCIE_MISC_MISC_CTRL, 0x07c00000, 22, 0x0f);
	/* field: SCB2_SIZE = 1 GB */
	if (num_memc > 2)
		wr_fld(base + PCIE_MISC_MISC_CTRL, 0x0000001f, 0, 0x0f);

	if (IS_ENABLED(CONFIG_BCM3384))
		wr_fld_rb(base + PCIE_MISC_UBUS_BAR2_CONFIG_REMAP, 0x1, 0, 1);

	/* disable the PCIE->GISB memory window */
	__raw_writel(0x00000000, base + PCIE_MISC_RC_BAR1_CONFIG_LO);

	/* disable the PCIE->SCB memory window */
	__raw_writel(0x00000000, base + PCIE_MISC_RC_BAR3_CONFIG_LO);

	/* disable MSI (for now...) */
	__raw_writel(0x00000000, base + PCIE_MISC_MSI_BAR_CONFIG_LO);

	/* set up L2 interrupt masks */
	__raw_writel(0x00000000, base + PCIE_INTR2_CPU_CLEAR);
	(void) __raw_readl(base + PCIE_INTR2_CPU_CLEAR);
	__raw_writel(0x00000000, base + PCIE_INTR2_CPU_MASK_CLEAR);
	(void) __raw_readl(base + PCIE_INTR2_CPU_MASK_CLEAR);
	__raw_writel(0xffffffff, base + PCIE_INTR2_CPU_MASK_SET);
	(void) __raw_readl(base + PCIE_INTR2_CPU_MASK_SET);

	if (pcie->ssc)
		if (set_ssc(base))
			dev_err(pcie->dev, "error while configuring ssc mode\n");
	if (pcie->gen)
		set_gen(base, pcie->gen);

	/* take the EP device out of reset */
	pcie_reset(pcie, false);
}


static int brcm_setup_pcie_bridge(struct brcm_pcie *pcie)
{
	void __iomem *base = pcie->base;
	const int limit = pcie->suspended ? 1000 : 100;
	struct clk *clk;
	unsigned status;
	static const char *link_speed[4] = { "???", "2.5", "5.0", "8.0" };
	int i, j;

	/* Give the RC/EP time to wake up, before trying to configure RC.
	 * Intermittently check status for link-up, up to a total of 100ms
	 * when we don't know if the device is there, and up to 1000ms if
	 * we do know the device is there. */
	for (i = 1, j = 0; j < limit && !is_pcie_link_up(pcie); j += i, i = i*2)
		mdelay(i + j > limit ? limit - j : i);

	if (!is_pcie_link_up(pcie)) {
		dev_info(pcie->dev, "link down\n");
		goto FAIL;
	}

	/* For config space accesses on the RC, show the right class for
	 * a PCI-PCI bridge */
	wr_fld_rb(base + PCIE_RC_CFG_PRIV1_ID_VAL3, 0x00ffffff, 0, 0x060400);

	status = __raw_readl(base + PCIE_RC_CFG_PCIE_LINK_STATUS_CONTROL);
	dev_info(pcie->dev, "link up, %s Gbps x%u\n",
		 link_speed[((status & 0x000f0000) >> 16) & 0x3],
		 (status & 0x03f00000) >> 20);

	if (pcie->ssc && is_ssc(base) != 0)
		dev_err(pcie->dev, "failed to enter ssc mode\n");

	/* Enable configuration request retry (see pci_scan_device()) */
	/* field RC_CRS_EN = 1 */
	wr_fld(base + PCIE_RC_CFG_PCIE_ROOT_CAP_CONTROL, 0x00000010, 4, 1);

	/* PCIE->SCB endian mode for BAR */
	/* field ENDIAN_MODE_BAR2 = DATA_ENDIAN */
	wr_fld_rb(base + PCIE_RC_CFG_VENDOR_VENDOR_SPECIFIC_REG1, 0x0000000c, 2,
		  DATA_ENDIAN);

	return 1;
FAIL:
#if defined(CONFIG_PM)
	turn_off(pcie);
#endif
	clk = pcie->clk;
	if (pcie->suspended)
		clk_disable(clk);
	else {
		clk_disable_unprepare(clk);
		clk_put(clk);
	}
	return 0;

}


#if defined(CONFIG_PM)
static void turn_off(struct brcm_pcie *pcie)
{
	pcie_reset(pcie, true);
	/* SERDES_IDDQ = 1 */
	wr_fld_rb(pcie->base + PCIE_MISC_HARD_PCIE_HARD_DEBUG, 0x08000000,
		  27, 1);
	bridge_reset(pcie, true);
}


static int pcie_suspend(void)
{
	int i;

	for (i = 0; i < brcm_num_pci_controllers; i++) {
		struct brcm_pcie *pcie = brcm_pcie[i];
		void __iomem *base;
		struct list_head *pos;
		struct brcm_dev_pwr_supply *supply;

		if (!pcie)
			continue;

		base = pcie->base;
		turn_off(pcie);
		clk_disable(pcie->clk);
		list_for_each(pos, &pcie->pwr_supplies) {
			supply = list_entry(pos, struct brcm_dev_pwr_supply,
					    node);
			if (regulator_disable(supply->regulator))
				pr_debug("Unable to turn off %s supply.\n",
					 supply->name);

		}
		pcie->suspended = true;
	}
	return 0;
}


static void pcie_resume(void)
{
	int i;

	for (i = 0; i < brcm_num_pci_controllers; i++) {
		struct brcm_pcie *pcie = brcm_pcie[i];
		void __iomem *base;
		struct list_head *pos;
		struct brcm_dev_pwr_supply *supply;

		if (!pcie)
			continue;

		list_for_each(pos, &pcie->pwr_supplies) {
			supply = list_entry(pos, struct brcm_dev_pwr_supply,
					    node);
			if (regulator_enable(supply->regulator))
				pr_debug("Unable to turn on %s supply.\n",
					 supply->name);
		}
		base = pcie->base;
		clk_enable(pcie->clk);

		/* Take bridge out of reset so we can access the SERDES reg */
		bridge_reset(pcie, false);

		/* SERDES_IDDQ = 0 */
		wr_fld_rb(base + PCIE_MISC_HARD_PCIE_HARD_DEBUG, 0x08000000,
			  27, 0);
		/* wait for serdes to be stable */
		udelay(100);

		brcm_pcie_setup_early(pcie);
	}

	for (i = 0; i < brcm_num_pci_controllers; i++) {
		struct brcm_pcie *pcie = brcm_pcie[i];

		if (!pcie)
			continue;

		brcm_setup_pcie_bridge(pcie);
		pcie->suspended = false;
	}
}


static struct syscore_ops pcie_pm_ops = {
	.suspend        = pcie_suspend,
	.resume         = pcie_resume,
};
#endif


/***********************************************************************
 * Read/write PCI configuration registers
 ***********************************************************************/
static int cfg_index(int busnr, int devfn, int reg)
{
	return ((PCI_SLOT(devfn) & 0x1f) << PCI_SLOT_SHIFT)
		| ((PCI_FUNC(devfn) & 0x07) << PCI_FUNC_SHIFT)
		| (busnr << PCI_BUSNUM_SHIFT)
		| (reg & ~3);
}


static u32 read_config(void __iomem *base, int cfg_idx)
{
	__raw_writel(cfg_idx, IDX_ADDR(base));
	__raw_readl(IDX_ADDR(base));
	return __raw_readl(DATA_ADDR(base));
}


static void write_config(void __iomem *base, int cfg_idx, u32 val)
{
	__raw_writel(cfg_idx, IDX_ADDR(base));
	__raw_readl(IDX_ADDR(base));
	__raw_writel(val, DATA_ADDR(base));
	__raw_readl(DATA_ADDR(base));
}


static int brcm_pci_write_config(struct pci_bus *bus, unsigned int devfn,
				 int where, int size, u32 data)
{
	u32 val = 0, mask, shift;
	struct brcm_pcie *pcie = bus->sysdata;
	void __iomem *base = pcie->base;
	bool rc_access;
	int idx;

	if (!is_pcie_link_up(pcie))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (!pcie->controller.bus)
		pcie->controller.bus = bus;
	rc_access = bus == pcie->controller.bus;
	idx = cfg_index(bus->number, devfn, where);
	BUG_ON(((where & 3) + size) > 4);

	if (rc_access && PCI_SLOT(devfn))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (size < 4) {
		/* partial word - read, modify, write */
		if (rc_access)
			val = __raw_readl(base + (where & ~3));
		else
			val = read_config(pcie->base_cfg, idx);
	}

	shift = (where & 3) << 3;
	mask = (0xffffffff >> ((4 - size) << 3)) << shift;
	val = (val & ~mask) | ((data << shift) & mask);

	if (rc_access) {
		__raw_writel(val, base + (where & ~3));
		__raw_readl(base + (where & ~3));
	} else {
		write_config(pcie->base_cfg, idx, val);
	}
	return PCIBIOS_SUCCESSFUL;
}


static int brcm_pci_read_config(struct pci_bus *bus, unsigned int devfn,
				int where, int size, u32 *data)
{
	u32 val, mask, shift;
	struct brcm_pcie *pcie = bus->sysdata;
	void __iomem *base = pcie->base;
	bool rc_access;
	int idx;

	if (!is_pcie_link_up(pcie))
		return PCIBIOS_DEVICE_NOT_FOUND;

	if (!pcie->controller.bus)
		pcie->controller.bus = bus;
	rc_access = bus == pcie->controller.bus;
	idx = cfg_index(bus->number, devfn, where);
	BUG_ON(((where & 3) + size) > 4);

	if (rc_access && PCI_SLOT(devfn)) {
		*data = 0xffffffff;
		return PCIBIOS_FUNC_NOT_SUPPORTED;
	}

	if (rc_access)
		val = __raw_readl(base + (where & ~3));
	else
		val = read_config(pcie->base_cfg, idx);

	shift = (where & 3) << 3;
	mask = (0xffffffff >> ((4 - size) << 3)) << shift;
	*data = (val & mask) >> shift;

	return PCIBIOS_SUCCESSFUL;
}


/***********************************************************************
 * PCI slot to IRQ mappings (aka "fixup")
 ***********************************************************************/
int __init pcibios_map_irq(const struct pci_dev *dev, u8 slot, u8 pin)
{
	return of_irq_parse_and_map_pci(dev, slot, pin);
}


/***********************************************************************
 * Per-device initialization
 ***********************************************************************/
static void __attribute__((__section__("pci_fixup_early")))
brcm_pcibios_fixup(struct pci_dev *dev)
{
	int slot = PCI_SLOT(dev->devfn);
	struct brcm_pcie *pcie = dev->bus->sysdata;
	u8 pin = 0;

	pci_read_config_byte(dev, PCI_INTERRUPT_PIN, &pin);
	dev_info(pcie->dev,
		 "found device %04x:%04x on bus %d, slot %d, pin %d (irq %d)\n",
		 dev->vendor, dev->device, dev->bus->number, slot,
		 (int)pin, pcibios_map_irq(dev, slot, (int)pin));
}
DECLARE_PCI_FIXUP_EARLY(PCI_ANY_ID, PCI_ANY_ID, brcm_pcibios_fixup);


/***********************************************************************
 * Perst Platform Driver
 ***********************************************************************/
#if CONFIG_BCM3384
static int __init brcm_perst_probe(struct platform_device *pdev)
{
	void __iomem *base;
	struct resource *r;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -EINVAL;

	base = devm_request_and_ioremap(&pdev->dev, r);
	if (!base)
		return -ENOMEM;
	brcm_perst.base = base;
	spin_lock_init(&brcm_perst.lock);
	return 0;
}
#endif


/***********************************************************************
 * PCI Platform Driver
 ***********************************************************************/
static int __init brcm_pci_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node, *mdn;
	int len, i, rlen, pna, np, ret;
	struct resource *r;
	const u32 *ranges;
	void __iomem *base;
	u32 tmp;
	const __be32 *value;
	struct brcm_pcie *pcie = devm_kzalloc(&pdev->dev,
					      sizeof(struct brcm_pcie),
					      GFP_KERNEL);
	int supplies;
	const char *name;
	struct brcm_dev_pwr_supply *supply;

	if (!pcie)
		return -ENOMEM;

	INIT_LIST_HEAD(&pcie->pwr_supplies);
	supplies = of_property_count_strings(dn, "supply-names");
	if (supplies <= 0)
		supplies = 0;
	for (i = 0; i < supplies; i++) {
		if (of_property_read_string_index(dn, "supply-names", i,
						  &name))
			continue;
		supply = devm_kzalloc(&pdev->dev, sizeof(*supply), GFP_KERNEL);
		if (!supply)
			return -ENOMEM;
		strncpy(supply->name, name, sizeof(supply->name));
		supply->name[sizeof(supply->name) - 1] = '\0';
		supply->regulator = devm_regulator_get(&pdev->dev, name);
		if (IS_ERR(supply->regulator)) {
			dev_err(&pdev->dev, "Unable to get %s supply, err=%d\n",
				name, (int)PTR_ERR(supply->regulator));
			continue;
		}
		if (regulator_enable(supply->regulator))
			dev_err(&pdev->dev, "Unable to enable %s supply.\n",
				name);
		list_add(&supply->node, &pcie->pwr_supplies);
	}

	/* 'num_memc' will be set only by the first controller, and all
	 * other controllers will use the value set by the first. */
	if (IS_ENABLED(CONFIG_BCM3384))
		num_memc = 1;
	else if (num_memc == 0)
		for_each_compatible_node(mdn, NULL, "brcm,brcmstb-memc")
			if (of_device_is_available(mdn))
				num_memc++;

	r = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!r)
		return -EINVAL;

	base = devm_request_and_ioremap(&pdev->dev, r);
	if (!base)
		return -ENOMEM;

	if (IS_ENABLED(CONFIG_BCM3384)) {
		struct resource *r_cfg;
		r_cfg = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		if (!r_cfg)
			return -EINVAL;
		pcie->base_cfg = devm_request_and_ioremap(&pdev->dev, r_cfg);
		if (!pcie->base_cfg)
			return -ENOMEM;
	} else {
		pcie->base_cfg = base;
	}

	snprintf(pcie->name,
		 sizeof(pcie->name)-1, "PCIe%d", brcm_num_pci_controllers);
	pcie->suspended = false;
	pcie->clk = of_clk_get_by_name(dn, "sw_pcie");
	if (IS_ERR(pcie->clk)) {
		dev_err(&pdev->dev, "could not get clock\n");
		pcie->clk = NULL;
	}
	ret = clk_prepare_enable(pcie->clk);
	if (ret) {
		dev_err(&pdev->dev, "could not enable clock\n");
		return ret;
	}
	pcie->controller.of_node = dn;
	pcie->base = base;
	pcie->dev = &pdev->dev;
	pcie->gen = 0;

	ret = of_property_read_u32(dn, "brcm,gen", &tmp);
	if (ret == 0) {
		if (tmp > 0 && tmp < 3)
			pcie->gen = (int) tmp;
		else
			dev_warn(pcie->dev, "bad DT value for prop 'brcm,gen");
	} else if (ret != -EINVAL) {
		dev_warn(pcie->dev, "error reading DT prop 'brcm,gen");
	}

	value = of_get_property(dn, "linux,pci-domain", &len);
	if (!value || len < sizeof(*value))
		return -EINVAL;
	/* For now, we use the domain number as our controller id. */
	pcie->id = be32_to_cpup(value);

	pcie->ssc = of_property_read_bool(dn, "brcm,ssc");

	ranges = of_get_property(dn, "ranges", &rlen);
	if (ranges == NULL) {
		dev_err(pcie->dev, "no ranges property in dev tree.\n");
		return -EINVAL;
	}
	/* set up CPU->PCIE memory windows (max of four) */
	pna = of_n_addr_cells(dn);
	np = pna + 5;

	pcie->num_out_wins = rlen / (np * 4);

	for (i = 0; i < pcie->num_out_wins; i++) {
		struct brcm_window *w = &pcie->out_wins[i];
		w->info = (u32) of_read_ulong(ranges + 0, 1);
		w->pci_addr = of_read_number(ranges + 1, 2);
		w->cpu_addr = of_translate_address(dn, ranges + 3);
		w->size = of_read_number(ranges + pna + 3, 2);
		ranges += np;
	}

	brcm_pcie_setup_early(pcie);
	if (!brcm_setup_pcie_bridge(pcie))
		return -EINVAL;
	register_pci_controller(&pcie->controller);
	brcm_pcie[brcm_num_pci_controllers++] = pcie;

	return 0;
}


/***********************************************************************
 * PCI reset platform driver for BCM3384
 ***********************************************************************/
#if CONFIG_BCM3384
static const struct of_device_id brcm_perst_match[] = {
	{ .compatible = "brcm,brcm3384-pci-perst" },
	{},
};
MODULE_DEVICE_TABLE(of, brcm_perst_match);

static struct platform_driver __refdata brcm_perst_driver = {
	.probe = brcm_perst_probe,
	.driver = {
		.name = "brcm-perst",
		.owner = THIS_MODULE,
		.of_match_table = brcm_perst_match,
	},
};
#endif


/***********************************************************************
 * PCI host bridge platform driver for BCM3384
 ***********************************************************************/
static const struct of_device_id brcm_pci_match[] = {
	{ .compatible = "brcm,bcm3384-pci-plat-dev" },
	{},
};
MODULE_DEVICE_TABLE(of, brcm_pci_match);

static struct platform_driver __refdata brcm_pci_driver = {
	.probe = brcm_pci_probe,
	.driver = {
		.name = "brcm-pci",
		.owner = THIS_MODULE,
		.of_match_table = brcm_pci_match,
	},
};


/***********************************************************************
 * pcibios init
 ***********************************************************************/
static int __init brcm_pcibios_init(void)
{
	int ret;

	if (IS_ENABLED(CONFIG_BCM3384)) {
		ret = platform_driver_probe(&brcm_perst_driver,
					    brcm_perst_probe);
		if (ret) {
			pr_err("DT is missing 'brcm,pci-perst' node\n");
			return ret;
		}
	}
	ret = platform_driver_probe(&brcm_pci_driver, brcm_pci_probe);
	pci_fixup_irqs(pci_common_swizzle, pcibios_map_irq);
#if defined(CONFIG_PM)
	if (!ret && brcm_num_pci_controllers > 0)
		register_syscore_ops(&pcie_pm_ops);
#endif
	return ret;
}

int pcibios_plat_dev_init(struct pci_dev *dev)
{
	return 0;
}

fs_initcall(brcm_pcibios_init);
