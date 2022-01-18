#
#	Makefile -- Build instructions for user level apps
#

CURRENT_MAKEFILE_LIST := $(MAKEFILE_LIST)

.EXPORT_ALL_VARIABLES:

#
# Include architecture specific build rules.
#

ROOTDIR ?= $(dir $(abspath $(lastword $(CURRENT_MAKEFILE_LIST))))..

UCLINUX_BUILD_USER=1
-include $(LINUX_CONFIG)
#-include $(CONFIG_CONFIG)
-include $(ARCH_CONFIG)
-include $(MODULES_CONFIG)

VEND=$(ROOTDIR)/vendors

#
# must run the vendor build first
#
# dir_v = $(VEND)/$(CONFIG_VENDOR)/$(CONFIG_PRODUCT)/.

# -include $(dir_v)/config.vendor-2.6.x
ifeq ($(BUILD_ROOT_SIGN), y)
CONFIG_USER_ROOT_SIGN=y
else
-include $(ROOTDIR)/config/.config
endif

dir_v =

dir_y =
dir_n =
dir_  =

dir_$(CONFIG_USER_BASH_BASH)                += bash
dir_$(CONFIG_USER_WDMD)                     += wdmd
dir_$(CONFIG_USER_BUSYBOX_BUSYBOX)          += busybox
dir_$(CONFIG_USER_DNSMASQ_DNSMASQ)          += dnsmasq
dir_$(CONFIG_USER_E2FSPROGS_E2FSCK_E2FSCK)  += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_MKE2FS)    += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_BADBLOCKS) += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_CHATTR)    += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_DUMPE2FS)  += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_FSCK)      += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_LSATTR)    += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_MKLOST_FOUND) += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_TUNE2FS)   += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_RESIZE_RESIZE2FS) += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_UUIDGEN)   += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_MISC_LIBUUID)   += e2fsprogs
dir_$(CONFIG_USER_E2FSPROGS_DEBUGFS_DEBUGFS) += e2fsprogs
dir_$(CONFIG_USER_ETHTOOL)                  += ethtool
dir_$(CONFIG_USER_IFENSLAVE)                += ifenslave
dir_$(CONFIG_USER_GPTFDISK_GDISK)           += gptfdisk
dir_$(CONFIG_USER_GPTFDISK_SGDISK)          += gptfdisk
dir_$(CONFIG_USER_I2C_TOOLS)                += i2c-tools
dir_$(CONFIG_USER_IPTABLES_IPTABLES)        += iptables
dir_$(CONFIG_USER_MTDUTILS)                 += mtdutils
dir_$(CONFIG_USER_PROCPS_FREE)              += procps
dir_$(CONFIG_USER_PROCPS_KILL)              += procps
dir_$(CONFIG_USER_PROCPS_PGREP)             += procps
dir_$(CONFIG_USER_PROCPS_PKILL)             += procps
dir_$(CONFIG_USER_PROCPS_PS)                += procps
dir_$(CONFIG_USER_PROCPS_SKILL)             += procps
dir_$(CONFIG_USER_PROCPS_SNICE)             += procps
dir_$(CONFIG_USER_PROCPS_SYSCTL)            += procps
dir_$(CONFIG_USER_PROCPS_TLOAD)             += procps
dir_$(CONFIG_USER_PROCPS_TOP)               += procps
dir_$(CONFIG_USER_PROCPS_UPTIME)            += procps
dir_$(CONFIG_USER_PROCPS_VMSTAT)            += procps
dir_$(CONFIG_USER_PROCPS_W)                 += procps
dir_$(CONFIG_USER_PROCPS_WATCH)             += procps

ifneq ($(INITRD_SUBDIR),) 
ifneq ($(shell echo $(filter user%,$(INITRD_SUBDIR))|sed -e 's/user[\/]/user/'),user)
# dir_v:=$(subst vendors,$(VEND),$(findstring vendors/$(CONFIG_VENDOR)/$(CONFIG_PRODUCT),$(INITRD_SUBDIR)))
dir_y:=$(patsubst user/%,%,$(filter user/%,$(INITRD_SUBDIR)))
endif
endif

all:
	@for i in $(dir_v) $(sort $(dir_y)); do \
		echo "--> making all in user/$$i"; \
		$(MAKE) -C $$i || exit $$?; \
	done

clean:
	-for i in $(dir_v) $(sort $(dir_y) $(dir_n) $(dir_) $(dir_clean) ); do \
		[ ! -d $$i ] || $(MAKE) -C $$i clean; \
	done

distclean:
	-for i in $(dir_v) $(sort $(dir_y) $(dir_n) $(dir_)); do \
		[ ! -d $$i ] || $(MAKE) -C $$i distclean; \
	done

prune:
	-for i in $(sort $(dir_n) $(dir_)) ; do \
		found=0; \
		for j in $(sort $(dir_y)) ; do \
			if [ $$i == $$j ]; then \
				found=1; \
			fi; \
		done; \
		if [ $$found == 0 ]; then \
			rm -r $$i; \
		fi; \
	done

%::
	$(MAKE) -C $* all || exit 1
