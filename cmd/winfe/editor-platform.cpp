/* Native Windows callbacks for the original Gold editing engine. */
#include <afxwin.h>
#include <afxdlgs.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include "editor.h"
#include "editor-preferences.h"
#include "preferences.h"
#include "images.h"
extern "C" {
#include "edt.h"
#include "proto.h"
#include "xp_list.h"
#include "xp_file.h"
void WFE_CloseWindow(MWContext *context);
}

#ifdef EDITOR
struct EditorPlatform {
    int32 x = 0, y = 0, width = 2, height = 16;
    bool caret = false, saving = false, dirty = false, cancelled = false, checking_save = false;
    int status = ED_ERROR_NONE, count = 1, completed = 0;
    std::string destination, saved, filename;
    HWND progress = nullptr, owner = nullptr;
    bool owner_enabled = false, image_loading = false;
};
static std::map<MWContext *, EditorPlatform> editors;
static MWContext *caret_context;
static HWND caret_window;
static bool editor_test_mode;
extern "C" void WFE_EditorTestMode(int enabled) { editor_test_mode = enabled != 0; }

static std::string local_path(const char *url)
{
    if (!url || _strnicmp(url, "file:", 5)) return "";
    std::vector<char> path(32768);
    DWORD length = (DWORD)path.size();
    return SUCCEEDED(PathCreateFromUrlA(url, path.data(), &length, 0)) ? path.data() : "";
}

static std::string file_url(const char *path)
{
    if (!path || !*path) return "";
    char absolute[32768];
    DWORD length = GetFullPathNameA(path, sizeof(absolute), absolute, nullptr);
    if (!length || length >= sizeof(absolute)) return "";
    std::vector<char> url(32768);
    length = (DWORD)url.size();
    return SUCCEEDED(UrlCreateFromPathA(absolute, url.data(), &length, 0)) ? url.data() : "";
}

static char *editor_local_filename(const char *url)
{
    /* NET_IsLocalFileURL also accepts mailbox:, which this API must reject. */
    if (!url || _strnicmp(url, "file:", 5) || !NET_IsLocalFileURL(const_cast<char *>(url))) return nullptr;
    std::string address = url;
    size_t suffix = address.find_first_of("?#");
    if (suffix != std::string::npos) address.erase(suffix);
    /* PathCreateFromUrl performs the one URL-unescape operation. A '%' in
     * the resulting native filename must not be decoded again. */
    std::string path = local_path(address.c_str());
    if (path.empty()) return nullptr;
    DWORD length = GetFullPathNameA(path.c_str(), 0, nullptr, nullptr);
    if (!length) return nullptr;
    char *absolute = (char *)XP_ALLOC(length);
    if (!absolute) return nullptr;
    DWORD written = GetFullPathNameA(path.c_str(), length, absolute, nullptr);
    if (!written || written >= length) { XP_FREE(absolute); return nullptr; }
    return absolute;
}

extern "C" Bool XP_ConvertUrlToLocalFile(const char *url, char **local_name)
{
    if (local_name) *local_name = nullptr;
    char *path = editor_local_filename(url);
    if (!path) return FALSE;
    XP_StatStruct information;
    Bool exists = XP_Stat(path, &information, xpURL) == 0 && (information.st_mode & S_IFREG) != 0;
    /* BeginTransaction needs the allocated destination even for a new file. */
    if (local_name) *local_name = path;
    else XP_FREE(path);
    return exists;
}

extern "C" char *XP_BackupFileName(const char *url)
{
    char *path = editor_local_filename(url);
    if (!path) return nullptr;
    size_t length = strlen(path);
    char *backup = (char *)XP_ALLOC(length + 2);
    if (backup) {
        memcpy(backup, path, length);
        /* Match the in-tree XFE Gold transaction name. The caller owns all
         * deletion, rename, commit and rollback operations on this sibling. */
        backup[length] = '~'; backup[length + 1] = '\0';
    }
    XP_FREE(path);
    return backup;
}

static void destroy_progress(EditorPlatform &data)
{
    HWND progress = data.progress;
    data.progress = nullptr;
    if (IsWindow(progress)) DestroyWindow(progress);
    if (data.owner_enabled && IsWindow(data.owner)) EnableWindow(data.owner, TRUE);
    data.owner_enabled = false;
    data.owner = nullptr;
}

static void finish_save(MWContext *context)
{
    auto found = editors.find(context);
    if (found == editors.end()) return;
    EditorPlatform &data = found->second;
    if (!data.saving || context->edit_saving_url) return;
    data.saving = false;
    if (data.cancelled && data.status == ED_ERROR_NONE) data.status = ED_ERROR_CANCEL;
    if (data.status == ED_ERROR_NONE) {
        data.saved = local_path(data.destination.c_str());
        WFE_DocumentAddress(context, data.destination.c_str());
        WFE_Status(context, "Document saved.");
    } else {
        /* WriteToFile clears the core flag before checking the stream error. */
        if (EDT_HaveEditBuffer(context)) EDT_SetDirtyFlag(context, TRUE);
        WFE_Status(context, data.status == ED_ERROR_CANCEL ? "Save cancelled; changes retained." :
                   "Could not save document; changes retained.");
    }
    destroy_progress(data);
    WFE_EditorFormattingChanged(context);
}

static LRESULT CALLBACK progress_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lparam)->lpCreateParams);
    MWContext *context = (MWContext *)GetWindowLongPtr(window, GWLP_USERDATA);
    auto found = editors.find(context);
    if (found != editors.end()) {
        EditorPlatform &data = found->second;
        if (message == WM_TIMER) { finish_save(context); return 0; }
        if (message == WM_CLOSE || (message == WM_COMMAND && LOWORD(wparam) == IDCANCEL)) {
            if (data.image_loading) EDT_ImageLoadCancel(context);
            else if (data.saving && !data.cancelled) {
                data.cancelled = true;
                SetDlgItemTextA(window, 1, "Cancelling...");
                EnableWindow(GetDlgItem(window, IDCANCEL), FALSE);
                EDT_SaveCancel(context);
            }
            return 0;
        }
    }
    return DefWindowProc(window, message, wparam, lparam);
}

static void create_progress(MWContext *context, bool image)
{
    EditorPlatform &data = editors[context];
    if (IsWindow(data.progress)) return;
    WNDCLASSA cls = {};
    cls.lpfnWndProc = progress_proc;
    cls.hInstance = AfxGetInstanceHandle();
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW);
    cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpszClassName = "NetscapeGoldSaveProgress";
    RegisterClassA(&cls);
    data.owner = GetAncestor(WFE_DocumentWindow(context), GA_ROOT);
    data.image_loading = image;
    RECT rect = {0, 0, 420, 155};
    if (data.owner) GetWindowRect(data.owner, &rect);
    data.progress = CreateWindowExA(WS_EX_DLGMODALFRAME, cls.lpszClassName,
        image ? "Loading Image" : "Saving Document", WS_POPUP | WS_CAPTION | WS_SYSMENU,
        rect.left + 60, rect.top + 80, 420, 155, data.owner, nullptr, cls.hInstance, context);
    if (!data.progress) { WFE_Status(context, "Could not create editor progress window."); return; }
    HWND label = CreateWindowExA(0, "STATIC", image ? "Loading image..." : "Saving document...",
        WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS, 12, 15, 385, 36, data.progress, (HMENU)1, cls.hInstance, nullptr);
    HWND progress = CreateWindowExA(0, PROGRESS_CLASSA, "", WS_CHILD | WS_VISIBLE,
        12, 57, 385, 18, data.progress, (HMENU)2, cls.hInstance, nullptr);
    SendMessage(progress, PBM_SETRANGE32, 0, data.count);
    HWND cancel = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_DEFPUSHBUTTON,
        310, 88, 86, 25, data.progress, (HMENU)IDCANCEL, cls.hInstance, nullptr);
    SendMessage(label, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
    SendMessage(cancel, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), FALSE);
    data.owner_enabled = data.owner && IsWindowEnabled(data.owner);
    if (data.owner_enabled) EnableWindow(data.owner, FALSE);
    if (!image) SetTimer(data.progress, 1, 50, nullptr);
    ShowWindow(data.progress, SW_SHOW);
    UpdateWindow(data.progress);
}

extern "C" void WFE_EditorRefreshCaret(MWContext *context)
{
    auto found = editors.find(context);
    HWND window = WFE_DocumentWindow(context);
    if (caret_context == context) {
        ::DestroyCaret(); caret_context = nullptr; caret_window = nullptr;
    }
    if (found == editors.end() || !found->second.caret || GetFocus() != window || !IsWindow(window)) return;
    EditorPlatform &data = found->second;
    int32 x, y;
    WFE_Position(context, &x, &y);
    if (::CreateCaret(window, nullptr, data.width, data.height)) {
        caret_context = context; caret_window = window;
        SetCaretPos(data.x - x, data.y - y);
        ::ShowCaret(window);
    }
}

static void set_caret(MWContext *context, int32 x, int32 y, int32 width, int32 height)
{
    if (!context) return;
    EditorPlatform &data = editors[context];
    data.x = x; data.y = y;
    data.width = (std::max)(1L, width); data.height = (std::max)(1L, height); data.caret = true;
    int32 sx, sy, ww, wh;
    WFE_Position(context, &sx, &sy); WFE_WindowSize(context, &ww, &wh);
    int32 nx = sx, ny = sy;
    if (x < sx) nx = x; else if (x + width > sx + ww) nx = x + width - ww;
    if (y < sy) ny = y; else if (y + height > sy + wh) ny = y + height - wh;
    if (nx != sx || ny != sy) WFE_Scroll(context, (std::max)(0L, nx), (std::max)(0L, ny));
    WFE_EditorRefreshCaret(context);
}

static bool text_caret(MWContext *context, LO_TextStruct *text, int offset, int32 &x, int32 &y, int32 &height)
{
    if (!text || !text->text_attr) return false;
    LO_TextStruct prefix = *text;
    prefix.text_len = (int16)(std::max)(0, (std::min)(offset, (int)text->text_len));
    LO_TextInfo metrics = {};
    WFE_TextMetrics(context, &prefix, &metrics);
    x = text->x + text->x_offset + metrics.max_width;
    y = text->y + text->y_offset;
    height = metrics.ascent + metrics.descent;
    return true;
}

extern "C" void FE_DisplayTextCaret(MWContext *context, int, LO_TextStruct *text, int offset)
{
    int32 x, y, height;
    if (text_caret(context, text, offset, x, y, height)) set_caret(context, x, y, 2, height);
}
extern "C" void FE_DisplayImageCaret(MWContext *context, LO_ImageStruct *image, ED_CaretObjectPosition position)
{
    if (!image) return;
    int32 x = image->x + image->x_offset, width = image->width + 2 * image->border_width;
    if (position == ED_CARET_BEFORE) --x; else if (position == ED_CARET_AFTER) x += width;
    set_caret(context, x, image->y + image->y_offset, position != ED_CARET_BEFORE && position != ED_CARET_AFTER ? width : 2,
              image->height + 2 * image->border_width);
}
extern "C" void FE_DisplayGenericCaret(MWContext *context, LO_Any *element, ED_CaretObjectPosition position)
{
    if (!element) return;
    int32 x = element->x + element->x_offset;
    if (position == ED_CARET_BEFORE) --x; else if (position == ED_CARET_AFTER) x += element->width;
    set_caret(context, x, element->y + element->y_offset, position != ED_CARET_BEFORE && position != ED_CARET_AFTER ? element->width : 2,
              element->line_height);
}
extern "C" Bool FE_GetCaretPosition(MWContext *context, LO_Position *where, int32 *x, int32 *low, int32 *high)
{
    if (!context || !where || !where->element || !x || !low || !high) return FALSE;
    LO_Any &element = where->element->lo_any;
    *x = element.x + element.x_offset; *low = element.y + element.y_offset;
    int32 height = element.height;
    if (element.type == LO_TEXT) {
        if (!text_caret(context, &where->element->lo_text, where->position, *x, *low, height)) return FALSE;
    } else {
        if (where->position) *x += element.width + (element.type == LO_IMAGE ? 2 * where->element->lo_image.border_width : 0);
        else --*x;
    }
    *high = *low + height;
    return TRUE;
}
extern "C" void FE_DestroyCaret(MWContext *context)
{
    auto found = editors.find(context);
    if (found != editors.end()) found->second.caret = false;
    if (caret_context == context) { ::DestroyCaret(); caret_context = nullptr; caret_window = nullptr; }
}
extern "C" void FE_ShowCaret(MWContext *context)
{
    auto found = editors.find(context);
    if (found != editors.end()) found->second.caret = true;
    WFE_EditorRefreshCaret(context);
}
extern "C" void FE_DocumentChanged(MWContext *context, int32 y, int32 height)
{
    HWND window = WFE_DocumentWindow(context);
    int32 sx, sy; WFE_Position(context, &sx, &sy);
    RECT rect; GetClientRect(window, &rect);
    rect.top = (std::max)(0L, y - sy);
    if (height >= 0) rect.bottom = (std::min)(rect.bottom, y + height - sy);
    if (rect.bottom > rect.top) InvalidateRect(window, &rect, TRUE);
    /* Layout may have shortened the document, so re-clamp the scroll offset. */
    WFE_Scroll(context, sx, sy);
    WFE_EditorRefreshCaret(context);
}
extern "C" void FE_GetDocAndWindowPosition(MWContext *context, int32 *x, int32 *y, int32 *width, int32 *height)
{ WFE_Position(context, x, y); WFE_WindowSize(context, width, height); }
extern "C" MWContext *FE_CreateNewEditWindow(MWContext *, URL_Struct *url)
{ return WFE_NewEditorWindow(url ? url->address : nullptr); }
extern "C" void FE_EditFormattingUpdate(MWContext *context) { WFE_EditorFormattingChanged(context); }
extern "C" void FE_EditorDocumentLoaded(MWContext *context) { WFE_EditorCopyLoaded(context); WFE_EditorApplyPreferences(context,context->is_new_document!=FALSE); WFE_EditorDocumentLoaded(context); }

extern "C" void FE_SetNewDocumentProperties(MWContext *context)
{
    const GeneralPreferences &prefs = WFE_GeneralPreferences();
    EDT_PageData *page = EDT_NewPageData();
    if (!page) return;
    LO_Color colors[4];
    for (int i = 0; i < 4; ++i) {
        colors[i].red = GetRValue(prefs.colors[i]); colors[i].green = GetGValue(prefs.colors[i]);
        colors[i].blue = GetBValue(prefs.colors[i]);
    }
    /* A null color preserves the browser's default rather than baking it in. */
    if (prefs.background_custom) page->pColorBackground = &colors[0];
    if (prefs.text_custom) page->pColorText = &colors[1];
    if (prefs.link_custom) page->pColorLink = &colors[2];
    if (prefs.visited_custom) page->pColorFollowedLink = &colors[3];
    page->bKeepImagesWithDoc = TRUE;
    EDT_SetPageData(context, page);
    page->pColorBackground = page->pColorText = page->pColorLink = page->pColorFollowedLink = nullptr;
    EDT_FreePageData(page);
}
extern "C" void FE_ClearBackgroundImage(MWContext *context)
{ WFE_SetBackdrop(context, nullptr); WFE_Invalidate(context); }
extern "C" void FE_ImageLoadDialog(MWContext *context) { create_progress(context, true); }
extern "C" void FE_ImageLoadDialogDestroy(MWContext *context)
{ auto found = editors.find(context); if (found != editors.end()) destroy_progress(found->second); }

extern "C" void FE_SaveDialogCreate(MWContext *context, int count, Bool upload)
{
    if (upload) { WFE_Unsupported(context, "Publishing documents"); NET_InterruptWindow(context); return; }
    EditorPlatform &data = editors[context];
    if (!data.saving) {
        data.saving = true; data.status = ED_ERROR_NONE; data.cancelled = false;
        const char *address = WFE_CurrentDocumentURL(context);
        data.destination = address ? address : "";
    }
    data.count = (std::max)(1, count);
    create_progress(context, false);
    if (data.progress) SendDlgItemMessage(data.progress, 2, PBM_SETRANGE32, 0, data.count);
}
extern "C" void FE_SaveDialogSetFilename(MWContext *context, char *filename)
{
    EditorPlatform &data = editors[context]; data.filename = filename ? filename : "";
    if (data.progress) SetDlgItemTextA(data.progress, 1, data.filename.c_str());
}
extern "C" void FE_FinishedSave(MWContext *context, int status, char *, int number)
{
    EditorPlatform &data = editors[context];
    if (status != ED_ERROR_NONE) data.status = status;
    data.completed = number;
    if (data.progress) SendDlgItemMessage(data.progress, 2, PBM_SETPOS, number, 0);
}
extern "C" void FE_SaveDialogDestroy(MWContext *context, int status, char *)
{
    EditorPlatform &data = editors[context];
    if (status != ED_ERROR_NONE) data.status = status;
    /* The core clears edit_saving_url after this callback returns. */
    if (!data.saving) destroy_progress(data);
}
extern "C" ED_SaveOption FE_SaveFileExistsDialog(MWContext *context, char *filename)
{
    std::string message = "Replace the existing file?\n\n";
    if (filename) message += filename;
    int answer = MessageBoxA(editors[context].progress ? editors[context].progress : WFE_DocumentWindow(context),
        message.c_str(), "Save Document", MB_YESNOCANCEL | MB_ICONQUESTION);
    return answer == IDYES ? ED_SAVE_OVERWRITE_THIS : answer == IDNO ? ED_SAVE_DONT_OVERWRITE_THIS : ED_SAVE_CANCEL;
}
extern "C" Bool FE_SaveErrorContinueDialog(MWContext *context, char *filename, ED_FileError error)
{
    /* The engine also sends this notification after a successful file open. */
    if (error == ED_ERROR_NONE) return TRUE;
    editors[context].status = error;
    if (EDT_HaveEditBuffer(context)) EDT_SetDirtyFlag(context, TRUE);
    std::string message = "Could not save "; message += filename ? filename : "document";
    message += ". Changes will remain in the editor.";
    if (!editor_test_mode)
        MessageBoxA(editors[context].progress ? editors[context].progress : WFE_DocumentWindow(context),
            message.c_str(), "Save Document", MB_OK | MB_ICONERROR);
    return FALSE;
}

extern "C" int WFE_EditorSave(MWContext *context, const char *path, int save_as)
{
    if (!context || !EDT_HaveEditBuffer(context) || EDT_IsBlocked(context) || WFE_EditorSaving(context)) return -1;
    EditorPlatform &data = editors[context];
    std::string destination = path ? path : "";
    if (!path && !save_as && !context->is_new_document) {
        /* Navigation can reuse this context; its last successful save may
         * belong to a different document. The current URL is authoritative. */
        destination = local_path(WFE_CurrentDocumentURL(context));
    }
    if (destination.empty()) {
        CFileDialog dialog(FALSE, "html", "Untitled.html", OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY | OFN_NOCHANGEDIR,
            "HTML Documents (*.html;*.htm)|*.html;*.htm|All Files (*.*)|*.*||", CWnd::FromHandle(WFE_DocumentWindow(context)));
        if (dialog.DoModal() != IDOK) { data.status = CommDlgExtendedError() ? ED_ERROR_FILE_OPEN : ED_ERROR_CANCEL; return data.status == ED_ERROR_CANCEL ? 0 : -1; }
        destination = (const char *)dialog.GetPathName(); save_as = TRUE;
    }
    if (!_strnicmp(destination.c_str(), "file:", 5)) destination = local_path(destination.c_str());
    else if (destination.find("://") != std::string::npos) { data.status = ED_ERROR_BAD_URL; return -1; }
    std::string url = file_url(destination.c_str());
    if (url.empty()) { data.status = ED_ERROR_BAD_URL; return -1; }
    std::string source = WFE_CurrentDocumentURL(context) ? WFE_CurrentDocumentURL(context) : "file:///Untitled";
    data.destination = url; data.status = ED_ERROR_NONE; data.cancelled = false;
    data.completed = 0; data.dirty = EDT_DirtyFlag(context) != FALSE; data.saving = true;
    FE_SaveDialogCreate(context, 1, FALSE);
    FE_SaveDialogSetFilename(context, const_cast<char *>(destination.c_str()));
    EditorPreferences preferences=WFE_EditorPreferencesRead();
    XP_Bool images=preferences.keep_images;
    ED_FileError result = EDT_SaveFile(context, const_cast<char *>(source.c_str()), const_cast<char *>(url.c_str()),
                                     save_as || context->is_new_document, images, preferences.maintain_links);
    if (result != ED_ERROR_NONE) data.status = result;
    finish_save(context);
    return data.saving || data.status == ED_ERROR_NONE ? 1 : data.status == ED_ERROR_CANCEL ? 0 : -1;
}
extern "C" int WFE_EditorSaving(MWContext *context)
{ finish_save(context); auto found = editors.find(context); return found != editors.end() && found->second.saving; }
extern "C" int WFE_EditorSaveStatus(MWContext *context)
{ finish_save(context); auto found = editors.find(context); return found == editors.end() ? ED_ERROR_NONE : found->second.status; }
extern "C" const char *WFE_EditorSavedPath(MWContext *context)
{ finish_save(context); auto found = editors.find(context); return found == editors.end() ? "" : found->second.saved.c_str(); }

static bool save_context_alive(MWContext *context, HWND window)
{
    return IsWindow(window) && editors.find(context) != editors.end() && WFE_DocumentWindow(context) == window;
}

static bool wait_save(MWContext *context)
{
    HWND window = WFE_DocumentWindow(context);
    while (save_context_alive(context, window)) {
        if (!WFE_EditorSaving(context))
            return save_context_alive(context, window) && WFE_EditorSaveStatus(context) == ED_ERROR_NONE;
        WFE_Pump();
        if (!save_context_alive(context, window)) return false;
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) { PostQuitMessage((int)message.wParam); return false; }
            TranslateMessage(&message); DispatchMessage(&message);
            if (!save_context_alive(context, window)) return false;
        }
        if (WFE_EditorSaving(context)) MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
    }
    return false;
}
extern "C" int WFE_EditorCheckSave(MWContext *context)
{
    if (!context) return TRUE;
    auto found = editors.find(context);
    if (found != editors.end() && found->second.checking_save) return FALSE;
    if (!EDT_HaveEditBuffer(context)) return TRUE;
    HWND window = WFE_DocumentWindow(context);
    if (!IsWindow(window)) return FALSE;
    editors[context].checking_save = true;
    /* MessageBox and wait_save both dispatch messages. A nested WM_CLOSE must
     * leave the outer frame alive, and destruction must not leave a dangling
     * reference into the adapter map when this guard unwinds. */
    struct SaveCheckGuard {
        MWContext *context;
        ~SaveCheckGuard() {
            auto found = editors.find(context);
            if (found != editors.end()) found->second.checking_save = false;
        }
    } guard = {context};
    if (WFE_EditorSaving(context) && !wait_save(context)) return FALSE;
    if (!save_context_alive(context, window)) return FALSE;
    if (!EDT_DirtyFlag(context)) return TRUE;
    int answer = MessageBoxA(window, "Save changes to this document?", "Netscape Gold",
                             MB_YESNOCANCEL | MB_ICONQUESTION);
    if (!save_context_alive(context, window)) return FALSE;
    if (answer == IDNO) return TRUE;
    return answer == IDYES && WFE_EditorSave(context, nullptr, FALSE) == 1 && wait_save(context)
        && save_context_alive(context, window);
}
extern "C" Bool FE_CheckAndSaveDocument(MWContext *context) { return WFE_EditorCheckSave(context); }
extern "C" Bool FE_CheckAndAutoSaveDocument(MWContext *context)
{ Bool result = WFE_EditorCheckSave(context); if (!result) EDT_SetAutoSavePeriod(context, 0); return result; }
extern "C" Bool FE_SaveNonLocalDocument(MWContext *context, Bool save_new)
{
    if (!context || (!save_new && context->is_new_document)) return TRUE;
    if (!local_path(WFE_CurrentDocumentURL(context)).empty() && !context->is_new_document) return TRUE;
    return WFE_EditorSave(context, nullptr, TRUE) == 1 && wait_save(context);
}
extern "C" void WFE_EditorForget(MWContext *context)
{
    FE_DestroyCaret(context);
    auto found = editors.find(context);
    if (found != editors.end()) { destroy_progress(found->second); editors.erase(found); }
}
extern "C" char *FE_URLToLocalName(char *url)
{
    if (!url) return nullptr;
    std::string name = url;
    size_t end = name.find_first_of("?#"); if (end != std::string::npos) name.erase(end);
    size_t slash = name.find_last_of("/\\"); if (slash != std::string::npos) name.erase(0, slash + 1);
    if (name.empty()) return nullptr;
    char *decoded = XP_STRDUP(name.c_str());
    if (decoded) {
        NET_UnEscape(decoded);
        for (char *p = decoded; *p; ++p) if ((unsigned char)*p < 32 || strchr("<>:\"/\\|?*", *p)) *p = '_';
    }
    return decoded;
}
extern "C" Bool FE_EditorPrefConvertFileCaseOnWrite(void) { return FALSE; }
extern "C" MWContext *FE_ActivateFrameWithURL(char *url, Bool editor)
{
    if (!url) return nullptr;
    XP_List *list = XP_GetGlobalContextList(); MWContext *context;
    while ((context = (MWContext *)XP_ListNextObject(list))) {
        if ((editor ? !context->is_editor : context->type != MWContextBrowser || context->is_editor) || context->grid_parent) continue;
        const char *current = WFE_CurrentDocumentURL(context);
        if (current && !strcmp(current, url)) {
            HWND window = GetAncestor(WFE_DocumentWindow(context), GA_ROOT);
            ShowWindow(window, SW_RESTORE); SetForegroundWindow(window); return context;
        }
    }
    return nullptr;
}
extern "C" void FE_LoadUrl(char *url, Bool editor)
{
    if (!url || !*url || FE_ActivateFrameWithURL(url, editor)) return;
    if (editor) WFE_NewEditorWindow(url); else WFE_NewWindow(url);
}
extern "C" void FE_RevertToPreviousFrame(char *url, MWContext *context)
{
    if (!WFE_EditorCheckSave(context)) return;
    MWContext *other = FE_ActivateFrameWithURL(url, FALSE);
    if (!other) other = WFE_NewWindow(url ? url : "about:");
    if (other) WFE_CloseWindow(context);
}
extern "C" void FE_EditorGetUrlExitRoutine(URL_Struct *, int status, MWContext *context)
{
    if (status < 0) WFE_Status(context, "The editor could not finish loading this document.");
}
#endif /* EDITOR */
