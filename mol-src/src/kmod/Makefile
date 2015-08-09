
include			../../config/Makefile.top

SUBDIRS-$(PPC)		= Linux
SUBDIRS-$(OSX)		= . Darwin
SUBDIRS-$(MPC107)	= mpc107

ifeq ($(OSX),y)
include			Makefile.osx
endif

include			$(rules)/Rules.make
