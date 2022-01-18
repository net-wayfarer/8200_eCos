#
# STB Linux build system v2.2
# Copyright (C) 2011 Broadcom Corporation
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

# hostname of TFTP server (defaults to the build machine)
TFTPHOST	:= $(shell hostname)

# root directory of TFTP server
TFTPBOOT	:= /tftpboot

# full (local) TFTP path, where "make install" will copy the images.
# Typical usage: user overrides TFTPDIR on the command line, and
# this Makefile will derive the correct TFTPPATH from TFTPDIR/TFTPBOOT
TFTPDIR		?= $(TFTPBOOT)/$(USER)

# relative (remote) TFTP path under $(TFTPBOOT), for use by the TFTP client
TFTPPATH	:= $(subst $(TFTPBOOT)/,,$(TFTPDIR))


SHELL := /bin/bash
CUR_WORK_DIR = $(shell pwd)

LINUX_CONFIG ?= $(CUR_WORK_DIR)/../linux/.config
CONFIG_CONFIG ?= $(CUR_WORK_DIR)/config/.config
ARCH_CONFIG ?= $(CUR_WORK_DIR)/../config.arch
ROOTDIR ?= $(CUR_WORK_DIR)
BLDROOT = $(CUR_WORK_DIR)

export BLDROOT ROOTDIR ARCH_CONFIG CONFIG_CONFIG LINUX_CONFIG

.PHONY: rootfs
rootfs: 
	# build rootfs contents and install to romfs/
	$(MAKE) -C lib all
	$(MAKE) -C user all

.IGNORE: clean_rootfs distclean_rootfs
.PHONY: clean_rootfs
clean_rootfs:
	$(MAKE) -C lib clean
	$(MAKE) -C user clean
	if [ -d user/root_sign ]; then \
		$(MAKE) -C user/root_sign clean; \
	fi ;\

.PHONY: distclean_rootfs
distclean_rootfs:
	$(MAKE) -C lib distclean clean
	$(MAKE) -C user distclean clean
	$(MAKE) -C config clean
	if [ -d user/root_sign ]; then \
		$(MAKE) -C user/root_sign clean; \
	fi ;\
	rm -f config.arch
	rm -f version


# These two target below are just temporary so that the build can be still
# invoked from rootfs dir as some build scripts expect. Both of these targets
# will be removed in the near future.
.PHONY: all
all:
	@echo ""
	@echo "Invoking \"all\" target in rootfs directory is depricated now and it"
	@echo "will be supported just for a short time to keep some build scripts happy."
	@echo ""
	@echo "!!! Please invoke this target from one directory up !!!"
	@echo ""
	@sleep 5
	$(MAKE) -C .. all
	ln -sf ../images/ .

.PHONY: defaults-%
defaults-%:
	@echo ""
	@echo "Invoking \"defaults-$*\" target in rootfs directory is depricated now and it"
	@echo "will be supported just for a short time to keep some build scripts happy."
	@echo ""
	@echo "!!! Please invoke this target from one directory up !!!"
	@echo ""
	@sleep 5
	$(MAKE) -C .. defaults-$*

.PHONY: clean
clean:
	$(MAKE) -C .. clean

.PHONY: distclean
distclean:
	$(MAKE) -C .. distclean
