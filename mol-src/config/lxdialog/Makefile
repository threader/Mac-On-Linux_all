
include ../Makefile.top

PROGRAMS	:= lxdialog
lxdialog-OBJS	:= checklist.o menubox.o textbox.o yesno.o inputbox.o \
		   util.o lxdialog.o msgbox.o
lxdialog-LIBS   := -lncurses

CFLAGS		+= -DLOCALE 

ifeq (/usr/include/ncurses/ncurses.h, $(wildcard /usr/include/ncurses/ncurses.h))
        CFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses.h>"
else
ifeq (/usr/include/ncurses/curses.h, $(wildcard /usr/include/ncurses/curses.h))
        CFLAGS += -I/usr/include/ncurses -DCURSES_LOC="<ncurses/curses.h>"
else
ifeq (/usr/include/ncurses.h, $(wildcard /usr/include/ncurses.h))
        CFLAGS += -DCURSES_LOC="<ncurses.h>"
else
	CFLAGS += -DCURSES_LOC="<curses.h>"
endif
endif
endif

ncurses:
	@echo "main() {}" > lxtemp.c
	@if $(HOSTCC) lxtemp.c  $(HOST_LOADLIBES); then \
		rm -f lxtemp.c a.out; \
	else \
		rm -f lxtemp.c; \
		echo -e "\007" ;\
		echo ">> Unable to find the Ncurses libraries." ;\
		echo ">>" ;\
		echo ">> You must have Ncurses installed in order" ;\
		echo ">> to use 'make menuconfig'" ;\
		echo ;\
		exit 1 ;\
	fi

lxdialog: ncurses
.PHONY: ncurses

include 	$(rules)/Rules.make
