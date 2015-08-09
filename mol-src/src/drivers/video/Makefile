
include			../../../config/Makefile.top

XTARGETS		= video
video-OBJS		= $(obj-y) $(obj-y-y)

INCLUDES		= -I../include

X11			= $(if $(CONFIG_X11),$(HAVE_X11))

obj-$(X11)		+= x11.o xvideo.o checksum.o
obj-$(PPC)-$(X11)	+= checksum-ppc.o
obj-$(CONFIG_VNC)	+= vncvideo.o
obj-$(CONFIG_XDGA)	+= xdga.o
obj-$(CONFIG_FBDEV)	+= fbdev.o
obj-y			+= video.o offscreen.o


CFLAGS			+= $(X_CFLAGS) $(XDGA_CFLAGS) 


include			$(rules)/Rules.make
