
include 	Makefile.top

SUBDIRS		:= lxdialog kconfig

%onfig: all-local
		$(MAKE) -C kconfig $@

$(top_srcdir)/.config:
	true

include 	$(rules)/Rules.make
