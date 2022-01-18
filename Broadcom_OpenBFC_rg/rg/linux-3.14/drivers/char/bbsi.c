/*
<:copyright-BRCM:2013:DUAL/GPL:standard

   Copyright (c) 2013 Broadcom Corporation
   All Rights Reserved

Unless you and Broadcom execute a separate written software license
agreement governing use of this software, this software is licensed
to you under the terms of the GNU General Public License version 2
(the "GPL"), available at http://www.broadcom.com/licenses/GPLv2.php,
with the following added to such license:

   As a special exception, the copyright holders of this software give
   you permission to link this software with independent modules, and
   to copy and distribute the resulting executable under terms of your
   choice, provided that you also meet, for each linked independent
   module, the terms and conditions of the license of that module.
   An independent module is a module which is not derived from this
   software.  The special exception does not apply to any modifications
   of the software.

Not withstanding the above, under no circumstances may you combine
this software in any way with any other Broadcom software provided
under a license other than the GPL, without Broadcom's express prior
written consent.

:>
*/

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/brcmstb/brcmstb.h>
#include <linux/if.h>
#include <linux/ioctl.h>
#include <linux/gpio/consumer.h>
#include <linux/of.h>
#include <asm/uaccess.h>
#include "6802_map_part.h"
#include "bbsi.h"

/***************************************************************************
* File Name  : bbsi.c
*
* Description: This file contains the functions for communicating between a brcm
*              cpe chip(eg 7366) to another brcm chip(6802) which is connected
*              as a spi slave device. This protocol used to communicate is BBSI.
*
***************************************************************************/

const char bbsi_driver_name[] = "bbsi";

#define MAX_STATUS_RETRY 5

#define BBSI_MAJOR    235
#define BBSI_CLASS    "bbsi"

#define MODULE_NAME	"brcm-bbsi"
#define MODULE_VER	"0.1"

struct pidev {
	struct spi_device *pdev;
};

struct output_prop {
	struct device_node *np;
	u32 gpio;
	u32 active_hi_lo;
};

int bbsi_initialized = 0;
static struct mutex bcmSpiSlaveMutex[MAX_SPISLAVE_DEV_NUM];

struct bbsi_dev {
	struct class *bbsi_class;
	struct spi_device *bbsi_spi_device;
};

static struct bbsi_dev bbsi_dev;	// for now, just support 1 bbsi device

static void resetSpiSlaveDevice(int dev);

static int isBBSIDone(int dev)
{
	uint8_t read_status[2] = { BBSI_COMMAND_BYTE,	// | 0x1, // Do a Read
		STATUS_REGISTER_ADDR
	};
	uint8_t read_rx;
	int status;
	int i;
	int ret = 0;

	for (i = 0; i < MAX_STATUS_RETRY; i++) {
		status =
		    spi_write_then_read(bbsi_dev.bbsi_spi_device, read_status,
					2, &read_rx, 1);

		if (0 != status) {
			printk(KERN_ERR "isBBSIDone: spi returned error\n");
			ret = 0;
			break;
		}

		if (read_rx & 0xF) {
			printk(KERN_ERR
			       "isBBSIDone: BBSI transaction error, status=0x%x\n",
			       read_rx);
			ret = 0;
			break;
		} else if ((read_rx & (1 << BUSY_SHIFT)) == 0) {
			ret = 1;
			break;
		}
	}

	return ret;
}

int kerSysBcmSpiSlaveRead(int dev, unsigned long addr, unsigned long *data,
			  unsigned long len)
{
	uint8_t buf[12];
	int status;

	if (dev >= MAX_SPISLAVE_DEV_NUM || (bbsi_dev.bbsi_spi_device == NULL)) {
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return (-1);
	}

	mutex_lock(&bcmSpiSlaveMutex[dev]);

	buf[0] = BBSI_COMMAND_BYTE | 0x1;
	buf[1] = CONFIG_REGISTER_ADDR;	/* Start the writes from this addr */
	buf[2] = ((4 - len) << XFER_MODE_SHIFT) | 0x1;	/* Indicates the transaction is 32bit, 24bit, 16bit or 8bit. Len is 1..4 */
	buf[3] = (addr >> 24) & 0xFF;	/* Assuming MSB bytes are always sent first */
	buf[4] = (addr >> 16) & 0xFF;
	buf[5] = (addr >> 8) & 0xFF;
	buf[6] = (addr >> 0) & 0xFF;

	status = spi_write(bbsi_dev.bbsi_spi_device, buf, 7);

	if (0 != status) {
		printk(KERN_ERR "kerSysBcmSpiSlaveRead: Spi returned error\n");
		mutex_unlock(&bcmSpiSlaveMutex[dev]);
		return (-1);
	}

	if (!isBBSIDone(dev)) {
		printk(KERN_ERR
		       "kerSysBcmSpiSlaveRead: read to addr:0x%lx failed\n",
		       addr);
		mutex_unlock(&bcmSpiSlaveMutex[dev]);
		return (-1);
	}

	buf[0] = BBSI_COMMAND_BYTE;	//read
	buf[1] = DATA0_REGISTER_ADDR;
	status = spi_write_then_read(bbsi_dev.bbsi_spi_device, buf, 2, buf, 4);
	if (0 != status) {
		printk(KERN_ERR
		       "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
		mutex_unlock(&bcmSpiSlaveMutex[dev]);
		return (-1);
	}

	*data = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];

	mutex_unlock(&bcmSpiSlaveMutex[dev]);
	return (0);
}

int kerSysBcmSpiSlaveWrite(int dev, unsigned long addr, unsigned long data,
			   unsigned long len)
{
	uint8_t buf[12];
	int status;

	if (dev >= MAX_SPISLAVE_DEV_NUM || (bbsi_dev.bbsi_spi_device == NULL)) {
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return (-1);
	}

	mutex_lock(&bcmSpiSlaveMutex[dev]);

	data <<= (8 * (4 - len));	// Do we have to do this? It will matter only for len = 1 or 2.

	buf[0] = BBSI_COMMAND_BYTE | 0x1;	/* Assumes write signal is 0 */
	buf[1] = CONFIG_REGISTER_ADDR;	/* Start the writes from this addr */
	buf[2] = (4 - len) << XFER_MODE_SHIFT;	/* Indicates the transaction is 32bit, 24bit, 16bit or 8bit. Len is 1..4 */
	buf[3] = (addr >> 24) & 0xFF;	/* Assuming MSB bytes are always sent first */
	buf[4] = (addr >> 16) & 0xFF;
	buf[5] = (addr >> 8) & 0xFF;
	buf[6] = (addr >> 0) & 0xFF;
	buf[7] = (data >> 24) & 0xFF;
	buf[8] = (data >> 16) & 0xFF;
	buf[9] = (data >> 8) & 0xFF;
	buf[10] = (data >> 0) & 0xFF;

	status = spi_write(bbsi_dev.bbsi_spi_device, buf, 11);
	if (0 != status) {
		printk(KERN_ERR "kerSysBcmSpiSlaveWrite: Spi returned error\n");
		mutex_unlock(&bcmSpiSlaveMutex[dev]);
		return (-1);
	}

	if (!isBBSIDone(dev)) {
		printk(KERN_ERR
		       "kerSysBcmSpiSlaveWrite: write to addr:0x%lx failed\n",
		       addr);
		mutex_unlock(&bcmSpiSlaveMutex[dev]);
		return (-1);
	}

	mutex_unlock(&bcmSpiSlaveMutex[dev]);

	return (0);
}

static int doReadBuffer(int dev, unsigned long addr, unsigned long *data,
			unsigned long len)
{
	uint8_t buf[12];
	int status;

	buf[0] = BBSI_COMMAND_BYTE | 0x1;
	buf[1] = CONFIG_REGISTER_ADDR;	/* Start the writes from this addr */
	buf[2] = 0x3;		/* Indicates the transaction is 32bit, 24bit, 16bit or 8bit. Len is 1..4 */
	buf[3] = (addr >> 24) & 0xFF;	/* Assuming MSB bytes are always sent first */
	buf[4] = (addr >> 16) & 0xFF;
	buf[5] = (addr >> 8) & 0xFF;
	buf[6] = (addr >> 0) & 0xFF;

	status = spi_write(bbsi_dev.bbsi_spi_device, buf, 7);
	if (0 != status) {
		printk(KERN_ERR "kerSysBcmSpiSlaveWrite: Spi returned error\n");
		return (-1);
	}

	if (0 != status) {
		printk(KERN_ERR
		       "SPI Slave Read: BcmSpiSyncTrans returned error\n");
		return (-1);
	}

	if (!isBBSIDone(dev)) {
		printk(KERN_ERR "SPI Slave Read: read to addr:0x%lx failed\n",
		       addr);
		return (-1);
	}

	buf[0] = BBSI_COMMAND_BYTE;	//read
	buf[1] = DATA0_REGISTER_ADDR;

	while (len) {
		unsigned int count;

		count = (len > 4 ? 4 : len);

		status =
		    spi_write_then_read(bbsi_dev.bbsi_spi_device, buf, 2, data,
					count);
		if (0 != status) {
			printk(KERN_ERR
			       "kerSysBcmSpiSlaveRead: BcmSpiSyncTrans returned error\n");
			return (-1);
		}

		if (!isBBSIDone(dev)) {
			printk(KERN_ERR
			       "SPI Slave Read: read to addr:0x%lx failed\n",
			       addr);
			return (-1);
		}

		len -= count;
		data += count / 4;
	}

	return (0);
}

static int doWriteBuffer(int dev, unsigned long addr, unsigned long *data,
			 unsigned long len)
{
	static uint8_t buf[4104];
	int status;

	if (len > sizeof(buf)-7)		// 7 bytes are used for addressing and BBSI protocol
	{
		printk(KERN_ERR
		       "SPI Slave Write: write to addr:0x%lx failed.  Len (%ld) too long.\n",
		       addr, len);
		return (-1);
	}

	buf[0] = BBSI_COMMAND_BYTE | 0x1;	/* Assumes write signal is 0 */
	buf[1] = CONFIG_REGISTER_ADDR;	/* Start the writes from this addr */
	buf[2] = 0;		/* Transactions are 32-bits */
	buf[3] = (addr >> 24) & 0xFF;	/* Assuming MSB bytes are always sent first */
	buf[4] = (addr >> 16) & 0xFF;
	buf[5] = (addr >> 8) & 0xFF;
	buf[6] = (addr >> 0) & 0xFF;

	memcpy(&buf[7], data, len);

	status = spi_write(bbsi_dev.bbsi_spi_device, buf, 7 + len);
	if (0 != status) {
		printk(KERN_ERR
		       "SPI Slave Write: BcmSpiSyncTrans returned error\n");
		return (-1);
	}

	if (!isBBSIDone(dev)) {
		printk(KERN_ERR "SPI Slave Write: write to addr:0x%lx failed\n",
		       addr);
		return (-1);
	}
	return (0);
}

int kerSysBcmSpiSlaveInit(int dev)
{
	unsigned long data;
	int32_t retVal = 0;

	if (dev >= MAX_SPISLAVE_DEV_NUM)
	{
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return (-1);
	}
	resetSpiSlaveDevice(dev);

	if ((kerSysBcmSpiSlaveRead(dev, SUN_TOP_CTRL_CHIP_FAMILY_ID, &data, 4)
	     == -1) || (data == 0) || (data == 0xffffffff)) {
		printk(KERN_ERR
		       "%s: Failed to read the SUN_TOP_CTRL_CHIP_FAMILY_ID: 0x%08x\n",
		       __FUNCTION__, (unsigned int)data);
		return -1;
	} else {
		printk(KERN_INFO "%s: SUN_TOP_CTRL_CHIP_FAMILY_ID: 0x%08x\n",
		       __FUNCTION__, (unsigned int)data);
	}

	if ((kerSysBcmSpiSlaveRead(dev, SUN_TOP_CTRL_PRODUCT_ID, &data, 4) ==
	     -1) || (data == 0) || (data == 0xffffffff)) {
		printk(KERN_ERR
		       "%s: Failed to read the SUN_TOP_CTRL_PRODUCT_ID: 0x%08x\n",
		       __FUNCTION__, (unsigned int)data);
		return -1;
	} else {
		printk(KERN_ALERT "%s: SUN_TOP_CTRL_PRODUCT_ID: 0x%08x\n",
		       __FUNCTION__, (unsigned int)data);
	}

	return (retVal);

}

int kerSysBcmSpiSlaveReadBuf(int dev, unsigned long addr, unsigned long *data,
			     unsigned long len, unsigned int unitSize)
{
	int ret = -1;

	if (dev >= MAX_SPISLAVE_DEV_NUM || (bbsi_dev.bbsi_spi_device == NULL)) {
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return (-1);
	}

	addr &= 0x1fffffff;

	mutex_lock(&bcmSpiSlaveMutex[dev]);
	ret = doReadBuffer(dev, addr, data, len);
	mutex_unlock(&bcmSpiSlaveMutex[dev]);

	return ret;
}

int kerSysBcmSpiSlaveWriteBuf(int dev, unsigned long addr, unsigned long *data,
			      unsigned long len, unsigned int unitSize)
{
	int ret = -1;
	int count = 0;

	if (dev >= MAX_SPISLAVE_DEV_NUM || (bbsi_dev.bbsi_spi_device == NULL)) {
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return (-1);
	}

	addr &= 0x1fffffff;

	mutex_lock(&bcmSpiSlaveMutex[dev]);
	while (len) {
		count = (len > 500 ? 500 : len);

		ret = doWriteBuffer(dev, addr, data, count);
		if (ret)
			break;

		len -= count;
		addr += count;
		data += count / sizeof(unsigned long);
	}
	mutex_unlock(&bcmSpiSlaveMutex[dev]);

	return ret;
}

unsigned long kerSysBcmSpiSlaveReadReg32(int dev, unsigned long addr)
{
	unsigned long data = 0;
	BUG_ON(addr & 3);
	addr &= 0x1fffffff;

	if (kerSysBcmSpiSlaveRead(dev, addr, &data, 4) < 0) {
		printk(KERN_ERR "kerSysBcmSpiSlaveReadReg32: can't read %08x\n",
		       (unsigned int)addr);
	}

	return (data);
}

void kerSysBcmSpiSlaveWriteReg32(int dev, unsigned long addr,
				 unsigned long data)
{
	BUG_ON(addr & 3);
	addr &= 0x1fffffff;

	if (kerSysBcmSpiSlaveWrite(dev, addr, data, 4) < 0) {
		printk(KERN_ERR
		       "kerSysBcmSpiSlaveWriteReg32: can't write %08x (data %08x)\n",
		       (unsigned int)addr, (unsigned int)data);
	}

}

static void resetSpiSlaveDevice(int dev)
{
//	unsigned int reg;
//	struct gpio_desc *gpio;

	if (dev >= MAX_SPISLAVE_DEV_NUM || (bbsi_dev.bbsi_spi_device == NULL)) {
		printk(KERN_ERR
		       "%s: dev (%d) out of range or spi_device not initialized\n",
		       __FUNCTION__, dev);
		return;
	}

#if 0
	gpio = get_pinctrl_dev_from_devname("brcm-bcmcm-pinctrl-gpio");

	//gpio = devm_gpiod_get_index(&dev, "output", i);

	gpiod_direction_output(gpio, gpiod_is_active_low(gpio));
	pr_info("%s: initializing gpio %d as output, %s.\n",
			MODULE_NAME, desc_to_gpio(gpio),
			initvals[i] == 1 ? "active" : "inactive");
	gpiod_set_value(gpio, 1); //initvals[i]);
	devm_gpiod_put(&pdev->dev, gpio);
#endif

	// This config (pin mux and gpio 14 direction) should be done in BOLT instead of here:
/*
	reg = BDEV_RD(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_4);
	reg &= ~(0x0F000000);
	reg |= 0x03000000;
	BDEV_WR(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_4, reg);	// sck

	reg = BDEV_RD(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_5);
	reg &= ~(0x00000F00);
	reg |= 0x300;
	BDEV_WR(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_5, reg);	// SPI Slave Select

	reg = BDEV_RD(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_8);
	reg &= ~(0x00000FF0);
	reg |= 0x220;
	BDEV_WR(BCHP_SUN_TOP_CTRL_PIN_MUX_CTRL_8, reg);	// miso, mosi
*/
	// using GPIO 14 for reset line:


/* MOD RTE - REMOVED - need to work this for 3384 */
/*
	reg = BDEV_RD(BCHP_GIO_IODIR_LO);
	reg &= ~(1 << 14);
	BDEV_WR(BCHP_GIO_IODIR_LO, reg);

	// High, Low, High - High is out of reset, low is in reset
	reg = BDEV_RD(BCHP_GIO_DATA_LO);
	reg |= (1 << 14);
	BDEV_WR(BCHP_GIO_DATA_LO, reg);
	mdelay(50);

	reg = BDEV_RD(BCHP_GIO_DATA_LO);
	reg &= ~(1 << 14);
	BDEV_WR(BCHP_GIO_DATA_LO, reg);
	mdelay(300);

	reg = BDEV_RD(BCHP_GIO_DATA_LO);
	reg |= (1 << 14);
	BDEV_WR(BCHP_GIO_DATA_LO, reg);
	mdelay(300);
*/
}

int bbsi_probe(struct spi_device *spi_device)
{
	struct pidev *pidev;
	int status;
	struct device_node *of_node = spi_device->dev.of_node;
	struct gpio_desc *gpio;
	int count;
	int i;
	u32 *initvals = NULL;

	printk(KERN_ALERT "MoCA BBSI probe called. ss=0x%x freq:%lu\n",
		   (unsigned int)spi_device->chip_select, (long unsigned int)spi_device->max_speed_hz);

	pidev = kzalloc(sizeof(struct pidev), GFP_KERNEL);
	if (!pidev) {
		status = -ENOMEM;
		goto done;
	}
	pidev->pdev = spi_device;

	count = of_property_count_elems_of_size(of_node, "mocapwr-gpio",
						 sizeof(struct output_prop));
	/* -EINVAL (doesn't exist) is OK */
	if (count <= 0 && count != -EINVAL) {
		pr_err("%s: mocapwr-gpios property is invalid.\n", __func__);
		status = count;
		goto err_free_pidev;
	}

	if (count > 0) {
		i = of_property_count_u32_elems(of_node, "mocapwr-initval");
		if (i != count) {
			pr_err("%s: mocapwr-initval property is invalid or ",
				   __func__);
			pr_cont("# of mocapwr-gpios (%d) does not match", count);
			pr_cont(" # of mocapwr-initval (%d).\n", i);
			goto err_free_pidev;
		}
		initvals = kzalloc(i * sizeof(u32), GFP_KERNEL);
		if (!initvals) {
			pr_err("%s: Failed to alloc array for mocapwr-initval.\n",
				   __func__);
			status = -ENOMEM;
			goto err_free_pidev;
		}
		status = of_property_read_u32_array(of_node, "mocapwr-initval",
											initvals, i);
		if (status) {
			pr_err("%s: Failed to read output-initvals.\n",
			       __func__);
			goto err_free_initvals;
		}

		for (i = 0; i < count; i++) {
			gpio = devm_gpiod_get_index(&spi_device->dev, "mocapwr", i);
			if (IS_ERR(gpio)) {
				pr_err("%s: Failed to get output GPIO ",
					   __func__);
				pr_cont("descriptor index %d: erro code %d.\n",
					i, (int)gpio);
				status = PTR_ERR(gpio);
				goto err_free_initvals;
			}
			gpiod_direction_output(gpio, gpiod_is_active_low(gpio));
			pr_info("%s: initializing gpio %d as output, %s.\n",
				MODULE_NAME, desc_to_gpio(gpio),
				initvals[i] == 1 ? "active" : "inactive");
			gpiod_set_value(gpio, initvals[i]);
			devm_gpiod_put(&spi_device->dev, gpio);
		}
		kfree(initvals);
		initvals = NULL;
	}
	goto done;

err_free_initvals:
	if (initvals)
		kfree(initvals);

err_free_pidev:
	kfree(pidev);

done:
	bbsi_dev.bbsi_spi_device = spi_device;
	mutex_init(&bcmSpiSlaveMutex[0]);

	spi_setup(spi_device);
	mdelay(500);

	status = kerSysBcmSpiSlaveInit(0);
	if (status != 0)
		goto bad;

	device_create(bbsi_dev.bbsi_class, NULL,
		      MKDEV(BBSI_MAJOR, 0), NULL, "bbsi0");

	bbsi_initialized = 1;

	return 0;

bad:
	mutex_destroy(&bcmSpiSlaveMutex[0]);
	return status;
}

enum bbsi_device_ids {
	ID_BBSI
};

static const struct spi_device_id bbsi_id[] = {
	{"bbsi", ID_BBSI},
	{},
};

//MODULE_DEVICE_TABLE(spi, bbsi_id);

static struct of_device_id bbsi_of_match[] = {
    {.compatible = "brcm,bbsi"},
    {}
};

static const struct spi_device_id bbsi_ids[] = {
        { "bbsi", },
        {},
};

static struct spi_driver bbsi_driver = {
	.driver = {
		   .name = bbsi_driver_name,
		   .owner = THIS_MODULE,
                    .of_match_table = bbsi_of_match
		   },
    .id_table = bbsi_ids,
	.probe = bbsi_probe,
};

/* Character device */

static int bbsi_file_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int bbsi_file_release(struct inode *inode, struct file *file)
{
	return 0;
}

static long bbsi_file_ioctl(struct file *file, unsigned int cmd,
			    unsigned long arg)
{
	struct bbsi_write wr;
	struct bbsi_read rd;
	unsigned int val;
	long ret = -ENOTTY;

	switch (cmd) {
	case BBSI_IOCTL_READ:
		if (copy_from_user(&rd, (void __user *)arg, sizeof(rd)))
			return -EFAULT;
		if (kerSysBcmSpiSlaveRead
		    (0, (long unsigned int)rd.addr, (long unsigned int *)&val,
		     4))
			return -EFAULT;
		if (put_user(val, rd.val))
			return -EFAULT;
		ret = 0;
		break;
	case BBSI_IOCTL_WRITE:
		if (copy_from_user(&wr, (void __user *)arg, sizeof(wr)))
			return -EFAULT;
		if (kerSysBcmSpiSlaveWrite(0, wr.addr, wr.val, 4))
			return -EFAULT;
		ret = 0;
		break;
	}
	return ret;
}

struct file_operations bbsi_fops = {
 unlocked_ioctl:bbsi_file_ioctl,
 open:	bbsi_file_open,
 release:bbsi_file_release
};

static int __init bbsi_init(void)
{
	int ret;

	printk(KERN_ALERT "BBSI initialization...\n");

	memset(&bbsi_dev, 0, sizeof(bbsi_dev));

	ret = register_chrdev(BBSI_MAJOR, BBSI_CLASS, &bbsi_fops);
	if (ret < 0) {
		pr_err("can't register major %d\n", BBSI_MAJOR);
		goto bad;
	}

	bbsi_dev.bbsi_class = class_create(THIS_MODULE, BBSI_CLASS);
	if (IS_ERR(bbsi_dev.bbsi_class)) {
		pr_err("can't create device class\n");
		ret = PTR_ERR(bbsi_dev.bbsi_class);
		goto bad;
	}

	return 0;

 bad:
	unregister_chrdev(BBSI_MAJOR, BBSI_CLASS);
	return ret;
}

module_init(bbsi_init);

void __exit bbsi_exit(void)
{
	unregister_chrdev(BBSI_MAJOR, BBSI_CLASS);
}

module_exit(bbsi_exit);

module_spi_driver(bbsi_driver);

MODULE_AUTHOR("Chris Jaszczur");
MODULE_DESCRIPTION("BBSI module");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");
