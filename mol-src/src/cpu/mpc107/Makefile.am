
include			$(top_srcdir)/src/Makefile.libs

#######################################################################

AS_ 			= $(PPC_AS)
include 		$(top_srcdir)/src/Makefile.extra

#######################################################################

noinst_LIBRARIES	= libmain.a

INCLUDES		= -I./ -I../../include -I../../shared -I-
libmain_a_SOURCES	= init.c
CLEANFILES		= asm_offsets.h asm_offsets.s asm_offsets.c

EXTRA_DIST		= mpcio.h

mainloop_asm.o: asm_offsets.h

asm_offsets.h: ../../shared/asm_offsets.c 
	cat $^ > asm_offsets.c
	$(COMPILE) -Wall -S asm_offsets.c
	@$(RM) $@
	@echo "/* WARNING! AUTOMATICALLY GENERATED FROM 'asm_offsets.c' - DO NOT EDIT! */" > $@
	@chmod u+rw $@
	grep '^#define' asm_offsets.s | sed 's/[$$]//' >> $@
	rm asm_offsets.s asm_offsets.c
