
include			../../../config/Makefile.top

XTARGETS		= net
net-OBJS		= $(obj-y)

obj-$(OSX)		+= if-tun-darwin.o
obj-$(LINUX)		+= if-tun.o if-sheep.o
obj-y			+= iface.o mac_enet.o enet2.o packet.o
obj-$(PPC)		+= ipchksum-ppc.o
obj-$(X86)		+= ipchksum-x86.o

INCLUDES		= -I../include

include			$(rules)/Rules.make
