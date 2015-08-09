
include			../config/Makefile.top

SUBDIRS 		= fs libc main accel .

XTARGETS		= bootx

bootx-OBJS		= fs/libfs.a libc/liblibc.a main/libmain.a \
			  accel/libaccel.a
CFLAGS			+= -fno-builtin -msoft-float -static $(NOSTACKPROTECT)

# Linux and OSX link a little differently
LINKCMD-$(LINUX)	= $(LD) -Ttext=0x1c01000 -Bstatic 
LINKCMD-$(OSX)		= $(LD) -e __start -static

# WARNING: We may only put things at 0x1c00000 - 0x1d00000.
# We are very close to the end limit.

.dummy.c: Makefile
	rm -f $@
	echo "extern void _start(void);" > $@
	echo "extern int main( void ); int main( void ) { _start(); return 0; }" >> $@


bootx: Makefile $(ODIR)/libbootx.a $(ODIR)/.dummy.o
	$(LINKCMD-y) $(ODIR)/.dummy.o $(ODIR)/libbootx.a $(LIBGCC) -o $@ 
	nm $@ | sort > bootx.syms
	$(STRIP) $@
	cp bootx ../mollib/drivers/bootx

all-local: bootx

include			$(rules)/Rules.make
