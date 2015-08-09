
include			../config/Makefile.top

PROGRAMS		= asfilter kver_approx
asfilter-OBJS		= asfilter.o
kver_approx-OBJS	= kver_approx.o
kver_approx-TARGET	= $(molbin)/kver_approx

BINSCRIPTS		= mol_uname modload

all-local:
	@test -d $(molbin) || $(INSTALL) -d $(molbin)
	@ln -s -f $(addprefix ../../stree/scripts/, $(BINSCRIPTS)) $(molbin)

include			$(rules)/Rules.make
