
include			../../config/Makefile.top

INCLUDES		= -I ../include

CFLAGS			+= -fno-builtin -msoft-float -static $(NOSTACKPROTECT)

XTARGETS		= accel
accel-OBJS		= mol.o molaccel.o kvm.o kvm_emul.o

include			$(rules)/Rules.make
