#   File:       BlockDriver.make
#   Target:     BlockDriver, All
#   Sources:    String:Desktop Folder:New MOL-drivers:Shared:vsprintf.c
#               String:Desktop Folder:New MOL-drivers:Shared:MakeFunction.s
#               String:Desktop Folder:New MOL-drivers:Shared:logger.c
#               String:Desktop Folder:New MOL-drivers:Shared:LinuxOSI.s
#               :Src:driver.c
#               :Src:general.c
#               :Src:main.c
#               :Src:util.c
#   Created:    Saturday, February 5, 2000 03:28:35 PM

MAKEFILE     = Makefile
¥MondoBuild¥ = {MAKEFILE}  # Make blank to avoid rebuilds when makefile is modified
Includes     = ¶
				-i :Src: ¶
				-i ::Shared:
Sym¥PPC      = 
Sym¥PPCLink  = -@export BlockDriver.Æ.exp
ObjDir¥PPC   = :obj:

PPCCOptions  = {Includes} {Sym¥PPC} -opt speed -w off

Objects¥PPC  = ¶
		{ObjDir¥PPC}vsprintf.c.x 		¶
		{ObjDir¥PPC}logger.c.x			¶
		{ObjDir¥PPC}LinuxOSI.s.x 		¶
		¶
		{ObjDir¥PPC}driver.c.x 			¶
		{ObjDir¥PPC}general.c.x 		¶
		{ObjDir¥PPC}main.c.x 			¶
		{ObjDir¥PPC}util.c.x

All Ä BlockDriver

# Note... the -w flag suppresses warnings (unfortunately, we currently
# must link to the InterfaceLib which causes a lot of multiply defined symbols)
#
BlockDriver ÄÄ {¥MondoBuild¥} {Objects¥PPC}
	PPCLink ¶
		-o ::OF:osi_blk.nw {Sym¥PPCLink} ¶
		{Objects¥PPC} ¶
		-t 'shlb' ¶
		-c '????' ¶
		-w -init CFMInitialize -term CFMTerminate ¶
		-xm s ¶
		"{SharedLibraries}DriverLoaderLib" 		¶
		"{SharedLibraries}DriverServicesLib"	¶
		"{SharedLibraries}NameRegistryLib" 		¶
		"{SharedLibraries}InterfaceLib"			¶
		¶
		"{PPCLibraries}PPCCRuntime.o" 

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
