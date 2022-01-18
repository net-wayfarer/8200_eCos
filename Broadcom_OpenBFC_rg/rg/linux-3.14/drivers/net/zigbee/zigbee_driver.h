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
#ifndef _ZIGBEE_DRIVER_H_
#define _ZIGBEE_DRIVER_H_

#include <linux/ioctl.h>

struct fw {
    unsigned char *pImage;
    unsigned int size_in_bytes;
};

#define ZIGBEE_IOCTL_COPY_TO_KERNEL         _IOWR(0,0,unsigned int)
#define ZIGBEE_IOCTL_COPY_FROM_KERNEL       _IOWR(0,1,unsigned int)
#define ZIGBEE_IOCTL_WAIT_FOR_INTERRUPTS    _IOWR(0,2,unsigned int)
#define ZIGBEE_IOCTL_READ_FROM_MBOX         _IOWR(0,3,unsigned int)
#define ZIGBEE_IOCTL_WRITE_TO_MBOX          _IOWR(0,4,unsigned int)
#define ZIGBEE_IOCTL_START                  _IOWR(0,5,struct fw)
#define ZIGBEE_IOCTL_STOP                   _IOWR(0,6,unsigned int)
#define ZIGBEE_IOCTL_WAIT_FOR_WDT_INTERRUPT _IOWR(0,7,unsigned int)
#define ZIGBEE_IOCTL_GET_RF4CE_MAC_ADDR     _IOWR(0,8,unsigned int)
#define ZIGBEE_IOCTL_GET_ZBPRO_MAC_ADDR     _IOWR(0,9,unsigned int)

#endif /*_ZIGBEE_DRIVER_H_*/
