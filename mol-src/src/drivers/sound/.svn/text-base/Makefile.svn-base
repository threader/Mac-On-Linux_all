
include			../../../config/Makefile.top

XTARGETS		= sound
sound-OBJS		= $(obj-y)

INCLUDES		= -I../include $(SDL_DIRS)

obj-$(CONFIG_ALSA)	+= alsa.o
obj-$(CONFIG_OSS)	+= oss.o
obj-$(CONFIG_SDL_SOUND) += sdl_snd.o
obj-y			+= sound.o

include			$(rules)/Rules.make
