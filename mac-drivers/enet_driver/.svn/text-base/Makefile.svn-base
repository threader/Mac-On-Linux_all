#   File:       Makefile
#   Target:     EnetDriver, All

MAKEFILE     = Makefile
¥MondoBuild¥ = {MAKEFILE}  # Make blank to avoid rebuilds when makefile is modified
Includes     =  -i :Src: -i ::Shared: -i :Linux:
Sym¥PPC      = 
Sym¥PPCLink  = -@export EnetDriver.Æ.exp
ObjDir¥PPC   = :obj:

PPCCOptions  = {Includes} {Sym¥PPC} -opt speed -w off

Objects¥PPC  = ¶
		{ObjDir¥PPC}vsprintf.c.x 		¶
		{ObjDir¥PPC}logger.c.x			¶
		{ObjDir¥PPC}LinuxOSI.s.x 		¶
		¶
		{ObjDir¥PPC}EnetDriver.c.x 

All Ä EnetDriver

# Note... the -w flag suppresses warnings (unfortunately, we currently
# must link to the InterfaceLib which causes a lot of multiply defined symbols)
#
EnetDriver ÄÄ {¥MondoBuild¥} {Objects¥PPC}
	PPCLink ¶
		-o ::OF:osi_enet.nw {Sym¥PPCLink} ¶
		{Objects¥PPC} ¶
		-t 'shlb' ¶
		-c '????' ¶
		-w -init CFMInitialization -term CFMTermination ¶
		-xm s ¶
		"{SharedLibraries}DriverLoaderLib" 		¶
		"{SharedLibraries}DriverServicesLib"	¶
		"{SharedLibraries}NameRegistryLib" 		¶
		"{SharedLibraries}OpenTptModuleLib" 	¶
		¶
		"{PPCLibraries}PPCCRuntime.o" 

#		"{SharedLibraries}InterfaceLib"			¶
#		"{PPCLibraries}PPCToolLibs.o" ¶
#		"{SharedLibraries}StdCLib" ¶
#		"{SharedLibraries}MathLib" ¶
#		"{PPCLibraries}StdCRuntime.o" ¶

{ObjDir¥PPC} Ä :Src:
{ObjDir¥PPC} Ä ::Shared:


.c.x Ä .c
	{PPCC} {depDir}{default}.c -o {Targ} {PPCCOptions}

.s.x Ä .s
	{PPCAsm} {depDir}{default}.s -o {Targ} {PPCAOptions}
