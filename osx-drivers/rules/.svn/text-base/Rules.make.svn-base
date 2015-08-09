# -*- makefile -*- 
#
#   Rules.make
#   
#   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
#   
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   version 2


#################################################################
# recursion
#################################################################

SUBS 		:= $(filter-out .,$(SUBDIRS) $(SUBDIRS-y))
DSUB 		:= $(SUBDIRS) $(SUBDIRS-y)
ifneq ($(filter .,$(DSUB)),.)
DSUB		+= .
endif
VSUBS		:= $(patsubst sub-.-all,all-local,$(foreach dir,$(DSUB),sub-$(dir)-all))

SUBTARG		= $(word $1,$(VSUBS))
DIRORDER 	= $(call SUBTARG,$1): $(call SUBTARG,$2)

SUBTARGETS 	:= clean all distclean maintainerclean
_SUBTARGETS	:= $(foreach target, $(SUBTARGETS), $(foreach dir,$(SUBS), sub-$(dir)-$(target)))

SUBDEP		= $(word $1,$(SUBS))/%.a : sub-$(word $1,$(SUBS))-all ; @test -f $@

$(call DIRORDER,2,1)
$(call DIRORDER,3,2)
$(call DIRORDER,4,3)
$(call DIRORDER,5,4)
$(call DIRORDER,6,5)
$(call DIRORDER,7,6)
$(call DIRORDER,8,7)
$(call DIRORDER,9,8)
$(call DIRORDER,10,9)
$(call DIRORDER,11,10)
$(call DIRORDER,12,11)
$(call DIRORDER,13,12)
$(call DIRORDER,14,13)

$(call SUBDEP,1)
$(call SUBDEP,2)
$(call SUBDEP,3)
$(call SUBDEP,4)
$(call SUBDEP,5)
$(call SUBDEP,6)
$(call SUBDEP,7)
$(call SUBDEP,8)
$(call SUBDEP,9)
$(call SUBDEP,10)
$(call SUBDEP,11)
$(call SUBDEP,12)
$(call SUBDEP,13)
$(call SUBDEP,14)

$(_SUBTARGETS) : % :
	@echo + "`echo $@ | sed -e s/.*all/Entering/ -e s/.*clean/cleaning/`" \
		"$(word 2,$(subst -, ,$@))"
	@$(MAKE) --no-print-directory -C $(word 2,$(subst -, ,$@)) $(word 3,$(subst -, ,$@))

all-subs: $(call SUBTARG,$(words $(VSUBS)))
clean-local: $(foreach x, $(SUBS), sub-$(x)-clean )


#################################################################
# compilation
#################################################################

COMPILE		= $(CC) $(IDIRS) $(WFLAGS) $(CFLAGS)
CXXCOMPILE	= $(CXX) $(IDIRS) $(WFLAGS) $(CXXFLAGS)
MAGICDIR	:= $(shell mkdir .deps > /dev/null 2>&1 || true )
DEPFILES	:= .deps/dummy $(wildcard .deps/*.P)
-include	$(DEPFILES)
TARGETDIR	= $(dir $1)

$(ODIR)/%.o: %.c Makefile
	@printf "    Compiling %-20s: " $(notdir $@)
	compiling=
	@$(INSTALL) -d $(dir $@)
	$(COMPILE) $(DEPFLAGS) -c $< -o $@
	@$(DEPEXTRA)
	@echo "ok"

$(ODIR)/%.o: %.cpp Makefile
	@printf "    Compiling %-20s: " $(notdir $@)
	cc_compiling=
	@$(INSTALL) -d $(dir $@)
	$(CXXCOMPILE) $(DEPFLAGS) -c $< -o $@
	@$(DEPEXTRA)
	@echo "ok"

clean-deps:
	@$(RM) -rf .deps
clean-local: clean-deps

#################################################################
# program targets
#################################################################

PROGS 	:= $(addprefix $(ODIR)/,$(PROGRAMS) $(XPROGRAMS))
NPROGS 	:= $(addprefix $(ODIR)/,$(PROGRAMS))
PROG 	= $(addprefix $(ODIR)/,$($(notdir $1)-$2))
_PROG	= $($(notdir $1)-$2)
PROGDEP = $(call PROG,$(word $1,$(PROGS)),OBJS) \
	  $(call _PROG,$(word $1,$(PROGS)),LDADD) \
	  $(LDADD)

$(word 1,$(PROGS)):	$(call PROGDEP,1)
$(word 2,$(PROGS)):	$(call PROGDEP,2)
$(word 3,$(PROGS)):	$(call PROGDEP,3)
$(word 4,$(PROGS)):	$(call PROGDEP,4)

all-local: $(PROGS)

$(NPROGS) : % : Makefile
	@printf "= Building %-22s : " $(notdir $@)
	linking=
	$(CC) -o $@ $(LDFLAGS) $(call _PROG,$@,LDFLAGS) $(call PROG,$@,OBJS) \
	  $(call _PROG,$@,LDADD) $(LDADD) $(call _PROG,$@,LIBS) $(LIBS)
	@$(call _PROG,$@,POST)
	@echo "ok"
	@TARGET=$(call _PROG,$@,TARGET) ; test $$TARGET && { \
		test -d $$TARGET || $(INSTALL) -d `dirname $$TARGET` ; \
		$(RM) -f $$TARGET ; \
		cp $@ $$TARGET ; \
		$(STRIP) $$TARGET ; \
	} ; true

prog-clean:
	@$(RM) .dummy $(foreach x,1 2 3 4,$(call _PROG,$(word $(x),$(PROGS)),TARGET))

#################################################################
# library targets
#################################################################

TLIBS		:= $(addprefix $(ODIR)/lib,$(addsuffix .a, $(XTARGETS)))
TLIB		= $(patsubst %.o,$(ODIR)/%.o,$($(patsubst lib%.a,%,$(notdir $1))-$2))
_TLIB		= $($(patsubst lib%.a,%,$(notdir $1))-$2)

$(word 1,$(TLIBS)):	$(call TLIB,$(word 1,$(TLIBS)),OBJS)
$(word 2,$(TLIBS)):	$(call TLIB,$(word 2,$(TLIBS)),OBJS)
$(word 3,$(TLIBS)):	$(call TLIB,$(word 3,$(TLIBS)),OBJS)
$(word 4,$(TLIBS)):	$(call TLIB,$(word 4,$(TLIBS)),OBJS)

all-local: $(TLIBS)

$(ODIR)/.dummy:
	@test -d $(ODIR) || mkdir $(ODIR)
	@touch $(ODIR)/.dummy

$(TLIBS) : % : Makefile $(ODIR)/.dummy
	@test -d $(ODIR) || mkdir $(ODIR)
	@rm -f $@_
	ar cru $@_ $(filter %.o,$(call TLIB,$@,OBJS)) $(ODIR)/.dummy
	for x in $(filter %.a,$(call TLIB,$@,OBJS)) ; do \
          rm -rf $(ODIR)/.artmp ; mkdir $(ODIR)/.artmp ; \
          ln $$x $(ODIR)/.artmp/lib.a ; \
          cd $(ODIR)/.artmp ; \
	  ar x lib.a || exit $$? ; cd ../../ ; \
	  ar q $@_ `find $(ODIR)/.artmp -name *.o -o -name .dummy` || exit $$? ; \
	done ; true
	ranlib $@_ && mv $@_ $@
	@$(RM) -r $(ODIR)/.artmp

#################################################################
# flex / yacc
#################################################################

MM_YFLAGS = -d

%.c: %.l
	$(LEX) $(LFLAGS) $< && mv $(LEX_OUTPUT_ROOT).c $@
%.h %.c: %.y
	$(YACC) $(MM_YFLAGS) $(YFLAGS) $< && mv y.tab.c $*.c
	@test -f y.tab.h && { \
		if cmp -s y.tab.h $*.h; then rm -f y.tab.h; else mv y.tab.h $*.h; fi; \
	} ; true


#################################################################
# rules for asm targets
#################################################################

M_ASMFLAGS	= -D__ASSEMBLY__ $(ALTIVEC) $(ASMFLAGS)
FILTERBIN	= $(scriptdir)/$(ODIR)/asfilter
ASFILTER	= $(shell if test -x $(FILTERBIN) ; then echo $(FILTERBIN) \
			; else echo "tr ';' '\n'" ; fi)
INVOKE_M4	= | $(M4) -s $(M4_NO_GNU) | $(ASFILTER)

$(ODIR)/%.o: %.S Makefile
	@printf "    Compiling %-20s: " $(notdir $@)
	assembly=
	@$(INSTALL) -d $(dir $@)
	@$(RM) $@ $@.s
	@$(CPP) $(M_ASMFLAGS) $(IDIRS) $< > /dev/null
	$(CPP) $(M_ASMFLAGS) $(IDIRS) $(DEPFLAGS) $< $(INVOKE_M4) > $@.s
	$(AS) $(IDIRS) $@.s $(ASFLAGS) -o $@
	@$(DEPEXTRA)
	@$(RM) $@.s
	@echo "ok"


#################################################################
# targets
#################################################################

clean-obdir:
	@rm -rf $(ODIR)
clean-local:	prog-clean clean-obdir

all-local:	$(all-y)
all:		all-first all-subs all-last

distclean-files:
		@rm -rf .dummy $(DISTCLEANFILES)
clean-files:
		@rm -rf .dummy $(CLEANFILES)
maintainerclean-files:
		@rm -rf .dummy $(MAINTAINERCLEANFILES)

distclean-local: distclean-files
clean-local:	clean-files
maintainerclean-local: maintainerclean-files

clean:		clean-local
distclean:	clean-local distclean-local
maintainerclean: clean-local maintainerclean-local distclean-local

.PHONY:		all all-first all-local all-last
.PHONY:		clean clean-local distclean distclean-local maintainerclean-local


#################################################################
# END
#################################################################
