
include		../../rules/Makefile.top

CFLAGS			+= -msoft_float
CXXFLAGS		+= -msoft_float

MolPE-VERSION	= 1.1
KMODS		= MolPE
MolPE-OBJS	= MolPE.o MolCPU.o MolPIC.o MolNVRAM.o ../common/$(ODIR)/libcommon.a		  
include		$(rules)/Rules.osx
include		$(rules)/Rules.make

