/******************************************************************************
* Copyright (C) 2017 Broadcom.
* The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
*
* This program is the proprietary software of Broadcom and/or its
* licensors, and may only be used, duplicated, modified or distributed pursuant
* to the terms and conditions of a separate, written license agreement executed
* between you and Broadcom (an "Authorized License").  Except as set forth in
* an Authorized License, Broadcom grants no license (express or implied), right
* to use, or waiver of any kind with respect to the Software, and Broadcom
* expressly reserves all rights in and to the Software and all intellectual
* property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
* HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
* NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
*
* Except as expressly set forth in the Authorized License,
*
* 1. This program, including its structure, sequence and organization,
*    constitutes the valuable trade secrets of Broadcom, and you shall use all
*    reasonable efforts to protect the confidentiality thereof, and to use
*    this information only in connection with your use of Broadcom integrated
*    circuit products.
*
* 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
*    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
*    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
*    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
*    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
*    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
*    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
*    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
*
* 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
*    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
*    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
*    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
*    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
*    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
*    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
*    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
******************************************************************************/

#define BDEV_RD(x) (rf4ce_controller_readl(x))
#define BDEV_WR(x, y) do { rf4ce_controller_writel(y, x); } while (0)
#define BDEV_UNSET(x, y) do { BDEV_WR((x), BDEV_RD(x) & ~(y)); } while (0)
#define BDEV_SET(x, y) do { BDEV_WR((x), BDEV_RD(x) | (y)); } while (0)

/*
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
*/

#define BCHP_SUN_TOP_CTRL_CHIP_FAMILY_ID            0x00000
#define BCHP_AON_CTRL_ANA_XTAL_CONTROL              0x00074 /* [RW] Ana xtal gisb control */

#define BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK                                0x00000001
#define BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK                              0x00000010
#define BCHP_RF4CE_CPU_CTRL_CTRL_HOSTIF_SEL_MASK                             0x00000020
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK   0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK            0x00000002

#ifndef BCHP_3390B0

#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK        0x00000004
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_SEM_INTR_MASK            0x00000004
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_MASK             0x00000004

#else

#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK        0x00000008
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_SEM_INTR_MASK            0x00000008
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_MASK             0x00000008

#endif

#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_MASK                           0x00000001
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_SHIFT                          0
#define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK                 0x01000000
#define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_SHIFT                24
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK              0x00000001
#define BCHP_RF4CE_CPU_L2_CPU_SET_MBOX_H2Z_FULL_INTR_MASK                    0x01000000

#ifndef BCHP_3390B0
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_MASK          0x00000004
#else
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_MASK          0x00000008
#endif

#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_Z2H_FULL_INTR_MASK       0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK        0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_WDOG_INTR_MASK                 0x00000002
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_WDOG_INTR_MASK                0x00000002
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_SW_INTR_MASK                  0xfffffff8
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_MASK     0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_WDOG_INTR_MASK              0x00000002

#define BCHP_RF4CE_CPU_PROG0_MEM_WORDi_ARRAY_BASE    0x00000
#define BCHP_RF4CE_CPU_DATA_MEM_REG_END              0x47ffc
#define BCHP_RF4CE_CPU_CTRL_REVID                    0x80000   /* [RO] RF4CE CPU Revision ID */
#define BCHP_RF4CE_CPU_CTRL_CTRL                     0x80004   /* [RW] Main Control Register */
#define BCHP_RF4CE_CPU_CTRL_ACCESS_LOCK              0x80008   /* [RW] Access Lock Register */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE0                0x80010   /* [RW] Software Spare Register 0 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE1                0x80014   /* [RW] Software Spare Register 1 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE2                0x80018   /* [RW] Software Spare Register 2 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE3                0x8001c   /* [RW] Software Spare Register 3 */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_ADDR            0x80020   /* [RW] RBUS Error Address */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_DATA            0x80024   /* [RW] RBUS Error Write Data */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_XAC             0x80028   /* [RW] RBUS Error Transaction */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_CTRL            0x8002c   /* [RW] RBUS Error Control */
#define BCHP_RF4CE_CPU_CTRL_TIMER0                   0x80030   /* [RW] Timer0 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER0_COUNT             0x80034   /* [RO] Timer0 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER1                   0x80038   /* [RW] Timer1 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER1_COUNT             0x8003c   /* [RO] Timer1 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER2                   0x80040   /* [RW] Timer2 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER2_COUNT             0x80044   /* [RO] Timer2 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER3                   0x80048   /* [RW] Timer3 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER3_COUNT             0x8004c   /* [RO] Timer3 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER4                   0x80050   /* [RW] Timer4 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER4_COUNT             0x80054   /* [RO] Timer4 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER5                   0x80058   /* [RW] Timer5 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER5_COUNT             0x8005c   /* [RO] Timer5 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER6                   0x80060   /* [RW] Timer6 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER6_COUNT             0x80064   /* [RO] Timer6 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER7                   0x80068   /* [RW] Timer7 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER7_COUNT             0x8006c   /* [RO] Timer7 Current Count */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DATA       0x80070   /* [RW] Mailbox FIFO Write/Read Data */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DEPTH      0x80074   /* [RO] Mailbox FIFO Current Depth */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS   0x80078   /* [RW] Mailbox FIFO Reset Pointers */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DATA       0x8007c   /* [RW] Mailbox FIFO Write/Read Data */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH      0x80080   /* [RO] Mailbox FIFO Current Depth */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_RST_PTRS   0x80084   /* [RW] Mailbox FIFO Reset Pointers */
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM                 0x80088   /* [RW] HOST2ZIGBEE Mailbox Semaphore */
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_TIMER           0x8008c   /* [RW] Mailbox Semaphore Timer Control Register */

#define BCHP_RF4CE_CPU_L2_CPU_STATUS                 0x80300   /* [RO] CPU interrupt Status Register */
#define BCHP_RF4CE_CPU_L2_CPU_SET                    0x80304   /* [WO] CPU interrupt Set Register */
#define BCHP_RF4CE_CPU_L2_CPU_CLEAR                  0x80308   /* [WO] CPU interrupt Clear Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_STATUS            0x8030c   /* [RO] CPU interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_SET               0x80310   /* [WO] CPU interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_CLEAR             0x80314   /* [WO] CPU interrupt Mask Clear Register */

#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0       0x80500   /* [RO] CPU interrupt Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_SET0          0x80504   /* [WO] CPU interrupt Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0        0x80508   /* [WO] CPU interrupt Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_STATUS0  0x8050c   /* [RO] CPU interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0     0x80510   /* [WO] CPU interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0   0x80514   /* [WO] CPU interrupt Mask Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS1       0x80518   /* [RO] Host Interrupt Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_SET1          0x8051c   /* [WO] Host Interrupt Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR1        0x80520   /* [WO] Host Interrupt Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_STATUS1  0x80524   /* [RO] Host Interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET1     0x80528   /* [WO] Host Interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR1   0x8052c   /* [WO] Host Interrupt Mask Clear Register */
