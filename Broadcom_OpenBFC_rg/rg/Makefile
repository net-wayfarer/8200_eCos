#
# Broadcom Proprietary and Confidential. (c) 2016 Broadcom.  All rights reserved.
# The term "Broadcom" refers to Broadcom Limited and/or its subsidiaries.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
#


BLDTOPDIR	:= $(shell pwd)
ROOTDIR		:= $(BLDTOPDIR)/rootfs

SAVEVARS	:= PATH SHELL HOME USER MFLAGS MAKEFLAGS MAKELEVEL \
		MAKEOVERRIDES TERM DISPLAY CCACHE_DIR V VERBOSE
unexport $(shell perl -w $(ROOTDIR)/bin/cleanenv.pl $(SAVEVARS))

# don't pass command line options like SUBDIRS
MAKEOVERRIDES	:=

LINUXDIR        := $(BLDTOPDIR)/linux
RGTOOLSDIR      := $(ROOTDIR)/rg_tools
RGAPPSDIR       := $(BLDTOPDIR)/rg_apps
IMAGEPATH       ?= $(BLDTOPDIR)/images
IMAGESDIR       := $(IMAGEPATH)
LINUX_CONFIG    := $(LINUXDIR)/.config
CONFIG_CONFIG   := $(ROOTDIR)/config/.config
ARCH_CONFIG     := $(BLDTOPDIR)/config.arch
ROMFSDIR        := $(ROOTDIR)/romfs
ROMFSINST       := $(ROOTDIR)/bin/romfs-inst.sh
EXTMODDIR       := $(RGAPPSDIR)/bcmdrivers/broadcom
DEFKERNELDIR    := linux-3.14
RUNNER          ?= $(EXTMODDIR)/runner/rdp_cable.tar.gz

ifneq ($(shell which rsync),)
COPY_HELPER	:= rsync -qa
else
COPY_HELPER	:= cp -f
endif

SHELL		:= /bin/bash
CFG		:= perl -w bin/config.pl

ifneq ($(strip $(MAX_JOBS)),)
  ACTUAL_MAX_JOBS := $(MAX_JOBS)
else
  NUM_CORES := $(shell grep processor /proc/cpuinfo | wc -l)
  ACTUAL_MAX_JOBS := $(NUM_CORES)
endif

# list of targets that can be run without "make defaults-<chip>" first
NON_BUILD_TARGET = false
NON_BUILD_TARGETS = "help"
ifneq ($(findstring $(MAKECMDGOALS), $(NON_BUILD_TARGETS)),)
  NON_BUILD_TARGET = true
endif
ifneq ($(findstring clean, $(MAKECMDGOALS)),)
  NON_BUILD_TARGET = true
endif
ifneq ($(findstring openwrt_arch, $(MAKECMDGOALS)),)
  NON_BUILD_TARGET = true
endif
ifneq ($(findstring setversion-, $(MAKECMDGOALS)),)
  NON_BUILD_TARGET = true
endif
ifneq ($(findstring setkernel-, $(MAKECMDGOALS)),)
  NON_BUILD_TARGET = true
endif

# Set the default kernel build directory if not previously set
ifeq (,$(wildcard $(LINUXDIR)))
  $(shell ln -sfT "$(DEFKERNELDIR)" "linux" || exit 2)
endif

MIPS_TOP_PLATS = "3384"
ARM_TOP_PLATS = "7145 3390"
ifeq ($(wildcard .target),)
  PLAT = $(patsubst defaults-%,%,$(filter defaults-%,$(MAKECMDGOALS)))
  TOP_PLAT = $(shell echo $(PLAT) | cut -c1-4)
  ifneq (,$(findstring $(TOP_PLAT),$(MIPS_TOP_PLATS)))
    ARCH = mips
  else ifneq (,$(findstring $(TOP_PLAT),$(ARM_TOP_PLATS)))
    ARCH = arm
  endif
else
  PLAT = $(shell cat .target)
  ARCH = $(shell cat .arch)
endif

TOOLCHAINS_DIR ?= /opt/toolchains
TOOLCHAIN_VERSION = $(shell cat toolchain)
ifeq ($(ARCH), mips)
  CROSS_COMPILE_PREFIX := $(ARCH)-linux-gnu-
else ifeq ($(ARCH), arm)
  CROSS_COMPILE_PREFIX := $(ARCH)-linux-gnueabihf-
endif
COMPILER_PATH = $(shell dirname $(shell which $(CROSS_COMPILE_PREFIX)gcc 2> /dev/null) 2> /dev/null)
TOOLCHAIN_TOP ?=  $(subst /bin,,$(COMPILER_PATH))
ifeq ($(TOOLCHAIN_TOP),)
  TOOLCHAIN_TOP = $(TOOLCHAINS_DIR)/$(TOOLCHAIN_VERSION)
endif
ifeq ($(COMPILER_PATH),)
  PATH := $(TOOLCHAIN_TOP)/bin:$(PATH)
endif
CROSS_COMPILE := $(TOOLCHAIN_TOP)/bin/$(CROSS_COMPILE_PREFIX)

ifeq ($(findstring $(TOOLCHAIN_VERSION),$(TOOLCHAIN_TOP)),)
  ifeq ($(NON_BUILD_TARGET), false)
  $(info ****************************************************************)
  $(warning $(TOOLCHAIN_TOP) contains non-standard toolchain version.)
  $(warning Expected toolchain version: $(TOOLCHAIN_VERSION))
  $(info ****************************************************************)
  endif
endif

export ROOTDIR LINUXDIR LINUX_CONFIG CONFIG_CONFIG ARCH_CONFIG ROMFSDIR NOLOGIN IMAGEPATH IMAGESDIR \
	BLDTOPDIR ROMFSINST SHELL PATH ACTUAL_MAX_JOBS \
	TOOLCHAIN_TOP ARCH CROSS_COMPILE WIFIOPTS REAL_PLAT

REAL_PLAT := $(subst -cms,,$(subst -lattice,,$(subst -mixedap,,$(subst -nicap,,$(subst -pon,,$(subst -cert,,$(subst -bridge,,$(subst -wifi,,$(PLAT)))))))))
ifeq ($(REAL_PLAT),7145a0)
  RGPROFILE = 93385MWVG
else ifeq ($(REAL_PLAT),7145b0)
  RGPROFILE = 93385MWVG
else ifeq ($(REAL_PLAT),7445d0)
  RGPROFILE = 97445GW
else ifeq ($(REAL_PLAT),3390a0)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3390a2)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3390a2-perf)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3390b0)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3390b0-perf)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3390a0-dcm)
  RGPROFILE = 93390DCM
else ifeq ($(REAL_PLAT),3390a2-dcm)
  RGPROFILE = 93390DCM
else ifeq ($(REAL_PLAT),3390b0-dcm)
  RGPROFILE = 93390DCM
else ifeq ($(REAL_PLAT),3390b0-testnetxl)
  RGPROFILE = 93390MWVG
else ifeq ($(REAL_PLAT),3384b0)
  RGPROFILE = 93384
else ifeq ($(REAL_PLAT),3384b0-perf)
  RGPROFILE = 93384
else ifeq ($(REAL_PLAT),33843b0)
  RGPROFILE = 93384
else ifeq ($(REAL_PLAT),33843b0-perf)
  RGPROFILE = 93384
else
  ifeq ($(NON_BUILD_TARGET), false)
    $(error "Unknown platform: $(REAL_PLAT)")
  endif
endif

PREVERSION	:= $(shell make -C $(LINUXDIR) -s --no-print-directory kernelversion | grep -v "make" > version)
VERSION 	:= $(shell cat version 2>/dev/null)

MODULE_OPTS	:= INSTALL_MOD_STRIP=1 INSTALL_MOD_PATH=$(ROOTDIR)/romfs
MAKEOPTS	:= TFTPHOST=$(TFTPHOST) TFTPDIR=$(TFTPDIR) TFTPPATH=$(TFTPPATH)

print_start_banner = @echo -e "\n=== Making $@ `date +%H:%M` start ==="
print_stop_banner = @echo -e "\n=== Making $@ `date +%H:%M` finish ==="

RGOPTS = BRCM_CABLE=y PROFILE=$(RGPROFILE) PLAT=$(PLAT) LINUX_VER_STR=$(VERSION) $(WIFIOPTS)

ifneq (,$(findstring 3384,$(REAL_PLAT)))
  include Makefile.3384
else ifneq (,$(findstring 3390,$(REAL_PLAT)))
  include Makefile.3390
endif

ifneq (,$(findstring 4.1,$(VERSION)))
	KSUFFIX:=-4.1
endif

include Makefile.extra_targets
#########################################
# primary targets
#########################################
.PHONY: all parallel_kernel_steps
all:
	$(MAKE) -j $(ACTUAL_MAX_JOBS) parallel_kernel_steps

parallel_kernel_steps:
	$(MAKE) -j $(ACTUAL_MAX_JOBS) initrd_kernel
	$(MAKE) -j $(ACTUAL_MAX_JOBS) kernel_img

#########################################
# kernel, modules, and device tree blob
#########################################
# Unfortunately zImage is not a valid target for linux mips build. Thus kernel
# buid must use vmlinux target for 3384 and zImage target 3390.
.PHONY: kernel
kernel:
	$(call print_start_banner)
	$(MAKE) -j $(ACTUAL_MAX_JOBS) -C $(LINUXDIR) ARCH=$(ARCH) $(KERNEL_BUILD_TARGET)
	$(call print_stop_banner)

# TO DO check if there is really dependency between "kernel_modules" and
# "extmodules".
.PHONY: kernel_mods
kernel_mods_deps = kernel
kernel_mods: $(kernel_mods_deps)
	$(call print_start_banner)
	$(MAKE) -j $(ACTUAL_MAX_JOBS) -C $(LINUXDIR) ARCH=$(ARCH) $(MODULE_OPTS) modules
	$(MAKE) -j $(ACTUAL_MAX_JOBS) -C $(LINUXDIR) ARCH=$(ARCH) $(MODULE_OPTS) modules_install
	# TO DO remove depmod if possible. The line above should take care of
	# it.
	/sbin/depmod -b $(ROMFSDIR) $(VERSION)
	$(call print_stop_banner)


#############################################
# userspace is composed of rootfs and rgapps
#############################################
.PHONY: rootfs
rootfs:
	$(call print_start_banner)
	$(MAKE) -C rootfs KVER=$(VERSION) PLAT=$(PLAT) rootfs
	$(call print_stop_banner)


######################
# configuration
######################
.PHONY: silentoldconfig
silentoldconfig:
	$(MAKE) -C $(LINUXDIR) ARCH=$(ARCH) silentoldconfig
	$(MAKE) -C $(LINUXDIR) ARCH=$(ARCH) prepare scripts
	$(MAKE) -C $(ROOTDIR)/lib/uClibc oldconfig
	$(MAKE) -C $(ROOTDIR)/user/busybox silentoldconfig
	$(MAKE) -C $(ROOTDIR)/config oldconfig

.PHONY: setkernel-%
setkernel-%:
	@if [ -L linux ]; then \
	  if [ `readlink linux` != "linux-$*" ]; then \
	    echo "Setting kernel build directory to: linux-$*"; \
	    echo "Perform a \"make distclean_nokernel\" to avoid userspace incompatibilities"; \
	    ln -sfT linux-$* linux; \
	    rm -f version; \
	  else \
	    echo "Kernel build directory already set to linux-$*"; \
	  fi \
	else \
	  echo "Refusing to set kernel build directory: linux should be symbolic link"; \
	  exit 2; \
	fi

ifneq ($(findstring setkernel-3.14, $(MAKECMDGOALS)),)
  SETKERNEL=setkernel-3.14
endif
ifneq ($(findstring setkernel-4.1, $(MAKECMDGOALS)),)
  SETKERNEL=setkernel-4.1
endif


.PHONY: defaults-% openwrt_arch-%
defaults-%: |$(SETKERNEL)
	@mkdir -p $(IMAGESDIR)
	@touch .dirty
	@chmod -R 0755 $(ROOTDIR)/config/scripts/{Configure,Menuconfig} $(ROOTDIR)/bin
	$(CFG) defaults $*
	# TO DO remove the two symlinks below
	@cd $(ROOTDIR); ln -fs ../config.arch .; ln -fs ../version .
	$(MAKE) silentoldconfig
	@echo "***************************************************************"
	@echo "* Build configuration"
	@echo "*"
	@echo "* Target:                  $*"
	@echo "* Version:                 $(VERSION)"
	@echo "* TFTP host:               $(TFTPHOST)"
	@echo "* Remote TFTP path:        $(TFTPPATH)"
	@echo "* Local TFTP path:         $(TFTPDIR)"
	@echo "* ARCH:                    $(ARCH)"
	@echo "* Make Command line:       make $(MAKEFLAGS) $(MAKECMDGOALS)"
	@echo "* Make Version:            `make -v | sed -n 1p`"
	@echo "* Toolchain:               $(TOOLCHAIN_TOP)"
	@echo "***************************************************************"
	@echo ""

openwrt_arch-%:
	$(CFG) openwrt_arch $*

.PHONY: reporevs
REPOREVS_FILE     := $(RGAPPS_INSTDIR)/etc/repo_revs.txt
reporevs_deps = rgapps
reporevs: $(reporevs_deps)
	@echo -e "=== Repository revisions ==="
	@$(RGTOOLSDIR)/repo_revs.sh > $(REPOREVS_FILE)
	@echo


#############
# cleaning
#############
CLEANLOG = clean.log
CLEANOPTS ?= >> $(CLEANLOG) 2>&1

.PHONY: clean \
	rootfs_clean kernel_clean  
.IGNORE: clean \
	rootfs_clean kernel_clean 

clean:  rootfs_clean \
	kernel_clean 
	rm -f $(BLDTOPDIR)/version
	rm -rf $(BLDTOPDIR)/tmp

rootfs_clean:
	$(MAKE) -C $(ROOTDIR) clean_rootfs

kernel_clean:
	$(MAKE) -C $(LINUXDIR) ARCH=$(ARCH) clean

##############################################
# distclean: clean and remove configuration
##############################################
.PHONY: distclean rootfs_distclean kernel_distclean
.IGNORE: distclean rootfs_distclean kernel_distclean

rootfs_distclean:
	$(MAKE) -C $(ROOTDIR) distclean_rootfs

kernel_distclean:
	$(MAKE) -C $(LINUXDIR) ARCH=$(ARCH) distclean

.PHONY: distclean distclean_nokernel
distclean:
	rm -f $(CLEANLOG)
	$(MAKE) rootfs_distclean \
	kernel_distclean $(CLEANOPTS)
	rm -f .target .arch .dirty config.arch version $(CLEANOPTS)

distclean_nokernel:
	rm -f $(CLEANLOG)
	$(MAKE) rootfs_distclean \
	$(CLEANOPTS)
	rm -f .target .arch .dirty config.arch version $(CLEANOPTS)

#############
# help
#############
.PHONY: help
help:
	@echo ""
	@echo "Primary build targets:"
	@echo ""
	@echo "defaults-<target>          - Install default config files for <target>"
	@echo "                             must be executed before invoking \"all\""
	@echo "IMAGEPATH=<dir>            - Image path for the output images"
	@echo "RUNNER=<path_to_tarball>   - Full path to runner .tgz file"
	@echo ""
	@echo "all                        - Build all"
	@echo ""
	@echo "clean                      - Clean objects but not configurations"
	@echo "distclean                  - Like clean but also cleans configuration"
	@echo "distclean_nokernel         - Like distclean but does not clean kernel"
	@echo ""
	@echo "Secondary build targets:"
	@echo ""
	@echo "help                       - display this help text"
	@echo ""
	@echo "menuconfig-linux           - Customize Linux config"
	@echo "menuconfig-vendor          - Customize user/lib package config"
	@echo "menuconfig-busybox         - Customize busybox config"
	@echo "save_defaults              - Overwrite default config with current settings"
	@echo ""
	@echo "initrd_kernel              - Build initramfs bootable kernel image for"
	@echo "                             current target"
	@echo "kernel_img                 - Build non-initramfs bootable kernel image"
	@echo "                             for current target"
	@echo "userspace_img                 - Build userspace image for current target"
	@echo ""
	@echo "rootfs                     - Populates romfs/"
	@echo ""
	@echo "wifi                       - Build the WiFi drivers and apps/"
	@echo "wifi_force                 - Force a rebuild of the WiFi code - driver and apps."
	@echo "                             This is needed because once the tarball of precompiled"
	@echo "                             WiFi is created it is always used."
	@echo ""
	@echo "rgapps                     - build everything under rg_apps, but not the kernel.  Does not create images."
	@echo "rgapps-<dir>-<dir>         - build a subset of rgapps. E.g. \"make rgapps-userspace-public-apps\" "
	@echo ""
	@echo "Examples:"
	@echo ""
	@echo " make defaults-3384b0      - Create default configuration for 3384"
	@echo " make defaults-33390a0 all - Create default configuration for 3390 and build"
	@echo "                             all 3390 images"
	@echo " make defaults-33390a0 all IMAGEPATH=/tftpboot/images - Build all 3390 images and put them in /tftpboot/images"
	@echo " make MAX_JOBS=1 all       - build everything with parallel execution disabled"
	@echo "                             (single command at a time)"
	@echo " make kernel_img_ND        - Create kernel image without running any dependencies; useful for development"
	@echo ""
	@echo "NOTE: \"<target>_deps=<prerequisites>\" can be added to make command line for any"
	@echo "      top-level <target> to change this target's <prerequisites>. If <prerequisites> is"
	@echo "      an empty string then all prerequisites (dependencies) of this target will"
	@echo "      be skipped"
	@echo "      \"<target>_ND\" can be used with top-level targets to build without building dependencies first."
	@echo "      E.g. \"make wifi_mods_ND\" to build the wifi driver without building kernel first"
	@echo ""
	@echo "WARNING: The build is parallel by default. It will use -j x option where"
	@echo "         x is number of build machine cores. If you want to disable"
	@echo "         parallel build set MAX_JOBS=1 on the build command line"
	@echo "         Example: make MAX_JOBS=1 all"
	@echo ""
