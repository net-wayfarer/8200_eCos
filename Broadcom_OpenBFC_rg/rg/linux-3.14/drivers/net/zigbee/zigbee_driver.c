/*
 * Copyright 2016 Broadcom
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License version 2.1 for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License v2.1 (LGPLv2.1) along with this source code.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
//#define DEBUG
#define COMPILE_TIME

/* This version demonstrates how to create a kernel module WITH a /proc interface */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/timer.h>

#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/seq_file.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/cdev.h>
#include <linux/of.h>
#include <linux/completion.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_net.h>

#include "zigbee_driver.h" /* my ioctl definitions */
#include "zigbee_driver_reg.h"

static int open_count;
static int debug;

typedef void (*FUNCPTR)(int nArg1,...);

#define DRV_NAME	"brcm-zigbee"

struct zigbee_priv_data {
    struct cdev chr_dev;
    struct platform_device *pdev;
    struct device *dev;
    void __iomem *base;
    unsigned int wakeup_irq;
    unsigned int mbox_irq;

    unsigned int wake_timeout;
    struct notifier_block reboot_notifier;
    const struct zigbee_regs *regs;
};

#define NUM_MINORS		1
static struct zigbee_priv_data *minor_tbl[NUM_MINORS];

struct zigbee_platform_data {
    unsigned int rf4ce_macaddr_hi;
    unsigned int rf4ce_macaddr_lo;
    unsigned int zbpro_macaddr_hi;
    unsigned int zbpro_macaddr_lo;
    u32 chip_id;
};

#define PROCFS_NAME "zigbee_driver"
struct proc_dir_entry *Our_Proc_File;

//void *base_addr;
unsigned long base_addr;

#define FIFO_SIZE 64
#define MBOX_MSG_SIZE_BYTES 128
typedef struct mbox_fifo_t {
    int depth;
    int wptr;
    int rptr;
    struct {
        unsigned int data[MBOX_MSG_SIZE_BYTES/4];
    } fifo[FIFO_SIZE];
} mbox_fifo_t;

static mbox_fifo_t mbox_fifo_tx;
static mbox_fifo_t mbox_fifo_rx;
static struct completion completeWDT;
static struct completion completeTx;
static struct completion completeRx;

static void init_fifo(mbox_fifo_t *f)
{
    f->depth=0;
    f->wptr=0;
    f->rptr=0;
}

static int procfile_read(struct seq_file *m, void *v)
{
    int ret;

    printk(KERN_INFO "DRIVER:  procfile_read (/proc/%s) called\n", PROCFS_NAME);
    ret = seq_printf(m, "Hello World!\n");

    return 0;
}

static int procfile_open(struct inode *inode, struct file *file)
{
    return single_open(file, procfile_read, NULL);
}

static const struct file_operations fops = {
    .open    = procfile_open,
    .read    = seq_read,
    .llseek  = seq_lseek,
    .release = single_release,
};

static int init_proc(void)
{
    Our_Proc_File = proc_create(PROCFS_NAME, S_IFREG | S_IRUGO, NULL, &fops);

    if (Our_Proc_File == NULL) {
        printk(KERN_ALERT "DRIVER:  Error: Could not initialize /proc/%s\n", PROCFS_NAME);
        return -ENOMEM;
    }

    pr_notice("/proc/%s created\n", PROCFS_NAME);
    return 0; /*success*/
}

static void remove_proc(void)
{
    if (Our_Proc_File != NULL) {
        remove_proc_entry(PROCFS_NAME, NULL);
        Our_Proc_File = NULL;
        printk(KERN_INFO "DRIVER:  /proc/%s removed\n", PROCFS_NAME);
    }
}

/*\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\*/

static int open_device(struct inode *inode, struct file *file)
{
    /*int minor = MINOR(inode->i_rdev);*/ /* do I need this for anything??? */

    file->private_data = minor_tbl[0];
    open_count++;

#ifdef COMPILE_TIME
    BDEV_WR(BCHP_RF4CE_CPU_DATA_MEM_REG_END, 0);
#else
    BDEV_WR(base_addr + 0x47ffc, 0);
#endif
    return 0;
}

static int close_device(struct inode *inode, struct file *file)
{
    open_count--;
    return 0;
}

static signed int my_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
    signed int result = 0; /* assume success */
    struct zigbee_priv_data *priv = filp->private_data;
    int rc;

    switch (cmd)
    {
        case ZIGBEE_IOCTL_WAIT_FOR_WDT_INTERRUPT:
        {

            /* Be sure to go half asleep before checking condition. */
            /* Otherwise we have a race condition between when we   */
            /* check the condition and when we call schedule().     */
            result = wait_for_completion_interruptible(&completeWDT);
            if (!result) {
                printk(KERN_ALERT "DRIVER:  Got WDT interrupt\n");
            }

            break;
        }

        case ZIGBEE_IOCTL_START:
            {
                int i;
                struct fw f;

                rc = copy_from_user(&f, (struct f *)arg, sizeof(f));
                if (rc) {
                    printk(KERN_ALERT "copy_from_user returned %d\n", rc);
                    return (rc);
                }

#ifdef SETUP_UART
                // Set up either ARC UART for 7366B0 (both TX confirmed):
                //SUN_TOP_CTRL.TEST_PORT_CTRL.encoded_tp_enable.Write(16)   # 16 = SYS
                BDEV_WR_F(SUN_TOP_CTRL_TEST_PORT_CTRL, encoded_tp_enable, BCHP_SUN_TOP_CTRL_TEST_PORT_CTRL_encoded_tp_enable_SYS);  /* Set HOSTIF_SEL to 1 (currently reversed) */

                //SUN_TOP_CTRL.UART_ROUTER_SEL_1.port_10_cpu_sel.Write(10)  #RF4CE_TOP (alt_tp_in[13], alt_tp_out[14]) “UART1”
                BDEV_WR_F(SUN_TOP_CTRL_UART_ROUTER_SEL_1, port_10_cpu_sel, BCHP_SUN_TOP_CTRL_UART_ROUTER_SEL_1_port_10_cpu_sel_RF4CE_TOP);

                //SUN_TOP_CTRL.PIN_MUX_CTRL_13.gpio_072.Write(5)      # 5 = ALT_TP_IN_13 (UART1)
                BDEV_WR_F(SUN_TOP_CTRL_PIN_MUX_CTRL_13, gpio_072, BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_13_gpio_072_ALT_TP_IN_13);

                //SUN_TOP_CTRL.PIN_MUX_CTRL_13.gpio_073.Write(4)      # 4 = ALT_TP_OUT_14 (UART1)
                BDEV_WR_F(SUN_TOP_CTRL_PIN_MUX_CTRL_13, gpio_073, BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_13_gpio_073_ALT_TP_OUT_14);

                printk(KERN_ALERT "UART set up for ZigBee core.");
#endif

                BDEV_SET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK);

                pr_notice("Delay after setting CPU_RST bit...\n");  /* TBD - need some delay here */

                BDEV_UNSET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK);
                BDEV_UNSET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK);

                i=0;
                while ((i<(256*1024)) && (i<f.size_in_bytes)) {
                    BDEV_WR(BCHP_RF4CE_CPU_PROG0_MEM_WORDi_ARRAY_BASE+i, *((unsigned int *)(f.pImage+i)));
                    i+=4;
                }

#ifdef VERIFY_ZIGBEE_FW
                i=0;
                printk(KERN_ALERT "Starting verify...\n");
                while ((i<(256*1024)) && (i<f.size_in_bytes)) {
                    unsigned int data;
                    data = BDEV_RD(BCHP_RF4CE_CPU_PROG0_MEM_WORDi_ARRAY_BASE+i);
                    if (data != *((unsigned int *)(f.pImage+i))) {
                        printk(KERN_ALERT "miscompare at address=0x%08x:  expected=0x%08x, received=0x%08x\n", i, *((unsigned int *)(f.pImage+i)), data);
                    }
                    i+=4;
                }
                printk(KERN_ALERT "Verify complete...\n");
#endif

                BDEV_SET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_HOSTIF_SEL_MASK);
                BDEV_SET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK);
                BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK);
                BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK);
            }
            break;

        case ZIGBEE_IOCTL_STOP:
            BDEV_UNSET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK);
            BDEV_SET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK);
            pr_notice("Delay after setting CPU_RST bit...\n");  /* TBD - need some delay here */
            BDEV_UNSET(BCHP_RF4CE_CPU_CTRL_CTRL, BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK);
            break;

        /*
         * Writing to the mailbox involves the following:
         * 1) Having the caller place up to 128 bytes in a buffer for the driver to fill the mailbox up with.
         * 2) Getting interrupted to then try to grab the mailbox semaphore
         *    a) if successful, copy the 128 bytes into the mailbox and then set a bit.
         *    b) if not successful, go back to sleep and try again.
         */
        case ZIGBEE_IOCTL_WRITE_TO_MBOX:
        {
//            spinlock_t gSpinLock = __SPIN_LOCK_UNLOCKED(gSpinLock);
//            unsigned long flags;

            /* find a slot to fill the data with */

//            spin_lock_irqsave(&gSpinLock, flags); /* Need to protect mbox_tx.rptr as it could be updated by the ISR */
            pr_debug("ZIGBEE_IOCTL_WRITE_TO_MBOX, depth=%d\n", mbox_fifo_tx.depth);

            if (mbox_fifo_tx.depth == FIFO_SIZE) {
                result = wait_for_completion_interruptible(&completeTx);
                if (result) {
                    break;
                }
                if (debug) printk(KERN_ALERT "DRIVER:  Got Read From Mailbox interrupt\n");
            }

//            spin_unlock_irqrestore(&gSpinLock, flags);

            /* slot found, copy the data */
            rc = copy_from_user((unsigned int *)&mbox_fifo_tx.fifo[mbox_fifo_tx.wptr], (unsigned int *)arg, 128);
            if (rc) {
                printk(KERN_ALERT "copy_from_user returned %d\n", rc);
                return (rc);
            }
            mbox_fifo_tx.wptr++;
            if (mbox_fifo_tx.wptr == FIFO_SIZE) mbox_fifo_tx.wptr=0;
            mbox_fifo_tx.depth++;

            /* enable interrupt for the mailbox semaphore */
            BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK);
            pr_debug("clear MBOX_SEM_INTR mask, depth=%d\n", mbox_fifo_tx.depth);

            break;
        }

        /*
         * Reading from the mailbox involves the following:
         * 1) If there is data in the rx queue of buffers, copy the data out and return.
         * 2) If there is not, put the caller to sleep.
         * 3) The interrupt handler will fill the buffer up with data, when a message arrives in the receive mailbox.
         * 4) The interrupt handler will then wake up the sleeping task.
         */
        case ZIGBEE_IOCTL_READ_FROM_MBOX:
        {
            if (mbox_fifo_rx.depth == 0) {
                /* nothing, go to sleep */
                result = wait_for_completion_interruptible(&completeRx);
                if (result) {
                    break;
                }
                if (debug) printk(KERN_ALERT "DRIVER:  Got Read From Mailbox interrupt\n");
            }

            /* Check depth, because if the mailbox adapter in host has terminated (due to CTRL-C), the process will wakeup from schedule_timeout.
               We have no way to know if this wakeup was due to CTRL-C or if it was due to an actual frame from the mailbox, except by checking the depth */
            if (mbox_fifo_rx.depth) {
                rc = copy_to_user((unsigned int *)arg, (unsigned int *)&mbox_fifo_rx.fifo[mbox_fifo_rx.rptr], MBOX_MSG_SIZE_BYTES);
                if (rc) {
                    printk(KERN_ALERT "copy_to_user returned %d\n", rc);
                    return (rc);
                }
                mbox_fifo_rx.rptr++;
                if (mbox_fifo_rx.rptr == FIFO_SIZE) mbox_fifo_rx.rptr=0;
                mbox_fifo_rx.depth--;
            } else {
                result = -1;
            }
            break;
        }

        case ZIGBEE_IOCTL_GET_RF4CE_MAC_ADDR:
        {
            unsigned int data[2];
            struct zigbee_platform_data *pd = priv->pdev->dev.platform_data;

            data[0]= pd->rf4ce_macaddr_hi;
            data[1]= pd->rf4ce_macaddr_lo;
            rc = copy_to_user((unsigned int *)arg, &data[0], sizeof(data));
            if (rc) printk(KERN_ALERT "copy_to_user returned %d\n", rc);
            break;
        }

        case ZIGBEE_IOCTL_GET_ZBPRO_MAC_ADDR:
        {
            unsigned int data[2];
            struct zigbee_platform_data *pd = priv->pdev->dev.platform_data;

            data[0]= pd->zbpro_macaddr_hi;
            data[1]= pd->zbpro_macaddr_lo;
            rc = copy_to_user((unsigned int *)arg, &data[0], sizeof(data));
            if (rc) printk(KERN_ALERT "copy_to_user returned %d\n", rc);
            break;
        }

        default:
            printk("DRIVER:  Received an unknown ioctl request! (cmd=%d)\n", cmd);
            result = -1; /*failure*/
            break;
    }

    return result;
}

#if HAVE_UNLOCKED_IOCTL
long my_ioctl_1(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = filp->f_path.dentry->d_inode;
    return my_ioctl(inode, filp, cmd, arg);
}
long my_ioctl_2 (struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct inode *inode = filp->f_path.dentry->d_inode;
    printk(KERN_ALERT "DRIVER:  Entered ioctl2 (compat_ioctl) module\n");
    return my_ioctl(inode, filp, cmd, arg);
}
#endif

/*\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\//\\*/

#define ZIGBEE_MAJOR 105 /* use this for our major device number */
static const char zigbee_name[] = "zigbee";

static struct file_operations module_ops = {
    .owner = THIS_MODULE,
    .open = open_device,
#if HAVE_UNLOCKED_IOCTL
    .unlocked_ioctl = my_ioctl_1,
    .compat_ioctl = my_ioctl_2,
#else
    .ioctl = my_ioctl,
#endif
    .release = close_device,
};

irqreturn_t brcmstb_rf4ce_cpu_host_stb_l2_irq(int irq, void *dev_id)
{
    int status = BDEV_RD(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0) & ~BDEV_RD(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_STATUS0);

    if (status & BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_SEM_INTR_MASK) { /* MBOX_SEM_INTR asserted */

        /* No need to clear the interrupt, as that will be handled
            automatically by the hw as a result of reading from
            BCHP_RF4CE_CPU_CTRL_MBOX_SEM */

        /* On second thought, this is not true.  We need to clear the interrupt ourselves */
        BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_MASK);

        if ((BDEV_RD(BCHP_RF4CE_CPU_CTRL_MBOX_SEM) & BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_MASK) >> BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_SHIFT) { /* we have the mailbox */
            pr_debug("MBOX_SEM_INTR asserted and we have the mailbox\n");

            /*
               At this point, we have the semaphore.  If for some reason, we
               take too long from this point, until we set MBOX_H2Z_FULL_INTR,
               then the HW will assert MBOX_SEM again.
               One way to look for this condition is to read MBOX_SEM.
               Normally, at this point, it should be zero.  If it is set to one,
               then it means that hw timed us out, but that we got the semaphore
               back.  A more reliable way to detect the condition is to poll the
               interrupt status for MBOX_SEM_INTR.  If asserted, after we
               cleared it, it means that the core as set the MBOX_SEM again.

               Policy:
               If we see that MBOX_SEM_INTR is asserted, while we are trying
               to fill the HW FIFO, then clear MBOX_SEM_INTR and then read the
               MBOX_SEM.
               1. If one, then we have the semaphore, log it, and then
                  continue where we left off with the HW FIFO.
               2. If zero, then we no longer have the semaphre, log it, reset
                  our fifo_tx pointers, and exit.
            */

            /* sanity check */
            if ((BDEV_RD(BCHP_RF4CE_CPU_L2_CPU_STATUS) & BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK) >> BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_SHIFT) /*   READL(BCHP_RF4CE_CPU_L2_CPU_STATUS) & BCHP_RF4CE_CPU_L2_UB_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK) */{
                printk(KERN_ALERT "DRIVER:  ERROR:  MBOX_H2Z_FULL set when MBOX_SEM_INTR asserted\n"); while (1);
            } else {
                /* sanity check */
/*
                if (READL(BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DEPTH) != 0) {
                    printk(KERN_ALERT "ERROR:  H2Z_MBOX_FIFO_DEPTH not zero when we begin\n");
                } else
*/
                {
                    int i;
                    unsigned int header;
                    unsigned int size_in_words;

                    if (BDEV_RD(BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DEPTH) != 0) {
                        printk(KERN_ALERT "DRIVER:  ERROR:  H2Z_MBOX_FIFO_DEPTH not zero when we begin\n");

                        /* Let's reset the fifo ptrs */
                        BDEV_SET(BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS, BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK);
                        BDEV_UNSET(BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS, BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK);
                    }

                    /* copy the data from the fifo */
                    if (debug) printk(KERN_ALERT "DRIVER:  copying the data\n");

                    header = mbox_fifo_tx.fifo[mbox_fifo_tx.rptr].data[0];
                    size_in_words = (header & 0x1f) + 1;

                    if (debug) printk(KERN_ALERT "DRIVER:  size_in_words=%d\n", size_in_words);

                    for (i=0; i<size_in_words; i++) {
                        BDEV_WR(BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DATA, mbox_fifo_tx.fifo[mbox_fifo_tx.rptr].data[i]);
                    }
                    mbox_fifo_tx.rptr++;
                    if (mbox_fifo_tx.rptr == FIFO_SIZE) mbox_fifo_tx.rptr=0;
                    mbox_fifo_tx.depth--;

                    /* enable the H2Z Full bit */
                    BDEV_SET(BCHP_RF4CE_CPU_L2_CPU_SET, BCHP_RF4CE_CPU_L2_CPU_SET_MBOX_H2Z_FULL_INTR_MASK);
                    pr_debug("set MBOX_H2Z_FULL_INTR, depth=%d\n", mbox_fifo_tx.depth);

                    /* If the mbox_tx fifo is empty, mask the interrupt */
                    if (mbox_fifo_tx.depth == 0) {
                        BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_MASK);
                        pr_debug("set setting MBOX_SEM_INTR mask\n");
                    }

                    complete(&completeTx);
                }
            }
        } else {
            /* We didn't get the semaphore.  Leave the interrupt unmasked, and get interrupted again later. */
            pr_debug("MBOX_SEM_INTR asserted and we don't have the mailbox\n");
        }
    }

    if (status & BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_Z2H_FULL_INTR_MASK) { /* We have received a message from the Zigbee */
        int i;
        unsigned int size_in_words, z2h_mbox_fifo_depth, header;
        pr_debug("MBOX_Z2H_FULL_INTR asserted\n");

        /* clear the interrupt */
        BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK);

        z2h_mbox_fifo_depth = BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH);

        if (debug) printk(KERN_ALERT "DRIVER:  z2h_mbox_fifo_depth=%d\n", z2h_mbox_fifo_depth);

        /* Copy the data into next available slot.  Remember that this slot
            is always free, otherwise, we would not have enabled the receive
            interrupt. */
        for (i=0; i<z2h_mbox_fifo_depth; i++) {
            mbox_fifo_rx.fifo[mbox_fifo_rx.wptr].data[i] = BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DATA);
        }

        /* sanity check */
        header = mbox_fifo_rx.fifo[mbox_fifo_rx.wptr].data[0];
        size_in_words = (header & 0x1f) + 1;

        if (size_in_words != z2h_mbox_fifo_depth) {
            printk(KERN_ALERT "DRIVER:  size_in_words=%d, z2h_mbox_fifo_depth=%d\n", size_in_words, z2h_mbox_fifo_depth);
        }

        /* another sanity check */
        if (BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH) != 0) {
            printk(KERN_ALERT "DRIVER:  fifo depth not zero after reading\n");
        }

        /* Update the write pointer in the rx fifo */
        mbox_fifo_rx.wptr++;
        if (mbox_fifo_rx.wptr == FIFO_SIZE) mbox_fifo_rx.wptr=0;
        mbox_fifo_rx.depth++;

        BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK);

        /* Wake up the potentially sleeping task */
        complete(&completeRx);
    }

    if (status & BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_WDOG_INTR_MASK) { /* Watchdog timer interrupt */
        printk(KERN_ALERT "DRIVER:  WDOG_INTR asserted\n");

        /* Clear interrupt */
        BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_WDOG_INTR_MASK);

        /* Wake up the potentially sleeping task */
        complete(&completeWDT);
    }

    if (status & BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_SW_INTR_MASK) { /* SW interrupt */
        printk(KERN_ALERT "DRIVER:  SW_INTR asserted\n");
    }

    return IRQ_HANDLED;
}

static void brcmstb_zigbee_interrupts_init(struct zigbee_priv_data *priv)
{
    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK);
    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK);
}

static void brcmstb_zigbee_interrupts_uninit(struct zigbee_priv_data *priv)
{
    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_WDOG_INTR_MASK);
    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_MASK);
}

static irqreturn_t brcmstb_zigbee_wakeup_irq(int irq, void *data)
{
    struct zigbee_priv_data *priv = data;
    printk(KERN_ALERT "brcmstb_zigbee_wakeup_irq\n");
    pm_wakeup_event(priv->dev, 0);
    return IRQ_HANDLED;
}

static int brcmstb_zigbee_prepare_suspend(struct zigbee_priv_data *priv)
{
    /*struct device *dev = zigbee->dev;*/
    int ret;

    disable_irq(priv->mbox_irq);
    ret = enable_irq_wake(priv->wakeup_irq);
    if (ret) {
        printk(KERN_ALERT "failed to enable wake-up interrupt\n");
        //dev_err(dev, "failed to enable wake-up interrupt\n");
        return ret;
    }

    return 0;
}

static inline void mac_to_u32(uint32_t *hi, uint32_t *lo, const uint8_t *mac)
{
    *hi = (mac[0] << 24) | (mac[1] << 16) | (mac[2] << 8) | (mac[3] << 0);
    *lo = (mac[4] << 24) | (mac[5] << 16);
}

static inline void eui64_to_u32(uint32_t *hi, uint32_t *lo, const uint8_t *mac)
{
    *hi = (mac[0] << 24) | (mac[1] << 16) | (mac[2] << 8) | (mac[3] << 0);
    *lo = (mac[4] << 24) | (mac[5] << 16) | (mac[6] << 8) | (mac[7] << 0);
}


/**
 * Search the device tree for the best MAC address to use.  'mac-address' is
 * checked first, because that is supposed to contain to "most recent" MAC
 * address. If that isn't set, then 'local-mac-address' is checked next,
 * because that is the default address.  If that isn't set, then the obsolete
 * 'address' is checked, just in case we're using an old device tree.
 *
 * Note that the 'address' property is supposed to contain a virtual address of
 * the register set, but some DTS files have redefined that property to be the
 * MAC address.
 *
 * All-zero MAC addresses are rejected, because those could be properties that
 * exist in the device tree, but were not set by U-Boot.  For example, the
 * DTS could define 'mac-address' and 'local-mac-address', with zero MAC
 * addresses.  Some older U-Boots only initialized 'local-mac-address'.  In
 * this case, the real MAC is in 'local-mac-address', and 'mac-address' exists
 * but is all zeros.
*/
const void *of_get_eui64_address(struct device_node *np)
{
	struct property *pp;

	pp = of_find_property(np, "local-extended-address", NULL);
	if (pp && (pp->length == 8))
		return pp->value;

	return NULL;
}

static int brcmstb_zigbee_parse_dt_node(struct zigbee_priv_data *priv)
{
    struct platform_device *pdev = priv->pdev;
    struct zigbee_platform_data pd;
    struct device_node *of_node = pdev->dev.of_node, *of_node2;
    int status = 0;
    const u8 *macaddr;

    of_node2 = of_find_node_by_name(of_node, "mac-rf4ce");
    if (of_node2) {
        macaddr = of_get_eui64_address(of_node2);
        if (!macaddr) {
            printk(KERN_ALERT "can't find 64 bit RF4CE MAC address\n");
            macaddr = of_get_mac_address(of_node2);
            if (!macaddr) {
                printk(KERN_ALERT "can't find 48 bit RF4CE MAC address\n");
            } else {
                mac_to_u32(&pd.rf4ce_macaddr_hi, &pd.rf4ce_macaddr_lo, macaddr);
            }
        } else {
            eui64_to_u32(&pd.rf4ce_macaddr_hi, &pd.rf4ce_macaddr_lo, macaddr);
        }
    }
    of_node2 = of_find_node_by_name(of_node, "mac-zbpro");
    if (of_node2) {
        macaddr = of_get_eui64_address(of_node2);
        if (!macaddr) {
            printk(KERN_ALERT "can't find 64 bit ZBPRO MAC address\n");
            macaddr = of_get_mac_address(of_node2);
            if (!macaddr) {
                printk(KERN_ALERT "can't find 48 bit ZBPRO MAC address\n");
            } else {
                mac_to_u32(&pd.zbpro_macaddr_hi, &pd.zbpro_macaddr_lo, macaddr);
            }
        } else {
            eui64_to_u32(&pd.zbpro_macaddr_hi, &pd.zbpro_macaddr_lo, macaddr);
        }
    }

    /* Try to read the chip-id property.  If not present, fall back to
     * reading it from the chip family ID register.
     */
    if (of_property_read_u32(of_node, "chip-id", &pd.chip_id)) {
        int val;
        printk(KERN_INFO "unable to obtain chip_id from device tree\n");
#ifdef COMPILE_TIME
        val = BDEV_RD(BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID);
#else
        val = BDEV_RD(0x00404000);
#endif

        if (val >> 28)
            /* 4-digit chip ID */
            pd.chip_id = (val >> 16) << 16;
        else
            /* 5-digit chip ID */
            pd.chip_id = (val >> 8) << 8;
        //pd.chip_id |= (BRCM_CHIP_REV() + 0xa0);
    }
    printk(KERN_INFO "chip_id=0x%08x\n", pd.chip_id);

    status = platform_device_add_data(pdev, &pd, sizeof(pd));
    return status;
}

int brcmstb_zigbee_probe(struct platform_device *pdev)
{
    int rc=0;
    struct device *dev = &pdev->dev;
    struct zigbee_priv_data *priv;
    struct resource *res;
    int ret;
    struct zigbee_platform_data *pd;
#ifdef DYNAMIC_MAJOR_NUMBER
    dev_t dev_no;
#endif
#ifndef COMPILE_TIME
    u32 val;
#endif

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);
    if (!priv)
        return -ENOMEM;
    platform_set_drvdata(pdev, priv);
    priv->pdev = pdev;
    priv->dev = dev;

    brcmstb_zigbee_parse_dt_node(priv);
    pd = pdev->dev.platform_data;

#ifndef COMPILE_TIME
    if ((pd->chip_id & 0xffff0000) == 0x33900000) {
        printk(KERN_INFO "using 3390 registers\n");
        base_addr = 0x20e00000;
    } else {
        printk(KERN_INFO "using 736x registers\n");
        base_addr = 0x01400000;
    }
#endif

    printk(KERN_INFO "BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH=%d\n", (unsigned int)BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH));

    while (BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH)) {
        printk(KERN_INFO "Emptying out MBOX fifo...\n");
        BDEV_RD(BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DATA);
    }

    res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
    priv->base = devm_request_and_ioremap(dev, res);
    if (!priv->base)
        return -ENODEV;

    priv->wakeup_irq = platform_get_irq(pdev, 1);
    if ((int)priv->wakeup_irq < 0)
        return -ENODEV;

    ret = devm_request_irq(dev, priv->wakeup_irq, brcmstb_zigbee_wakeup_irq, 0, DRV_NAME, priv);
    if (ret < 0)
        return -ENODEV;

    //printk(KERN_ALERT "DRIVER:  Hello, world!\n");
#ifdef DYNAMIC_MAJOR_NUMBER
    cdev_init(&priv->chr_dev,&module_ops);
    rc = alloc_chrdev_region(&dev_no,0,1,zigbee_name);
    if (rc != 0) {
        dev_err(dev,"Failed to alloc chr region\n");
        return -ENOMEM;
    }
    rc = cdev_add(&priv->chr_dev,dev_no,1);
#else
    register_chrdev(ZIGBEE_MAJOR, zigbee_name, &module_ops);
#endif

    init_proc();

    //base_addr = ioremap_nocache(0xf0000000, BCHP_REGISTER_END);

    //printk(KERN_INFO "base_addr=%p\n", base_addr);

#ifdef COMPILE_TIME
    printk(KERN_INFO "DRIVER:  BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID=0x%08lx\n", BDEV_RD(BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID));
#else
    printk(KERN_INFO "DRIVER:  BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID=0x%08lx\n", BDEV_RD(0x00404000));
#endif

    /* Make sure to init this first, as we may get an interrupt as soon as we request it */
    init_completion(&completeWDT);
    init_completion(&completeTx);
    init_completion(&completeRx);

    /* Initialize the mailbox fifos */
    init_fifo(&mbox_fifo_tx);
    init_fifo(&mbox_fifo_rx);
    debug = 0;

    /* For S3 */
#ifdef COMPILE_TIME
#if BCHP_CHIP==7364
    BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cml_in_s3, 0x4); /* Set bit 14 */
    BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cmos_in_s3, 0x10); /* Set bit 20 */
#elif BCHP_CHIP==7366
    BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cml_in_s3, 0x2); /* Set bit 13 */
    BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cmos_in_s3, 0x10); /* Set bit 20 */
#endif
#else

    if ((pd->chip_id & 0xffff0000) == 0x33900000) {
        printk(KERN_INFO "S3 support to be determined\n");
    } else if ((pd->chip_id & 0xffff0000) == 0x73640000) {
//        BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cml_in_s3, 0x4); /* Set bit 14 */
        val = BDEV_RD(0x00410074);
        val &= ~0x0000f000;
        val |= ((0x4 << 12) & 0x0000f000);
        BDEV_WR(0x00410074, val);

//        BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cmos_in_s3, 0x10); /* Set bit 20 */
        val = BDEV_RD(0x00410074);
        val &= ~0x003f0000;
        val |= ((0x10 << 16) & 0x003f0000);
        BDEV_WR(0x00410074, val);
    } else if ((pd->chip_id & 0xffff0000) == 0x73660000) {
//#define BCHP_AON_CTRL_ANA_XTAL_CONTROL           0x00410074 /* [RW] Ana xtal gisb control */
//#define BCHP_AON_CTRL_ANA_XTAL_CONTROL_en_osc_cml_in_s3_MASK       0x0000f000
//#define BCHP_AON_CTRL_ANA_XTAL_CONTROL_en_osc_cml_in_s3_SHIFT      12
//        BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cml_in_s3, 0x2); /* Set bit 13 */
        val = BDEV_RD(0x00410074);
        val &= ~0x0000f000;
        val |= ((0x2 << 12) & 0x0000f000);
        BDEV_WR(0x00410074, val);

//#define BCHP_AON_CTRL_ANA_XTAL_CONTROL_en_osc_cmos_in_s3_MASK      0x003f0000
//#define BCHP_AON_CTRL_ANA_XTAL_CONTROL_en_osc_cmos_in_s3_SHIFT     16
//        BDEV_WR_F(AON_CTRL_ANA_XTAL_CONTROL, en_osc_cmos_in_s3, 0x10); /* Set bit 20 */
        val = BDEV_RD(0x00410074);
        val &= ~0x003f0000;
        val |= ((0x10 << 16) & 0x003f0000);
        BDEV_WR(0x00410074, val);
    } else {
        printk(KERN_INFO "unknown chip_id for S3\n");
    }

#endif

    brcmstb_zigbee_interrupts_init(priv);

    priv->mbox_irq = platform_get_irq(pdev, 0);
    if ((int)priv->mbox_irq < 0)
        return -ENODEV;

    ret = devm_request_irq(dev, priv->mbox_irq, brcmstb_rf4ce_cpu_host_stb_l2_irq, 0, DRV_NAME, priv);
    if (ret < 0)
        return -ENODEV;

    minor_tbl[0] = priv;

    return rc;
}

static int brcmstb_zigbee_remove(struct platform_device *pdev)
{
    struct zigbee_priv_data *priv = platform_get_drvdata(pdev);
    printk(KERN_ALERT "brcmstb_zigbee_remove\n");

    brcmstb_zigbee_interrupts_uninit(priv);
    remove_proc();

    /* Question: can I prevent this from happening if I have open devices??? */
    printk(KERN_ALERT "DRIVER:  Goodbye, cruel world\n");
#ifdef DYNAMIC_MAJOR_NUMBER
    cdev_del(&priv->chr_dev);
#else
    unregister_chrdev(ZIGBEE_MAJOR, zigbee_name);
#endif

    return 0;
}

#ifdef CONFIG_PM_SLEEP
static int brcmstb_zigbee_suspend(struct device *dev)
{
    struct zigbee_priv_data *priv = dev_get_drvdata(dev);

    printk(KERN_ALERT "brcmstb_zigbee_suspend\n");

    return brcmstb_zigbee_prepare_suspend(priv);
}

static int brcmstb_zigbee_resume(struct device *dev)
{
    struct zigbee_priv_data *priv = dev_get_drvdata(dev);
    int ret;

    printk(KERN_ALERT "brcmstb_zigbee_resume\n");

    ret = disable_irq_wake(priv->wakeup_irq);

    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK);
    BDEV_SET(BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0, BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK);

    enable_irq(priv->mbox_irq);

    /*brcmstb_waketmr_clear_alarm(timer);*/

    return ret;
}
#endif /* CONFIG_PM_SLEEP */

static SIMPLE_DEV_PM_OPS(brcmstb_zigbee_pm_ops, brcmstb_zigbee_suspend,
        brcmstb_zigbee_resume);

static const struct of_device_id brcmstb_zigbee_of_match[] = {
    { .compatible = "brcm,rf4ce" }
};

struct platform_driver brcmstb_zigbee_driver = {
    .probe          = brcmstb_zigbee_probe,
    .remove         = brcmstb_zigbee_remove,
    .driver = {
        .name       = DRV_NAME,
        .owner      = THIS_MODULE,
        .pm = &brcmstb_zigbee_pm_ops,
        .of_match_table = of_match_ptr(brcmstb_zigbee_of_match),
    }
};
#ifndef BYPASS_MODULE_LICENSE
module_platform_driver(brcmstb_zigbee_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom");
MODULE_DESCRIPTION("Zigbee driver for STB chips");
#endif
