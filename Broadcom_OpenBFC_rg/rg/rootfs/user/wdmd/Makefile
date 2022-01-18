include $(ROOTDIR)/config.arch

CMD_TARGET = wdmd
LIB_TARGET = libwdmd
HEADER_TARGET = wdmd.h
TEST_TARGET = wdmd_client
TASKS_TARGET = wdmd_tasks
TASKS_TARGET_LATTICE = wdmd_tasks.lattice

SOMAJOR=1
SOMINOR=0
SHLIB_TARGET = $(LIB_TARGET).so.$(SOMAJOR).$(SOMINOR)


CMD_SOURCE = main.c wdmd_task.c wdmd_sock.c

LIB_SOURCE = client.c wdmd_sock.c

TEST_SOURCE = wdmd_client.c

CFLAGS += -I. -L. -Wall \
	-D_GNU_SOURCE -g

CMD_LDFLAGS += -Wl,-z,now -Wl,-z,relro -pie

CMD_LDADD += -lwdmd

LIB_LDFLAGS += -Wl,-z,relro -pie

TEST_LDFLAGS = -lwdmd

#Add $(TEST_TARGET) to compile wdmd_client
.PHONY: all
all: $(SHLIB_TARGET) $(CMD_TARGET)

$(SHLIB_TARGET): $(LIB_SOURCE)
	$(CC) $(CFLAGS) $(LIB_LDFLAGS) -shared -fPIC -o $@ -Wl,-soname=$(LIB_TARGET).so.$(SOMAJOR) $^
	ln -sf $(SHLIB_TARGET) $(LIB_TARGET).so
	ln -sf $(SHLIB_TARGET) $(LIB_TARGET).so.$(SOMAJOR)

$(CMD_TARGET): $(SHLIB_TARGET) $(CMD_SOURCE)
	$(CC) $(CFLAGS) $(CMD_LDFLAGS) $(CMD_SOURCE) $(CMD_LDADD) -fPIC -o $@ -L.

$(TEST_TARGET): $(SHLIB_TARGET) $(TEST_SOURCE)
	$(CC) $(CFLAGS) $(TEST_LDFLAGS) $(TEST_SOURCE) $(CMD_LDADD) -fPIC -o $@ -L.

BINDIR=/bin
LIBDIR=/lib
ETCDIR=/etc

.PHONY: clean distclean romfs
clean:
	rm -f *.o *.so *.so.* $(CMD_TARGET) $(TEST_TARGET)

distclean: clean

romfs:
	$(ROMFSINST) -e CONFIG_USER_WDMD $(CMD_TARGET) $(BINDIR)/$(CMD_TARGET)
	$(ROMFSINST) -e CONFIG_USER_WDMD $(SHLIB_TARGET) $(LIBDIR)/$(SHLIB_TARGET)
	$(ROMFSINST) -e CONFIG_USER_WDMD $(LIB_TARGET).so $(LIBDIR)/$(LIB_TARGET).so
	$(ROMFSINST) -e CONFIG_USER_WDMD $(LIB_TARGET).so.$(SOMAJOR) $(LIBDIR)/$(LIB_TARGET).so.$(SOMAJOR)
ifneq (,$(findstring -lattice,$(PLAT)))
	$(ROMFSINST) -e CONFIG_USER_WDMD $(TASKS_TARGET_LATTICE) $(ETCDIR)/$(TASKS_TARGET)
else
	$(ROMFSINST) -e CONFIG_USER_WDMD $(TASKS_TARGET) $(ETCDIR)/$(TASKS_TARGET)
endif
	#$(ROMFSINST) -e CONFIG_USER_WDMD $(TEST_TARGET) $(BINDIR)/$(TEST_TARGET)
