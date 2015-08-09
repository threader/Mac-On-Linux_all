
include			../../config/Makefile.top

SUBDIRS			= disk video net sound $(sub-y)
sub-$(CONFIG_OLDWORLD)	= oldworld

XTARGETS		= drivers
drivers-OBJS		= $(obj-y)
obj-y			+= driver_mgr.o ioports.o keycodes.o kbd.o via-cuda.o \
			   adb.o gc.o pic.o osi_pic.o nvram.o escc.o dbdma.o pci.o \
			   pci-bridges.o osi_mouse.o osi_driver.o usb.o rtas.o hostirq.o

obj-$(LINUX)		+= console.o
obj-$(CONFIG_USBDEV)	+= usbdev.o
obj-$(CONFIG_TTYDRIVER)	+= tty.o
obj-$(CONFIG_PCIPROXY)	+= pciproxy.o
obj-$(CONFIG_SDL)	+= sdl.o
obj-y			+= disk/libdisk.a
obj-y			+= video/libvideo.a
obj-y			+= net/libnet.a
obj-y			+= sound/libsound.a
obj-$(CONFIG_OLDWORLD)	+= oldworld/liboldworld.a

include			$(rules)/Rules.make
