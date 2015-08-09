# -*- makefile -*-

if MPC107

A_CFLAGS		= @CFLAGS@ -Wall -msoft-float -fno-builtin
A_INCLUDES		= -D__KERNEL__ -DKERNEL -I$(top_srcdir)/src/kmod/include \
			  -I$(top_srcdir)/src/shared -I$(top_srcdir)/src/kmod/mpc107 -I- 

###########################################################################
# Kernel module generation
###########################################################################

A_KERNEL_MODULE		= mpckernel

$(A_KERNEL_MODULE): $(KMOD_OBJS) mpc107/ld.script
	@$(RM) $@
	@$(LD) -r $(KMOD_OBJS) `cat mpc107/objs` -o $@.o
	echo $(LD) -r $(KMOD_OBJS) `cat mpc107/objs` -o $@.o
	$(STRIP) -g $@.o
	$(LD) -T mpc107/ld.script -Ttext 0x00000000 -Bstatic $@.o -o $@
#	ln -f $(A_KERNEL_MODULE) $(top_srcdir)/mollib/modules/`./kuname`/

endif
