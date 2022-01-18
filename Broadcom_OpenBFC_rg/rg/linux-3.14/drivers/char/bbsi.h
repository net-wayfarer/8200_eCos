/*
<:label-BRCM::DUAL/GPL:standard
:>
*/

#ifndef _BBSI_H
#define _BBSI_H


#define BBSI_COMMAND_BYTE 0x80


#define STATUS_REGISTER_ADDR  0x6


#define CPU_RUNNING_SHIFT 0x6
#define CPU_RUNNING_MASK  0x1

#define HAB_REQ_SHIFT     0x5
#define HAB_REQ_MASK      0x1

#define BUSY_SHIFT        0x4
#define BUSY_MASK         0x1

#define RBUS_UNEXP_TX_SHIFT  0x3
#define RBUS_UNEXP_TX_MASK   0x1

#define RBUS_TIMEOUT_SHIFT   0x2
#define RBUS_TIMEOUT_MASK    0x1

#define RBUS_ERR_ACK_SHIFT   0x1
#define RBUS_ERR_ACK_MASK    0x1

#define ERROR_SHIFT   0x0
#define ERROR_MASK    0x1



#define CONFIG_REGISTER_ADDR  0x7

#define DATA0_REGISTER_ADDR  0xC

#define XFER_MODE_SHIFT  0x3
#define XFER_MODE_MASK   0x3

#define NO_RBUS_ADDR_INC_SHIFT     0x2
#define NO_RBUS_ADDR_INC_MASK      0x1

#define SPECULATIVE_READ_EN_SHIFT   0x1
#define SPECULATIVE_READ_EN_MASK    0x1

#define READ_RBUS_SHIFT   0x0
#define READ_RBUS_MASK    0x1

#define MAX_SPISLAVE_DEV_NUM	   1

int kerSysBcmSpiSlaveInit(int dev);
int kerSysBcmSpiSlaveRead(int dev, unsigned long addr, unsigned long * data, unsigned long len);
unsigned long kerSysBcmSpiSlaveReadReg32(int dev, unsigned long addr);
int kerSysBcmSpiSlaveWrite(int dev, unsigned long addr, unsigned long data, unsigned long len);
void kerSysBcmSpiSlaveWriteReg32(int dev, unsigned long addr, unsigned long data);
int kerSysBcmSpiSlaveWriteBuf(int dev, unsigned long addr, unsigned long *data, unsigned long len, unsigned int unitSize);
int kerSysBcmSpiSlaveReadBuf(int dev, unsigned long addr, unsigned long *data, unsigned long len, unsigned int unitSize);

extern int bbsi_initialized;


struct bbsi_write
{
   unsigned int addr;
   unsigned int val;
};

struct bbsi_read
{
   unsigned int addr;
   unsigned int *val;
};

#define BBSI_IOC_MAGIC    'B'

#define BBSI_IOCTL_WRITE  _IOR(BBSI_IOC_MAGIC, 1, struct bbsi_write)
#define BBSI_IOCTL_READ   _IOR(BBSI_IOC_MAGIC, 2, struct bbsi_read)


#endif
