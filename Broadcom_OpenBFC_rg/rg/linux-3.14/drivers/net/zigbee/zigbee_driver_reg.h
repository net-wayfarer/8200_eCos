/******************************************************************************
* (c) 2014 Broadcom Corporation
*
* This program is the proprietary software of Broadcom Corporation and/or its
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

#ifdef COMPILE_TIME
#include <linux/brcmstb/brcmstb.h>
#include "bchp_hif_cpu_intr1.h"
#include "bchp_aon_ctrl.h"
#include "bchp_common.h"
#include "bchp_sun_top_ctrl.h"
#include "bchp_rf4ce_cpu_host_stb_l2.h"
#include "bchp_rf4ce_cpu_ctrl.h" /* Masks are defined here for 7364 and 7366 */
#include "bchp_rf4ce_cpu_l2.h"

#if BCHP_CHIP!=7366 && BCHP_CHIP!=7364
    #include "bchp_rf4ce_cpu_host_stb_l2_ub.h" // needed for the mask values
    #include "bchp_rf4ce_cpu_ctrl_ub.h" // needed for the mask values
    #include "bchp_rf4ce_cpu_l2_ub.h" // needed for the mask values
#endif

#include "bchp_rf4ce_cpu_prog0_mem.h"
#include "bchp_rf4ce_cpu_prog1_mem.h"

#if BCHP_CHIP==7364 || BCHP_CHIP==7366
    #define BCHP_HIF_CPU_INTR1_INTR_W4_STATUS                                       BCHP_HIF_CPU_INTR1_INTR_W3_STATUS
    #define BCHP_HIF_CPU_INTR1_INTR_W4_MASK_STATUS                                  BCHP_HIF_CPU_INTR1_INTR_W3_MASK_STATUS
    #define BCHP_HIF_CPU_INTR1_INTR_W4_STATUS_RF4CE_STB_CPU_INTR_MASK               BCHP_HIF_CPU_INTR1_INTR_W3_STATUS_RF4CE_STB_CPU_INTR_MASK
#else
    #define BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK                                   BCHP_RF4CE_CPU_CTRL_UB_CTRL_CPU_RST_MASK /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_SHIFT                                  BCHP_RF4CE_CPU_CTRL_UB_CTRL_CPU_RST_SHIFT /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK                                 BCHP_RF4CE_CPU_CTRL_UB_CTRL_START_ARC_MASK /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_SHIFT                                BCHP_RF4CE_CPU_CTRL_UB_CTRL_START_ARC_SHIFT /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_CTRL_HOSTIF_SEL_MASK                                BCHP_RF4CE_CPU_CTRL_UB_CTRL_HOSTIF_SEL_MASK /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_CTRL_HOSTIF_SEL_SHIFT                               BCHP_RF4CE_CPU_CTRL_UB_CTRL_HOSTIF_SEL_SHIFT /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_MASK                              BCHP_RF4CE_CPU_CTRL_UB_MBOX_SEM_MBOX_SEM_MASK /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_SHIFT                             BCHP_RF4CE_CPU_CTRL_UB_MBOX_SEM_MBOX_SEM_SHIFT /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK                 BCHP_RF4CE_CPU_CTRL_UB_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */
    #define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_SHIFT                BCHP_RF4CE_CPU_CTRL_UB_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_ctrl_ub.h */

    #define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK                    BCHP_RF4CE_CPU_L2_UB_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_l2_ub.h */
    #define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_SHIFT                   BCHP_RF4CE_CPU_L2_UB_CPU_STATUS_MBOX_H2Z_FULL_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_l2_ub.h */
    #define BCHP_RF4CE_CPU_L2_CPU_SET_MBOX_H2Z_FULL_INTR_MASK                       BCHP_RF4CE_CPU_L2_UB_CPU_SET_MBOX_H2Z_FULL_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_l2_ub.h */
    #define BCHP_RF4CE_CPU_L2_CPU_SET_MBOX_H2Z_FULL_INTR_SHIFT                      BCHP_RF4CE_CPU_L2_UB_CPU_SET_MBOX_H2Z_FULL_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_l2_ub.h */

    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK      BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS1_MBOX_Z2H_FULL_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_SHIFT     BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS1_MBOX_Z2H_FULL_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK               BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_WDOG_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_SHIFT              BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_WDOG_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK           BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_SHIFT          BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_CLEAR0_MBOX_SEM_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_MASK                BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_MBOX_SEM_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_SHIFT               BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_MBOX_SEM_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_MASK             BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_MBOX_SEM_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_SHIFT            BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_MBOX_SEM_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK           BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_SHIFT          BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_WDOG_INTR_MASK                   BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS0_WDOG_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_WDOG_INTR_MASK                    BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_WDOG_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_WDOG_INTR_SHIFT                   BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_CLEAR0_WDOG_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_SW_INTR_MASK                     BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS0_SW_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_SEM_INTR_MASK               BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS0_MBOX_SEM_INTR_MASK  /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_Z2H_FULL_INTR_MASK          BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_STATUS0_MBOX_Z2H_FULL_INTR_MASK  /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_WDOG_INTR_MASK                 BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_WDOG_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_WDOG_INTR_SHIFT                BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_WDOG_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_MASK        BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_MASK /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
    #define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_SHIFT       BCHP_RF4CE_CPU_HOST_STB_L2_UB_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_SHIFT /* Latter is defined in bchp_rf4ce_cpu_host_stb_l2_ub.h */
#endif

#else

#define BRCMSTB_PERIPH_VIRT	0xfc000000
#define BVIRTADDR(x)		(BRCMSTB_PERIPH_VIRT + ((x) & 0x0fffffff))
#define DEV_RD(x) (*((volatile unsigned long *)(x)))
#define DEV_WR(x, y) do { *((volatile unsigned long *)(x)) = (y); } while (0)
#define BDEV_RD(x) (DEV_RD(BVIRTADDR(x)))
#define BDEV_WR(x, y) do { DEV_WR(BVIRTADDR(x), (y)); } while (0)
#define BDEV_UNSET(x, y) do { BDEV_WR((x), BDEV_RD(x) & ~(y)); } while (0)
#define BDEV_SET(x, y) do { BDEV_WR((x), BDEV_RD(x) | (y)); } while (0)

#define BCHP_RF4CE_CPU_CTRL_CTRL_CPU_RST_MASK                                0x00000001
#define BCHP_RF4CE_CPU_CTRL_CTRL_START_ARC_MASK                              0x00000010
#define BCHP_RF4CE_CPU_CTRL_CTRL_HOSTIF_SEL_MASK                             0x00000020
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_Z2H_FULL_INTR_MASK   0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_WDOG_INTR_MASK            0x00000002
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0_MBOX_SEM_INTR_MASK        0x00000004
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_SEM_INTR_MASK            0x00000004
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_SEM_INTR_MASK             0x00000004
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_MASK                           0x00000001
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_MBOX_SEM_SHIFT                          0
#define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_MASK                 0x01000000
#define BCHP_RF4CE_CPU_L2_CPU_STATUS_MBOX_H2Z_FULL_INTR_SHIFT                24
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS_RST_PTR_MASK              0x00000001
#define BCHP_RF4CE_CPU_L2_CPU_SET_MBOX_H2Z_FULL_INTR_MASK                    0x01000000
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_SEM_INTR_MASK          0x00000004
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_MBOX_Z2H_FULL_INTR_MASK       0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_MBOX_Z2H_FULL_INTR_MASK        0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0_WDOG_INTR_MASK                 0x00000002
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_WDOG_INTR_MASK                0x00000002
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0_SW_INTR_MASK                  0xfffffff8
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_MBOX_Z2H_FULL_INTR_MASK     0x00000001
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0_WDOG_INTR_MASK              0x00000002

#define BCHP_RF4CE_CPU_PROG0_MEM_WORDi_ARRAY_BASE    (base_addr + 0x00000)
#define BCHP_RF4CE_CPU_CTRL_REVID                    (base_addr + 0x80000)   /* [RO] RF4CE CPU Revision ID */
#define BCHP_RF4CE_CPU_CTRL_CTRL                     (base_addr + 0x80004)   /* [RW] Main Control Register */
#define BCHP_RF4CE_CPU_CTRL_ACCESS_LOCK              (base_addr + 0x80008)   /* [RW] Access Lock Register */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE0                (base_addr + 0x80010)   /* [RW] Software Spare Register 0 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE1                (base_addr + 0x80014)   /* [RW] Software Spare Register 1 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE2                (base_addr + 0x80018)   /* [RW] Software Spare Register 2 */
#define BCHP_RF4CE_CPU_CTRL_SW_SPARE3                (base_addr + 0x8001c)   /* [RW] Software Spare Register 3 */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_ADDR            (base_addr + 0x80020)   /* [RW] RBUS Error Address */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_DATA            (base_addr + 0x80024)   /* [RW] RBUS Error Write Data */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_XAC             (base_addr + 0x80028)   /* [RW] RBUS Error Transaction */
#define BCHP_RF4CE_CPU_CTRL_RBUS_ERR_CTRL            (base_addr + 0x8002c)   /* [RW] RBUS Error Control */
#define BCHP_RF4CE_CPU_CTRL_TIMER0                   (base_addr + 0x80030)   /* [RW] Timer0 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER0_COUNT             (base_addr + 0x80034)   /* [RO] Timer0 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER1                   (base_addr + 0x80038)   /* [RW] Timer1 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER1_COUNT             (base_addr + 0x8003c)   /* [RO] Timer1 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER2                   (base_addr + 0x80040)   /* [RW] Timer2 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER2_COUNT             (base_addr + 0x80044)   /* [RO] Timer2 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER3                   (base_addr + 0x80048)   /* [RW] Timer3 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER3_COUNT             (base_addr + 0x8004c)   /* [RO] Timer3 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER4                   (base_addr + 0x80050)   /* [RW] Timer4 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER4_COUNT             (base_addr + 0x80054)   /* [RO] Timer4 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER5                   (base_addr + 0x80058)   /* [RW] Timer5 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER5_COUNT             (base_addr + 0x8005c)   /* [RO] Timer5 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER6                   (base_addr + 0x80060)   /* [RW] Timer6 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER6_COUNT             (base_addr + 0x80064)   /* [RO] Timer6 Current Count */
#define BCHP_RF4CE_CPU_CTRL_TIMER7                   (base_addr + 0x80068)   /* [RW] Timer7 Control Register */
#define BCHP_RF4CE_CPU_CTRL_TIMER7_COUNT             (base_addr + 0x8006c)   /* [RO] Timer7 Current Count */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DATA       (base_addr + 0x80070)   /* [RW] Mailbox FIFO Write/Read Data */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_DEPTH      (base_addr + 0x80074)   /* [RO] Mailbox FIFO Current Depth */
#define BCHP_RF4CE_CPU_CTRL_H2Z_MBOX_FIFO_RST_PTRS   (base_addr + 0x80078)   /* [RW] Mailbox FIFO Reset Pointers */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DATA       (base_addr + 0x8007c)   /* [RW] Mailbox FIFO Write/Read Data */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_DEPTH      (base_addr + 0x80080)   /* [RO] Mailbox FIFO Current Depth */
#define BCHP_RF4CE_CPU_CTRL_Z2H_MBOX_FIFO_RST_PTRS   (base_addr + 0x80084)   /* [RW] Mailbox FIFO Reset Pointers */
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM                 (base_addr + 0x80088)   /* [RW] HOST2ZIGBEE Mailbox Semaphore */
#define BCHP_RF4CE_CPU_CTRL_MBOX_SEM_TIMER           (base_addr + 0x8008c)   /* [RW] Mailbox Semaphore Timer Control Register */

#define BCHP_RF4CE_CPU_L2_CPU_STATUS                 (base_addr + 0x80300)   /* [RO] CPU interrupt Status Register */
#define BCHP_RF4CE_CPU_L2_CPU_SET                    (base_addr + 0x80304)   /* [WO] CPU interrupt Set Register */
#define BCHP_RF4CE_CPU_L2_CPU_CLEAR                  (base_addr + 0x80308)   /* [WO] CPU interrupt Clear Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_STATUS            (base_addr + 0x8030c)   /* [RO] CPU interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_SET               (base_addr + 0x80310)   /* [WO] CPU interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_L2_CPU_MASK_CLEAR             (base_addr + 0x80314)   /* [WO] CPU interrupt Mask Clear Register */

#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS0       (base_addr + 0x80500)   /* [RO] CPU interrupt Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_SET0          (base_addr + 0x80504)   /* [WO] CPU interrupt Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR0        (base_addr + 0x80508)   /* [WO] CPU interrupt Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_STATUS0  (base_addr + 0x8050c)   /* [RO] CPU interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET0     (base_addr + 0x80510)   /* [WO] CPU interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR0   (base_addr + 0x80514)   /* [WO] CPU interrupt Mask Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_STATUS1       (base_addr + 0x80518)   /* [RO] Host Interrupt Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_SET1          (base_addr + 0x8051c)   /* [WO] Host Interrupt Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_CLEAR1        (base_addr + 0x80520)   /* [WO] Host Interrupt Clear Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_STATUS1  (base_addr + 0x80524)   /* [RO] Host Interrupt Mask Status Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_SET1     (base_addr + 0x80528)   /* [WO] Host Interrupt Mask Set Register */
#define BCHP_RF4CE_CPU_HOST_STB_L2_CPU_MASK_CLEAR1   (base_addr + 0x8052c)   /* [WO] Host Interrupt Mask Clear Register */

#endif // #ifdef COMPILE_TIME
