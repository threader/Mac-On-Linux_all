
include			../../config/Makefile.top

PROGRAMS		= molvconfig
molvconfig-TARGET	= $(molbin)/molvconfig
molvconfig-LIBS		= $(LEXLIB)
molvconfig-OBJS		= main.o vmodeparser.o modes-y.o modes-l.o
molvconfig-OBJS		+= ../main/libres.a ../lib/libcommon.a
MAINTAINERCLEANFILES	= modes-y.c modes-y.h modes-l.c

# Get rid of warnings... (flex buf)
$(ODIR)/modes-l.o:	WFLAGS=

modes-l.c: 		modes-y.c modes-y.h
.PRECIOUS:		modes-y.c modes-l.c modes-y.h

include			$(rules)/Rules.make
