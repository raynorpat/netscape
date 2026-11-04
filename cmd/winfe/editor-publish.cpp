#include <afxwin.h>
/* Native Gold publishing through the original mkhttp/mkftp PUT engines. */
#include <commctrl.h>
#include <shlwapi.h>
#include <algorithm>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <stdio.h>
#include <ctype.h>
#include "editor-publish.h"
#include "editor-preferences.h"
#include "classic.h"
extern "C" {
#include "edt.h"
#include "xp_file.h"
extern int MK_ZERO_LENGTH_FILE;
}
#ifdef EDITOR
namespace {
const UINT publish_command = 42216, browse_command = 34402;
/* An unused private output type discards server response bodies; publishing
 * must never replace the live editor with a PUT response or download dialog. */
const FO_Present_Types publish_output = (FO_Present_Types)21;
enum { location_id = 100, username_id, password_id, files_id, status_id,
       progress_id, all_id, select_all_id, select_none_id };
struct PublishFile { std::string local, name; bool selected = true; };
struct Transfer {
    URL_Struct *url = nullptr;
    bool done = false, good = false, cancelled = false;
    int status = -1, server_status = 0;
};
struct PublishDialog {
    MWContext *context = nullptr;
    HWND owner = nullptr, window = nullptr;
    bool running = false, cancelled = false, closing = false;
    std::vector<PublishFile> files;
    std::shared_ptr<Transfer> transfer;
};
std::map<URL_Struct *, std::shared_ptr<Transfer> > transfers;
std::map<MWContext *, PublishDialog *> dialogs;
std::string last_location;
unsigned tracing_guards;
int previous_tracing;

static void wipe(std::string &value)
{
    if (!value.empty()) SecureZeroMemory(&value[0], value.size());
    value.clear();
}
static char *escape_component(const std::string &value)
{
    /* Original NET_Escape treats '@' as an xalpha. In userinfo it would be
     * mistaken for the authority delimiter, and NET_MakeUploadURL also scans
     * the destination filename for it. Escape every non-unreserved byte. */
    char *escaped = (char *)XP_ALLOC(value.size() * 3 + 1);
    if (!escaped) return nullptr;
    static const char hex[] = "0123456789ABCDEF";
    char *out = escaped;
    for (unsigned char c : value) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~') *out++ = c;
        else { *out++ = '%'; *out++ = hex[c >> 4]; *out++ = hex[c & 15]; }
    }
    *out = 0; return escaped;
}
static std::string path_from_url(const char *url)
{
    if (!url || _strnicmp(url, "file:", 5)) return "";
    char path[32768]; DWORD length = sizeof(path);
    return SUCCEEDED(PathCreateFromUrlA(url, path, &length, 0)) ? path : "";
}
static std::string basename(const std::string &path)
{
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}
static bool regular_file(const std::string &path)
{
    DWORD attributes = GetFileAttributesA(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}
static bool destination_valid(const std::string &location)
{
    size_t prefix = !_strnicmp(location.c_str(), "http://", 7) ? 7 :
                    !_strnicmp(location.c_str(), "ftp://", 6) ? 6 : 0;
    if (!prefix || location.size() <= prefix || location.back() != '/') return false;
    for (unsigned char c : location) if (c <= 32 || c == 127 || c == '\\') return false;
    for (size_t i = 0; i < location.size(); ++i) if (location[i] == '%') {
        if (i + 2 >= location.size() || !isxdigit((unsigned char)location[i + 1]) ||
            !isxdigit((unsigned char)location[i + 2])) return false;
        char hex[] = {location[i + 1], location[i + 2], 0};
        unsigned decoded = strtoul(hex, nullptr, 16);
        if (decoded < 32 || decoded == 127) return false;
        i += 2;
    }
    if (location.find_first_of("?#@") != std::string::npos) return false;
    size_t slash = location.find('/', prefix);
    if (slash == std::string::npos || slash == prefix) return false;
    std::string host = location.substr(prefix, slash - prefix);
    size_t colon = host.find(':');
    if (colon != std::string::npos) {
        if (!colon || colon + 1 == host.size() || host.find(':', colon + 1) != std::string::npos) return false;
        unsigned port = 0;
        for (size_t i = colon + 1; i < host.size(); ++i) {
            if (host[i] < '0' || host[i] > '9') return false;
            port = port * 10 + host[i] - '0'; if (port > 65535) return false;
        }
        if (!port) return false;
        host.erase(colon);
    }
    for (unsigned char c : host) if (!(isalnum(c) || c == '.' || c == '-')) return false;
    return !host.empty();
}
static bool alive(PublishDialog &dialog)
{
    return dialog.context && IsWindow(dialog.owner) && WFE_DocumentWindow(dialog.context) == dialog.owner;
}
static void dispatch_messages(PublishDialog *dialog)
{
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) {
            if (dialog) dialog->cancelled = dialog->closing = true;
            PostQuitMessage((int)message.wParam); break;
        }
        if (!dialog || !IsWindow(dialog->window) || !IsDialogMessage(dialog->window, &message)) {
            TranslateMessage(&message); DispatchMessage(&message);
        }
    }
}
static bool wait_save(PublishDialog &dialog)
{
    DWORD started = GetTickCount();
    while (alive(dialog) && WFE_EditorSaving(dialog.context)) {
        WFE_Pump(); dispatch_messages(&dialog);
        if (!alive(dialog)) return false;
        if (dialog.cancelled || GetTickCount() - started > 60000) {
            EDT_SaveCancel(dialog.context); return false;
        }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    return alive(dialog) && WFE_EditorSaveStatus(dialog.context) == ED_ERROR_NONE;
}
static int discard_write(void *, const char *, int32) { return 0; }
static unsigned int discard_ready(void *) { return MAX_WRITE_READY; }
static void discard_complete(void *) {}
static void discard_abort(void *, int) {}
static NET_StreamClass *discard_response(FO_Present_Types, void *, URL_Struct *, MWContext *context)
{
    return NET_NewStream((char *)"Publish response", discard_write, discard_complete,
                         discard_abort, discard_ready, nullptr, context);
}
static void upload_exit(URL_Struct *url, int status, MWContext *)
{
    auto found = transfers.find(url);
    if (found == transfers.end()) return;
    std::shared_ptr<Transfer> transfer = found->second;
    transfers.erase(found); transfer->url = nullptr;
    transfer->status = status; transfer->server_status = url->server_status;
    bool http = url->address && !_strnicmp(url->address, "http:", 5);
    transfer->good = !transfer->cancelled && (http ?
        url->server_status >= 200 && url->server_status < 300 && (status >= 0 || status == MK_ZERO_LENGTH_FILE) :
        status >= 0 && url->server_status >= 200 && url->server_status < 300);
    transfer->done = true;
    /* Never send address/error_msg (possibly containing credentials) to status,
     * history, preferences or test reports. URL owns its post-data filename. */
    if (url->address) SecureZeroMemory(url->address, strlen(url->address));
    NET_FreeURLStruct(url);
    if (tracing_guards && !--tracing_guards) MKLib_trace_flag = previous_tracing;
}
static void cancel_transfer(const std::shared_ptr<Transfer> &transfer)
{
    if (!transfer || transfer->done) return;
    transfer->cancelled = true;
    if (transfer->url) NET_InterruptStream(transfer->url);
}
static std::shared_ptr<Transfer> begin_upload(MWContext *context, const PublishFile &file,
    const std::string &destination, const std::string &username, const std::string &password)
{
    std::shared_ptr<Transfer> transfer = std::make_shared<Transfer>();
    if (!destination_valid(destination) || !regular_file(file.local)) { transfer->done = true; return transfer; }
    static bool registered;
    if (!registered) {
        NET_RegisterContentTypeConverter((char *)"*", publish_output, nullptr, discard_response);
        registered = true;
    }
    char *escaped_name = escape_component(file.name);
    char *escaped_user = escape_component(username);
    char *escaped_password = escape_component(password);
    char *full = nullptr;
    std::string address = destination + (escaped_name ? escaped_name : "");
    Bool assembled = escaped_name && escaped_user && escaped_password &&
        NET_MakeUploadURL(&full, const_cast<char *>(address.c_str()), escaped_user, escaped_password);
    if (escaped_name) XP_FREE(escaped_name);
    if (escaped_user) XP_FREE(escaped_user);
    if (escaped_password) { SecureZeroMemory(escaped_password, strlen(escaped_password)); XP_FREE(escaped_password); }
    URL_Struct *url = assembled && full ? NET_CreateURLStruct(full, NET_SUPER_RELOAD) : nullptr;
    if (full) { SecureZeroMemory(full, strlen(full)); XP_FREE(full); }
    if (!url) { transfer->done = true; return transfer; }
    url->method = URL_PUT_METHOD; url->post_data = XP_STRDUP(file.local.c_str());
    url->post_data_is_file = TRUE; url->load_background = TRUE;
    WIN32_FILE_ATTRIBUTE_DATA attributes;
    if (!GetFileAttributesExA(file.local.c_str(), GetFileExInfoStandard, &attributes) ||
        attributes.nFileSizeHigh || attributes.nFileSizeLow > 0x7fffffffUL) {
        NET_FreeURLStruct(url); transfer->done = true; return transfer;
    }
    /* Direct FTP PUT also uses this size for its original progress math. */
    url->content_length = (int32)attributes.nFileSizeLow;
    if (!_strnicmp(destination.c_str(), "http:", 5)) {
        char headers[150];
        sprintf(headers, "Content-Length: %lu\r\nContent-Type: application/octet-stream\r\n\r\n", attributes.nFileSizeLow);
        url->post_headers = XP_STRDUP(headers);
    }
    if (!url->post_data || (!_strnicmp(destination.c_str(), "http:", 5) && !url->post_headers)) {
        NET_FreeURLStruct(url); transfer->done = true; return transfer;
    }
    transfer->url = url; transfers[url] = transfer;
    /* Like NET_PublishFiles, use file-backed uploads; take the direct netlib
     * completion hook instead of FE_GetURL, which would navigate the editor.
     * NET_GetURL can call upload_exit before it returns. */
    /* Original FTP diagnostics include the PASS command, and the netlib URL
     * diagnostics include credentials. Suppress that optional global trace
     * while publishing; restore it after the last outstanding upload exits. */
    if (!tracing_guards++) { previous_tracing = MKLib_trace_flag; MKLib_trace_flag = 0; }
    NET_GetURL(url, publish_output, context, upload_exit);
    return transfer;
}
static bool wait_transfer(PublishDialog &dialog)
{
    std::shared_ptr<Transfer> transfer = dialog.transfer;
    DWORD interrupted = 0;
    while (!transfer->done) {
        /* Netlib owns connection timeouts. A slow but active upload must be
         * allowed to complete; interruption comes from Cancel or closure. */
        if (!alive(dialog) || dialog.cancelled) {
            cancel_transfer(transfer);
            if (!interrupted) interrupted = GetTickCount();
        }
        if (transfer->done) break;
        /* The global transfer owns callbacks even if interruption is deferred. */
        if (interrupted && GetTickCount() - interrupted > 1000) return false;
        WFE_Pump(); dispatch_messages(&dialog);
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    return transfer->good;
}
static void collect_files(PublishDialog &dialog, bool all)
{
    std::string document = WFE_EditorSavedPath(dialog.context);
    if (document.empty()) document = path_from_url(WFE_CurrentDocumentURL(dialog.context));
    dialog.files.clear(); if (!regular_file(document)) return;
    dialog.files.push_back({document, basename(document), true});
    std::string directory = document.substr(0, document.size() - basename(document).size());
    if (all) {
        WIN32_FIND_DATAA data; HANDLE search = FindFirstFileA((directory + "*").c_str(), &data);
        if (search != INVALID_HANDLE_VALUE) {
            do {
                if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && _stricmp(data.cFileName, basename(document).c_str()))
                    dialog.files.push_back({directory + data.cFileName, data.cFileName, false});
            } while (FindNextFileA(search, &data));
            FindClose(search);
        }
    } else {
        /* Match the original Gold Publish dialog: only existing associated
         * local files in the document's own directory are offered by EDT. */
        char *names = EDT_GetAllDocumentFiles(dialog.context);
        for (char *name = names; name && *name; name += strlen(name) + 1) {
            char *absolute = NET_MakeAbsoluteURL(const_cast<char *>(WFE_CurrentDocumentURL(dialog.context)), name);
            std::string path = path_from_url(absolute); if (absolute) XP_FREE(absolute);
            if (regular_file(path) && _stricmp(path.c_str(), document.c_str()) &&
                std::none_of(dialog.files.begin(), dialog.files.end(), [&](const PublishFile &item) {
                    return !_stricmp(item.local.c_str(), path.c_str());
                })) dialog.files.push_back({path, basename(path), true});
        }
        if (names) XP_FREE(names);
    }
}
static void fill_files(PublishDialog &dialog)
{
    HWND list = GetDlgItem(dialog.window, files_id); ListView_DeleteAllItems(list);
    for (size_t i = 0; i < dialog.files.size(); ++i) {
        std::string label = dialog.files[i].name + (i == 0 ? " (document; always included)" : "");
        LVITEMA item = {}; item.mask = LVIF_TEXT; item.iItem = (int)i; item.pszText = (char *)label.c_str();
        ListView_InsertItem(list, &item); ListView_SetCheckState(list, (int)i, dialog.files[i].selected);
    }
}
static std::string control_text(HWND window, int id)
{
    HWND child = GetDlgItem(window, id); std::vector<char> buffer(GetWindowTextLengthA(child) + 1);
    GetWindowTextA(child, buffer.data(), (int)buffer.size());
    std::string result = buffer.data(); SecureZeroMemory(buffer.data(), buffer.size()); return result;
}
static bool upload_files(PublishDialog &dialog, const std::string &destination,
                         const std::string &username, const std::string &password)
{
    size_t completed = 0;
    /* Assets first; publish the referring HTML only when assets succeeded. */
    for (size_t n = 1; n <= dialog.files.size(); ++n) {
        size_t i = n % dialog.files.size(); if (!dialog.files[i].selected) continue;
        if (dialog.window) {
            std::string status = "Uploading " + dialog.files[i].name;
            SetDlgItemTextA(dialog.window, status_id, status.c_str());
        }
        dialog.transfer = begin_upload(dialog.context, dialog.files[i], destination, username, password);
        if (!wait_transfer(dialog)) {
            if (alive(dialog) && EDT_HaveEditBuffer(dialog.context)) EDT_SetDirtyFlag(dialog.context, TRUE);
            return false;
        }
        if (dialog.window) SendDlgItemMessage(dialog.window, progress_id, PBM_SETPOS, ++completed, 0);
    }
    return !dialog.files.empty();
}
static void publish_files(PublishDialog &dialog)
{
    if (dialog.running || !alive(dialog) || dialog.files.empty()) return;
    std::string destination = control_text(dialog.window, location_id);
    if (!destination_valid(destination)) {
        SetDlgItemTextA(dialog.window, status_id, "Enter an http:// or ftp:// directory URL ending in /. Put credentials in the fields below.");
        SetFocus(GetDlgItem(dialog.window, location_id)); return;
    }
    std::string username = control_text(dialog.window, username_id), password = control_text(dialog.window, password_id);
    for (unsigned char c : username + password) if (c < 32 || c == 127) {
        wipe(password); wipe(username);
        SetDlgItemTextA(dialog.window, status_id, "User name and password cannot contain control characters."); return;
    }
    if (username.empty() && !password.empty()) {
        wipe(password); SetDlgItemTextA(dialog.window, status_id, "Enter a username for this password."); return;
    }
    HWND list = GetDlgItem(dialog.window, files_id); size_t count = 0;
    for (size_t i = 0; i < dialog.files.size(); ++i) {
        dialog.files[i].selected = !i || ListView_GetCheckState(list, (int)i);
        if (dialog.files[i].selected) ++count;
    }
    dialog.running = true; dialog.cancelled = false;
    for (int id : std::initializer_list<int>{location_id, username_id, password_id, files_id, all_id, select_all_id, select_none_id, IDOK})
        EnableWindow(GetDlgItem(dialog.window, id), FALSE);
    SendDlgItemMessage(dialog.window, progress_id, PBM_SETRANGE32, 0, (LPARAM)count);
    SendDlgItemMessage(dialog.window, progress_id, PBM_SETPOS, 0, 0);
    bool good = upload_files(dialog, destination, username, password);
    wipe(password); wipe(username); SetDlgItemTextA(dialog.window, password_id, ""); dialog.running = false;
    if (alive(dialog)) {
        const char *message = good ? "Publishing complete." : dialog.cancelled ?
            "Publishing cancelled. Local work is retained. Some remote files may have been uploaded." :
            "Publishing failed. Check the directory and credentials, then retry. Local work is retained.";
        WFE_Status(dialog.context, message); SetDlgItemTextA(dialog.window, status_id, message);
        for (int id : std::initializer_list<int>{location_id, username_id, password_id, files_id, all_id, select_all_id, select_none_id, IDOK})
            EnableWindow(GetDlgItem(dialog.window, id), TRUE);
        if (good) last_location = destination; /* Nonsecret, session-only. */
    }
    if (dialog.closing && IsWindow(dialog.window)) DestroyWindow(dialog.window);
}
static HWND control(HWND window, const char *kind, const char *text, DWORD style,
                    int x, int y, int width, int height, int id)
{
    HWND child = CreateWindowExA(!strcmp(kind, "EDIT") || !strcmp(kind, WC_LISTVIEWA) ? WS_EX_CLIENTEDGE : 0,
        kind, text, WS_CHILD | WS_VISIBLE | style, x, y, width, height, window,
        (HMENU)(INT_PTR)id, AfxGetInstanceHandle(), nullptr);
    SendMessage(child, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE); return child;
}
static LRESULT CALLBACK dialog_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam)
{
    if (message == WM_NCCREATE)
        SetWindowLongPtr(window, GWLP_USERDATA, (LONG_PTR)((CREATESTRUCT *)lparam)->lpCreateParams);
    PublishDialog *dialog = (PublishDialog *)GetWindowLongPtr(window, GWLP_USERDATA);
    if (!dialog) return DefWindowProc(window, message, wparam, lparam);
    if (message == WM_CREATE) {
        dialog->window = window;
        control(window, "STATIC", "Document and local assets to publish:", 0, 14, 12, 575, 20, -1);
        HWND list = control(window, WC_LISTVIEWA, "", WS_TABSTOP | LVS_REPORT | LVS_NOCOLUMNHEADER,
                            14, 35, 575, 158, files_id);
        ListView_SetExtendedListViewStyle(list, LVS_EX_CHECKBOXES | LVS_EX_FULLROWSELECT);
        LVCOLUMNA column = {}; column.mask = LVCF_WIDTH; column.cx = 550; ListView_InsertColumn(list, 0, &column);
        control(window, "BUTTON", "Show all files in document folder", WS_TABSTOP | BS_AUTOCHECKBOX, 14, 201, 330, 24, all_id);
        control(window, "BUTTON", "Select All", WS_TABSTOP, 365, 201, 105, 25, select_all_id);
        control(window, "BUTTON", "Select None", WS_TABSTOP, 480, 201, 109, 25, select_none_id);
        control(window, "STATIC", "Publish directory (http:// or ftp://, ending in /):", 0, 14, 237, 575, 20, -1);
        control(window, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, 14, 259, 575, 25, location_id);
        control(window, "STATIC", "User name:", 0, 14, 297, 110, 20, -1);
        control(window, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL, 125, 293, 175, 25, username_id);
        control(window, "STATIC", "Password:", 0, 315, 297, 75, 20, -1);
        control(window, "EDIT", "", WS_TABSTOP | ES_AUTOHSCROLL | ES_PASSWORD, 392, 293, 197, 25, password_id);
        control(window, "STATIC", "Passwords are not saved to preferences.", 0, 14, 328, 575, 20, -1);
        control(window, PROGRESS_CLASSA, "", 0, 14, 354, 575, 18, progress_id);
        control(window, "STATIC", "Select local assets, then choose Publish.", 0, 14, 382, 575, 45, status_id);
        control(window, "BUTTON", "Publish", WS_TABSTOP | BS_DEFPUSHBUTTON, 366, 435, 105, 29, IDOK);
        control(window, "BUTTON", "Cancel", WS_TABSTOP, 484, 435, 105, 29, IDCANCEL);
        EditorPreferences preferences = WFE_EditorPreferencesRead();
        std::string location = preferences.publish_location.empty() ? last_location : preferences.publish_location;
        SetDlgItemTextA(window, location_id, destination_valid(location) ? location.c_str() : "");
        SetDlgItemTextA(window, username_id, preferences.publish_username.c_str());
        SendDlgItemMessage(window, location_id, EM_LIMITTEXT, 4096, 0);
        SendDlgItemMessage(window, username_id, EM_LIMITTEXT, 256, 0);
        SendDlgItemMessage(window, password_id, EM_LIMITTEXT, 256, 0);
        fill_files(*dialog); ClassicUI::Apply(CWnd::FromHandle(window)); return 0;
    }
    if (message == WM_ERASEBKGND) {
        RECT rect; GetClientRect(window, &rect);
        CDC::FromHandle((HDC)wparam)->FillSolidRect(&rect, ClassicUI::Face()); return TRUE;
    }
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN || message == WM_CTLCOLOREDIT) {
        UINT type = message == WM_CTLCOLOREDIT ? CTLCOLOR_EDIT :
            message == WM_CTLCOLORBTN ? CTLCOLOR_BTN : CTLCOLOR_STATIC;
        return (LRESULT)ClassicUI::ControlColor(CDC::FromHandle((HDC)wparam), CWnd::FromHandle((HWND)lparam), type);
    }
    if (message == WM_COMMAND) {
        int command = LOWORD(wparam);
        if (command == IDOK) { publish_files(*dialog); return 0; }
        if (command == all_id && !dialog->running && alive(*dialog)) {
            collect_files(*dialog, IsDlgButtonChecked(window, all_id) == BST_CHECKED); fill_files(*dialog); return 0;
        }
        if (command == select_all_id || command == select_none_id) {
            HWND list = GetDlgItem(window, files_id);
            for (int i = 0; i < ListView_GetItemCount(list); ++i) ListView_SetCheckState(list, i, !i || command == select_all_id);
            return 0;
        }
        if (command != IDCANCEL) return DefWindowProc(window, message, wparam, lparam);
        if (dialog->running) {
            dialog->cancelled = true; cancel_transfer(dialog->transfer);
            SetDlgItemTextA(window, status_id, "Cancelling publish..."); return 0;
        }
        DestroyWindow(window); return 0;
    }
    if (message == WM_CLOSE) {
        if (dialog->running) { dialog->closing = dialog->cancelled = true; cancel_transfer(dialog->transfer); }
        else DestroyWindow(window);
        return 0;
    }
    if (message == WM_DESTROY) { dialog->window = nullptr; return 0; }
    return DefWindowProc(window, message, wparam, lparam);
}
static HWND create_dialog(PublishDialog &dialog)
{
    WNDCLASSA cls = {}; cls.lpfnWndProc = dialog_proc; cls.hInstance = AfxGetInstanceHandle();
    cls.hCursor = LoadCursor(nullptr, IDC_ARROW); cls.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    cls.lpszClassName = "NetscapeGoldPublish"; RegisterClassA(&cls);
    HWND frame = GetAncestor(dialog.owner, GA_ROOT); RECT rect = {};
    GetWindowRect(frame, &rect);
    return CreateWindowExA(WS_EX_DLGMODALFRAME | WS_EX_CONTROLPARENT, cls.lpszClassName,
        "Publish Document", WS_POPUP | WS_CAPTION | WS_SYSMENU, rect.left + 60, rect.top + 55,
        620, 510, frame, nullptr, cls.hInstance, &dialog);
}
static bool capture_dialog(HWND window, const char *path)
{
    RECT rect = {}; if (!GetWindowRect(window, &rect)) return false;
    int width = rect.right - rect.left, height = rect.bottom - rect.top;
    BITMAPINFO info = {}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    info.bmiHeader.biCompression = BI_RGB;
    HDC screen = GetDC(window), memory = CreateCompatibleDC(screen);
    void *pixels = nullptr; HBITMAP bitmap = CreateDIBSection(screen, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    if (!bitmap || !memory) {
        if (bitmap) DeleteObject(bitmap); if (memory) DeleteDC(memory); ReleaseDC(window, screen); return false;
    }
    HGDIOBJ previous = SelectObject(memory, bitmap);
    RedrawWindow(window, nullptr, nullptr, RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
    bool captured = PrintWindow(window, memory, 0) != FALSE;
    DWORD bytes = width * height * 4;
    BITMAPFILEHEADER header = {}; header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader); header.bfSize = header.bfOffBits + bytes;
    FILE *file = fopen(path, "wb"); bool written = false;
    if (file) {
        written = fwrite(&header, sizeof(header), 1, file) == 1 &&
            fwrite(&info.bmiHeader, sizeof(info.bmiHeader), 1, file) == 1 && fwrite(pixels, bytes, 1, file) == 1;
        written = fclose(file) == 0 && written;
    }
    SelectObject(memory, previous); DeleteObject(bitmap); DeleteDC(memory); ReleaseDC(window, screen);
    return captured && written;
}
static void CALLBACK smoke_cancel(HWND window, UINT, UINT_PTR timer, DWORD)
{
    KillTimer(window, timer); SendMessage(window, WM_COMMAND, IDCANCEL, 0);
}
}
void WFE_EditorPublishForget(MWContext *context)
{
    auto found = dialogs.find(context); if (found == dialogs.end()) return;
    PublishDialog *dialog = found->second;
    dialog->context = nullptr; dialog->cancelled = dialog->closing = true; cancel_transfer(dialog->transfer);
    if (!dialog->running && IsWindow(dialog->window)) DestroyWindow(dialog->window);
    dialogs.erase(found);
}
bool WFE_EditorPublishCommand(MWContext *context, UINT command)
{
    if (command != publish_command && command != browse_command) return false;
    if (!context) return true;
    if (command == browse_command) {
        std::string location = WFE_EditorPreferencesRead().browse_location;
        if (!location.empty() && (!_strnicmp(location.c_str(), "http://", 7) || !_strnicmp(location.c_str(), "https://", 8)))
            WFE_NewWindow(location.c_str());
        else WFE_Status(context, "Set the public browse address in Editor Preferences.");
        return true;
    }
    if (!EDT_HaveEditBuffer(context)) return true;
    auto existing = dialogs.find(context);
    if (existing != dialogs.end()) { if (existing->second->window) SetForegroundWindow(existing->second->window); return true; }
    PublishDialog dialog; dialog.context = context; dialog.owner = WFE_DocumentWindow(context); dialogs[context] = &dialog;
    /* A declined Save must not publish an older disk version. */
    if (!WFE_EditorCheckSave(context) || !alive(dialog) || EDT_DirtyFlag(context)) {
        if (alive(dialog)) WFE_Status(context, "Save current changes before publishing.");
        dialogs.erase(context); return true;
    }
    if (WFE_EditorSave(context, nullptr, FALSE) != 1 || !wait_save(dialog)) { dialogs.erase(context); return true; }
    collect_files(dialog, false);
    if (dialog.files.empty()) { WFE_Status(context, "Save a local HTML document before publishing."); dialogs.erase(context); return true; }
    HWND frame = GetAncestor(dialog.owner, GA_ROOT), window = create_dialog(dialog);
    if (window) {
        EnableWindow(frame, FALSE); ShowWindow(window, SW_SHOW); SetFocus(GetDlgItem(window, location_id));
        while (IsWindow(window) && alive(dialog)) {
            dispatch_messages(&dialog); if (dialog.closing && !dialog.running) break;
            MsgWaitForMultipleObjects(0, nullptr, FALSE, 20, QS_ALLINPUT);
        }
        if (IsWindow(window)) DestroyWindow(window);
        if (IsWindow(frame)) { EnableWindow(frame, TRUE); SetForegroundWindow(frame); }
    } else WFE_Status(context, "Could not open Publish Document.");
    dialogs.erase(context); return true;
}
bool WFE_EditorPublishUpdate(MWContext *context, CCmdUI *command)
{
    if (!command || (command->m_nID != publish_command && command->m_nID != browse_command)) return false;
    if (command->m_nID == browse_command) {
        std::string location = WFE_EditorPreferencesRead().browse_location;
        command->Enable(context && IsWindow(WFE_DocumentWindow(context)) && !location.empty()); return true;
    }
    command->Enable(context && EDT_HaveEditBuffer(context) && !EDT_IsBlocked(context) &&
        !WFE_EditorSaving(context) && dialogs.find(context) == dialogs.end()); return true;
}
int WFE_EditorPublishSmoke(MWContext *context, const char *report, const char *destination)
{
    /* Even a mis-invoked smoke command can only upload to IPv4 loopback. */
    std::string location = destination ? destination : ""; size_t start = location.find("://");
    bool loopback = start != std::string::npos && location.compare(start + 3, 10, "127.0.0.1:") == 0;
    if (!report || !context || !loopback || !destination_valid(location)) return 1;
    FILE *output = fopen(report, "w"); if (!output) return 1;
    PublishDialog dialog; dialog.context = context; dialog.owner = WFE_DocumentWindow(context); dialogs[context] = &dialog;
    bool loaded = EDT_HaveEditBuffer(context), saved = false, published = false, failed = false;
    bool retry = false, cancelled = false, dirty = false, selection = false;
    bool native_dialog = false, native_cancel = false, capture = false;
    bool credentials = _strnicmp(location.c_str(), "http:", 5) != 0;
    bool invalid = !destination_valid("file:///tmp/") && !destination_valid("http://127.0.0.1:0/") &&
        !destination_valid("http://user:password@127.0.0.1/") && !destination_valid("http://127.0.0.1/no-slash");
    std::string remote_name = basename(path_from_url(WFE_CurrentDocumentURL(context)));
    std::string saved_path = std::string(report) + ".saved.html";
    if (loaded) {
        EDT_EndOfDocument(context, FALSE); EDT_PasteText(context, (char *)" Native Gold publish");
        WFE_EditorTestMode(TRUE);
        saved = WFE_EditorSave(context, saved_path.c_str(), TRUE) == 1 && wait_save(dialog);
        WFE_EditorTestMode(FALSE);
    }
    if (saved && alive(dialog)) {
        collect_files(dialog, false);
        if (!dialog.files.empty()) {
            dialog.files[0].name = remote_name;
            HWND window = create_dialog(dialog);
            if (window) {
                ShowWindow(window, SW_SHOWNOACTIVATE); UpdateWindow(window);
                HWND list = GetDlgItem(window, files_id);
                native_dialog = IsWindow(list) && ListView_GetItemCount(list) == (int)dialog.files.size() &&
                    IsWindow(GetDlgItem(window, progress_id)) && IsWindow(GetDlgItem(window, IDOK)) &&
                    (GetWindowLongPtr(GetDlgItem(window, password_id), GWL_STYLE) & ES_PASSWORD) != 0;
                CheckDlgButton(window, all_id, BST_CHECKED); SendMessage(window, WM_COMMAND, all_id, 0);
                native_dialog = native_dialog && std::any_of(dialog.files.begin(), dialog.files.end(),
                    [](const PublishFile &file) { return file.name == "empty.bin"; });
                dialog.files[0].name = remote_name; fill_files(dialog);
                SendMessage(window, WM_COMMAND, select_none_id, 0);
                selection = ListView_GetCheckState(list, 0) != 0;
                for (size_t i = 1; i < dialog.files.size(); ++i) selection = selection && !ListView_GetCheckState(list, (int)i);
                SendMessage(window, WM_COMMAND, select_all_id, 0);
                for (size_t i = 0; i < dialog.files.size(); ++i) selection = selection && ListView_GetCheckState(list, (int)i);
                size_t selected = 1;
                for (size_t i = 1; i < dialog.files.size(); ++i) {
                    bool include = dialog.files[i].name == "pixel.gif" || dialog.files[i].name == "empty.bin";
                    ListView_SetCheckState(list, (int)i, include); if (include) ++selected;
                }
                selection = selection && selected == 3;
                SetDlgItemTextA(window, username_id, ""); SetDlgItemTextA(window, password_id, "");
                SetDlgItemTextA(window, location_id, "file:///invalid/"); SendMessage(window, WM_COMMAND, IDOK, 0);
                invalid = invalid && !dialog.transfer && control_text(window, status_id).find("Enter an ") == 0;
                std::string origin = location.substr(0, location.find('/', start + 3));
                SetDlgItemTextA(window, location_id, (origin + "/error/").c_str()); SendMessage(window, WM_COMMAND, IDOK, 0);
                failed = dialog.transfer && !dialog.transfer->good && !dialog.cancelled &&
                    control_text(window, status_id).find("Publishing failed.") == 0;
                dirty = alive(dialog) && EDT_DirtyFlag(context) != FALSE;
                SetDlgItemTextA(window, location_id, (origin + "/cancel/").c_str());
                UINT_PTR timer = SetTimer(window, 1, 250, smoke_cancel);
                if (timer) {
                    SendMessage(window, WM_COMMAND, IDOK, 0); KillTimer(window, timer);
                    cancelled = dialog.transfer && dialog.transfer->cancelled && !dialog.transfer->good &&
                        dialog.cancelled && IsWindow(window) && !dialog.running;
                }
                SetDlgItemTextA(window, location_id, location.c_str()); SendMessage(window, WM_COMMAND, IDOK, 0);
                published = dialog.transfer && dialog.transfer->good &&
                    control_text(window, status_id) == "Publishing complete.";
                retry = failed && published && IsWindowEnabled(GetDlgItem(window, IDOK));
                dirty = dirty && alive(dialog) && EDT_DirtyFlag(context) != FALSE;
                if (!_strnicmp(location.c_str(), "http:", 5)) {
                    SetDlgItemTextA(window, location_id, (origin + "/auth/").c_str());
                    SetDlgItemTextA(window, username_id, "gold@user%+");
                    SetDlgItemTextA(window, password_id, "p@ss%+ word");
                    SendMessage(window, WM_COMMAND, IDOK, 0);
                    credentials = dialog.transfer && dialog.transfer->good &&
                        control_text(window, status_id) == "Publishing complete." &&
                        control_text(window, password_id).empty();
                }
                capture = capture_dialog(window, (std::string(report) + ".publish.bmp").c_str());
                SendMessage(window, WM_COMMAND, IDCANCEL, 0);
                native_cancel = !IsWindow(window) && alive(dialog);
            }
        }
    }
    bool passed = loaded && saved && published && selection && invalid && failed && retry && cancelled && dirty &&
        native_dialog && native_cancel && capture && credentials;
    fprintf(output, "passed=%d\nsaved=%d\npublished=%d\nselection=%d\ninvalid_destination=%d\nfailed=%d\nretry=%d\ncancelled=%d\ndirty_retained=%d\nnative_dialog=%d\nnative_cancel=%d\ncapture=%d\ncredentials=%d\n",
        passed, saved, published, selection, invalid, failed, retry, cancelled, dirty, native_dialog, native_cancel, capture, credentials);
    fclose(output); dialogs.erase(context); return passed ? 0 : 1;
}
#endif
