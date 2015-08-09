# 
# This makefile constructs a new symlinked source tree
# which is then build using the kernel kbuild system.
#
# Different makefiles are used for 2.4 and 2.6 kernels

DEFINE_KERNEL_SOURCE	= yes
include			../../../config/Makefile.top

BDIR			= $(ODIR)/../
KUNAME			= `cat $(BDIR)/.kuname`
LV			:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo 24 || echo 26)
MP			:= $(shell test -f $(KERNEL_SOURCE)/Rules.make && echo "o" || echo "ko")
MOD_TARGET		:= $(BDIR)/mol.$(MP)
MOD_DEST		= $(mollib)/modules/$(KUNAME)

dummy			:= $(shell test -d $(BDIR) || $(INSTALL) -d $(BDIR))
SUBDIRARG		= $(shell echo SUBDIRS=`cd $(BDIR) ; pwd`) modules
BUILD			= $(SUBDIRARG) modules
DEPS_24			= $(MAKE) --no-print-directory -C $(KERNEL_SOURCE) $(SUBDIRARG) depend 
DEPS_26			= true

KILL_WARNING_26		= 2>&1 | sed -e '/\*\*\*.*SUBDIRS/ d' \
				     -e '/\*\*\*.*inconsistencies/ d' \
				     -e '/scripts\/Makefile.*complain if/ d' 1>&2

sanity-test:
	@test -d $(KERNEL_SOURCE) || { \
		echo "Kernel source missing. KERNEL_SOURCE=$(KERNEL_SOURCE)"; exit 1; }

RELCHECK	= $(BDIR)/relcheck.pl
make-relcheck:
	@test -d $(BDIR) || $(INSTALL) -d $(BDIR)
	@rm -f $(BDIR)/relcheck.pl $(BDIR)/checker.pl
	@echo "#!/usr/bin/perl -w" > $(RELCHECK)
	@echo '$$'"NM = '$(NM)';" >> $(RELCHECK)
	@cp -f $(RELCHECK) $(BDIR)/checker.pl
	@echo '$$'"OBJDUMP = '$(OBJDUMP)';" >> $(RELCHECK)
	@cat relcheck.pl >> $(RELCHECK)
	@cat checker.pl >> $(BDIR)/checker.pl
	@chmod +x-w $(RELCHECK) $(BDIR)/checker.pl

setup-common: sanity-test make-relcheck
	@rm -f $(BDIR)/dir1 $(BDIR)/dir2
	@ln -sf ../../../stree/src/kmod $(BDIR)/dir1
	@ln -sf ../../../stree/src/kmod/Linux $(BDIR)/dir2
	@SAVEPWD=`pwd` ; cd .. ; FILES=`echo *.inc *.c *.S` ; cd "$$SAVEPWD" ; \
	for x in $$FILES ; do ln -sf dir1/$$x $(BDIR)/ || exit 1 ; done
	@for x in *.S ; do ln -sf dir2/$$x $(BDIR)/ || exit 1 ; done

	@ln -sf ../../../include/kern/kconfig.h $(BDIR)/
	@ln -sf ../../scripts/asfilter $(BDIR)/
	@ln -sf dir2/Makefile.$(LV) $(BDIR)/Makefile
	@for x in *.h *.S ; do ln -sf dir2/$$x $(BDIR)/ || exit 1 ; done 
	@for x in *.c ; do ln -sf dir2/$$x $(BDIR)/_$$x || exit 1 ; done
	@SAVEPWD=`pwd` ; cd ../../shared/ ; FILES=`echo *.h *.m4 *.c` ; cd "$$SAVEPWD" ; \
	for x in $$FILES ; do \
		ln -sf ../../../stree/src/shared/$$x $(BDIR)/$$x || exit 1 ; \
	done
	@SAVEPWD=`pwd` ; cd ../include/ ; FILES=`echo *.h` ; cd "$$SAVEPWD" ; \
	for x in $$FILES ; do \
		ln -sf dir1/include/$$x $(BDIR)/$$x || exit 1 ; \
	done
	@ln -sf ../../../include/molversion.h $(BDIR)/

all-local: setup-common
	@$(DEPS_$(LV)) > /dev/null 2>&1 || ( echo "$(DEPS_$(LV))" ; $(DEPS_$(LV)) ; exit 1 ; )
	# Replace the if [ blah ] with a mechanism to set the proper architecture independent of the build host.
	@{ if [ $(ARCH) = "ppc" -o $(ARCH) = "ppc64" ]; then ARCH=powerpc; fi; $(MAKE) --no-print-directory -C $(KERNEL_SOURCE) $(BUILD) || exit $$? ; } $(KILL_WARNING_$(LV))
	@$(BDIR)/checker.pl $(MOD_TARGET) || (echo "checker.pl failed" ; rm $(MOD_TARGET) ; exit 1)
#	$(STRIP) -g build mol.o

	@printf "    %-29s : %s\n" "Kernel source" "$(KERNEL_SOURCE)"
	@printf "    %-29s : %s\n" "Module compiled for" "$(KUNAME)"
	@printf "    %-29s : %s\n" "Running kernel" \
		"`$(top_srcdir)/scripts/mol_uname -p`"

	@test -d $(MOD_DEST) || $(INSTALL) -d $(MOD_DEST)
	ln -f $(MOD_TARGET) $(MOD_DEST)/


clean-local:
	rm -rf $(top_odir)/build/src/kmod

include		$(rules)/Rules.make
