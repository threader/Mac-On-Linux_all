##############################################################
# This makefile is responsible for buildning architectural 
# specific modules (like molsymglue.o) as well as arch
# specific sources.
##############################################################

CFLAGS			= $(A_CFLAGS)
INCLUDES		= $(A_INCLUDES)
CXXFLAGS		= $(A_CXXFLAGS)
KERNEL_MODULE		= $(A_KERNEL_MODULE)

include 		$(top_srcdir)/src/Makefile.extra
include 		Makefile.mod

KMOD_SRCS		= test.c init.c fault.c libc.c vsprintf.c

noinst_LIBRARIES	= libdummy.a
libdummy_a_SOURCES	= $(KMOD_SRCS)

EXTRA_DIST		= Makefile.mod mpc107.S ld.script *.h
CLEANFILES		= objs

libdummy.a:		$(KUNAME_DEP) $(KMOD_SRCS:.c=.o) objs


##############################################################
# Kernel module objects
##############################################################

objs: Makefile.am
	@$(RM) $@
	echo $(addprefix "mpc107/", $(KMOD_SRCS)) | sed -e s/[.][cS]/.o/g > $@
