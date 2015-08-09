
include			../../config/Makefile.top

ifneq ($(PPC),y)
PPCCROSS		:= /usr/local/bin/powerpc-linux-
CC			:= $(PPCCROSS)$(CC)
AS			:= $(PPCCROSS)$(AS)
LD			:= $(PPCCROSS)$(LD)
STRIP			:= $(PPCCROSS)$(STRIP)
endif

XPROGRAMS		= selftest keyremap
XTARGETS		= xselftest xkeyremap
selftest-TARGET		= $(molbin)/selftest
keyremap-TARGET		= $(molbin)/keyremap
xselftest-OBJS		= performance.o selftest.o vsprintf.o
xkeyremap-OBJS		= keyremap.o vsprintf.o

LINKCMD-$(LINUX)	= $(LD) -Ttext=0x101000 -Bstatic $^ -o $@
LINKCMD-$(OSX)		= $(LD) -e __start -static $^ -o $@
CFLAGS			+= -static $(NOSTACKPROTECT)

$(ODIR)/selftest: $(ODIR)/init.o $(ODIR)/libxselftest.a
	$(LINKCMD-y)
	$(STRIP) $@

$(ODIR)/keyremap: $(ODIR)/init.o $(ODIR)/libxkeyremap.a
	$(LINKCMD-y)
	$(STRIP) $@

all-local:
	@test -d $(molbin) || $(INSTALL) -d $(molbin)
	@ln -f $(ODIR)/keyremap $(ODIR)/selftest $(molbin)/

clean-local:
	@$(RM) $(ODIR)/keyremap $(ODIR)/selftest

include			$(rules)/Rules.make
