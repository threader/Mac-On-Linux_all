# -*- makefile -*- 
#
#   Rules.make
#   
#   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
#   
#   This program is free software; you can redistribute it and/or
#   modify it under the terms of the GNU General Public License
#   version 2


# the following sed line removes "dir/.." pairs from the path
# (make gets confused if dir does not exist)

CANONICALIZE	= $(shell echo $1 | sed -e ':0' \
			-e 's/[a-zA-Z0-9,.]*[a-zA-Z0-9]\/[.][.]\///g' -e t0)

CYELLOW		:= `test "$$TERM" != "dumb" && echo "\33[0;33m"`
CYELLOWB	:= `test "$$TERM" != "dumb" && echo "\33[1;33m"`
CBOLD		:= `test "$$TERM" != "dumb" && echo "\33[1;23m"`
CNORMAL		:= `test "$$TERM" != "dumb" && echo "\33[0;39m"`


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

SUBDEP		= $(ODIR)/$(word $1,$(SUBS))/%.a : sub-$(word $1,$(SUBS))-all ; @test -f $@

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
	@printf "$(CBOLD)"
	@echo + "`echo $@ | sed -e s/.*all/Entering/ -e s/.*clean/cleaning/`" \
		"$(word 2,$(subst -, ,$@))"
	@printf "$(CNORMAL)"
	@$(MAKE) --no-print-directory -C $(word 2,$(subst -, ,$@)) $(word 3,$(subst -, ,$@))

all-subs: $(call SUBTARG,$(words $(VSUBS)))
clean-local: $(foreach x, $(SUBS), sub-$(x)-clean )


#################################################################
# compilation
#################################################################

MAGICDIR	:= $(shell test "$(INSTALL)" && test ! -d $(DEPSDIR) && $(INSTALL) -d $(DEPSDIR))
COMPILE		= $(CC) $(IDIRS) $(WFLAGS) $(CFLAGS)
CXXCOMPILE	= $(CXX) $(IDIRS) $(WFLAGS) $(CXXFLAGS)
DEPFILES	:= $(DEPSDIR)/dummy $(wildcard $(DEPSDIR)/*.P)
-include	$(DEPFILES)
TARGETDIR	= $(dir $1)

$(ODIR)/%.o: %.c Makefile
	@printf "    $(CYELLOW)Compiling    %-20s$(CNORMAL)\n" $(notdir $@)
	@$(INSTALL) -d $(dir $@)
	$(COMPILE) $(DEPFLAGS) -c $< -o $@
	@$(DEPEXTRA)

$(ODIR)/%.o: %.cpp Makefile
	@printf "    $(CYELLOW)Compiling    %-20s$(CNORMAL)\n" $(notdir $@)
	@$(INSTALL) -d $(dir $@)
	$(CXXCOMPILE) $(DEPFLAGS) -c $< -o $@
	@$(DEPEXTRA)

clean-deps:
	@$(RM) -rf $(DEPSDIR)
clean-local: clean-deps


#################################################################
# program targets
#################################################################

PROGS 		:= $(addprefix $(ODIR)/,$(PROGRAMS) $(XPROGRAMS))
NPROGS 		:= $(addprefix $(ODIR)/,$(PROGRAMS))
PROG 		= $(addprefix $(ODIR)/,$($(notdir $1)-$2))
_PROG		= $($(notdir $1)-$2)
PROGDEP 	= $(call CANONICALIZE, $(call PROG,$(word $1,$(PROGS)),OBJS) \
		  $(call _PROG,$(word $1,$(PROGS)),LDADD) \
		  $(LDADD))

$(word 1,$(PROGS)):	$(call PROGDEP,1)
$(word 2,$(PROGS)):	$(call PROGDEP,2)
$(word 3,$(PROGS)):	$(call PROGDEP,3)
$(word 4,$(PROGS)):	$(call PROGDEP,4)

all-local: $(PROGS)

$(NPROGS) : % : Makefile
	@printf "$(CBOLD)= Building %-22s$(CNORMAL)\n" $(notdir $@)
	$(CC) -o $@ $(LDFLAGS) $(call _PROG,$@,LDFLAGS) $(call PROG,$@,OBJS) \
	  $(call _PROG,$@,LDADD) $(LDADD) $(call _PROG,$@,LIBS) $(LIBS)
	@$(call _PROG,$@,POST)
	@TARGET=$(call _PROG,$@,TARGET) ; test $$TARGET && { \
		test -d $$TARGET || $(INSTALL) -d `dirname $$TARGET` ; \
		$(RM) -f $$TARGET ; \
		cp $@ $$TARGET ; \
		$(STRIP) $$TARGET ; \
	} ; true

prog-clean:
	@$(RM) $(foreach x,1 2 3 4,$(call _PROG,$(word $(x),$(PROGS)),TARGET))


#################################################################
# library targets
#################################################################

TLIBS		:= $(addprefix $(ODIR)/lib,$(addsuffix .a, $(XTARGETS)))
TLIB		= $(call CANONICALIZE, $(addprefix $(ODIR)/,$($(notdir $1)-$2)))
_TLIB		= $(call TLIB,$(patsubst lib%.a,%,$(notdir $1)),$2)
xTLIB		= $($(notdir $1)-$2)
__TLIB		= $(call xTLIB,$(patsubst lib%.a,%,$(notdir $1)),$2)

$(word 1,$(TLIBS)):	$(call TLIB,$(word 1,$(XTARGETS)),OBJS)
$(word 2,$(TLIBS)):	$(call TLIB,$(word 2,$(XTARGETS)),OBJS)
$(word 3,$(TLIBS)):	$(call TLIB,$(word 3,$(XTARGETS)),OBJS)
$(word 4,$(TLIBS)):	$(call TLIB,$(word 4,$(XTARGETS)),OBJS)

all-local: $(TLIBS)

AR_TMPDIR	= .tmpdir-$(notdir $@)
AR_TMP		= .tmp-$(notdir $@)

$(TLIBS) : % : Makefile
	@printf "    $(CYELLOWB)Linking      %-22s$(CNORMAL)\n" $(notdir $@)
	@test -d $(dir $@) || $(INSTALL) -d $(dir $@)
	@rm -rf $@_ $(ODIR)/$(AR_TMPDIR) ; $(INSTALL) -d $(ODIR)/$(AR_TMPDIR)

	@ofiles='$(addprefix ../,$(filter %.o,$(call __TLIB,$@,OBJS)))' ; \
	for x in $$ofiles ; do ln -s $$x $(ODIR)/$(AR_TMPDIR)/ ; done
	@list='$(filter %.a,$(call __TLIB,$@,OBJS))' ;			\
	for x in $$list ; do 						\
            rm -rf $(ODIR)/$(AR_TMP) ; mkdir $(ODIR)/$(AR_TMP) ;	\
            ln -s ../$$x $(ODIR)/$(AR_TMP)/lib.a ; 			\
            SAVEPWD=`pwd` ; cd $(ODIR)/$(AR_TMP) ;			\
	    ar x lib.a || exit $$? ;					\
	    ofiles=`find . -name '*.o'` ;				\
	    for x in $$ofiles ; do					\
	        DX=`basename $$x` ;					\
		while test -f ../$(AR_TMPDIR)/$$DX ; do			\
		    DX=_$$DX ;						\
                done ;							\
	        mv $$x ../$(AR_TMPDIR)/$$DX ;				\
	    done ;							\
	    cd "$$SAVEPWD" ;						\
	done ; true

	@ofiles=`find $(ODIR)/$(AR_TMPDIR) -name '*.o'` ;		\
	test "$$ofiles" || {						\
	    echo "void dummy_placerholder_$$$$(void){}"			\
	    	> $(ODIR)/$(AR_TMPDIR)/xdummy.c ;			\
	    $(CC) $(ODIR)/$(AR_TMPDIR)/xdummy.c -c			\
		-o $(ODIR)/$(AR_TMPDIR)/xdummy.o ;			\
	} ; true

	ofiles=`find $(ODIR)/$(AR_TMPDIR) -name '*.o'` ;		\
	ar cru $@_ $$ofiles && ar s $@_ && mv $@_ $@

	@$(RM) -rf $(ODIR)/$(AR_TMP) $(ODIR)/$(AR_TMPDIR)


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
FILTERBIN	= $(top_odir)/build/scripts/asfilter
ASFILTER	= $(shell if test -x $(FILTERBIN) ; then echo $(FILTERBIN) \
			; else echo "tr ';' '\n'" ; fi)
INVOKE_M4	= | $(M4) -s $(M4_NO_GNU) | $(ASFILTER)

$(ODIR)/%.o: %.S Makefile
	@printf "    $(CYELLOW)Compiling    %-20s$(CNORMAL)\n" $(notdir $@)
	@$(INSTALL) -d $(dir $@)
	@$(RM) $@ $@.s
	@$(CPP) $(M_ASMFLAGS) $(IDIRS) $< > /dev/null
	$(CPP) $(M_ASMFLAGS) $(IDIRS) $(DEPFLAGS) $< $(INVOKE_M4) > $@.s
	$(AS) $(IDIRS) $@.s $(ASFLAGS) -o $@
	@$(DEPEXTRA)
	@$(RM) $@.s


#################################################################
# automatic directory generation
#################################################################

%/.dir:
	@test -d $(dir $@) || $(INSTALL) -d $(dir $@)
	@touch $@


#################################################################
# targets
#################################################################

clean-obdir:
	@rm -rf $(ODIR)

clean-local:	prog-clean clean-obdir

all-local:	$(all-y)
all:		all-first all-subs all-last

distclean-files:
		@rm -rf $(DISTCLEANFILES)
clean-files:
		@rm -rf $(CLEANFILES)
maintainerclean-files:
		@rm -rf $(MAINTAINERCLEANFILES)

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
