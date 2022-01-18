#
#	Makefile -- Master makefile for all libraries.
#

.EXPORT_ALL_VARIABLES:

#
# Include architecture specific build rules.
#

ifndef ROOTDIR
ROOTDIR=..
endif

UCLINUX_BUILD_LIB=1
include $(LINUX_CONFIG)
include $(CONFIG_CONFIG)
include $(ARCH_CONFIG)

#
# always build these dir_
#
dir_y  = $(LIBCDIR)
dir_n  =
dir_   =

ifeq ($(LIBCDIR),libc)
dir_y += libm
endif

#
# and build these if someone needs them
#
dir_$(CONFIG_LIB_NCURSES)        += ncurses
dir_$(CONFIG_LIB_NCURSES_FORCE)  += ncurses
dir_$(CONFIG_LIB_POPT)           += popt

ifneq ($(INITRD_SUBDIR),) 
ifneq ($(shell echo $(filter lib%,$(INITRD_SUBDIR))|sed -e 's/lib[\/]/lib/'),lib)
dir_y:=$(patsubst lib/%,%,$(filter lib/%,$(INITRD_SUBDIR)))
endif
endif

all:
ifeq ($(LIBCDIR),glibc/build)
	[ -d glibc/build ] || mkdir glibc/build
	if [ ! -f glibc/build/config.cache ]; then \
		cd glibc/build; \
		export CFLAGS="$(VENDOR_CFLAGS) -DEMBED -O2"; \
		unset LDFLAGS; \
		../configure  --prefix= \
			$(GLIBC_TARGET_OPTS) --build=i386-redhat-linux \
			--with-headers=$(ROOTDIR)/$(LINUXDIR)/include \
			--without-cvs --without-gd \
			--disable-profile --disable-force-install \
			--enable-add-ons ; \
	fi
endif
	@for i in $(dir_y); do \
		echo "--> making all in lib/$$i"; \
		$(MAKE) -C $$i || exit $$?; \
	done

ifeq ($(INITRD_SUBDIR),) 
ifeq ($(findstring y,$(CONFIG_LIB_LIBSTDCXX_FORCE)$(CONFIG_LIB_LIBSTDCXX)),y)
	-$(RM) -f $(ROMFSDIR)/lib/libstdc++.*
	cp -af $(GCCLIB)/libstdc++.so $(GCCLIB)/libstdc++.so*[0-9] \
		$(ROMFSDIR)/lib/
	$(STRIP) $(ROMFSDIR)/lib/libstdc++.so
endif

ifeq ($(findstring y,$(CONFIG_LIB_LIBUTIL_FORCE)$(CONFIG_LIB_LIBUTIL)),y)
	-$(RM) -f $(ROMFSDIR)/lib/libutil.so*
	cp -af $(GCCSYSROOT)/lib/libutil* $(ROMFSDIR)/lib/
	$(STRIP) $(ROMFSDIR)/lib/libutil.so.[0-9]
endif
ifeq ($(findstring y,$(CONFIG_LIB_LIBCRYPT_FORCE)$(CONFIG_LIB_LIBCRYPT)),y)
	-$(RM) -f $(ROMFSDIR)/lib/libcrypt.so*
	cp -af $(GCCSYSROOT)/lib/libcrypt* $(ROMFSDIR)/lib/
	$(STRIP) $(ROMFSDIR)/lib/libcrypt.so.[0-9]
endif
endif

clean:
ifeq ($(LIBCDIR),glibc/build)
	rm -rf glibc/build
endif
	-for i in $(dir_y) $(dir_n) $(dir_); do \
		$(MAKE) -C $$i clean ; \
	done

distclean:
	-for i in $(dir_y) $(dir_n) $(dir_); do \
		$(MAKE) -C $$i distclean ; \
	done
	$(MAKE) -C uClibc distclean
