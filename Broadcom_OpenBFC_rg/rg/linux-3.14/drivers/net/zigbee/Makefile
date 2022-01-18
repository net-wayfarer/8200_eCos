#############################################################################
# Broadcom Proprietary and Confidential. (c)2016 Broadcom. All rights reserved.
#
# This program is the proprietary software of Broadcom and/or its
# licensors, and may only be used, duplicated, modified or distributed pursuant
# to the terms and conditions of a separate, written license agreement executed
# between you and Broadcom (an "Authorized License").  Except as set forth in
# an Authorized License, Broadcom grants no license (express or implied), right
# to use, or waiver of any kind with respect to the Software, and Broadcom
# expressly reserves all rights in and to the Software and all intellectual
# property rights therein.  IF YOU HAVE NO AUTHORIZED LICENSE, THEN YOU
# HAVE NO RIGHT TO USE THIS SOFTWARE IN ANY WAY, AND SHOULD IMMEDIATELY
# NOTIFY BROADCOM AND DISCONTINUE ALL USE OF THE SOFTWARE.
#
# Except as expressly set forth in the Authorized License,
#
# 1. This program, including its structure, sequence and organization,
#    constitutes the valuable trade secrets of Broadcom, and you shall use all
#    reasonable efforts to protect the confidentiality thereof, and to use
#    this information only in connection with your use of Broadcom integrated
#    circuit products.
#
# 2. TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
#    AND WITH ALL FAULTS AND BROADCOM MAKES NO PROMISES, REPRESENTATIONS OR
#    WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT
#    TO THE SOFTWARE.  BROADCOM SPECIFICALLY DISCLAIMS ANY AND ALL IMPLIED
#    WARRANTIES OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A
#    PARTICULAR PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET
#    ENJOYMENT, QUIET POSSESSION OR CORRESPONDENCE TO DESCRIPTION. YOU ASSUME
#    THE ENTIRE RISK ARISING OUT OF USE OR PERFORMANCE OF THE SOFTWARE.
#
# 3. TO THE MAXIMUM EXTENT PERMITTED BY LAW, IN NO EVENT SHALL BROADCOM OR ITS
#    LICENSORS BE LIABLE FOR (i) CONSEQUENTIAL, INCIDENTAL, SPECIAL, INDIRECT,
#    OR EXEMPLARY DAMAGES WHATSOEVER ARISING OUT OF OR IN ANY WAY RELATING TO
#    YOUR USE OF OR INABILITY TO USE THE SOFTWARE EVEN IF BROADCOM HAS BEEN
#    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES; OR (ii) ANY AMOUNT IN EXCESS
#    OF THE AMOUNT ACTUALLY PAID FOR THE SOFTWARE ITSELF OR U.S. $1, WHICHEVER
#    IS GREATER. THESE LIMITATIONS SHALL APPLY NOTWITHSTANDING ANY FAILURE OF
#    ESSENTIAL PURPOSE OF ANY LIMITED REMEDY.
#
#############################################################################

ifeq ($(_REFSW_),)

ccflags-$(CONFIG_BCM3390A0) += -Iinclude/linux/brcmstb/3390a0
ccflags-$(CONFIG_BCM3390B0) += -Iinclude/linux/brcmstb/3390b0

obj-$(CONFIG_ZIGBEE_DRIVER) := zigbee_drv.o
zigbee_drv-objs := zigbee_driver.o

else  # _REFSW_

CWD := $(shell pwd)
B_REFSW_ROOT := ${CWD}/../../../..

ifneq ($(B_REFSW_VERBOSE),)
Q_:=
else
Q_:=@
endif

ifeq ($(PLATFORM),)
PLATFORM=$(NEXUS_PLATFORM)
endif
ifeq ($(PLATFORM),)
$(error You must define PLATFORM)
endif

ifeq ($(BCHP_CHIP),)
    ifeq ($(PLATFORM),93390)
        BCHP_CHIP := 3390
    endif

    ifeq ($(PLATFORM),97364)
        BCHP_CHIP := 7364
    endif

    ifeq ($(PLATFORM),97366)
        BCHP_CHIP := 7366
    endif
endif

ifeq ($(LINUX),)
LINUX = /opt/brcm/linux
endif

ifdef DEBUG
B_REFSW_DEBUG ?= $(DEBUG)
endif
ifeq ($(B_REFSW_DEBUG),)
B_REFSW_DEBUG=y
endif

ifdef B_REFSW_KERNEL_CROSS_COMPILE
CROSS_COMPILE := $(B_REFSW_KERNEL_CROSS_COMPILE)
endif

B_REFSW_OBJ_DIR ?= obj.${PLATFORM}
B_REFSW_OBJ_ROOT ?= ${B_REFSW_ROOT}/${B_REFSW_OBJ_DIR}
BCM_OBJ_ROOT := ${B_REFSW_OBJ_ROOT}/BSEAV/linux/driver/zigbee
BCM_OBJ_PATH := ${BCM_OBJ_ROOT}/$(B_REFSW_ARCH)

obj-m += zigbee_drv.o

BCHP_VER_LOWERCASE = $(shell awk 'BEGIN{print tolower("$(BCHP_VER)")}')
PWD := $(shell pwd)
ccflags-y += -I$(M)/../../../../../../magnum/basemodules/chp/include/$(BCHP_CHIP)/rdb/$(BCHP_VER_LOWERCASE) -DBCHP_CHIP=$(BCHP_CHIP)

ifeq ($(ANDROID_BUILD),y)
ccflags-y += -DB_REFSW_ANDROID
endif

ifeq ($(BYPASS_MODULE_LICENSE),y)
ccflags-y += -DBYPASS_MODULE_LICENSE
endif

ifeq ($(DYNAMIC_MAJOR_NUMBER),y)
ccflags-y += -DDYNAMIC_MAJOR_NUMBER
endif

zigbee_drv-objs := zigbee_driver.o

all: $(BCM_OBJ_PATH)
	${Q_}${MAKE} -C $(LINUX) M=$(BCM_OBJ_PATH) modules

clean: $(BCM_OBJ_PATH)
	${Q_}${MAKE} -C $(LINUX) M=$(BCM_OBJ_PATH) clean
	${Q_}rm $(BCM_OBJ_PATH)/*.c $(BCM_OBJ_PATH)/*.h $(BCM_OBJ_PATH)/Makefile

.PHONY: $(BCM_OBJ_PATH)

$(BCM_OBJ_PATH)/exists:
	${Q_}mkdir -p $@

$(BCM_OBJ_PATH): $(BCM_OBJ_PATH)/exists
	${Q_}cp -f * $@

endif # _REFSW_
