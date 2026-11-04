/* Optional user-interface services for the 3.02 browser frontend. */
#include "frontend.h"
#include "bkmks.h"
#include "msgcom.h"
#include "mime.h"
#include "libi18n.h"
#include "csid.h"
#include "xplocale.h"
#include "prefapi.h"
#include "npapi.h"
#include "npupp.h"
#include <windows.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static const char *
wfe_Environment(const char *name)
{
    const char *value = getenv(name);
    return value && *value ? value : NULL;
}

const char *FE_UsersMailAddress(void)
{
    return wfe_Environment("EMAIL");
}

const char *FE_UsersFullName(void)
{
    const char *name = wfe_Environment("NAME");
    if (!name) name = wfe_Environment("USERNAME");
    return name;
}

const char *FE_UsersOrganization(void)
{
    return wfe_Environment("ORGANIZATION");
}

const char *FE_UsersSignature(void)
{
    /* A login name or a signature filename is not signature text. */
    return NULL;
}

char *FE_GetAcceptLanguage(void)
{
    static char language[LOCALE_NAME_MAX_LENGTH];
    if (!language[0]) {
        if (!GetLocaleInfoA(LOCALE_USER_DEFAULT, LOCALE_SISO639LANGNAME,
                            language, sizeof(language)))
            return NULL;
    }
    return language;
}

int FE_StrColl(const char *first, const char *second)
{
    return strcoll(first ? first : "", second ? second : "");
}

size_t FE_StrfTime(MWContext *context, char *result, size_t maxsize,
                  int format, const struct tm *timeptr)
{
    const char *pattern;
    (void)context;
    if (!result || !maxsize || !timeptr) return 0;
    switch (format) {
    case XP_TIME_FORMAT: pattern = "%H:%M"; break;
    case XP_WEEKDAY_TIME_FORMAT: pattern = "%a %H:%M"; break;
    case XP_DATE_TIME_FORMAT: pattern = "%x %H:%M"; break;
    default: pattern = "%c"; break;
    }
    return strftime(result, maxsize, pattern, timeptr);
}

int16 INTL_DefaultDocCharSetID(MWContext *context)
{
    (void)context;
    return CS_LATIN1;
}

char *INTL_ResourceCharSet(void)
{
    return "iso-8859-1";
}

int PREF_GetBoolPref(const char *pref, XP_Bool *value)
{
    (void)pref;
    if (value) *value = FALSE;
    return -1;                  /* No preference database is installed. */
}

static UINT
wfe_CodePage(int16 csid)
{
    switch (csid) {
    case CS_ASCII: return 20127;
    case CS_LATIN1: return 28591;
    case CS_LATIN2: return 28592;
    case CS_8859_3: return 28593;
    case CS_8859_4: return 28594;
    case CS_8859_5: return 28595;
    case CS_8859_6: return 28596;
    case CS_8859_7: return 28597;
    case CS_8859_8: return 28598;
    case CS_8859_9: return 28599;
    case CS_MAC_ROMAN: return 10000;
    case CS_MAC_CE: return 10029;
    case CS_MAC_CYRILLIC: return 10007;
    case CS_MAC_GREEK: return 10006;
    case CS_MAC_TURKISH: return 10081;
    case CS_CP_1250: return 1250;
    case CS_CP_1251: return 1251;
    case CS_CP_1253: return 1253;
    case CS_CP_1254: return 1254;
    case CS_KOI8_R: return 20866;
    case CS_TIS620: return 874;
    default: return 0;
    }
}

/* The i18n library keeps these handles for the lifetime of the process.
   Each table is immutable once published; no resource lock is necessary. */
typedef struct WFE_ByteTable {
    int16 from, to;
    char bytes[256];
    char *handle;
    struct WFE_ByteTable *next;
} WFE_ByteTable;

char **FE_GetSingleByteTable(int16 from, int16 to, int32 resourceid)
{
    static WFE_ByteTable *tables;
    WFE_ByteTable *table;
    UINT input = wfe_CodePage(from), output = wfe_CodePage(to);
    int i;
    (void)resourceid;
    for (table = tables; table; table = table->next)
        if (table->from == from && table->to == to) return &table->handle;
    if (!input || !output || !IsValidCodePage(input) || !IsValidCodePage(output)) {
        WFE_Unsupported(NULL, "This single-byte character encoding conversion");
        return NULL;
    }
    table = (WFE_ByteTable *)malloc(sizeof(*table));
    if (!table) return NULL;
    table->from = from;
    table->to = to;
    table->handle = table->bytes;
    for (i = 0; i < 256; ++i) {
        char byte = (char)i, converted;
        WCHAR wide;
        table->bytes[i] = '?';
        if (from == to) table->bytes[i] = byte;
        else if (MultiByteToWideChar(input, 0, &byte, 1, &wide, 1) == 1 &&
                 WideCharToMultiByte(output, 0, &wide, 1, &converted, 1,
                                     NULL, NULL) == 1)
            table->bytes[i] = converted;
    }
    table->next = tables;
    tables = table;
    return &table->handle;
}

char *FE_LockTable(char **handle)
{
    return handle ? *handle : NULL;
}

void FE_ConnectToRemoteHost(MWContext *context, int type, char *host,
                          char *port, char *user)
{
    (void)type; (void)host; (void)port; (void)user;
    WFE_Unsupported(context, "Telnet, TN3270 and rlogin sessions");
}

Bool FE_SecurityDialog(MWContext *context, int message)
{
    (void)message;
    WFE_Unsupported(context, "Security confirmation (request cancelled)");
    return FALSE;
}

int FE_PromptForFileName(MWContext *context, const char *prompt,
                         const char *path, XP_Bool must_exist,
                         XP_Bool directories, ReadFileNameCallbackFunction fn,
                         void *closure)
{
    (void)prompt; (void)path; (void)must_exist;
    (void)directories; (void)fn; (void)closure;
    WFE_Unsupported(context, "File selection dialog");
    return -1;                  /* On error the callback must not run. */
}

int FE_PromptForNewsHost(MWContext *context, const char *prompt,
                         ReadFileNameCallbackFunction fn, void *closure)
{
    (void)prompt; (void)fn; (void)closure;
    WFE_Unsupported(context, "News host selection dialog");
    return -1;
}

void FE_RememberPopPassword(MWContext *context, const char *password)
{
    (void)password;
    WFE_Unsupported(context, "Saving mail passwords");
}

MWContext *FE_MakeMessageCompositionContext(MWContext *context)
{
    WFE_Unsupported(context, "Mail and news composition");
    return NULL;
}

void FE_InitializeMailCompositionContext(MWContext *context,
    const char *from, const char *reply_to, const char *to, const char *cc,
    const char *bcc, const char *fcc, const char *newsgroups,
    const char *followup_to, const char *subject, const char *attachment)
{
    (void)from; (void)reply_to; (void)to; (void)cc; (void)bcc; (void)fcc;
    (void)newsgroups; (void)followup_to; (void)subject; (void)attachment;
    WFE_Unsupported(context, "Mail and news composition");
}

void FE_InsertMessageCompositionText(MWContext *context, const char *text,
                                     XP_Bool leave_cursor)
{
    (void)text; (void)leave_cursor;
    WFE_Unsupported(context, "Mail and news composition");
}

void FE_RaiseMailCompositionWindow(MWContext *context)
{
    WFE_Unsupported(context, "Mail and news composition");
}

void FE_DestroyMailCompositionContext(MWContext *context)
{
    /* No composition context is ever created by this frontend. */
    WFE_Unsupported(context, "Mail and news composition");
}

char *FE_PromptMessageSubject(MWContext *context)
{
    WFE_Unsupported(context, "Message subject dialog");
    return NULL;
}

void FE_MsgShowHeaders(MWContext *context, MSG_HEADER_SET headers)
{
    (void)headers;
    WFE_Unsupported(context, "Mail composition headers");
}

int FE_GetMessageBody(MWContext *context, char **body, uint32 *length,
                      MSG_FontCode **fonts)
{
    if (body) *body = NULL;
    if (length) *length = 0;
    if (fonts) *fonts = NULL;
    WFE_Unsupported(context, "Mail and news composition");
    return -1;
}

void FE_DoneWithMessageBody(MWContext *context, char *body, uint32 length)
{
    /* FE_GetMessageBody never lends or allocates a text buffer. */
    (void)context; (void)body; (void)length;
}

MWContext *FE_GetAddressBook(MWContext *context, XP_Bool viewnow)
{
    (void)viewnow;
    WFE_Unsupported(context, "Address book");
    return NULL;
}

const char *FE_GetFolderDirectory(MWContext *context)
{
    WFE_Unsupported(context, "Mail folders");
    return NULL;
}

void FE_UpdateFolderList(MWContext *context, int index, int length,
    int total, int visible, int *select, int numselect)
{
    (void)index; (void)length; (void)total; (void)visible;
    (void)select; (void)numselect;
    WFE_Unsupported(context, "Mail folder list");
}

void FE_UpdateThreadList(MWContext *context, int index, int length,
    int total, int visible, int *select, int numselect)
{
    (void)index; (void)length; (void)total; (void)visible;
    (void)select; (void)numselect;
    WFE_Unsupported(context, "Mail and news thread list");
}

void FE_UpdateFolderMenus(MWContext *context, MSG_FolderLine *lines, int count)
{
    (void)lines; (void)count;
    WFE_Unsupported(context, "Mail folder menus");
}

void FE_UpdateToolbar(MWContext *context)
{
    WFE_Unsupported(context, "Mail and news toolbar");
}

void FE_UpdateBiff(MSG_BIFF_STATE state)
{
    if (state == MSG_BIFF_NewMail) WFE_Status(NULL, "New mail is available.");
}

/* Bookmarks use a private clipboard format because their serialized blocks
   contain binary data. A separate read buffer remains valid after closing the
   Windows clipboard, as required by the bookmark library's borrowed pointer. */
static UINT wfe_BookmarkFormat;
static void *wfe_BookmarkClip;

void BMFE_SetClipContents(MWContext *context, void *buffer, int32 length)
{
    HGLOBAL memory;
    unsigned char *data;
    HWND owner = GetActiveWindow();
    if (!buffer || length <= 0) return;
    if (!owner) goto failed;
    if (!wfe_BookmarkFormat)
        wfe_BookmarkFormat = RegisterClipboardFormatA("Netscape 3.02 Bookmark Block");
    if (!wfe_BookmarkFormat) goto failed;
    memory = GlobalAlloc(GMEM_MOVEABLE, sizeof(int32) + (SIZE_T)length);
    if (!memory) goto failed;
    data = (unsigned char *)GlobalLock(memory);
    if (!data) { GlobalFree(memory); goto failed; }
    memcpy(data, &length, sizeof(length));
    memcpy(data + sizeof(length), buffer, length);
    GlobalUnlock(memory);
    if (!OpenClipboard(owner)) { GlobalFree(memory); goto failed; }
    if (!EmptyClipboard() || !SetClipboardData(wfe_BookmarkFormat, memory)) {
        CloseClipboard();
        GlobalFree(memory);
        goto failed;
    }
    CloseClipboard();
    return;
failed:
    WFE_Status(context, "Unable to copy bookmarks to the Windows clipboard.");
}

void *BMFE_GetClipContents(MWContext *context, int32 *length)
{
    HANDLE memory;
    unsigned char *data;
    SIZE_T size;
    int32 count = 0;
    void *copy = NULL;
    (void)context;
    if (length) *length = 0;
    if (!wfe_BookmarkFormat)
        wfe_BookmarkFormat = RegisterClipboardFormatA("Netscape 3.02 Bookmark Block");
    if (!wfe_BookmarkFormat || !OpenClipboard(NULL)) return NULL;
    memory = GetClipboardData(wfe_BookmarkFormat);
    size = memory ? GlobalSize(memory) : 0;
    data = memory ? (unsigned char *)GlobalLock(memory) : NULL;
    if (data && size >= sizeof(count)) {
        memcpy(&count, data, sizeof(count));
        if (count > 0 && (SIZE_T)count <= size - sizeof(count)) {
            copy = malloc(count);
            if (copy) memcpy(copy, data + sizeof(count), count);
        }
    }
    if (data) GlobalUnlock(memory);
    CloseClipboard();
    free(wfe_BookmarkClip);
    wfe_BookmarkClip = copy;
    if (length && copy) *length = count;
    return copy;
}

void BMFE_RefreshCells(MWContext *context, int32 first, int32 last, XP_Bool now)
{
    (void)first; (void)last; (void)now;
    WFE_Unsupported(context, "Bookmark list window");
}

void BMFE_SyncDisplay(MWContext *context)
{
    WFE_Unsupported(context, "Bookmark list window");
}

void BMFE_MeasureEntry(MWContext *context, BM_Entry *entry,
                       uint32 *width, uint32 *height)
{
    (void)context; (void)entry;
    /* BM_WidestEntry is unused: there is no bookmark list widget. */
    if (width) *width = 0;
    if (height) *height = 0;
}

void BMFE_OpenBookmarksWindow(MWContext *context)
{
    WFE_Unsupported(context, "Bookmark properties window");
}

void BMFE_EditItem(MWContext *context, BM_Entry *entry)
{
    (void)entry;
    WFE_Unsupported(context, "Bookmark properties window");
}

void BMFE_EntryGoingAway(MWContext *context, BM_Entry *entry)
{
    /* This frontend retains no BM_Entry pointers. */
    (void)context; (void)entry;
}

void BMFE_GotoBookmark(MWContext *context, const char *url)
{
    if (url && *url) WFE_Load(context, url);
}

void *BMFE_OpenFindWindow(MWContext *context, BM_FindInfo *info)
{
    (void)info;
    WFE_Unsupported(context, "Find bookmark dialog");
    return NULL;
}

void BMFE_ScrollIntoView(MWContext *context, BM_Entry *entry)
{
    (void)entry;
    WFE_Unsupported(context, "Bookmark list window");
}

void BMFE_BookmarkMenuInvalid(MWContext *context)
{
    /* There is no dynamically populated bookmarks menu to invalidate. */
    (void)context;
}

void BMFE_UpdateWhatsChanged(MWContext *context, const char *url,
                             int32 done, int32 total, const char *remaining)
{
    (void)done; (void)total; (void)remaining;
    WFE_Status(context, url ? url : "Checking bookmarks...");
}

void BMFE_FinishedWhatsChanged(MWContext *context, int32 checked,
                              int32 reached, int32 changed)
{
    char message[160];
    wsprintfA(message, "Bookmarks checked: %ld; reached: %ld; changed: %ld.",
              (long)checked, (long)reached, (long)changed);
    WFE_Status(context, message);
}

void FE_RegisterPlugins(void)
{
    /* No plugin descriptors or content converters are registered. */
    WFE_Status(NULL, "Browser plugins are unavailable in this frontend.");
}

NPPluginFuncs *FE_LoadPlugin(void *plugin, NPNetscapeFuncs *functions)
{
    (void)plugin; (void)functions;
    WFE_Unsupported(NULL, "Browser plugins");
    return NULL;
}

void FE_UnloadPlugin(void *plugin)
{
    (void)plugin;              /* FE_LoadPlugin never loads a module. */
}

void FE_EmbedURLExit(URL_Struct *url, int status, MWContext *context)
{
    (void)url; (void)status;
    WFE_Unsupported(context, "Embedded browser plugins");
}

NET_StreamClass *EmbedStream(int format, void *data, URL_Struct *url,
                             MWContext *context)
{
    (void)format; (void)data; (void)url;
    WFE_Unsupported(context, "Embedded browser plugins");
    return NULL;
}

void *WFE_BeginSetModuleState(void)
{
    /* The core and frontend live in one executable; plugins cannot load. */
    return NULL;
}

void WFE_EndSetModuleState(void *previous)
{
    (void)previous;
}

char *WFE_BuildCompuserveAuthString(URL_Struct *url)
{
    (void)url;
    return NULL;
}

int WFE_DoCompuserveAuthenticate(MWContext *context, URL_Struct *url,
                                char *header)
{
    (void)url; (void)header;
    WFE_Unsupported(context, "CompuServe authentication");
    return NET_AUTH_FAILED_DISPLAY_DOCUMENT;
}
