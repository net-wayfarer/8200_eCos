# scripts/dtc makefile

hostprogs-y	:= dtc fdtdump fdtget fdtput
always		:= $(hostprogs-y)

dtc-objs	:= dtc.o flattree.o fstree.o data.o livetree.o treesource.o \
		   srcpos.o checks.o util.o
dtc-objs	+= dtc-lexer.lex.o dtc-parser.tab.o

LIBFDT_objdir 	:= libfdt
include $(src)/$(LIBFDT_objdir)/Makefile.libfdt
LIBFDT_objs	:= $(addprefix $(LIBFDT_objdir)/,$(LIBFDT_OBJS))

fdtdump-objs	:= fdtdump.o util.o $(LIBFDT_objs)
fdtget-objs	:= fdtget.o util.o $(LIBFDT_objs)
fdtput-objs	:= fdtput.o util.o $(LIBFDT_objs)

# Source files need to get at the userspace version of libfdt_env.h to compile
HOST_EXTRACFLAGS += -I$(src) -I$(src)/libfdt

# dependencies on generated files need to be listed explicitly
$(obj)/dtc-lexer.lex.o: $(obj)/dtc-parser.tab.h

# generated files need to be cleaned explicitly
clean-files	:= dtc-lexer.lex.c dtc-parser.tab.c dtc-parser.tab.h
