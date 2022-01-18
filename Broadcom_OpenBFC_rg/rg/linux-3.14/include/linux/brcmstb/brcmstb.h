/*
 * Copyright © 2009-2016 Broadcom
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * A copy of the GPL is available at
 * http://www.broadcom.com/licenses/GPLv2.php or from the Free Software
 * Foundation at https://www.gnu.org/licenses/ .
 */

/*
 * **********************
 * READ ME BEFORE EDITING
 * **********************
 *
 * If you update this file, make sure to bump BRCMSTB_H_VERSION if there is an
 * API change!
 */

#ifndef _ASM_BRCMSTB_BRCMSTB_H
#define _ASM_BRCMSTB_BRCMSTB_H

#define BRCMSTB_H_VERSION  7

#if !defined(__ASSEMBLY__)

#include <linux/types.h>
#include <linux/smp.h>
#include <linux/device.h>
#include <linux/brcmstb/memory_api.h>
#include <linux/brcmstb/irq_api.h>
#include <linux/brcmstb/gpio_api.h>
#include <linux/brcmstb/reg_api.h>

#if defined(CONFIG_MIPS)
/* #include <linux/brcmstb/brcmapi.h> */
#include <asm/addrspace.h>
#include <asm/mipsregs.h>
#include <asm/setup.h>
#include <irq.h>
#include <spaces.h>
#endif

#endif /* !defined(__ASSEMBLY__) */

#if defined(CONFIG_MIPS)

#include <asm/bmips.h>
#define BVIRTADDR(x)		KSEG1ADDR(BPHYSADDR(x))

#include <asm/io.h>
#ifndef ioremap_cache
#define ioremap_cache(offset, size)	\
	ioremap_cachable(offset, size)
#endif

#else

#define BRCMSTB_PERIPH_VIRT	0xfc000000
#define BRCMSTB_PERIPH_PHYS	0xf0000000
#define BRCMSTB_PERIPH_LENGTH	0x02000000

/*
 * NOTE: for cable combo chips like 7145, this could wind up looking like:
 *
 * x = BCHP_UARTA_REG_START = 0x2040_6c00 (offset)
 * BCHP_PHYSICAL_OFFSET = 0xd000_0000 (base for UBUS registers)
 * BPHYSADDR = BCHP_PHYSICAL_OFFSET + x = 0xf040_6c00
 * BVIRTADDR = BRCMSTB_PERIPH_VIRT + (x & 0x0fffffff) = 0xfa406c00
 */
#define BVIRTADDR(x)		(BRCMSTB_PERIPH_VIRT + ((x) & 0x0fffffff))

#define BRCMRG_PERIPH_VIRT	0xe0000000
#define BRCMRG_PERIPH_PHYS	0xd0000000
#define BRCMRG_PERIPH_LENGTH	0x10000000

#endif

/***********************************************************************
 * BCHP header lists
 *
 * NOTE: This section is autogenerated.  Do not edit by hand.
 ***********************************************************************/

#if defined(CONFIG_BCM3390A0)
#include <linux/brcmstb/3390a0/bchp_aon_ctrl.h>
#include <linux/brcmstb/3390a0/bchp_aon_pin_ctrl.h>
#include <linux/brcmstb/3390a0/bchp_aon_pm_l2.h>
#include <linux/brcmstb/3390a0/bchp_bspi.h>
#include <linux/brcmstb/3390a0/bchp_bspi_raf.h>
#include <linux/brcmstb/3390a0/bchp_clkgen.h>
#include <linux/brcmstb/3390a0/bchp_common.h>
#include <linux/brcmstb/3390a0/bchp_ebi.h>
#include <linux/brcmstb/3390a0/bchp_fpm_ctrl_fpm.h>
#include <linux/brcmstb/3390a0/bchp_fpm_pool_fpm.h>
#include <linux/brcmstb/3390a0/bchp_fpm_pool_0_fpm.h>
#include <linux/brcmstb/3390a0/bchp_fpm_pool_1_fpm.h>
#include <linux/brcmstb/3390a0/bchp_gio.h>
#include <linux/brcmstb/3390a0/bchp_gio_aon.h>
#include <linux/brcmstb/3390a0/bchp_gpio_per.h>
#include <linux/brcmstb/3390a0/bchp_hif_continuation.h>
#include <linux/brcmstb/3390a0/bchp_hif_cpubiuctrl.h>
#include <linux/brcmstb/3390a0/bchp_hif_intr2.h>
#include <linux/brcmstb/3390a0/bchp_hif_mspi.h>
#include <linux/brcmstb/3390a0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/3390a0/bchp_hif_top_ctrl.h>
#include <linux/brcmstb/3390a0/bchp_irq0.h>
#include <linux/brcmstb/3390a0/bchp_irq1.h>
#include <linux/brcmstb/3390a0/bchp_moca_hostmisc.h>
#include <linux/brcmstb/3390a0/bchp_nand.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_dma.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_ext_cfg.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_intr2.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_misc.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_misc_perst.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_rc_cfg_pcie.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_rc_cfg_type1.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_rc_cfg_vendor.h>
#include <linux/brcmstb/3390a0/bchp_pcie_0_rgr1.h>
#include <linux/brcmstb/3390a0/bchp_periph_misc_per.h>
#include <linux/brcmstb/3390a0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/3390a0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/3390a0/bchp_switch_acb.h>
#include <linux/brcmstb/3390a0/bchp_switch_core.h>
#include <linux/brcmstb/3390a0/bchp_switch_fcb.h>
#include <linux/brcmstb/3390a0/bchp_switch_indir_rw.h>
#include <linux/brcmstb/3390a0/bchp_switch_intrl2_0.h>
#include <linux/brcmstb/3390a0/bchp_switch_intrl2_1.h>
#include <linux/brcmstb/3390a0/bchp_switch_mdio.h>
#include <linux/brcmstb/3390a0/bchp_switch_reg.h>
#include <linux/brcmstb/3390a0/bchp_usb_ctrl.h>
#include <linux/brcmstb/3390a0/bchp_mbdma_uni3.h>
#include <linux/brcmstb/3390a0/bchp_unimac_core0_uni3.h>
#include <linux/brcmstb/3390a0/bchp_unimac_interface0_uni3.h>
#include <linux/brcmstb/3390a0/bchp_mib0_uni3.h>

#elif defined(CONFIG_BCM3390B0)
#include <linux/brcmstb/3390b0/bchp_aon_ctrl.h>
#include <linux/brcmstb/3390b0/bchp_aon_pin_ctrl.h>
#include <linux/brcmstb/3390b0/bchp_aon_pm_l2.h>
#include <linux/brcmstb/3390b0/bchp_bspi.h>
#include <linux/brcmstb/3390b0/bchp_bspi_raf.h>
#include <linux/brcmstb/3390b0/bchp_clkgen.h>
#include <linux/brcmstb/3390b0/bchp_common.h>
#include <linux/brcmstb/3390b0/bchp_ebi.h>
#include <linux/brcmstb/3390b0/bchp_fpm_ctrl_fpm.h>
#include <linux/brcmstb/3390b0/bchp_fpm_pool_fpm.h>
#include <linux/brcmstb/3390b0/bchp_fpm_pool_0_fpm.h>
#include <linux/brcmstb/3390b0/bchp_fpm_pool_1_fpm.h>
#include <linux/brcmstb/3390b0/bchp_gio.h>
#include <linux/brcmstb/3390b0/bchp_gio_aon.h>
#include <linux/brcmstb/3390b0/bchp_gpio_per.h>
#include <linux/brcmstb/3390b0/bchp_hif_continuation.h>
#include <linux/brcmstb/3390b0/bchp_hif_cpubiuctrl.h>
#include <linux/brcmstb/3390b0/bchp_hif_intr2.h>
#include <linux/brcmstb/3390b0/bchp_hif_mspi.h>
#include <linux/brcmstb/3390b0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/3390b0/bchp_hif_top_ctrl.h>
#include <linux/brcmstb/3390b0/bchp_irq0.h>
#include <linux/brcmstb/3390b0/bchp_irq1.h>
#include <linux/brcmstb/3390b0/bchp_moca_hostmisc.h>
#include <linux/brcmstb/3390b0/bchp_nand.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_dma.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_ext_cfg.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_intr2.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_misc.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_misc_perst.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_rc_cfg_pcie.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_rc_cfg_type1.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_rc_cfg_vendor.h>
#include <linux/brcmstb/3390b0/bchp_pcie_0_rgr1.h>
#include <linux/brcmstb/3390b0/bchp_periph_misc_per.h>
#include <linux/brcmstb/3390b0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/3390b0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/3390b0/bchp_switch_acb.h>
#include <linux/brcmstb/3390b0/bchp_switch_core.h>
#include <linux/brcmstb/3390b0/bchp_switch_fcb.h>
#include <linux/brcmstb/3390b0/bchp_switch_indir_rw.h>
#include <linux/brcmstb/3390b0/bchp_switch_intrl2_0.h>
#include <linux/brcmstb/3390b0/bchp_switch_intrl2_1.h>
#include <linux/brcmstb/3390b0/bchp_switch_mdio.h>
#include <linux/brcmstb/3390b0/bchp_switch_reg.h>
#include <linux/brcmstb/3390b0/bchp_usb_ctrl.h>
#include <linux/brcmstb/3390b0/bchp_mbdma_uni3.h>
#include <linux/brcmstb/3390b0/bchp_unimac_core0_uni3.h>
#include <linux/brcmstb/3390b0/bchp_unimac_interface0_uni3.h>
#include <linux/brcmstb/3390b0/bchp_mib0_uni3.h>
#include <linux/brcmstb/3390b0/bchp_mbox_cpuc.h>

#elif defined(CONFIG_BCM7250B0)
#include <linux/brcmstb/7250b0/bchp_bspi.h>
#include <linux/brcmstb/7250b0/bchp_bspi_raf.h>
#include <linux/brcmstb/7250b0/bchp_common.h>
#include <linux/brcmstb/7250b0/bchp_ebi.h>
#include <linux/brcmstb/7250b0/bchp_hif_intr2.h>
#include <linux/brcmstb/7250b0/bchp_hif_mspi.h>
#include <linux/brcmstb/7250b0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/7250b0/bchp_irq0.h>
#include <linux/brcmstb/7250b0/bchp_nand.h>
#include <linux/brcmstb/7250b0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/7250b0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/7250b0/bchp_usb_ctrl.h>
#include <linux/brcmstb/7250b0/bchp_xpt_bus_if.h>
#include <linux/brcmstb/7250b0/bchp_xpt_fe.h>
#include <linux/brcmstb/7250b0/bchp_xpt_memdma_mcpb.h>
#include <linux/brcmstb/7250b0/bchp_xpt_memdma_mcpb_ch0.h>
#include <linux/brcmstb/7250b0/bchp_xpt_pmu.h>
#include <linux/brcmstb/7250b0/bchp_xpt_security_ns.h>
#include <linux/brcmstb/7250b0/bchp_xpt_security_ns_intr2_0.h>

#elif defined(CONFIG_BCM7364A0)
#include <linux/brcmstb/7364a0/bchp_bspi.h>
#include <linux/brcmstb/7364a0/bchp_bspi_raf.h>
#include <linux/brcmstb/7364a0/bchp_common.h>
#include <linux/brcmstb/7364a0/bchp_ebi.h>
#include <linux/brcmstb/7364a0/bchp_hif_intr2.h>
#include <linux/brcmstb/7364a0/bchp_hif_mspi.h>
#include <linux/brcmstb/7364a0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/7364a0/bchp_irq0.h>
#include <linux/brcmstb/7364a0/bchp_nand.h>
#include <linux/brcmstb/7364a0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/7364a0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/7364a0/bchp_usb_ctrl.h>
#include <linux/brcmstb/7364a0/bchp_xpt_bus_if.h>
#include <linux/brcmstb/7364a0/bchp_xpt_fe.h>
#include <linux/brcmstb/7364a0/bchp_xpt_memdma_mcpb.h>
#include <linux/brcmstb/7364a0/bchp_xpt_memdma_mcpb_ch0.h>
#include <linux/brcmstb/7364a0/bchp_xpt_pmu.h>
#include <linux/brcmstb/7364a0/bchp_xpt_security_ns.h>
#include <linux/brcmstb/7364a0/bchp_xpt_security_ns_intr2_0.h>

#elif defined(CONFIG_BCM7366C0)
#include <linux/brcmstb/7366c0/bchp_bspi.h>
#include <linux/brcmstb/7366c0/bchp_bspi_raf.h>
#include <linux/brcmstb/7366c0/bchp_common.h>
#include <linux/brcmstb/7366c0/bchp_ebi.h>
#include <linux/brcmstb/7366c0/bchp_hif_intr2.h>
#include <linux/brcmstb/7366c0/bchp_hif_mspi.h>
#include <linux/brcmstb/7366c0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/7366c0/bchp_irq0.h>
#include <linux/brcmstb/7366c0/bchp_nand.h>
#include <linux/brcmstb/7366c0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/7366c0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/7366c0/bchp_usb_ctrl.h>
#include <linux/brcmstb/7366c0/bchp_xpt_bus_if.h>
#include <linux/brcmstb/7366c0/bchp_xpt_fe.h>
#include <linux/brcmstb/7366c0/bchp_xpt_memdma_mcpb.h>
#include <linux/brcmstb/7366c0/bchp_xpt_memdma_mcpb_ch0.h>
#include <linux/brcmstb/7366c0/bchp_xpt_pmu.h>
#include <linux/brcmstb/7366c0/bchp_xpt_security_ns.h>
#include <linux/brcmstb/7366c0/bchp_xpt_security_ns_intr2_0.h>

#elif defined(CONFIG_BCM74371A0)
#include <linux/brcmstb/74371a0/bchp_bspi.h>
#include <linux/brcmstb/74371a0/bchp_bspi_raf.h>
#include <linux/brcmstb/74371a0/bchp_common.h>
#include <linux/brcmstb/74371a0/bchp_ebi.h>
#include <linux/brcmstb/74371a0/bchp_hif_intr2.h>
#include <linux/brcmstb/74371a0/bchp_hif_mspi.h>
#include <linux/brcmstb/74371a0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/74371a0/bchp_irq0.h>
#include <linux/brcmstb/74371a0/bchp_nand.h>
#include <linux/brcmstb/74371a0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/74371a0/bchp_usb_ctrl.h>
#include <linux/brcmstb/74371a0/bchp_usb_xhci_ec.h>

#elif defined(CONFIG_BCM7439B0)
#include <linux/brcmstb/7439b0/bchp_bspi.h>
#include <linux/brcmstb/7439b0/bchp_bspi_raf.h>
#include <linux/brcmstb/7439b0/bchp_common.h>
#include <linux/brcmstb/7439b0/bchp_ebi.h>
#include <linux/brcmstb/7439b0/bchp_hif_intr2.h>
#include <linux/brcmstb/7439b0/bchp_hif_mspi.h>
#include <linux/brcmstb/7439b0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/7439b0/bchp_irq0.h>
#include <linux/brcmstb/7439b0/bchp_nand.h>
#include <linux/brcmstb/7439b0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/7439b0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/7439b0/bchp_usb_ctrl.h>
#include <linux/brcmstb/7439b0/bchp_xpt_bus_if.h>
#include <linux/brcmstb/7439b0/bchp_xpt_fe.h>
#include <linux/brcmstb/7439b0/bchp_xpt_memdma_mcpb.h>
#include <linux/brcmstb/7439b0/bchp_xpt_memdma_mcpb_ch0.h>
#include <linux/brcmstb/7439b0/bchp_xpt_pmu.h>
#include <linux/brcmstb/7439b0/bchp_xpt_security_ns.h>
#include <linux/brcmstb/7439b0/bchp_xpt_security_ns_intr2_0.h>

#elif defined(CONFIG_BCM7445D0)
#include <linux/brcmstb/7445d0/bchp_bspi.h>
#include <linux/brcmstb/7445d0/bchp_bspi_raf.h>
#include <linux/brcmstb/7445d0/bchp_common.h>
#include <linux/brcmstb/7445d0/bchp_ebi.h>
#include <linux/brcmstb/7445d0/bchp_hif_intr2.h>
#include <linux/brcmstb/7445d0/bchp_hif_mspi.h>
#include <linux/brcmstb/7445d0/bchp_hif_spi_intr2.h>
#include <linux/brcmstb/7445d0/bchp_irq0.h>
#include <linux/brcmstb/7445d0/bchp_nand.h>
#include <linux/brcmstb/7445d0/bchp_sdio_0_cfg.h>
#include <linux/brcmstb/7445d0/bchp_sun_top_ctrl.h>
#include <linux/brcmstb/7445d0/bchp_usb_ctrl.h>
#include <linux/brcmstb/7445d0/bchp_xpt_bus_if.h>
#include <linux/brcmstb/7445d0/bchp_xpt_fe.h>
#include <linux/brcmstb/7445d0/bchp_xpt_memdma_mcpb.h>
#include <linux/brcmstb/7445d0/bchp_xpt_memdma_mcpb_ch0.h>
#include <linux/brcmstb/7445d0/bchp_xpt_pmu.h>
#include <linux/brcmstb/7445d0/bchp_xpt_security_ns.h>
#include <linux/brcmstb/7445d0/bchp_xpt_security_ns_intr2_0.h>

#elif defined(CONFIG_BCM3384)
#include <linux/brcmstb/3384a0/bchp_common.h>
#include <linux/brcmstb/3384a0/bchp_g2u_regs_gb.h>
#include <linux/brcmstb/3384a0/bchp_mbox_gb.h>
#include <linux/brcmstb/3384a0/bchp_hw_counter_gb.h>
#include <linux/brcmstb/3384a0/bchp_watchdog_gb.h>
#include <linux/brcmstb/3384a0/bchp_btm_gb.h>
#include <linux/brcmstb/3384a0/bchp_dqm_gb.h>
#include <linux/brcmstb/3384a0/bchp_gb_queue_0_cntrl_gb.h>
#include <linux/brcmstb/3384a0/bchp_gb_queue_0_data_gb.h>
#include <linux/brcmstb/3384a0/bchp_dqmqsts_gb.h>
#include <linux/brcmstb/3384a0/bchp_dqmqmib_gb.h>
#include <linux/brcmstb/3384a0/bchp_sharedmem_gb.h>
#include <linux/brcmstb/3384a0/bchp_int_per.h>
#include <linux/brcmstb/3384a0/bchp_timer_per.h>
#include <linux/brcmstb/3384a0/bchp_gpio_per.h>
#include <linux/brcmstb/3384a0/bchp_int_ext_per.h>
#include <linux/brcmstb/3384a0/bchp_uart0_per.h>
#include <linux/brcmstb/3384a0/bchp_uart1_per.h>
#include <linux/brcmstb/3384a0/bchp_uart2_per.h>
#include <linux/brcmstb/3384a0/bchp_i2c_per.h>
#include <linux/brcmstb/3384a0/bchp_led_per.h>
#include <linux/brcmstb/3384a0/bchp_memc_atw_ubus_0.h>
#include <linux/brcmstb/3384a0/bchp_memc_atw_ubus_ub_0.h>
#include <linux/brcmstb/3384a0/bchp_ub_g2u_regs_ub.h>
#include <linux/brcmstb/3384a0/bchp_dpe_hw_gfap.h>
#include <linux/brcmstb/3384a0/bchp_fpm_ctrl_fpm.h>
#include <linux/brcmstb/3384a0/bchp_fpm_multi_fpm.h>
#include <linux/brcmstb/3384a0/bchp_fpm_pool_fpm.h>
#include <linux/brcmstb/3384a0/bchp_fpm_search_fpm.h>
#include <linux/brcmstb/3384a0/bchp_dbg_per.h>
#include <linux/brcmstb/3384a0/bchp_mbdma.h>
#include <linux/brcmstb/3384a0/bchp_unimac_core0.h>
#include <linux/brcmstb/3384a0/bchp_unimac_core1.h>
#include <linux/brcmstb/3384a0/bchp_unimac_interface0.h>
#include <linux/brcmstb/3384a0/bchp_unimac_interface1.h>
#include <linux/brcmstb/3384a0/bchp_mib0.h>
#include <linux/brcmstb/3384a0/bchp_mib1.h>
#endif

/***********************************************************************
 * Register access macros - sample usage:
 *
 * DEV_RD(0xb0404000)                       -> reads 0xb0404000
 * BDEV_RD(0x404000)                        -> reads 0xb0404000
 * BDEV_RD(BCHP_SUN_TOP_CTRL_PROD_REVISION) -> reads 0xb0404000
 *
 * _RB means read back after writing.
 ***********************************************************************/

#define BPHYSADDR(x)	((x) + BCHP_PHYSICAL_OFFSET)

#if !defined(__ASSEMBLY__)

#define DEV_RD(x) (*((volatile unsigned long *)(x)))
#define DEV_WR(x, y) do { *((volatile unsigned long *)(x)) = (y); } while (0)
#define DEV_UNSET(x, y) do { DEV_WR((x), DEV_RD(x) & ~(y)); } while (0)
#define DEV_SET(x, y) do { DEV_WR((x), DEV_RD(x) | (y)); } while (0)

#define DEV_WR_RB(x, y) do { DEV_WR((x), (y)); DEV_RD(x); } while (0)
#define DEV_SET_RB(x, y) do { DEV_SET((x), (y)); DEV_RD(x); } while (0)
#define DEV_UNSET_RB(x, y) do { DEV_UNSET((x), (y)); DEV_RD(x); } while (0)

#define BDEV_RD(x) (DEV_RD(BVIRTADDR(x)))
#define BDEV_WR(x, y) do { DEV_WR(BVIRTADDR(x), (y)); } while (0)
#define BDEV_UNSET(x, y) do { BDEV_WR((x), BDEV_RD(x) & ~(y)); } while (0)
#define BDEV_SET(x, y) do { BDEV_WR((x), BDEV_RD(x) | (y)); } while (0)

#define BDEV_SET_RB(x, y) do { BDEV_SET((x), (y)); BDEV_RD(x); } while (0)
#define BDEV_UNSET_RB(x, y) do { BDEV_UNSET((x), (y)); BDEV_RD(x); } while (0)
#define BDEV_WR_RB(x, y) do { BDEV_WR((x), (y)); BDEV_RD(x); } while (0)

#define BDEV_RD_F(reg, field) \
	((BDEV_RD(BCHP_##reg) & BCHP_##reg##_##field##_MASK) >> \
	 BCHP_##reg##_##field##_SHIFT)
#define BDEV_WR_F(reg, field, val) do { \
	BDEV_WR(BCHP_##reg, \
	(BDEV_RD(BCHP_##reg) & ~BCHP_##reg##_##field##_MASK) | \
	(((val) << BCHP_##reg##_##field##_SHIFT) & \
	 BCHP_##reg##_##field##_MASK)); \
	} while (0)
#define BDEV_WR_F_RB(reg, field, val) do { \
	BDEV_WR(BCHP_##reg, \
	(BDEV_RD(BCHP_##reg) & ~BCHP_##reg##_##field##_MASK) | \
	(((val) << BCHP_##reg##_##field##_SHIFT) & \
	 BCHP_##reg##_##field##_MASK)); \
	BDEV_RD(BCHP_##reg); \
	} while (0)

/***********************************************************************
 * HIF L2 IRQ controller - shared by EDU, SDIO
 ***********************************************************************/

#define HIF_ENABLE_IRQ(bit) do { \
	BDEV_WR_RB(BCHP_HIF_INTR2_CPU_MASK_CLEAR, \
		   BCHP_HIF_INTR2_CPU_MASK_CLEAR_##bit##_INTR_MASK); \
	} while (0)

#define HIF_DISABLE_IRQ(bit) do { \
	BDEV_WR_RB(BCHP_HIF_INTR2_CPU_MASK_SET, \
		   BCHP_HIF_INTR2_CPU_MASK_SET_##bit##_INTR_MASK); \
	} while (0)

#define HIF_TEST_IRQ(bit) \
	(((BDEV_RD(BCHP_HIF_INTR2_CPU_STATUS) & \
	   ~BDEV_RD(BCHP_HIF_INTR2_CPU_MASK_STATUS)) & \
	  BCHP_HIF_INTR2_CPU_STATUS_##bit##_INTR_MASK))

#define HIF_ACK_IRQ(bit) do { \
	BDEV_WR_RB(BCHP_HIF_INTR2_CPU_CLEAR, \
		   BCHP_HIF_INTR2_CPU_CLEAR_##bit##_INTR_MASK); \
	} while (0)

#define HIF_TRIGGER_IRQ(bit) do { \
	BDEV_WR_RB(BCHP_HIF_INTR2_CPU_SET, \
		   BCHP_HIF_INTR2_CPU_SET_##bit##_INTR_MASK); \
	} while (0)

/***********************************************************************
 * Internal (BSP/driver) APIs and definitions
 ***********************************************************************/

#ifdef BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID

/* 40nm chips */

#define BRCM_CHIP_ID()		({ \
	u32 reg = BDEV_RD(BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID); \
	(reg >> 28 ? reg >> 16 : reg >> 8); \
	})
#define BRCM_PROD_ID()		({ \
	u32 reg = BDEV_RD(BCHP_SUN_TOP_CTRL_PRODUCT_ID); \
	(reg >> 28 ? reg >> 16 : reg >> 8); \
	})
#define BRCM_CHIP_REV()		\
	((u32)BDEV_RD(BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID) & 0xff)

#else

/* hardcode for now, later we need to pull these directly from the 3384 */
#if defined(CONFIG_BCM3384)
#define BRCM_CHIP_ID()	0x3384
#define BRCM_PROD_ID()	0x0
#define BRCM_CHIP_REV()	0xa0
#else

/* 130nm, 65nm chips */

#define BRCM_CHIP_ID()		({ \
	u32 reg = BDEV_RD(BCHP_SUN_TOP_CTRL_PROD_REVISION); \
	(reg >> 28 ? reg >> 16 : reg >> 8); \
	})
#define BRCM_PROD_ID()		BRCM_CHIP_ID()
#define BRCM_CHIP_REV()		\
	((u32)BDEV_RD(BCHP_SUN_TOP_CTRL_PROD_REVISION) & 0xff)
#endif

#endif

/*
 * Exclude a given memory range from the MAC authentication process during S3
 * suspend/resume. Ranges are reset after each MAC (i.e., after each S3
 * suspend/resume cycle). Returns non-zero on error.
 */
int brcmstb_pm_mem_exclude(phys_addr_t addr, size_t len);
/* So users can determine whether the kernel provides this API */
#define BRCMSTB_HAS_PM_MEM_EXCLUDE

#endif /* !defined(__ASSEMBLY__) */

#endif /* _ASM_BRCMSTB_BRCMSTB_H */
