/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2008 Maxime Bizon <mbizon@freebox.fr>
 * Copyright (C) 2014 Kevin Cernekee <cernekee@gmail.com>
 */

#include <linux/init.h>
#include <linux/bitops.h>
#include <linux/bootmem.h>
#include <linux/clk-provider.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_platform.h>
#include <linux/smp.h>
#include <linux/brcmstb/brcmstb.h>
#include <asm/addrspace.h>
#include <asm/bmips.h>
#include <asm/bootinfo.h>
#include <asm/cpu-type.h>
#include <asm/mipsregs.h>
#include <asm/prom.h>
#include <asm/smp-ops.h>
#include <asm/time.h>
#include <asm/traps.h>
#include <asm/reboot.h>

#define CMT_LOCAL_TPID		BIT(31)
#define RELO_NORMAL_VEC		BIT(18)
#define USB_HOST_CLIENT_ID	26

static const unsigned long kbase = VMLINUX_LOAD_ADDRESS & 0xfff00000;
/* static const unsigned long kbase =
   ALIGN((unsigned long)VMLINUX_LOAD_ADDRESS, 1 << 20); */

extern void send_halted_msg_to_ecos(void);
extern void send_restart_msg_to_ecos(void);
extern void send_poweroff_msg_to_ecos(void);

static void bcm3384_machine_halt(void)
{
	extern void wait_forever(void);

	int cpu = get_cpu();

	if (cpu == 0)
	{
		send_halted_msg_to_ecos();
		smp_call_function_single(1, (void(*)(void *))bcm3384_machine_halt, 0, 0);
	}
	wait_forever();
}

static void bcm3394_machine_restart(char *command)
{
	send_restart_msg_to_ecos();
	bcm3384_machine_halt();
}

static void bcm3384_machine_poweroff(void)
{
	send_poweroff_msg_to_ecos();
	bcm3384_machine_halt();
}

static int __init bcm3384_restart_setup(void)
{
    _machine_restart = bcm3394_machine_restart;
    _machine_halt = bcm3384_machine_halt;
    pm_power_off = bcm3384_machine_poweroff;

    return 0;
}
arch_initcall(bcm3384_restart_setup);

static void kbase_setup(void)
{
	__raw_writel(kbase | RELO_NORMAL_VEC,
		     BMIPS_GET_CBR() + BMIPS_RELO_VECTOR_CONTROL_1);
	ebase = kbase;
}

void __init prom_init(void)
{
	register_bmips_smp_ops();

	/*
	 * Some experimental CM boxes are set up to let CM own the Viper TP0
	 * and let Linux own TP1.  This requires moving the kernel
	 * load address to a non-conflicting region (e.g. via
	 * CONFIG_PHYSICAL_START) and supplying an alternate DTB.
	 * If we detect this condition, we need to move the MIPS exception
	 * vectors up to an area that we own.
	 *
	 * This is distinct from the OTHER special case mentioned in
	 * smp-bmips.c (boot on TP1, but enable SMP, then TP0 becomes our
	 * logical CPU#1).  For the Viper TP1 case, SMP is off limits.
	 *
	 * Also note that many BMIPS435x CPUs do not have a
	 * BMIPS_RELO_VECTOR_CONTROL_1 register, so it isn't safe to just
	 * write VMLINUX_LOAD_ADDRESS into that register on every SoC.
	 */
	if (current_cpu_type() == CPU_BMIPS4350 &&
	    kbase != CKSEG0 &&
	    read_c0_brcm_cmt_local() & CMT_LOCAL_TPID) {
		board_ebase_setup = &kbase_setup;
		bmips_smp_enabled = 0;
	}
}

void __init prom_free_prom_memory(void)
{
}

const char *get_system_type(void)
{
	return "BCM3384";
}

void __init plat_time_init(void)
{
	struct device_node *np;
	u32 freq;

	np = of_find_node_by_name(NULL, "cpus");
	if (!np)
		panic("missing 'cpus' DT node");
	if (of_property_read_u32(np, "mips-hpt-frequency", &freq) < 0)
		panic("missing 'mips-hpt-frequency' property");
	of_node_put(np);

	mips_hpt_frequency = freq;
}

void __init plat_mem_setup(void)
{
	extern void __init zephyr_mem_setup(void);
	extern char __dtb_start[];
	void *dtb = __dtb_start;

	set_io_port_base(0);
	ioport_resource.start = 0;
	ioport_resource.end = ~0;

	/* intended to somewhat resemble ARM; see Documentation/arm/Booting */
	if (fw_arg0 == 0 && fw_arg1 == 0xffffffff)
		dtb = phys_to_virt(fw_arg2);

	__dt_setup_arch(dtb);

	strlcpy(arcs_cmdline, boot_command_line, COMMAND_LINE_SIZE);

	zephyr_mem_setup();
}

void __init __attribute__((weak)) zephyr_mem_setup(void)
{
}

void __init device_tree_init(void)
{
	struct device_node *np;

	unflatten_and_copy_device_tree();

	/* Disable SMP boot unless both CPUs are listed in DT and !disabled */
	np = of_find_node_by_name(NULL, "cpus");
	if (np && of_get_available_child_count(np) <= 1)
		bmips_smp_enabled = 0;
	of_node_put(np);
}

int __init plat_of_setup(void)
{
	return __dt_register_buses("brcm,bcm3384", "simple-bus");
}

arch_initcall(plat_of_setup);

static void usb_setup(void)
{
	void __iomem *USB_SETUP_REG = (void *)CKSEG1ADDR(0x15400200);
	void __iomem *USB_PLLCTL1_REG = (void *)CKSEG1ADDR(0x15400204);
	void __iomem *USB_SWAP_REG = (void *)CKSEG1ADDR(0x1540020c);
	u32 tmp;

	tmp = __raw_readl(USB_SETUP_REG);
	tmp |= BIT(6);		/* soft reset */
	__raw_writel(tmp, USB_SETUP_REG);
	tmp &= ~BIT(6);		/* soft reset */
	__raw_writel(tmp, USB_SETUP_REG);
	tmp = __raw_readl(USB_SETUP_REG);
	tmp |= BIT(4);		/* IOC=1 => active low */
	__raw_writel(tmp, USB_SETUP_REG);

	__raw_writel(9, USB_SWAP_REG);
	__raw_writel(0x512750c0, USB_PLLCTL1_REG);
}

static int __init plat_dev_init(void)
{
	of_clk_init(NULL);
	usb_setup();
	return 0;
}

device_initcall(plat_dev_init);

static int __init cm_handshake(void)
{
	void __iomem *MBOX_DATA15 = (void *)CKSEG1ADDR(0x104b00bc);
	__raw_writel(0xC0FFEE, MBOX_DATA15);
	printk("CM handshake\n");
	return 0;
}

late_initcall(cm_handshake);
