
include			../../config/Makefile.top

DEB			:= $(if $(HAVE_NCURSES),$(CONFIG_DEBUGGER))

SUBDIRS			= $(sub-y)
sub-$(DEB)		:= deb mon

XTARGETS		:= debugger
debugger-OBJS		= $(obj-y)
obj-$(CONFIG_DEBUGGER)	:= dgram.o mmu_cmds.o nub.o

PROGRAMS		= $(progs-y)
progs-$(DEB)		:= moldeb

moldeb-TARGET		:= $(molbin)/moldeb
moldeb-OBJS		:= dgram.o deb/libdeb.a mon/libmon.a ../main/libres.a ../lib/libcommon.a
moldeb-LIBS		:= $(NCURSES_LIBS)

include			$(rules)/Rules.make
