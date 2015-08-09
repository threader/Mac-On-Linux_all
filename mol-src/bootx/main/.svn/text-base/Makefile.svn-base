
include		../../config/Makefile.top

INCLUDES	= -I ../include -I ../../src/drivers/disk/include \
		  -I ../../src/drivers/include

CFLAGS		+= -fno-builtin -msoft-float -static $(NOSTACKPROTECT)

XTARGETS	= main
main-OBJS	= main.o device_tree.o macho.o ci.o drivers.o drivers_simple.o \
		  util.o pseudofs.o init.o

include		$(rules)/Rules.make
