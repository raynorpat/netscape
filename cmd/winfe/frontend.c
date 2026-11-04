/* Native frontend boundary for the 3.02 core. Optional unavailable services
 * fail explicitly; browser rendering, navigation and event dispatch are real. */
#include "frontend.h"
#include "il.h"
#include "shist.h"
#include "libi18n.h"
#include "csid.h"
#include "mkformat.h"
#include "gui.h"
#include "msgcom.h"
#include "mailsettings.h"
#include "composer.h"
#include "images.h"
#include "forms.h"
#include "merrors.h"
#include "downloads.h"
#include "history.h"
#include "np.h"
#include <nspr/prglobal.h>
#include <stdio.h>
#include <stdarg.h>

extern int WFE_PlatformInit(void);
extern void WFE_PlatformShutdown(void);
extern void WFE_PollNetworking(void);
extern void WFE_PlatformForgetContext(MWContext *context);
extern void WFE_SetCallNetlibAllTheTime(MWContext *context);
extern void WFE_ClearCallNetlibAllTheTime(MWContext *context);
extern int WFE_Message(MWContext *, const char *, int);
extern MWContext *WFE_NewWindow(const char *address);
extern void WFE_CloseWindow(MWContext *context);
static ContextFuncs context_functions;
static PA_InitData parser_data;
static char *resource_strings[65536];
/* Export concrete C callbacks while other core units use the dispatch macros. */
#undef FE_CreateNewDocWindow
#undef FE_LayoutNewDocument
#undef FE_SetDocTitle
#undef FE_FinishedLayout
#undef FE_TranslateISOText
#undef FE_GetTextInfo
#undef FE_GetImageInfo
#undef FE_GetEmbedSize
#undef FE_GetJavaAppSize
#undef FE_GetFormElementInfo
#undef FE_GetFormElementValue
#undef FE_ResetFormElement
#undef FE_SetFormElementToggle
#undef FE_FreeFormElement
#undef FE_FreeImageElement
#undef FE_FreeEmbedElement
#undef FE_FreeJavaAppElement
#undef FE_HideJavaAppElement
#undef FE_FreeEdgeElement
#undef FE_FormTextIsSubmit
#undef FE_DisplaySubtext
#undef FE_DisplayText
#undef FE_DisplayImage
#undef FE_DisplayEmbed
#undef FE_DisplayJavaApp
#undef FE_DisplaySubImage
#undef FE_DisplayEdge
#undef FE_DisplayTable
#undef FE_DisplayCell
#undef FE_DisplaySubDoc
#undef FE_DisplayLineFeed
#undef FE_DisplayHR
#undef FE_DisplayBullet
#undef FE_DisplayFormElement
#undef FE_ClearView
#undef FE_SetDocDimension
#undef FE_SetDocPosition
#undef FE_GetDocPosition
#undef FE_BeginPreSection
#undef FE_EndPreSection
#undef FE_SetProgressBarPercent
#undef FE_SetBackgroundColor
#undef FE_Progress
#undef FE_Alert
#undef FE_SetCallNetlibAllTheTime
#undef FE_ClearCallNetlibAllTheTime
#undef FE_GraphProgressInit
#undef FE_GraphProgressDestroy
#undef FE_GraphProgress
#undef FE_UseFancyFTP
#undef FE_UseFancyNewsgroupListing
#undef FE_FileSortMethod
#undef FE_ShowAllNewsArticles
#undef FE_Confirm
#undef FE_Prompt
#undef FE_PromptUsernameAndPassword
#undef FE_PromptPassword
#undef FE_EnableClicking
#undef FE_ImageSize
#undef FE_ImageData
#undef FE_ImageIcon
#undef FE_ImageOnScreen
#undef FE_SetColormap
#undef FE_AllConnectionsComplete
#define MAKE_FE_FUNCS_PREFIX(name) FE_##name
#define MAKE_FE_FUNCS_EXTERN
#include "mk_cx_fn.h"

static void url_complete(URL_Struct *url, int status, MWContext *context)
{
    if (status == MK_CHANGING_CONTEXT) { WFE_Complete(context, 0); return; }
    if (status < 0 && url->error_msg) WFE_Status(context, url->error_msg);
    WFE_Complete(context, status);
    NET_FreeURLStruct(url);
}

int WFE_Init(void)
{
    PA_Functions parser_functions;
    if (!WFE_PlatformInit()) return 0;
    PR_Init("Netscape-Win32", 1, 1, 0);
    XP_AppName = "Netscape";
    XP_AppCodeName = "Mozilla";
    XP_AppVersion = "3.02 (Win32)";
    memset(&parser_functions, 0, sizeof(parser_functions));
    parser_functions.PA_ParsedTag = LO_ProcessTag;
    PA_ParserInit(&parser_functions);
    parser_data.output_func = LO_ProcessTag;
    if (NET_InitNetLib(32768, 20) < 0) { WFE_PlatformShutdown(); return 0; }
    WFE_ApplyMailPreferences();
    NET_SetDiskCacheSize(0);
    NET_SetMemoryCacheSize(4 * 1024 * 1024);
    NET_RegisterMIMEDecoders();
    NET_RegisterContentTypeConverter(TEXT_HTML, FO_PRESENT, NULL, INTL_ConvCharCode);
    NET_RegisterContentTypeConverter(INTERNAL_PARSER, FO_PRESENT, &parser_data, PA_BeginParseMDL);
    NET_RegisterContentTypeConverter(TEXT_PLAIN, FO_PRESENT, NULL, NET_PlainTextConverter);
    NET_RegisterContentTypeConverter(UNKNOWN_CONTENT_TYPE, FO_PRESENT, NULL, NET_PlainTextConverter);
    MSG_RegisterConverters();
    NET_RegisterContentTypeConverter(TEXT_HTML, FO_QUOTE_MESSAGE, NULL, INTL_ConvCharCode);
    NET_RegisterContentTypeConverter(INTERNAL_PARSER, FO_QUOTE_MESSAGE, &parser_data, PA_BeginParseMDL);
    NET_RegisterContentTypeConverter(TEXT_PLAIN, FO_QUOTE_MESSAGE, NULL, NET_PlainTextConverter);
    NPL_Init();
    WFE_DownloadInit();
    NET_RegisterContentTypeConverter(UNKNOWN_CONTENT_TYPE, FO_PRESENT, NULL, fe_MakeSaveAsStream);
    return 1;
}

void WFE_Shutdown(void)
{
    unsigned int index;
    WFE_DownloadShutdown();
    IL_Shutdown();
    NET_ShutdownNetLib();
    WFE_PlatformShutdown();
    for (index = 0; index < 65536; ++index) {
        free(resource_strings[index]);
        resource_strings[index] = NULL;
    }
}

void WFE_InitGridContext(MWContext *context, void *window, MWContext *parent, const char *name)
{
    memset(context, 0, sizeof(*context));
    context->type = MWContextBrowser;
    context->fe.cx = window;
    context->convertPixX = context->convertPixY = 1;
    context->win_csid = context->doc_csid = CS_LATIN1;
    context->grid_parent = parent;
    context->is_grid_cell = parent != NULL;
    context->name = name && *name ? _strdup(name) : NULL;
    if (parent) {
        context->win_csid = parent->win_csid;
        context->doc_csid = parent->doc_csid;
    }
    context->fancyFTP = context->fancyNews = TRUE;
    context->funcs = &context_functions;
#define MAKE_FE_FUNCS_PREFIX(name) FE_##name
#define MAKE_FE_FUNCS_ASSIGN context_functions.
#include "mk_cx_fn.h"
    SHIST_InitSession(context);
    XP_AddContextToList(context);
    IL_InitContext(context);
    WFE_ImageInitContext(context);
}
void WFE_InitContext(MWContext *context, void *window) { WFE_InitGridContext(context, window, NULL, NULL); }

void WFE_DestroyContext(MWContext *context)
{
    XP_InterruptContext(context);
    WFE_PlatformForgetContext(context);
    if (context->msgframe && context->type == MWContextMail) MSG_CleanupMailContext(context);
    else if (context->msgframe && context->type == MWContextNews) MSG_CleanupNewsContext(context);
    if (context->msg_cframe) MSG_CleanupMessageCompositionContext(context);
    LO_DiscardDocument(context);
    WFE_FormForgetContext(context);
    IL_DeleteImages(context);
    SHIST_EndSession(context);
    XP_RemoveContextFromList(context);
    IL_ScourContext(context);
    free(context->name);
    context->name = NULL;
    free(context->title);
    context->title = NULL;
}

void WFE_Load(MWContext *context, const char *address)
{
    URL_Struct *url;
    XP_InterruptContext(context);
    url = NET_CreateURLStruct(address, NET_DONT_RELOAD);
    if (!url) { WFE_Status(context, "Could not allocate a URL"); return; }
    WFE_Status(context, address);
    NET_GetURL(url, context->type == MWContextBrowser ? FO_CACHE_AND_PRESENT : FO_PRESENT, context, url_complete);
}

void WFE_Pump(void) { WFE_PollNetworking(); }
int FE_GetURL(MWContext *context, URL_Struct *url) { context = WFE_URLTarget(context, url); if (!context) { NET_FreeURLStruct(url); return -1; } if (context->type == MWContextBrowser) { WFE_HistorySavePosition(context); XP_InterruptContext(context); } WFE_BeginLoad(context, url->address); return NET_GetURL(url, context->type == MWContextBrowser ? FO_CACHE_AND_PRESENT : FO_PRESENT, context, url_complete); }
MWContext *FE_CreateNewDocWindow(MWContext *context, URL_Struct *url) { return WFE_NewWindow(url ? url->address : NULL); }
MWContext *FE_MakeNewWindow(MWContext *context, URL_Struct *url, char *name, Chrome *chrome) { MWContext *created = WFE_NewWindow(NULL); if (created && name && name[0] != '_') created->name = _strdup(name); if (created && url) { free(url->window_target); url->window_target = _strdup(""); FE_GetURL(created, url); } return created; }
MWContext *FE_MakeBlankWindow(MWContext *context, URL_Struct *url, char *name) { return FE_CreateNewDocWindow(context, url); }
void FE_DestroyWindow(MWContext *context) { WFE_CloseWindow(context); }
void FE_SetWindowLoading(MWContext *context, URL_Struct *url, Net_GetUrlExitFunc **exit_func) { WFE_BeginLoad(context, url->address); if (exit_func) *exit_func = url_complete; }
void FE_SetRefreshURLTimer(MWContext *context, uint32 seconds, char *address) { WFE_Unsupported(context, "Automatic page refresh"); }
void *FE_SetTimeout(TimeoutCallbackFunction callback, void *closure, uint32 milliseconds) { return WFE_SetTimer(callback, closure, milliseconds); }
void FE_ClearTimeout(void *timer) { WFE_KillTimer(timer); }
void FE_SetCallNetlibAllTheTime(MWContext *context) { WFE_SetCallNetlibAllTheTime(context); }
void FE_ClearCallNetlibAllTheTime(MWContext *context) { WFE_ClearCallNetlibAllTheTime(context); }
void FE_ShowScrollBars(MWContext *context, XP_Bool show) { }

void FE_LayoutNewDocument(MWContext *context, URL_Struct *url, int32 *width, int32 *height, int32 *margin_width, int32 *margin_height)
{
    if (context->type == MWContextBrowser) SHIST_AddDocument(context, SHIST_CreateHistoryEntry(url, ""));
    WFE_WindowSize(context, width, height);
    if (!context->is_grid_cell) *margin_width = *margin_height = 8;
    WFE_Scroll(context, 0, 0);
}
void FE_SetDocTitle(MWContext *context, char *title) { SHIST_SetTitleOfCurrentDoc(&context->hist, title); WFE_Title(context, title); }
void FE_FinishedLayout(MWContext *context) { WFE_Invalidate(context); }
char *FE_TranslateISOText(MWContext *context, int charset, char *text) { return text; }
int FE_GetTextInfo(MWContext *context, LO_TextStruct *text, LO_TextInfo *info) { WFE_TextMetrics(context, text, info); return 0; }
void FE_DisplayText(MWContext *context, int location, LO_TextStruct *text, XP_Bool background) { WFE_Text(context, text, 0, text->text_len, background); }
void FE_DisplaySubtext(MWContext *context, int location, LO_TextStruct *text, int32 start, int32 end, XP_Bool background) { WFE_Text(context, text, start, end + 1, background); }
void FE_ClearView(MWContext *context, int which) { WFE_Invalidate(context); }
void FE_SetBackgroundColor(MWContext *context, uint8 red, uint8 green, uint8 blue) { WFE_Background(context, RGB(red, green, blue)); WFE_ImageBackground(context, red, green, blue); }
void FE_SetDocDimension(MWContext *context, int location, int32 width, int32 height) { WFE_DocumentSize(context, width, height); }
void FE_SetDocPosition(MWContext *context, int location, int32 x, int32 y) { WFE_Scroll(context, x, y); }
void FE_ScrollDocTo(MWContext *context, int location, int32 x, int32 y) { WFE_Scroll(context, x, y); }
void FE_GetDocPosition(MWContext *context, int location, int32 *x, int32 *y) { WFE_Position(context, x, y); }
void FE_BeginPreSection(MWContext *context) { }
void FE_EndPreSection(MWContext *context) { }
void FE_Progress(MWContext *context, const char *message) { WFE_Status(context, message); }
void FE_Alert(MWContext *context, const char *message) { WFE_Message(context, message, 0); }
XP_Bool FE_Confirm(MWContext *context, const char *message) { return WFE_Message(context, message, 1); }
char *FE_Prompt(MWContext *context, const char *message, const char *initial) { return WFE_NativePrompt(context, message, initial, 0); }
char *FE_PromptPassword(MWContext *context, const char *message) { return WFE_NativePrompt(context, message, "", 1); }
XP_Bool FE_PromptUsernameAndPassword(MWContext *context, const char *message, char **user, char **password) { char *name = WFE_NativePrompt(context, message, user && *user ? *user : "", 0); char *secret; if (!name) return FALSE; secret = FE_PromptPassword(context, "Password"); if (!secret) { free(name); return FALSE; } if (user) { *user = name; } else free(name); if (password) { *password = secret; } else free(secret); return TRUE; }
void FE_SetProgressBarPercent(MWContext *context, int32 percent) { }
void FE_GraphProgressInit(MWContext *context, URL_Struct *url, int32 length) { }
void FE_GraphProgressDestroy(MWContext *context, URL_Struct *url, int32 length, int32 total) { }
void FE_GraphProgress(MWContext *context, URL_Struct *url, int32 received, int32 delta, int32 length) { WFE_DownloadProgress(context, url, received, length); }
XP_Bool FE_UseFancyFTP(MWContext *context) { return TRUE; }
XP_Bool FE_UseFancyNewsgroupListing(MWContext *context) { return TRUE; }
int FE_FileSortMethod(MWContext *context) { return SORT_BY_NAME; }
XP_Bool FE_ShowAllNewsArticles(MWContext *context) { return FALSE; }
void FE_EnableClicking(MWContext *context) { }
void FE_AllConnectionsComplete(MWContext *context) { if (context->type == MWContextMessageComposition) WFE_ComposerComplete(context); else { WFE_Invalidate(context); if (context->type == MWContextNews) WFE_MailChanged(context, 8); } }
void FE_UpdateStopState(MWContext *context) { }
int FE_EnableBackButton(MWContext *context) { return 0; }
int FE_DisableBackButton(MWContext *context) { return 0; }
int FE_EnableForwardButton(MWContext *context) { return 0; }
int FE_DisableForwardButton(MWContext *context) { return 0; }

/* Optional object types still use explicit placeholder geometry. */
void FE_GetEmbedSize(MWContext *context, LO_EmbedStruct *embed, NET_ReloadMethod reload) { embed->width = embed->height = 32; }
void FE_GetJavaAppSize(MWContext *context, LO_JavaAppStruct *applet, NET_ReloadMethod reload) { }
void FE_FreeEmbedElement(MWContext *context, LO_EmbedStruct *embed) { }
void FE_FreeJavaAppElement(MWContext *context, LJAppletData *applet) { }
void FE_HideJavaAppElement(MWContext *context, void *applet) { }
void FE_FreeEdgeElement(MWContext *context, LO_EdgeStruct *edge) { }
void FE_DisplayEmbed(MWContext *context, int location, LO_EmbedStruct *embed) { WFE_Rect(context, embed->x, embed->y, embed->width, embed->height, RGB(128,128,128), 0); }
void FE_DisplayJavaApp(MWContext *context, int location, LO_JavaAppStruct *applet) { }
void FE_DisplayEdge(MWContext *context, int location, LO_EdgeStruct *edge) { }
void FE_DisplayTable(MWContext *context, int location, LO_TableStruct *table) { WFE_Rect(context, table->x, table->y, table->width, table->height, RGB(128,128,128), 0); }
void FE_DisplayCell(MWContext *context, int location, LO_CellStruct *cell) { }
void FE_DisplaySubDoc(MWContext *context, int location, LO_SubDocStruct *doc) { }
void FE_DisplayLineFeed(MWContext *context, int location, LO_LinefeedStruct *line, XP_Bool background) { }
void FE_DisplayHR(MWContext *context, int location, LO_HorizRuleStruct *rule) { WFE_Rect(context, rule->x, rule->y, rule->width, rule->height, RGB(128,128,128), 1); }
void FE_DisplayBullet(MWContext *context, int location, LO_BullettStruct *bullet) { WFE_Rect(context, bullet->x, bullet->y, bullet->width, bullet->height, RGB(0,0,0), 1); }
void FE_GetFormElementInfo(MWContext *context, LO_FormElementStruct *element) { WFE_FormGetInfo(context, element); }
void FE_GetFormElementValue(MWContext *context, LO_FormElementStruct *element, XP_Bool hide) { WFE_FormGetValue(context, element, hide); }
void FE_ResetFormElement(MWContext *context, LO_FormElementStruct *element) { WFE_FormReset(context, element); }
void FE_SetFormElementToggle(MWContext *context, LO_FormElementStruct *element, XP_Bool toggle) { WFE_FormSetToggle(context, element, toggle); }
void FE_FreeFormElement(MWContext *context, LO_FormElementData *element) { WFE_FormFree(context, element); }
void FE_FormTextIsSubmit(MWContext *context, LO_FormElementStruct *element) { WFE_FormTextIsSubmit(context, element); }
void FE_DisplayFormElement(MWContext *context, int location, LO_FormElementStruct *element) { WFE_FormDisplay(context, location, element); }
MWContext *FE_MakeGridWindow(MWContext *context, void *history, int32 x, int32 y, int32 width, int32 height, char *url, char *name, int8 scrolling, NET_ReloadMethod reload, Bool no_edge) { return WFE_NewGridWindow(context, history, x, y, width, height, url, name, scrolling, reload, no_edge); }
void *FE_FreeGridWindow(MWContext *context, XP_Bool save) { return WFE_FreeGridWindow(context, save); }
void FE_RestructureGridWindow(MWContext *context, int32 x, int32 y, int32 width, int32 height) { WFE_ResizeGridWindow(context, x, y, width, height); }
void FE_LoadGridCellFromHistory(MWContext *context, void *history, NET_ReloadMethod reload) { WFE_LoadGridHistory(context, history, reload); }
void FE_GetFullWindowSize(MWContext *context, int32 *width, int32 *height) { WFE_WindowSize(context, width, height); }
void FE_GetEdgeMinSize(MWContext *context, int32 *size, Bool no_edge) { *size = 0; }
int32 FE_GetContextID(MWContext *context) { return (int32)context; }
void FE_URLEcho(URL_Struct *url, int status, MWContext *context) { }
void FE_UrlChangedContext(URL_Struct *url, MWContext *old_context, MWContext *new_context) { WFE_TransferURL(url, old_context, new_context); }
void FE_DeleteUrlData(URL_Struct *url) { }
XP_Bool FE_UseExternalProtocolModule(MWContext *context, FO_Present_Types format, URL_Struct *url, Net_GetUrlExitFunc *exit_func) { return FALSE; }
void *FE_AboutData(const char *which, char **data, int32 *length, char **content_type)
{
    static char about[] = "<html><title>Netscape 3.02 Windows</title><h1>Netscape 3.02</h1><p>Native Windows frontend port.</p><p>Browser text layout and navigation use the original 3.02 core. Native mail/news lists, reader and composer are available from Messages. Native images and HTML forms use the original core. Native nested frames use the original layout engine. Plugins, Java and security UI are still being ported.</p></html>";
    *data = about; *length = sizeof(about) - 1; *content_type = "text/html"; return about;
}
void FE_FreeAboutData(void *data, const char *which) { }
BOOL PMS_RegisterContentTypeConverter(char *input, FO_Present_Types output, void *data, NET_Converter *converter, BOOL automated) { RealNET_RegisterContentTypeConverter(input, output, data, converter); return TRUE; }
long random(void) { return rand(); }
char *XP_GetString(int16 identifier)
{
    unsigned short index = (unsigned short)(identifier + 7000);
    if (!resource_strings[index]) {
        char value[8192];
        if (!LoadStringA(GetModuleHandle(NULL), index, value, sizeof(value)))
            sprintf(value, "Unknown Netscape resource %d", identifier);
        resource_strings[index] = _strdup(value);
    }
    return resource_strings[index] ? resource_strings[index] : "Out of memory";
}
#ifdef DEBUG
void XP_Trace(const char *format, ...)
{
    char buffer[4096];
    va_list arguments;
    va_start(arguments, format);
    vsnprintf(buffer, sizeof(buffer), format, arguments);
    va_end(arguments);
    OutputDebugStringA(buffer);
    fputs(buffer, stderr);
    fputc('\n', stderr);
}
void XP_AssertAtLine(char *file, int line)
{
    XP_Trace("Assertion failed at %s:%d", file, line);
    if (IsDebuggerPresent()) DebugBreak();
    else ExitProcess(3);
}
char *NOT_NULL(const char *value)
{
    if (!value) { XP_Trace("Unexpected null string"); return ""; }
    return (char *)value;
}
#endif
