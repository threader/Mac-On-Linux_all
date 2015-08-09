
include		../../config/Makefile.top

INCLUDES	= -I ../include

CFLAGS		+= -fno-builtin -msoft-float -static $(NOSTACKPROTECT)

XTARGETS	= fs

fs-OBJS		= cache.o ext2fs_bswap.o ext2fs.o fs.o hfs.o \
		  HFSCompare.o net.o ufs.o ufs_byteorder.o


include		$(rules)/Rules.make
