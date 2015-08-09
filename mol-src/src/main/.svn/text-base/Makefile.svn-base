
include			../../config/Makefile.top

SUBDIRS-$(OSX)		= Darwin
SUBDIRS-$(LINUX)	= linux

XTARGETS		= res main
PROGRAMS		= molrcget
molrcget-TARGET		= $(molbin)/molrcget

INCLUDES		+= -DMOL_DATA_DIR=\"$(datadir)\" -DMOL_ETC_DIR=\"$(etcdir)\" \
			   -DMOL_VAR_DIR=\"$(vardir)\" -DMOL_LIB_DIR=\"$(libdir)\"

#CFLAGS			+= -msoft-float
molrcget-OBJS		= res_manager.o molrcget.o ../lib/libcommon.a

res-OBJS		= res_manager.o
main-OBJS		= async.o main.o memory.o os_interface.o promif.o session.o \
			  thread.o timer.o res_manager.o $(obj-y)

obj-$(LINUX)		+= linux/libarch.a
obj-$(OSX)		+= Darwin/libarch.a

include			$(rules)/Rules.make
