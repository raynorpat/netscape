#ifndef NETSCAPE_NATIVE_FRONTEND_H
#define NETSCAPE_NATIVE_FRONTEND_H
#include "xp.h"
#include "net.h"
#include "layout.h"
#include "fe_proto.h"
#ifdef __cplusplus
extern "C" {
#endif
int WFE_Init(void);
void WFE_Shutdown(void);
void WFE_InitContext(MWContext *context, void *window);
void WFE_DestroyContext(MWContext *context);
void WFE_Load(MWContext *context, const char *address);
void WFE_BeginLoad(MWContext *context, const char *address);
HWND WFE_DocumentWindow(MWContext *context);
void WFE_Pump(void);
void WFE_Status(MWContext *context, const char *message);
void WFE_Title(MWContext *context, const char *title);
void WFE_Invalidate(MWContext *context);
void WFE_WindowSize(MWContext *context, int32 *width, int32 *height);
void WFE_DocumentSize(MWContext *context, int32 width, int32 height);
void WFE_Scroll(MWContext *context, int32 x, int32 y);
void WFE_Position(MWContext *context, int32 *x, int32 *y);
void WFE_Background(MWContext *context, unsigned long color);
void WFE_TextMetrics(MWContext *context, LO_TextStruct *text, LO_TextInfo *info);
void WFE_Text(MWContext *context, LO_TextStruct *text, int32 start, int32 end, int background);
void WFE_Rect(MWContext *context, int32 x, int32 y, int32 width, int32 height, unsigned long color, int fill);
void WFE_Unsupported(MWContext *context, const char *feature);
void WFE_Complete(MWContext *context, int status);
void *WFE_SetTimer(TimeoutCallbackFunction callback, void *closure, uint32 milliseconds);
void WFE_KillTimer(void *timer);
const char *WFE_ProfileDirectory(void);
int WFE_RunSmokeTest(const char *report, const char *url);
void WFE_RegisterNativeWindow(MWContext *context, HWND window);
void WFE_UnregisterNativeWindow(MWContext *context);
void WFE_MailChanged(MWContext *context, unsigned int changes);
MWContext *WFE_NewMailWindow(int is_news, const char *server);
char *WFE_NativePrompt(MWContext *context, const char *message, const char *initial, int password);
#ifdef __cplusplus
}
#endif
#endif
