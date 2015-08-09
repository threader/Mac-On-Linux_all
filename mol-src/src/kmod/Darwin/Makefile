
include			../../../config/Makefile.top

BUNDLE			= MolExt.kext
BINNAME			= MolExt
BUNDLE-LIB		= libmolkern.a

INCLUDES		= -I../include -I$(OINC)/kern
CFLAGS			+= $(KERN_CFLAGS)
CXXFLAGS		+= -Os $(KERN_CXXFLAGS)

XTARGETS		= molkern


molkern-OBJS		= $(obj-y) MolExt.o stub.o dev.o alloc.o \
			  archmisc.o archfault.o ../libkcommon.a \
			  support.o


#########################################################################

insert:
	test $(BUNDLE)
	sudo rm -rf /tmp/$(BUNDLE)
	cp -rf $(mollib)/modules/$(BUNDLE) /tmp/$(BUNDLE)
	sudo chown -R root.wheel /tmp/$(BUNDLE)
	sudo find /tmp/$(BUNDLE) -type d -exec /bin/chmod 0755 '{}' \;
	sudo find /tmp/$(BUNDLE) -type f -exec /bin/chmod 0644 '{}' \;
	sudo kextload -t /tmp/$(BUNDLE)
	sudo rm -rf /tmp/$(BUNDLE)
remove:
	sudo kextunload -b se.ibrium.iokit.MolExt ; true
reload: remove insert 

kclean:
	sudo rm -rf /tmp/$(BUNDLE)

#########################################################################

include		Rules.osx
include		$(rules)/Rules.make
