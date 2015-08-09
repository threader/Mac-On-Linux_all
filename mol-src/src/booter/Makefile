
include			../../config/Makefile.top

XTARGETS		= booter
booter-OBJS		= booter.o common.o elf.o macho.o $(obj-y)
obj-$(CONFIG_OLDWORLD)	= oldworld.o

include			$(rules)/Rules.make
