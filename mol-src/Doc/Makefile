
include		../config/Makefile.top

DOCFILES = \
	Boot-ROM	\
	NewWorld-ROM	\
	Sound		\
	Video		\
	Networking

DEV_DOCFILES = \
	Debugger	\
	Addresses	\
	Overview

MOLRCFILES = \
	molrc.macos	\
	molrc.input	\
	molrc.net	\
	molrc.sound	\
	molrc.video	\
	molrc.ow	\
	molrc.linux	\
	molrc.osx	\
	session.map	\
	yaboot.conf	\
	dhcpd-mol.conf	\
	tunconfig

MAN1_FILES = \
	molvconfig.1	\
	startmol.1

MAN5_FILES = \
	molrc.5

_docconfig = $(_docdir)/config/

install-config-force:
	@$(INSTALL_DIR) -v $(_etcdir)
	@for x in $(MOLRCFILES) ; do \
		test -f config/$$x || exit $?					; \
		cmp -s config/$$x $(_etcdir)/$$x ||				  \
		  mv -f $(_etcdir)/$$x $(_etcdir)/$$x.bak			; \
		rm -f $(_etcdir)/$$x 						; \
		$(INSTALL_DATA) -v config/$$x $(_etcdir)/$$x || exit $$?	; \
	done ; true
	@chmod 755 $(_etcdir)/tunconfig $(_docconfig)/tunconfig

install-configfiles:
	@$(INSTALL_DIR) -v $(_etcdir) $(_docconfig)

	@for x in $(MOLRCFILES) ; do \
		$(INSTALL_DATA) -v config/$$x $(_docconfig)			; \
		test -f config/$$x || exit $?					; \
		test -f $(_etcdir)/$$x && continue				; \
		$(INSTALL_DATA) -v config/$$x $(_etcdir)/$$x || exit $$?	; \
	done

	@chmod 755 $(_etcdir)/tunconfig $(_docconfig)/tunconfig
	@$(INSTALL_DATA) -v config/README $(_docconfig)/


install: install-configfiles
	@$(INSTALL_DIR) -v $(_etcdir) $(_docconfig) $(_docdir) $(_docdir)/Dev
	@$(INSTALL_DIR) -v $(_mandir)/man1 $(_mandir)/man5

	@for x in $(DOCFILES) ; do \
		$(INSTALL_DATA) -v $$x $(_docdir) 		; \
		test -f $$x || exit $?				; \
	done

	@for x in $(addprefix Dev/,$(DEV_DOCFILES)) ; do \
		$(INSTALL_DATA) -v $$x $(_docdir)/Dev 		; \
		test -f $$x || exit $?				; \
	done

	@for x in $(MAN1_FILES) ; do \
		$(INSTALL_DATA) -v man/$$x $(_mandir)/man1/	; \
		test -f man/$$x || exit $?			; \
	done

	@for x in $(MAN5_FILES) ; do \
		$(INSTALL_DATA) -v man/$$x $(_mandir)/man5/	; \
		test -f man/$$x || exit $?			; \
	done

	@install $(top_srcdir)/COPYING $(_docdir)


include		$(rules)/Rules.make
