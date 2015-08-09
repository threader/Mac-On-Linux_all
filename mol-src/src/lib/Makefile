
include			../../config/Makefile.top

XTARGETS		= common
common-OBJS		= $(obj-y) extralib.o llseek.o elfload.o unicode.o backtrace.o

obj-$(NEED_OBSTACK)	+= obstack.o
obj-$(NEED_POLL)	+= poll.o
obj-$(PPC)		+= cache.o

include			$(rules)/Rules.make
