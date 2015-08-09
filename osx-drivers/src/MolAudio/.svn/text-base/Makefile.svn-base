
include			../../rules/Makefile.top

MolAudioDevice-VERSION	= 1.1
KMODS			= MolAudioDevice
MolAudioDevice-OBJS	= MolAudioClip.o MolAudioDevice.o MolAudioEngine.o \
			  PCMBlitterLibPPC.o ../common/$(ODIR)/libcommon.a

$(ODIR)/_blitter.s: PCMBlitterLibPPC.c
	$(COMPILE) $(DEPFLAGS) -S -c $< -o $@
	@$(DEPEXTRA)

$(ODIR)/PCMBlitterLibPPC.o: $(ODIR)/_blitter.s
	$(CC) -force_cpusubtype_ALL -c $< -o $@

include			$(rules)/Rules.osx
include			$(rules)/Rules.make
