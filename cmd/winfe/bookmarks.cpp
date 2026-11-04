#include <afxwin.h>
#include <afxcmn.h>
#include <algorithm>
#include <string>
#include <shlwapi.h>
#include "bookmarks.h"
extern "C" {
#include "bkmks.h"
#include "shist.h"
#include "shistele.h"
#include "xp_file.h"
}

enum {
    IDC_BOOKMARK_TREE = 4400, IDC_BOOKMARK_OPEN, IDC_BOOKMARK_ADD,
    IDC_BOOKMARK_FOLDER, IDC_BOOKMARK_EDIT, IDC_BOOKMARK_DELETE,
    IDC_BOOKMARK_CLOSE, WM_BOOKMARK_REFRESH = WM_APP + 41
};

class BookmarksFrame;
/* BM_CleanupBookmarksContext does not unlink ContextList in this core.
 * Keep this context, and its BM tree, alive until process termination. */
static MWContext bookmarks_context;
static bool bookmarks_initialized;
static BookmarksFrame *bookmarks_window;
static MWContext *bookmarks_source;
static std::string last_open_request;

static bool can_open(BM_Entry *entry)
{
    if (entry && BM_IsAlias(entry)) entry = BM_GetAliasOriginal(entry);
    return entry && BM_IsUrl(entry);
}

static bool entry_exists(BM_Entry *parent, BM_Entry *wanted)
{
    for (BM_Entry *entry = BM_GetChildren(parent); entry; entry = BM_GetNext(entry)) {
        if (entry == wanted) return true;
        if (BM_IsHeader(entry) && entry_exists(entry, wanted)) return true;
    }
    return false;
}

static bool save_bookmarks(bool alert)
{
    bool ok = BM_SaveBookmarks(&bookmarks_context, nullptr) >= 0;
    if (!ok && alert) AfxMessageBox("Unable to save bookmarks. Changes are still in memory.", MB_OK | MB_ICONERROR);
    return ok;
}

class BookmarksFrame : public CFrameWnd {
public:
    CTreeCtrl tree;
    CButton open, add, folder, edit, remove, close;
    bool dirty = true, rebuilding = false;
    void Arrange();
    void Refresh();
    void Append(BM_Entry *header, HTREEITEM parent);
    BM_Entry *Selected();
    void Select(BM_Entry *entry);
    void OpenSelected();
    void DeleteSelected();
    bool ApplyProperties(BM_Entry *entry, const char *name, const char *address);
    void PostNcDestroy() override { delete this; }
    BOOL PreTranslateMessage(MSG *message) override;
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnDestroy();
    afx_msg void OnClose();
    afx_msg LRESULT OnRefresh(WPARAM, LPARAM);
    afx_msg void OnOpen() { OpenSelected(); }
    afx_msg void OnAdd() { WFE_BookmarksAddPage(bookmarks_source); }
    afx_msg void OnFolder();
    afx_msg void OnEdit();
    afx_msg void OnDelete() { DeleteSelected(); }
    afx_msg void OnSelection(NMHDR *, LRESULT *result);
    afx_msg void OnDoubleClick(NMHDR *, LRESULT *result) { OpenSelected(); *result = 0; }
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(BookmarksFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_DESTROY()
 ON_WM_CLOSE()
 ON_MESSAGE(WM_BOOKMARK_REFRESH, OnRefresh)
 ON_COMMAND(IDC_BOOKMARK_OPEN, OnOpen)
 ON_COMMAND(IDC_BOOKMARK_ADD, OnAdd)
 ON_COMMAND(IDC_BOOKMARK_FOLDER, OnFolder)
 ON_COMMAND(IDC_BOOKMARK_EDIT, OnEdit)
 ON_COMMAND(IDC_BOOKMARK_DELETE, OnDelete)
 ON_COMMAND(IDC_BOOKMARK_CLOSE, OnClose)
 ON_NOTIFY(TVN_SELCHANGED, IDC_BOOKMARK_TREE, OnSelection)
 ON_NOTIFY(NM_DBLCLK, IDC_BOOKMARK_TREE, OnDoubleClick)
END_MESSAGE_MAP()

int BookmarksFrame::OnCreate(LPCREATESTRUCT create)
{
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    if (!tree.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP |
                     TVS_HASBUTTONS | TVS_HASLINES | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
                     CRect(0,0,0,0), this, IDC_BOOKMARK_TREE)) return -1;
    CFont *font = CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT));
    tree.SetFont(font);
    CButton *buttons[] = {&open, &add, &folder, &edit, &remove, &close};
    const char *names[] = {"Open", "Add Page", "New Folder...", "Edit...", "Delete", "Close"};
    for (int i = 0; i < 6; ++i) {
        if (!buttons[i]->Create(names[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_PUSHBUTTON,
                                CRect(0,0,0,0), this, IDC_BOOKMARK_OPEN + i)) return -1;
        buttons[i]->SetFont(font);
    }
    WFE_RegisterNativeWindow(&bookmarks_context, GetSafeHwnd());
    BM_SetFEData(&bookmarks_context, this);
    Refresh();
    Arrange();
    return 0;
}

void BookmarksFrame::Arrange()
{
    if (!tree.GetSafeHwnd() || !close.GetSafeHwnd()) return;
    CRect area; GetClientRect(&area);
    int right = (std::max)(area.Width() - 112, 8);
    tree.MoveWindow(8, 8, (std::max)(right - 16, 20), (std::max)(area.Height() - 16, 20));
    CButton *buttons[] = {&open, &add, &folder, &edit, &remove, &close};
    for (int i = 0; i < 6; ++i) buttons[i]->MoveWindow(right, 8 + i * 34, 104, 28);
}

void BookmarksFrame::OnSize(UINT type, int width, int height)
{
    CFrameWnd::OnSize(type, width, height);
    Arrange();
}

void BookmarksFrame::Append(BM_Entry *header, HTREEITEM parent)
{
    for (BM_Entry *entry = BM_GetChildren(header); entry; entry = BM_GetNext(entry)) {
        const char *name = BM_GetName(entry);
        if (BM_IsSeparator(entry)) name = "----------------";
        if (!name || !*name) name = BM_IsUrl(entry) ? BM_GetAddress(entry) : "Untitled";
        HTREEITEM item = tree.InsertItem(name ? name : "Untitled", parent);
        tree.SetItemData(item, reinterpret_cast<DWORD_PTR>(entry));
        if (BM_IsHeader(entry)) {
            Append(entry, item);
            tree.Expand(item, TVE_EXPAND);
        }
    }
}

void BookmarksFrame::Refresh()
{
    if (!tree.GetSafeHwnd() || rebuilding) return;
    rebuilding = true;
    tree.SetRedraw(FALSE);
    tree.DeleteAllItems();
    Append(BM_GetRoot(&bookmarks_context), TVI_ROOT);
    tree.SetRedraw(TRUE);
    tree.Invalidate();
    dirty = false;
    rebuilding = false;
    LRESULT ignored;
    OnSelection(nullptr, &ignored);
}

BM_Entry *BookmarksFrame::Selected()
{
    /* Never read a borrowed row pointer after a BM mutation. */
    if (dirty) Refresh();
    HTREEITEM item = tree.GetSelectedItem();
    return item ? reinterpret_cast<BM_Entry *>(tree.GetItemData(item)) : nullptr;
}

void BookmarksFrame::Select(BM_Entry *entry)
{
    if (dirty) Refresh();
    HTREEITEM item = tree.GetRootItem();
    while (item) {
        if (reinterpret_cast<BM_Entry *>(tree.GetItemData(item)) == entry) {
            tree.SelectItem(item); tree.EnsureVisible(item); return;
        }
        if (tree.GetChildItem(item)) item = tree.GetChildItem(item);
        else {
            while (item && !tree.GetNextSiblingItem(item)) item = tree.GetParentItem(item);
            if (item) item = tree.GetNextSiblingItem(item);
        }
    }
}

void BookmarksFrame::OnSelection(NMHDR *, LRESULT *result)
{
    *result = 0;
    if (rebuilding || !close.GetSafeHwnd()) return;
    BM_Entry *entry = Selected();
    open.EnableWindow(can_open(entry));
    edit.EnableWindow(entry && !BM_IsSeparator(entry) && !BM_IsAlias(entry));
    remove.EnableWindow(entry != nullptr);
}

LRESULT BookmarksFrame::OnRefresh(WPARAM, LPARAM) { if (dirty) Refresh(); return 0; }

void BookmarksFrame::OpenSelected()
{
    BM_Entry *entry = Selected();
    if (can_open(entry)) BM_GotoBookmark(&bookmarks_context, entry);
}

void BookmarksFrame::DeleteSelected()
{
    BM_Entry *entry = Selected();
    if (!entry) return;
    BM_SelectItem(&bookmarks_context, entry, FALSE, FALSE, TRUE);
    /* Attached entries belong to the core (including undo and aliases). */
    BM_ObeyCommand(&bookmarks_context, BM_Cmd_Delete);
    dirty = true;
    Refresh();
    save_bookmarks(true);
}

bool BookmarksFrame::ApplyProperties(BM_Entry *entry, const char *name, const char *address)
{
    if (!entry_exists(BM_GetRoot(&bookmarks_context), entry)) return false;
    BM_SetName(&bookmarks_context, entry, name);
    if (BM_IsUrl(entry) && address) BM_SetAddress(&bookmarks_context, entry, address);
    dirty = true;
    Refresh();
    Select(entry);
    return save_bookmarks(true);
}

void BookmarksFrame::OnEdit()
{
    BM_Entry *entry = Selected();
    if (!entry || BM_IsSeparator(entry) || BM_IsAlias(entry)) return;
    bool url = BM_IsUrl(entry) != FALSE;
    std::string old_address = url && BM_GetAddress(entry) ? BM_GetAddress(entry) : "";
    char *name = WFE_NativePrompt(&bookmarks_context, "Bookmark name:", BM_GetName(entry), 0);
    if (!name) return;
    std::string new_name(name); free(name);
    char *address = url ? WFE_NativePrompt(&bookmarks_context, "Bookmark address:", old_address.c_str(), 0) : nullptr;
    if (url && !address) return;
    if (!url || *address) ApplyProperties(entry, new_name.c_str(), address);
    free(address);
}

void BookmarksFrame::OnFolder()
{
    BM_Entry *parent = Selected();
    if (!parent || !BM_IsHeader(parent)) parent = BM_GetAddHeader(&bookmarks_context);
    char *name = WFE_NativePrompt(&bookmarks_context, "New folder name:", "New Folder", 0);
    if (!name) return;
    if (!entry_exists(BM_GetRoot(&bookmarks_context), parent)) parent = BM_GetRoot(&bookmarks_context);
    BM_Entry *entry = BM_NewHeader(name); free(name);
    if (!entry) { AfxMessageBox("Unable to allocate bookmark folder."); return; }
    BM_AppendToHeader(&bookmarks_context, parent, entry);
    dirty = true; Refresh(); Select(entry);
    save_bookmarks(true);
}

BOOL BookmarksFrame::PreTranslateMessage(MSG *message)
{
    if (message->message == WM_KEYDOWN) {
        if (message->wParam == VK_ESCAPE) { SendMessage(WM_CLOSE); return TRUE; }
        if (message->hwnd == tree.GetSafeHwnd() && message->wParam == VK_RETURN) { OpenSelected(); return TRUE; }
        if (message->hwnd == tree.GetSafeHwnd() && message->wParam == VK_DELETE) { DeleteSelected(); return TRUE; }
        if (message->wParam == VK_TAB) {
            HWND next = ::GetNextDlgTabItem(GetSafeHwnd(), message->hwnd, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            if (next) { ::SetFocus(next); return TRUE; }
        }
    }
    return CFrameWnd::PreTranslateMessage(message);
}

void BookmarksFrame::OnClose()
{
    if (save_bookmarks(true)) CFrameWnd::OnClose();
}

void BookmarksFrame::OnDestroy()
{
    BM_SetFEData(&bookmarks_context, nullptr);
    WFE_UnregisterNativeWindow(&bookmarks_context);
    if (bookmarks_window == this) bookmarks_window = nullptr;
    CFrameWnd::OnDestroy();
}

extern "C" int WFE_BookmarksInit(void)
{
    if (bookmarks_initialized) return 1;
    char *path = WH_FileName("", xpBookmarks);
    if (!path) return 0;
    WFE_InitContext(&bookmarks_context, nullptr);
    bookmarks_context.type = MWContextBookmarks;
    if (BM_InitializeBookmarksContext(&bookmarks_context) < 0) {
        WFE_DestroyContext(&bookmarks_context);
        free(path);
        return 0;
    }
    bookmarks_initialized = true;
    if (path) {
        char url[32768]; DWORD size = sizeof(url);
        bool has_url = SUCCEEDED(UrlCreateFromPathA(path, url, &size, 0));
        BM_ReadBookmarksFromDisk(&bookmarks_context, path, has_url ? url : nullptr);
        free(path);
    }
    /* Ensure an empty root exists before consulting add/menu header pointers. */
    BM_GetRoot(&bookmarks_context);
    return 1;
}

extern "C" void WFE_BookmarksShutdown(void)
{
    if (!bookmarks_initialized) return;
    save_bookmarks(true);
    if (bookmarks_window) bookmarks_window->DestroyWindow();
    /* Do not destroy/reset bookmarks_context: the core still retains it. */
    bookmarks_source = nullptr;
}

extern "C" void WFE_BookmarksChanged(MWContext *context)
{
    if (context != &bookmarks_context || !bookmarks_window) return;
    bookmarks_window->dirty = true;
    bookmarks_window->PostMessage(WM_BOOKMARK_REFRESH);
}

extern "C" void WFE_BookmarksShow(MWContext *source)
{
    if (!WFE_BookmarksInit()) { AfxMessageBox("Unable to initialize bookmarks."); return; }
    if (source && source != &bookmarks_context) bookmarks_source = source;
    if (!bookmarks_window) {
        BookmarksFrame *frame = new BookmarksFrame;
        if (!frame->Create(nullptr, "Bookmarks", WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                           CRect(120,120,780,580))) return;
        bookmarks_window = frame;
    }
    bookmarks_window->ShowWindow(SW_SHOWNORMAL);
    bookmarks_window->SetForegroundWindow();
}

extern "C" void WFE_BookmarksAddPage(MWContext *source)
{
    if (!source || !XP_IsContextInList(source) || source->type != MWContextBrowser || !WFE_BookmarksInit()) return;
    History_entry *history = SHIST_GetCurrent(&source->hist);
    BM_Entry *entry = history ? SHIST_CreateHotlistStructFromHistoryEntry(history) : nullptr;
    if (!entry) { AfxMessageBox("This page cannot be bookmarked."); return; }
    bookmarks_source = source;
    BM_AppendToHeader(&bookmarks_context, BM_GetAddHeader(&bookmarks_context), entry);
    if (bookmarks_window) { bookmarks_window->dirty = true; bookmarks_window->Refresh(); bookmarks_window->Select(entry); }
    save_bookmarks(true);
}

extern "C" void WFE_BookmarksEdit(MWContext *source, BM_Entry *entry)
{
    WFE_BookmarksShow(source);
    if (!bookmarks_window || !entry_exists(BM_GetRoot(&bookmarks_context), entry)) return;
    bookmarks_window->Select(entry);
    bookmarks_window->SendMessage(WM_COMMAND, IDC_BOOKMARK_EDIT);
}

extern "C" void WFE_BookmarkNavigate(MWContext *context, const char *address)
{
    if (!address || !*address) return;
    std::string copy(address);
    MWContext *source = context == &bookmarks_context ? bookmarks_source : context;
    if (source && XP_IsContextInList(source) && source->type == MWContextBrowser)
        WFE_NavigateTarget(source, copy.c_str(), nullptr);
    else WFE_NewWindow(copy.c_str());
    last_open_request = copy;
}

static BM_Entry *find_named(BM_Entry *header, const char *name)
{
    for (BM_Entry *entry = BM_GetChildren(header); entry; entry = BM_GetNext(entry)) {
        if (BM_GetName(entry) && !strcmp(BM_GetName(entry), name)) return entry;
        if (BM_IsHeader(entry)) { BM_Entry *found = find_named(entry, name); if (found) return found; }
    }
    return nullptr;
}

static const char *smoke_title = "Native \"quoted\" & <bookmark> &lt;";
static const char *smoke_retry_title = "Native retry \"quoted\" & <bookmark> &lt;";
static const std::string smoke_url = std::string("https://example.invalid/path?q=\"one\"&tag=<bookmark>&literal=&lt;") +
                                     std::string(600, '&');

static bool smoke_read_locked(HANDLE file, std::string &bytes)
{
    LARGE_INTEGER size, start = {};
    if (!GetFileSizeEx(file, &size) || size.QuadPart <= 0 || size.QuadPart > 16 * 1024 * 1024 ||
        !SetFilePointerEx(file, start, nullptr, FILE_BEGIN)) return false;
    bytes.resize((size_t)size.QuadPart);
    DWORD received = 0;
    return ReadFile(file, &bytes[0], (DWORD)bytes.size(), &received, nullptr) && received == bytes.size();
}

static bool smoke_save_snapshot(const char *report, const char *name, const std::string &bytes)
{
    std::string path = report;
    size_t slash = path.find_last_of("/\\");
    path = slash == std::string::npos ? name : path.substr(0, slash + 1) + name;
    FILE *file = fopen(path.c_str(), "wb");
    if (!file) return false;
    bool written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    return fclose(file) == 0 && written;
}

extern "C" int WFE_BookmarksSmoke(MWContext *source, const char *report, const char *phase)
{
    if (!WFE_BookmarksInit()) return 2;
    FILE *file = fopen(report, "ab");
    if (!file) return 2;
    int result = 1;
    const char *url = smoke_url.c_str();
    BM_Entry *root = BM_GetRoot(&bookmarks_context);
    if (!strcmp(phase, "seed")) {
        BM_Entry *folder = BM_NewHeader("Native & folders");
        BM_Entry *second = BM_NewUrl("Second bookmark", "https://example.invalid/second", "text/html", 0);
        BM_Entry *kept = BM_NewUrl("Before edit", "https://example.invalid/before-edit", "text/html", 0);
        BM_Entry *deleted = BM_NewUrl("Delete me", "about:blank", "text/html", 0);
        if (!folder || !second || !kept || !deleted) {
            if (folder) BM_FreeEntry(&bookmarks_context, folder);
            if (second) BM_FreeEntry(&bookmarks_context, second);
            if (kept) BM_FreeEntry(&bookmarks_context, kept);
            if (deleted) BM_FreeEntry(&bookmarks_context, deleted);
            fclose(file); return 2;
        }
        BM_AppendToHeader(&bookmarks_context, root, folder);
        BM_AppendToHeader(&bookmarks_context, folder, second);
        BM_AppendToHeader(&bookmarks_context, folder, kept);
        BM_AppendToHeader(&bookmarks_context, folder, deleted);
        fprintf(file, "bookmarks_created=1\n");
        WFE_BookmarksShow(source);
        if (bookmarks_window) {
            bool edited = bookmarks_window->ApplyProperties(kept, smoke_title, url);
            History_entry *current = source ? SHIST_GetCurrent(&source->hist) : nullptr;
            BM_Entry *add_parent = BM_GetAddHeader(&bookmarks_context);
            BM_Entry *previous = add_parent ? BM_GetChildren(add_parent) : nullptr;
            while (previous && BM_GetNext(previous)) previous = BM_GetNext(previous);
            bool added_page = false;
            if (current && current->address && add_parent) {
                std::string current_title = current->title ? current->title : "";
                std::string current_address = current->address;
                WFE_BookmarksAddPage(source);
                BM_Entry *added = previous ? BM_GetNext(previous) : BM_GetChildren(add_parent);
                added_page = added && BM_IsUrl(added) && !BM_GetNext(added) &&
                    current_title == (BM_GetName(added) ? BM_GetName(added) : "") &&
                    current_address == (BM_GetAddress(added) ? BM_GetAddress(added) : "");
                if (added) {
                    bookmarks_window->Select(added);
                    bookmarks_window->SendMessage(WM_COMMAND, IDC_BOOKMARK_DELETE);
                }
            }
            bookmarks_window->Select(deleted);
            last_open_request.clear();
            bookmarks_window->SendMessage(WM_COMMAND, IDC_BOOKMARK_OPEN);
            bool opened = last_open_request == "about:blank";
            bookmarks_window->SendMessage(WM_COMMAND, IDC_BOOKMARK_DELETE);
            bool removed = !find_named(root, "Delete me");
            bookmarks_window->SendMessage(WM_CLOSE);
            bool closed = bookmarks_window == nullptr;
            WFE_BookmarksShow(source);
            bool manager = closed && bookmarks_window && bookmarks_window->tree.GetCount() >= 3;
            bool saved = save_bookmarks(false);
            fprintf(file, "bookmarks_edited=%d\nbookmarks_add_page=%d\nbookmarks_deleted=%d\nbookmarks_manager=%d\nbookmarks_open_requested=%d\nbookmarks_persisted=%d\n",
                    edited, added_page, removed, manager, opened, saved);
            result = edited && added_page && removed && manager && opened && saved ? 0 : 1;
        }
    } else if (!strcmp(phase, "verify") || !strcmp(phase, "verify-retry") || !strcmp(phase, "verify-legacy")) {
        bool legacy = !strcmp(phase, "verify-legacy");
        std::string second_name = legacy ? std::string("Second") + std::string(6000, 'X') + " bookmark" : "Second bookmark";
        BM_Entry *folder = find_named(root, "Native & folders");
        BM_Entry *second = folder ? BM_GetChildren(folder) : nullptr;
        BM_Entry *kept = second ? BM_GetNext(second) : nullptr;
        bool loaded = second && kept && !BM_GetNext(kept) &&
            second_name == (BM_GetName(second) ? BM_GetName(second) : "") &&
            !strcmp(BM_GetAddress(second) ? BM_GetAddress(second) : "", "https://example.invalid/second") &&
            !strcmp(BM_GetName(kept) ? BM_GetName(kept) : "", strcmp(phase, "verify") ? smoke_retry_title : smoke_title) &&
            !strcmp(BM_GetAddress(kept) ? BM_GetAddress(kept) : "", url);
        fprintf(file, "bookmarks_loaded=%d\nbookmarks_count=%d\n", loaded, loaded ? 2 : 0);
        if (legacy) {
            std::string description = folder && BM_GetDescription(folder) ? BM_GetDescription(folder) : "";
            while (!description.empty() && (description.back() == '\r' || description.back() == '\n')) description.pop_back();
            bool preserved = description == std::string(2042, 'A');
            fprintf(file, "bookmarks_legacy_description=%d\n", preserved);
            loaded = loaded && preserved;
        }
        result = loaded ? 0 : 1;
    } else if (!strcmp(phase, "save-failure")) {
        BM_Entry *kept = find_named(root, smoke_title);
        char *path = WH_FileName("", xpBookmarks);
        HANDLE locked = path ? CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr) : INVALID_HANDLE_VALUE;
        free(path);
        std::string before, after;
        bool ready = kept && locked != INVALID_HANDLE_VALUE && smoke_read_locked(locked, before);
        bool failed = false, preserved = false, snapshots = false;
        if (ready) {
            BM_SetName(&bookmarks_context, kept, smoke_retry_title);
            failed = !save_bookmarks(false);
            preserved = smoke_read_locked(locked, after) && before == after;
            snapshots = smoke_save_snapshot(report, "bookmarks-before-failed-save.htm", before) &&
                        smoke_save_snapshot(report, "bookmarks-after-failed-save.htm", after);
        }
        if (locked != INVALID_HANDLE_VALUE) CloseHandle(locked);
        /* Retry without another mutation: a failed save must remain dirty. */
        bool retried = ready && save_bookmarks(false);
        fprintf(file, "bookmarks_save_failed=%d\nbookmarks_preserved=%d\nbookmarks_retried=%d\n",
                failed, preserved && snapshots, retried);
        result = failed && preserved && snapshots && retried ? 0 : 1;
    }
    fclose(file);
    return result;
}
