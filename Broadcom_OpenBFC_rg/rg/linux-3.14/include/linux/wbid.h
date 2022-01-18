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
* Filename:      wbid.h
* Author:        Russell Enderby <russell.enderby@broadcom.com>
* Creation Date: o3/24/2o16
*
****************************************************************************
* Description: This driver is an i2c device client used for accessing the
*		WiFi Board Identifier (WBID).
*
****************************************************************************/
#ifndef __LINUX_WBID_H_INCLUDED
#define __LINUX_WBID_H_INCLUDED
#include <linux/sched.h>  /* for s32 */

/* WBID API read function.  Call this from kernel space to read 16 bits
   of data from the WBID. */
u32 wbid_read(void);
#endif
