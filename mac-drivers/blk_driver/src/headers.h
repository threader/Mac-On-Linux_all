
#ifndef __MY_HEADERS__
#define __MY_HEADERS__

#ifndef FALSE
#define TRUE	1
#define FALSE	0
#endif

#include <ConditionalMacros.h>
#if GENERATINGPOWERPC == 0
	<< error: the following will not work >>
#endif
#include <Types.h>
#include <MixedMode.h>
#include <OSUtils.h>
#include <Files.h>
#include <QuickdrawText.h>
#include <QuickDraw.h>
#include <Events.h>
#include <Errors.h>
#include <Memory.h>
#include <Menus.h>
#include <Controls.h>
#include <Windows.h>
#include <TextEdit.h>
#include <Dialogs.h>
#include <MachineExceptions.h>
#include <PCI.h>
#include <Kernel.h>
#include <NameRegistry.h>
#include <CodeFragments.h>
#include <Devices.h>
#include <Fonts.h>
#include <Resources.h>
#include <LowMem.h>
#include <DriverServices.h>
#include <Interrupts.h>
#include <SCSI.h>

#endif /* __MY_HEADERS__ */
