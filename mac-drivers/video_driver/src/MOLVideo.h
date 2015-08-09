#ifndef __MOL_VIDEO_H__
#define __MOL_VIDEO_H__

extern OSStatus		MOLVideo_Init();
extern OSStatus		MOLVideo_Exit();

extern OSStatus		MOLVideo_Open();
extern OSStatus		MOLVideo_Close();

extern OSStatus		MOLVideo_SetDepth(UInt32 bpp);

extern OSStatus		MOLVideo_SetColorEntry(UInt32 index, RGBColor *color);
extern OSStatus		MOLVideo_GetColorEntry(UInt32 index, RGBColor *color);
extern OSStatus		MOLVideo_ApplyColorMap(void);

/* Hardware cursor */
extern int		MOLVideo_SupportsHWCursor(void);
extern OSStatus 	MOLVideo_GetHWCursorDrawState(VDHardwareCursorDrawStateRec *hwCursDStateRec);
extern OSStatus		MOLVideo_SetHWCursor(VDSetHardwareCursorRec *setHwCursRec);
extern OSStatus		MOLVideo_DrawHWCursor(VDDrawHardwareCursorRec *drawHwCursRec);

extern OSStatus 	MOLVideo_GetNextResolution(VDResolutionInfoRec *resInfo);

#endif
