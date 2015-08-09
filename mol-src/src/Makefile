
include			../config/Makefile.top

BUILD_MODS		:= $(if $(CONFIG_MOL),y)
NETMODS			:= $(CONFIG_TUN)$(CONFIG_SHEEP)
NETDRIVER		:= $(if $(LINUX),$(if $(NETMODS),$(BUILD_MODS)))

SUBDIRS			= lib main drivers debugger cpu booter .
SUBDIRS			+= molelf
SUBDIRS-$(CONFIG_FBDEV)	+= vconfig
SUBDIRS-$(BUILD_MODS)	+= kmod
SUBDIRS-$(NETDRIVER)	+= netdriver

PROGRAMS		= mol
mol-TARGET		= $(molbin)/mol
XTARGETS		= allmol

xlibs-$(CONFIG_X11)	= -lX11 -lXext
xlibs-$(CONFIG_XDGA)	+= -lXxf86dga

mol-OBJS		= 
mol-LDADD		= $(ODIR)/liballmol.a
mol-LDFLAGS		= $(X_LIBS) $(FRAMEWORKS)
mol-LIBS		= $(LIB_ALSA) $(LIB_PNG) $(SDL_LIBS) $(xlibs-y) -lpthread -lm -lz

allmol-OBJS		= $(obj-y)
obj-y			+= main/libmain.a
obj-y			+= cpu/libcpu.a
obj-y			+= lib/libcommon.a
obj-y			+= drivers/libdrivers.a
obj-y			+= booter/libbooter.a
obj-y			+= debugger/libdebugger.a

$(mollib)/mol.symbols: $(ODIR)/mol
	@nm $< | grep ' [tT] ' | sort > $@

$(molbin)/startmol: $(scriptdir)/startmol
	@$(RM) $@
	@echo "#!/bin/bash" > $@
	@echo >> $@
	@echo "BIN_PATH=$(libdir)/bin" >> $@
	@echo >> $@
	@cat $< >> $@
	@chmod +x-w $@

$(top_odir)/startmol:
	ln -sf lib/bin/startmol $@

CLEANFILES		+= $(top_odir)/startmol

all-local: $(top_odir)/startmol $(molbin)/startmol $(mollib)/mol.symbols


include			$(rules)/Rules.make
