
include			../Makefile.top

WFLAGS			:= -Wall
PROGRAMS		:= mconfig config
mconfig-OBJS		:= zconf-y.o mconf.o libkconf.a
config-OBJS		:= zconf-y.o conf.o
XTARGETS		:= kconf
kconf-OBJS		:= zconf-y.o
LIBS			:= $(LEXLIB)

dummy			:= $(shell test -d $(ODIR) || $(INSTALL) -d $(ODIR))
dummy2			:= $(shell test -d $(top_odir)/config || $(INSTALL) -d $(top_odir)/config)
ABSODIR			:= $(shell test -d $(ODIR) && cd $(ODIR) ; pwd)
LXDIALOG		:= $(ABSODIR)/../lxdialog/lxdialog
PROJECT			:= "Mac-on-Linux"
DEFCONFIG		:= config/defconfig-$(ARCH)
KCONFIG			:= config/Kconfig-$(ARCH)
CONFIGFILE		:= .config-$(ARCH)
CONFHEADER		:= .autoconf-$(ARCH).h

export 			LXDIALOG VERSION EXTRAVERSION PROJECT DEFCONFIG
export			CONFHEADER CONFIGFILE

DESTCONFIG		= $(top_odir)/config/autoconf.h
MOVECONFIG		:= test -f $(top_srcdir)/$(CONFIGFILE) || exit 0 ; \
			   mv -f $(top_srcdir)/$(CONFHEADER) $(DESTCONFIG)
distclean-local:
	@rm -rf .autoinclude

menuconfig: $(ODIR)/mconfig
	cd $(top_srcdir) ; $(ABSODIR)/mconfig $(KCONFIG)
	$(MOVECONFIG)

config: $(ODIR)/config
	cd $(top_srcdir) ; $(ABSODIR)/config $(KCONFIG)
	$(MOVECONFIG)

defconfig: $(ODIR)/config
	cd $(top_srcdir) ; $(ABSODIR)/config -d $(KCONFIG) 
	$(MOVECONFIG)

oldconfig: $(ODIR)/config
	cd $(top_srcdir) ; $(ABSODIR)/config -o $(KCONFIG) 
	$(MOVECONFIG)

YFLAGS			= -p zconf
MAINTAINERCLEANFILES	= zconf-y.c zconf-y.h zconf-l.c

$(ODIR)/zconf-y.o: zconf-y.c zconf-l.c zconf-y.h

all-local:
	@$(RM) lex.backup

include			$(rules)/Rules.make
