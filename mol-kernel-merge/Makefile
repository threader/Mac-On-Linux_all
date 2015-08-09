#
# Makefile for the Linux host MOL driver
#

obj-$(CONFIG_MOL)	:= mol.o

mol-objs		:= dev.o misc.o mmu.o hostirq.o init.o hash.o \
			   emu.o mmu.o mmu_fb.o mmu_io.o mmu_tracker.o skiplist.o \
		  	   mtable.o fault.o ptaccess.o misc.o moldbg.o \
			   context.o actions.o 

			   ### FIXME - these are still broken...
			   #traps.o performance.o 
