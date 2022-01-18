/****************************************************************************
*
* Broadcom Proprietary and Confidential. (c) 2016 Broadcom.
* All rights reserved.
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
*
* Filename:       g2u_mbox.h
* Author:         Russell Enderby <russell.enderby@broadcom.com>
* Creation Date:  o6/o2/2o16
*
****************************************************************************
* Description: This file has all the mbox's in the g2u block defined and who
*              is using them.
*
****************************************************************************/
#ifndef BCM_G2U_MBOX_H
#define BCM_G2U_MBOX_H

/* The G2U block has 16 mailbox registers.  The config registers have locking
   properties that we can use as an atomic HW spinlock.  Various drivers use
   these spinlocks across a variety of processors including
   viper, zephyr, and arm.                                                 */
#define G2U_MBOX_CFG_0    0
#define G2U_MBOX_CFG_1    1
#define G2U_MBOX_CFG_2    2
#define G2U_MBOX_CFG_3    3
#define G2U_MBOX_CFG_4    4
#define G2U_MBOX_CFG_5    5
#define G2U_MBOX_CFG_6    6
#define G2U_MBOX_CFG_7    7
#define G2U_MBOX_CFG_8    8
#define G2U_MBOX_CFG_9    9
#define G2U_MBOX_CFG_10  10
#define G2U_MBOX_CFG_11  11
#define G2U_MBOX_CFG_12  12
#define G2U_MBOX_CFG_13  13  /* I2C spinlock          */
#define G2U_MBOX_CFG_14  14  /* PinCtrl-GPIO spinlock */
#define G2U_MBOX_CFG_15  15  /* SPI spinlock          */

#endif //BCM_G2U_MBOX_H
