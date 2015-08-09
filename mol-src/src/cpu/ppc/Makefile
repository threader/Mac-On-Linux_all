
include			../../../config/Makefile.top

XTARGETS		= ppc
#CFLAGS			+= -msoft-float
INCLUDES		+= -I$(SDIR)

ppc-OBJS		= $(obj-y)
obj-y			+= init.o mainloop_asm.o

SDIR			= $(ODIR)/include

all-local: $(SDIR)/mregs-offs.h
$(ODIR)/mainloop_asm.o: $(SDIR)/mregs-offs.h

$(SDIR)/mregs-offs.h: ../../shared/asm_offsets.c ../../shared/mac_registers.h
	@test -d $(dir $@) || $(INSTALL) -d $(dir $@)
	cat $^ > mregs-offs.h.c
	$(COMPILE) -Wall -S mregs-offs.h.c
	@$(RM) $@
	@echo "/* WARNING! AUTOMATICALLY GENERATED FROM - DO NOT EDIT! */" > $@
	@chmod u+rw $@
	grep '^#define' mregs-offs.h.s | sed 's/[$$]//' >> $@
	rm mregs-offs.h.s mregs-offs.h.c

clean-local:
	rm -rf $(SDIR)

include			$(rules)/Rules.make
