
top-all:	
include		rules/Makefile.top

#####################################################################

configure: configure.in
	./autogen.sh

AUTOFILES	= rules/Makefile.defs $(ARCHINCLUDES)/config.h

top-all:	configure all
all-local:	$(ARCHINCLUDES) $(AUTOFILES)

$(AUTOFILES) : % : rules/Makefile.defs.in
	@rm -f $@
	@test -x config.status && ./config.status && exit 0 ; \
	test -x ./configure && ./configure && exit 0 ; \
	./autogen.sh && ./configure
	@for x in $(AUTOFILES) ; do test -f $$x && chmod -w $$x ; done ; true

AWKTEXT	:= ' { print "\#define MOL_MAJOR_VERSION " $$1 ; \
		print "\#define MOL_MINOR_VERSION " $$2 ; \
		print "\#define MOL_PATCHLEVEL " $$3 }' 

$(ARCHINCLUDES) $(ARCHINCLUDES)/%: configure.in Makefile rules/Makefile.top
	@rm -rf $(ARCHINCLUDES)
	@install -d $(ARCHINCLUDES)
	@{ DATE="$(shell echo `date +'%b %e %Y %H:%M'`)" ; \
	   echo "#define MOL_BUILD_DATE \"$$DATE\"" ; \
	   echo "#define MOL_VERSION_STR \"$(VERSION)\"" ; \
	   echo "#define MOL_RELEASE \"$(RELEASENAME)\"" ; \
	   echo $(VERSION) | awk -F . -- $(AWKTEXT) ; \
	   echo = "Building osxdrivers-$(VERSION)$(EXTRA_VERSION) [$$DATE]" 1>&2 ; \
	} 2>&1 > $(ARCHINCLUDES)/molversion.h


#####################################################################

clean-local:
	@rm -rf *.core .kmods $(ARCHINCLUDES)

distclean-local:
	@rm -f $(AUTOFILES) config.* .config .config-*

mrproper: distclean
	@rm -f ./configure

maintainerclean-local:
	@rm -f ./configure

#####################################################################

SUBDIRS		= . src
MAKE		+= -s

#####################################################################

include		Makefile.dist
include 	$(rules)/Rules.make
