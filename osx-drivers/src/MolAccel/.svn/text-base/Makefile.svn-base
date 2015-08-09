
include			../../rules/Makefile.top

CFLAGS			+= -msoft_float
CXXFLAGS		+= -msoft_float

ifdef OSX_10_1
KMODS			= MolAccel1
else
KMODS			= MolAccel2 MolAccel3
endif

MolAccel1-VERSION	= 1.1
MolAccel1-OBJS		= MolAccel.o MolAccel1.o

MolAccel2-VERSION	= 1.1
MolAccel2-OBJS		= MolAccel.o MolAccel2.o

MolAccel3-VERSION	= 1.1
MolAccel3-OBJS		= MolAccel.o MolAccel3.o

include			$(rules)/Rules.osx
include			$(rules)/Rules.make
