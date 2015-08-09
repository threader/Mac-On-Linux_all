
BUNDLE			= MolAudioDevice.kext
BUNDLE2			= 

KMOD_SRCS		= MolAudioClip.cpp MolAudioDevice.cpp MolAudioEngine.cpp lib.cpp PCMBlitterLibPPC.c
KMOD2_SRCS		= 

BINNAME			= MolAudioDevice
BINNAME2		= 

include 		../Makefile.inc

NOFPUCOMPILE		= $(shell echo $(CXXCOMPILE) | sed s/-msoft-float//g)
NOFPUCOMPILE_C		= $(shell echo $(COMPILE) | sed s/-msoft-float//g)

# This file needs to be compiled with the FPU turned on
MolAudioClip.o:		MolAudioClip.cpp
	$(NOFPUCOMPILE) -c $< -o $@

_blitter.s: PCMBlitterLibPPC.c
	$(NOFPUCOMPILE_C) -S -c $< -o $@
PCMBlitterLibPPC.o: _blitter.s
	$(CC) -force_cpusubtype_ALL -c $< -o $@

MolAudioClip.o:		MolAudioEngine.h
