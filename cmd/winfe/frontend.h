#ifndef NETSCAPE_NATIVE_FRONTEND_H
#define NETSCAPE_NATIVE_FRONTEND_H
#include "xp.h"
#include "net.h"
#include "layout.h"
#include "fe_proto.h"
#ifdef __cplusplus
extern "C" {
#endif
#ifdef NETSCAPE_USE_BEARSSL
/* Called with a value snapshot before the request URL is freed. */
void WFE_TLSRequestComplete(MWContext *, const NETTLSInfo *);
int WFE_TLSInit(void);
void WFE_TLSShutdown(void);
int WFE_TLSConfigure(const char *bundle, int allow_legacy, char *error, size_t error_size);
void WFE_TLSSetLegacy(int allow_legacy);
/* Borrowed diagnostic, valid until the next TLS configure/init/shutdown. */
const char *WFE_TLSError(void);
#endif
int WFE_Init(void);
void WFE_Shutdown(void);
void WFE_InitContext(MWContext *context, void *window);
void WFE_InitGridContext(MWContext *context, void *window, MWContext *parent, const char *name);
MWContext *WFE_URLTarget(MWContext *context, URL_Struct *url);
void WFE_TransferURL(URL_Struct *, MWContext *old_context, MWContext *new_context);
void WFE_NavigateTarget(MWContext *context, const char *address, const char *target);
MWContext *WFE_NewGridWindow(MWContext *, void *, int32, int32, int32, int32, char *, char *, int8, NET_ReloadMethod, Bool);
void *WFE_FreeGridWindow(MWContext *context, XP_Bool save);
void WFE_ResizeGridWindow(MWContext *, int32, int32, int32, int32);
void WFE_LoadGridHistory(MWContext *, void *, NET_ReloadMethod);
void WFE_DestroyContext(MWContext *context);
void WFE_Load(MWContext *context, const char *address);
void WFE_ViewSource(MWContext *context,const char *address);
void WFE_DocumentAddress(MWContext *context,const char *address);
void WFE_ConfigureChrome(MWContext *context,const Chrome *chrome);
MWContext *WFE_NewWindow(const char *address);
void WFE_BeginLoad(MWContext *context, const char *address);
HWND WFE_DocumentWindow(MWContext *context);
void WFE_GridEdge(MWContext *,LO_EdgeStruct *);
void WFE_FreeGridEdge(MWContext *,LO_EdgeStruct *);
void WFE_FullWindowSize(MWContext *,int32 *,int32 *);
int WFE_IsPrinting(MWContext *context);
void WFE_RecordPrintSpan(MWContext *context,int y,int height);
void WFE_Pump(void);
void WFE_Status(MWContext *context, const char *message);
int WFE_Message(MWContext *context, const char *message, int confirm);
int WFE_AboutOutstanding(void);
void WFE_ChromeProgress(MWContext *context, int32 received, int32 length);
void WFE_Title(MWContext *context, const char *title);
void WFE_Invalidate(MWContext *context);
void WFE_WindowSize(MWContext *context, int32 *width, int32 *height);
void WFE_DocumentSize(MWContext *context, int32 width, int32 height);
void WFE_DocumentDefaults(MWContext *context);
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
void WFE_ShowMailSettings(MWContext *context);
int WFE_LoadImagesAutomatically(MWContext *context);
MWContext *WFE_NewMailWindow(int is_news, const char *server);
char *WFE_NativePrompt(MWContext *context, const char *message, const char *initial, int password);
#ifdef __cplusplus
}
#endif
#endif
