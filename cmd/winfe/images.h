#ifndef NETSCAPE_NATIVE_IMAGES_H
#define NETSCAPE_NATIVE_IMAGES_H

#include "frontend.h"
#include "il.h"

#ifdef __cplusplus
extern "C" {
#endif

void WFE_ImageInitContext(MWContext *context);
void WFE_ImageBackground(MWContext *context, uint8 red, uint8 green, uint8 blue);
int WFE_DrawImage(HDC dc, MWContext *context, LO_ImageStruct *image);
void WFE_DrawBackdrop(HDC dc, MWContext *context, LO_ImageStruct *image,
                      int width, int height, int scroll_x, int scroll_y);

/* Native view hooks. The drawing DC already uses document coordinates. */
HDC WFE_DrawingContext(MWContext *context);
void WFE_SetBackdrop(MWContext *context, LO_ImageStruct *image);
void WFE_ClearBackdrop(MWContext *context, LO_ImageStruct *image);
void WFE_RecordImage(MWContext *context, LO_ImageStruct *image);

#ifdef __cplusplus
}
#endif
#endif
