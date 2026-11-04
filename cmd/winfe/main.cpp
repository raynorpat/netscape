#include <afxwin.h>
#include <afxext.h>
#include <afxcoll.h>
#include <afxcmn.h>
#include <shlwapi.h>
#include <map>
#include <algorithm>
#include <string>
#include <vector>
#ifdef _DEBUG
#include <crtdbg.h>
static int debug_report(int type, char *message, int *)
{
    if (message) { fputs(message, stderr); fflush(stderr); }
    if (type == _CRT_ASSERT) ExitProcess(3);
    return TRUE;
}
static void invalid_parameter(const wchar_t *expression, const wchar_t *function,
                              const wchar_t *file, unsigned int line, uintptr_t)
{
    fwprintf(stderr, L"Invalid parameter: %ls, function %ls, %ls:%u\n",
             expression ? expression : L"", function ? function : L"",
             file ? file : L"", line);
    fflush(stderr);
    ExitProcess(3);
}
#endif
#include "resource.h"
#include "legacy/genedit.h"
#include "legacy/cmdparse.h"
#include "frontend.h"
#include "msgcom.h"
#include "libi18n.h"
#include "mailsettings.h"
#include "composer.h"
#include "images.h"
#include "forms.h"
#include "shist.h"

class BrowserFrame;
static bool smoke_mode = false;
static std::string smoke_error;
static std::vector<BrowserFrame *> browser_windows;
static std::map<MWContext *, BrowserFrame *> browser_contexts;
static std::map<MWContext *, HWND> native_windows;
static const UINT WM_MAIL_CHANGED = WM_APP + 30;

class BrowserView : public CWnd {
public:
    BrowserFrame *frame = nullptr;
    CDC *drawing = nullptr;
    COLORREF background = RGB(255,255,255);
    int scroll_x = 0, scroll_y = 0, document_width = 0, document_height = 0;
    unsigned drawn_runs = 0;
    std::string drawn_text;
    LO_ImageStruct *backdrop = nullptr;
    std::vector<LO_ImageStruct *> drawn_images;
    CPoint link_point;
    std::string link_text;
    std::map<std::string, CPoint> links;
    void Render(CDC &dc, int width, int height);
protected:
    BOOL OnCommand(WPARAM command, LPARAM control) override;
    afx_msg void OnPaint();
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()
};

class BrowserFrame : public CFrameWnd {
public:
    BrowserView view;
    CGenericEdit address;
    CButton go;
    CStatusBar status;
    CListCtrl folders, messages;
    MWContext context;
    MWContext *grid_parent = nullptr;
    std::string grid_name;
    int grid_scrolling = LO_SCROLL_AUTO;
    bool resizing = false;
    int mode = 0; /* browser, mail, news */
    bool updating_lists = false;
    bool mail_update_pending = false;
    unsigned int mail_changes = 0;
    bool initialized = false, completed = false;
    int load_status = 0;
    std::string current_url;
    std::vector<std::string> history;
    size_t history_position = 0;
    void Navigate(const char *url, bool remember = true);
    void RefreshMailLists();
    void InitializeMessaging(const char *server);
    BOOL PreTranslateMessage(MSG *message) override;
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnOpen();
    afx_msg void OnGo();
    afx_msg void OnStop();
    afx_msg void OnReload();
    afx_msg void OnBack();
    afx_msg void OnForward();
    afx_msg void OnAbout();
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnMail();
    afx_msg void OnNews();
    afx_msg void OnCompose();
    afx_msg void OnPost();
    afx_msg void OnReply();
    afx_msg void OnReplyAll();
    afx_msg void OnForwardMessage();
    afx_msg void OnGetMail();
    afx_msg void OnSendQueued();
    afx_msg void OnShowAllMessages();
    afx_msg void OnShowUnreadMessages();
    afx_msg void OnDeleteMessage();
    afx_msg void OnMarkRead();
    afx_msg void OnSettings();
    afx_msg void OnFolderSelection(NMHDR *header, LRESULT *result);
    afx_msg void OnMessageSelection(NMHDR *header, LRESULT *result);
    afx_msg void OnFolderDoubleClick(NMHDR *header, LRESULT *result);
    afx_msg void OnMessageDoubleClick(NMHDR *header, LRESULT *result);
    afx_msg LRESULT OnMailChanged(WPARAM, LPARAM);
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(BrowserFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_COMMAND(ID_BROWSER_OPEN, OnOpen)
 ON_COMMAND(ID_NAV_GO, OnGo)
 ON_COMMAND(ID_NAV_STOP, OnStop)
 ON_COMMAND(ID_NAV_RELOAD, OnReload)
 ON_COMMAND(ID_NAV_BACK, OnBack)
 ON_COMMAND(ID_NAV_FORWARD, OnForward)
 ON_COMMAND(ID_BROWSER_ABOUT, OnAbout)
 ON_COMMAND(ID_CLIENT_MAIL, OnMail)
 ON_COMMAND(ID_CLIENT_NEWS, OnNews)
 ON_COMMAND(ID_CLIENT_COMPOSE, OnCompose)
 ON_COMMAND(ID_CLIENT_POST, OnPost)
 ON_COMMAND(ID_MESSAGE_REPLY, OnReply)
 ON_COMMAND(ID_MESSAGE_REPLY_ALL, OnReplyAll)
 ON_COMMAND(ID_MESSAGE_FORWARD, OnForwardMessage)
 ON_COMMAND(ID_MESSAGE_GET, OnGetMail)
 ON_COMMAND(ID_MESSAGE_SEND_QUEUED, OnSendQueued)
 ON_COMMAND(ID_MESSAGE_SHOW_ALL, OnShowAllMessages)
 ON_COMMAND(ID_MESSAGE_SHOW_UNREAD, OnShowUnreadMessages)
 ON_COMMAND(ID_MESSAGE_DELETE, OnDeleteMessage)
 ON_COMMAND(ID_MESSAGE_READ, OnMarkRead)
 ON_COMMAND(ID_MAIL_SETTINGS, OnSettings)
 ON_NOTIFY(LVN_ITEMCHANGED, ID_FOLDER_LIST, OnFolderSelection)
 ON_NOTIFY(LVN_ITEMCHANGED, ID_MESSAGE_LIST, OnMessageSelection)
 ON_NOTIFY(NM_DBLCLK, ID_FOLDER_LIST, OnFolderDoubleClick)
 ON_NOTIFY(NM_DBLCLK, ID_MESSAGE_LIST, OnMessageDoubleClick)
 ON_MESSAGE(WM_MAIL_CHANGED, OnMailChanged)
END_MESSAGE_MAP()
BEGIN_MESSAGE_MAP(BrowserView, CWnd)
 ON_WM_PAINT()
 ON_WM_VSCROLL()
 ON_WM_HSCROLL()
 ON_WM_MOUSEWHEEL()
 ON_WM_LBUTTONUP()
END_MESSAGE_MAP()

static BrowserFrame *frame_for(MWContext *context)
{
    auto found = browser_contexts.find(context);
    return found == browser_contexts.end() ? nullptr : found->second;
}

extern "C" void WFE_RegisterNativeWindow(MWContext *context, HWND window)
{
    native_windows[context] = window;
    if (!AfxGetApp()->m_pMainWnd) AfxGetApp()->m_pMainWnd = CWnd::FromHandlePermanent(window);
}
extern "C" void WFE_UnregisterNativeWindow(MWContext *context)
{
    auto found = native_windows.find(context);
    if (found == native_windows.end()) return;
    HWND closing = found->second;
    native_windows.erase(found);
    if (AfxGetApp()->m_pMainWnd && AfxGetApp()->m_pMainWnd->GetSafeHwnd() == closing
        && !native_windows.empty())
        AfxGetApp()->m_pMainWnd = CWnd::FromHandlePermanent(native_windows.begin()->second);
}

static std::string as_url(const char *input)
{
    char full[32768], url[32768];
    DWORD size = sizeof(url);
    if (PathFileExistsA(input) && GetFullPathNameA(input, sizeof(full), full, nullptr)
        && SUCCEEDED(UrlCreateFromPathA(full, url, &size, 0))) return url;
    if (strchr(input, ':')) return input;
    return std::string("http://") + input;
}

int BrowserFrame::OnCreate(LPCREATESTRUCT create)
{
    static UINT indicators[] = {ID_SEPARATOR};
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    if (grid_parent) {
        view.frame = this;
        DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN;
        if (grid_scrolling != LO_SCROLL_NO) style |= WS_VSCROLL | WS_HSCROLL;
        if (!view.CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW)),
                           "Frame document", style, CRect(0,0,1,1), this, 112)) return -1;
        browser_contexts[&context] = this;
        WFE_InitGridContext(&context, this, grid_parent, grid_name.c_str());
        initialized = true;
        return 0;
    }
    if (!address.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
                        CRect(8,8,700,32), this, ID_ADDRESS)) return -1;
    address.SetFont(CFont::FromHandle((HFONT)GetStockObject(DEFAULT_GUI_FONT)));
    if (!go.Create("Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                    CRect(710,8,770,32), this, ID_NAV_GO)) return -1;
    view.frame = this;
    if (!view.CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW)),
                       "Document", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
                       CRect(0,40,800,600), this, 112)) return -1;
    if (!status.Create(this) || !status.SetIndicators(indicators, 1)) return -1;
    if (mode) {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
        if (!folders.Create(style, CRect(0,40,260,550), this, ID_FOLDER_LIST)
            || !messages.Create(style, CRect(265,40,800,250), this, ID_MESSAGE_LIST)) return -1;
        folders.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        messages.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        folders.InsertColumn(0, mode == 2 ? "Server / newsgroup" : "Folder", LVCFMT_LEFT, 155);
        folders.InsertColumn(1, "Unread", LVCFMT_RIGHT, 48);
        folders.InsertColumn(2, "Total", LVCFMT_RIGHT, 48);
        messages.InsertColumn(0, "Subject", LVCFMT_LEFT, 300);
        messages.InsertColumn(1, "Sender", LVCFMT_LEFT, 190);
        messages.InsertColumn(2, "Date", LVCFMT_LEFT, 130);
        address.SetReadOnly(mode == 1);
        go.SetWindowText(mode == 2 ? "Connect" : "Refresh");
    }
    browser_contexts[&context] = this;
    WFE_InitContext(&context, this);
    initialized = true;
    SetTimer(1, 10, nullptr);
    return 0;
}

void BrowserFrame::OnSize(UINT type, int width, int height)
{
    CFrameWnd::OnSize(type, width, height);
    if (!view.GetSafeHwnd()) return;
    if (grid_parent) {
        view.MoveWindow(0, 0, (std::max)(width, 1), (std::max)(height, 1));
        if (initialized) { WFE_FormPositionAll(&context); SetTimer(2, 120, nullptr); }
        return;
    }
    address.MoveWindow(8, 8, (std::max)(width - 84, 20), 24);
    go.MoveWindow((std::max)(width - 68, 8), 8, 60, 24);
    if (mode && folders.GetSafeHwnd()) {
        int left = (std::min)(260, (std::max)(100, width / 3));
        int list_height = (std::max)(100, (height - 62) / 3);
        folders.MoveWindow(0, 40, left, (std::max)(height - 62, 1));
        messages.MoveWindow(left + 4, 40, (std::max)(width - left - 4, 1), list_height);
        view.MoveWindow(left + 4, list_height + 44, (std::max)(width - left - 4, 1), (std::max)(height - list_height - 66, 1));
    } else view.MoveWindow(0, 40, width, (std::max)(height - 62, 1));
    if (initialized) WFE_FormPositionAll(&context);
    if (initialized && mode == 0 && !current_url.empty()) SetTimer(2, 120, nullptr);
}
BOOL BrowserFrame::PreTranslateMessage(MSG *message)
{
    if (WFE_FormKey(&context, message->hwnd, message->message, message->wParam, message->lParam)) return TRUE;
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN
        && GetFocus() == &address) { OnGo(); return TRUE; }
    return CFrameWnd::PreTranslateMessage(message);
}
void BrowserFrame::Navigate(const char *input, bool remember)
{
    current_url = as_url(input);
    if (address.GetSafeHwnd()) address.SetWindowText(current_url.c_str());
    if (remember) {
        if (!history.empty()) history.resize(history_position + 1);
        history.push_back(current_url);
        history_position = history.size() - 1;
    }
    completed = false;
    view.drawn_runs = 0;
    view.drawn_text.clear();
    WFE_Load(&context, current_url.c_str());
}
void BrowserFrame::OnTimer(UINT_PTR timer)
{
    if (timer == 1) WFE_Pump();
    else if (timer == 2) {
        KillTimer(2);
        if (!initialized || current_url.empty()) return;
        if (resizing || XP_IsContextBusy(&context)) { SetTimer(2, 120, nullptr); return; }
        History_entry *entry = SHIST_GetCurrent(&context.hist);
        if (!entry || entry->method != URL_GET_METHOD) return;
        URL_Struct *url = SHIST_CreateURLStructFromHistoryEntry(&context, entry);
        if (!url) return;
        resizing = true; completed = false;
        url->force_reload = NET_RESIZE_RELOAD;
        FE_GetURL(&context, url);
    } else CFrameWnd::OnTimer(timer);
}
void BrowserFrame::OnGo() { CString text; address.GetWindowText(text); if (mode == 1) { MSG_MailDirectoryChanged(&context); RefreshMailLists(); } else if (mode == 2) { if (!text.IsEmpty() && WFE_SetMailPreference("news", text)) { WFE_ApplyMailPreferences(); MSG_NewsHostChanged(&context, text); RefreshMailLists(); } } else if (!text.IsEmpty()) Navigate(text); }
void BrowserFrame::OnOpen() { CFileDialog dialog(TRUE, "html", nullptr, OFN_FILEMUSTEXIST, "Documents|*.html;*.htm;*.txt|All files|*.*||", this); if (dialog.DoModal() == IDOK) Navigate(dialog.GetPathName()); }
void BrowserFrame::OnStop() { XP_InterruptContext(&context); }
void BrowserFrame::OnReload() { if (!current_url.empty()) Navigate(current_url.c_str(), false); }
void BrowserFrame::OnBack() { if (history_position > 0) { --history_position; Navigate(history[history_position].c_str(), false); } }
void BrowserFrame::OnForward() { if (history_position + 1 < history.size()) { ++history_position; Navigate(history[history_position].c_str(), false); } }
void BrowserFrame::OnAbout() { Navigate("about:"); }
void BrowserFrame::InitializeMessaging(const char *server)
{
    WFE_ApplyMailPreferences();
    context.type = mode == 2 ? MWContextNews : MWContextMail;
    MSG_SetAutoShowFirstUnread(FALSE);
    if (mode == 2) {
        MSG_InitializeNewsContext(&context);
        const char *host = server && *server ? server : WFE_MailPreference("news");
        address.SetWindowText(host);
        MSG_FolderLine line;
        for (int i = 0; context.msgframe && MSG_GetFolderLine(&context, i, &line, nullptr); ++i) {
            if ((line.flags & MSG_FOLDER_FLAG_NEWS_HOST) && line.name
                && (!_stricmp(line.name, host)
                    || (!_strnicmp(line.name, host, strlen(host))
                        && !strncmp(line.name + strlen(host), " (", 2)))) {
                if (line.flags & MSG_FOLDER_FLAG_ELIDED) MSG_ToggleFolderExpansion(&context, i);
                break;
            }
        }
        WFE_MailChanged(&context, 8);
    } else {
        address.SetWindowText(WFE_MailDirectory());
        MSG_InitializeMailContext(&context);
    }
    RefreshMailLists();
    CRect area; GetClientRect(area); OnSize(SIZE_RESTORED, area.Width(), area.Height());
}
static CString display_header(MWContext *context, const char *raw)
{
    char *decoded = IntlDecodeMimePartIIStr(raw ? raw : "", context->win_csid, FALSE);
    CString text(decoded ? decoded : (raw ? raw : ""));
    free(decoded);
    return text;
}
void BrowserFrame::RefreshMailLists()
{
    if (!initialized || !mode || !context.msgframe || updating_lists) return;
    updating_lists = true;
    folders.SetRedraw(FALSE); messages.SetRedraw(FALSE);
    int folder_selection = -1, message_selection = -1;
    folders.DeleteAllItems(); messages.DeleteAllItems();
    MSG_FolderLine folder;
    for (int i = 0; i < 100000 && MSG_GetFolderLine(&context, i, &folder, nullptr); ++i) {
        CString name(' ', (int)folder.depth * 2);
        if (folder.flags & MSG_FOLDER_FLAG_DIRECTORY) name += folder.flags & MSG_FOLDER_FLAG_ELIDED ? "+ " : "- ";
        name += folder.name ? folder.name : "";
        folders.InsertItem(i, name);
        if (!(folder.flags & MSG_FOLDER_FLAG_DIRECTORY)) {
            CString value; value.Format("%d", folder.unseen); folders.SetItemText(i, 1, value);
            value.Format("%d", folder.total); folders.SetItemText(i, 2, value);
        }
        if (folder.flags & MSG_FOLDER_FLAG_SELECTED) folder_selection = i;
    }
    MSG_MessageLine message;
    for (int i = 0; i < 100000 && MSG_GetThreadLine(&context, i, &message, nullptr); ++i) {
        CString subject(' ', (int)message.depth * 2);
        if (message.flags & MSG_FLAG_HAS_RE) subject += "Re: ";
        subject += message.subject ? display_header(&context, message.subject) : CString("(no subject)");
        messages.InsertItem(i, subject);
        messages.SetItemText(i, 1, display_header(&context, message.from));
        messages.SetItemText(i, 2, MSG_FormatDate(&context, message.date));
        if (message.flags & MSG_FLAG_SELECTED) message_selection = i;
    }
    if (folder_selection >= 0 && folder_selection < folders.GetItemCount()) folders.SetItemState(folder_selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    if (message_selection >= 0 && message_selection < messages.GetItemCount()) messages.SetItemState(message_selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    folders.SetRedraw(TRUE); messages.SetRedraw(TRUE);
    folders.Invalidate(); messages.Invalidate();
    updating_lists = false;
}
LRESULT BrowserFrame::OnMailChanged(WPARAM, LPARAM)
{
    unsigned int changes = mail_changes;
    mail_changes = 0; mail_update_pending = false;
    if ((changes & 8) && mode == 2 && context.msgframe
        && !NET_AreThereActiveConnectionsForWindow(&context)
        && MSG_GetToggleStatus(&context, MSG_ShowAllNewsGroups) != MSG_Checked)
        MSG_Command(&context, MSG_ShowAllNewsGroups);
    RefreshMailLists();
    return 0;
}
void BrowserFrame::OnFolderSelection(NMHDR *header, LRESULT *result)
{
    *result = 0;
    NMLISTVIEW *change = (NMLISTVIEW *)header;
    if (!updating_lists && initialized && change->iItem >= 0 && (change->uChanged & LVIF_STATE)
        && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) {
        MSG_SelectFolder(&context, change->iItem, TRUE);
        RefreshMailLists();
    }
}
void BrowserFrame::OnMessageSelection(NMHDR *header, LRESULT *result)
{
    *result = 0;
    NMLISTVIEW *change = (NMLISTVIEW *)header;
    if (!updating_lists && initialized && change->iItem >= 0 && (change->uChanged & LVIF_STATE)
        && (change->uNewState & LVIS_SELECTED) && !(change->uOldState & LVIS_SELECTED)) {
        completed = false; view.drawn_text.clear(); view.drawn_runs = 0;
        MSG_SelectMessage(&context, change->iItem, TRUE);
    }
}
void BrowserFrame::OnFolderDoubleClick(NMHDR *, LRESULT *result) { *result = 0; int item = folders.GetNextItem(-1, LVNI_SELECTED); if (item >= 0) { MSG_ToggleFolderExpansion(&context, item); RefreshMailLists(); } }
void BrowserFrame::OnMessageDoubleClick(NMHDR *, LRESULT *result) { *result = 0; int item = messages.GetNextItem(-1, LVNI_SELECTED); if (item >= 0) { MSG_ToggleThreadExpansion(&context, item); RefreshMailLists(); } }
void BrowserFrame::OnMail() { WFE_NewMailWindow(0, nullptr); }
void BrowserFrame::OnNews() { WFE_NewMailWindow(1, nullptr); }
void BrowserFrame::OnCompose() { WFE_NewComposer(&context, 0); }
void BrowserFrame::OnPost() { if (mode == 2) MSG_Command(&context, MSG_PostNew); else WFE_NewComposer(&context, 1); }
void BrowserFrame::OnReply() { if (mode) MSG_Command(&context, MSG_ReplyToSender); }
void BrowserFrame::OnReplyAll() { if (mode) MSG_Command(&context, MSG_ReplyToAll); }
void BrowserFrame::OnForwardMessage() { if (mode) MSG_Command(&context, MSG_ForwardMessage); }
void BrowserFrame::OnGetMail() { if (mode == 1) MSG_Command(&context, MSG_GetNewMail); else if (mode == 2) MSG_Command(&context, MSG_GetMoreMessages); }
void BrowserFrame::OnSendQueued() { if (mode == 1) MSG_Command(&context, MSG_DeliverQueuedMessages); }
void BrowserFrame::OnShowAllMessages() { if (mode) MSG_Command(&context, MSG_ShowAllMessages); }
void BrowserFrame::OnShowUnreadMessages() { if (mode) MSG_Command(&context, MSG_ShowOnlyUnreadMessages); }
void BrowserFrame::OnDeleteMessage() { if (mode == 1) MSG_Command(&context, MSG_DeleteMessage); }
void BrowserFrame::OnMarkRead() { if (mode) MSG_Command(&context, MSG_MarkSelectedRead); }

class MailSettingsDialog : public CDialog {
public:
    explicit MailSettingsDialog(CWnd *parent) : CDialog(IDD_MAIL_SETTINGS, parent) { }
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        const char *keys[] = {"email", "name", "organization", "smtp", "pop", "news"};
        for (int i = 0; i < 6; ++i) SetDlgItemText(ID_SETTING_EMAIL + i, WFE_MailPreference(keys[i]));
        return TRUE;
    }
    void OnOK() override {
        const char *keys[] = {"email", "name", "organization", "smtp", "pop", "news"};
        for (int i = 0; i < 6; ++i) {
            CString value; GetDlgItemText(ID_SETTING_EMAIL + i, value);
            if (!WFE_SetMailPreference(keys[i], value)) { MessageBox("Could not save account settings. Check the values and profile directory."); return; }
        }
        WFE_ApplyMailPreferences();
        CDialog::OnOK();
    }
};
void BrowserFrame::OnSettings() { MailSettingsDialog dialog(this); if (dialog.DoModal() == IDOK && mode == 2 && *WFE_MailPreference("news")) { address.SetWindowText(WFE_MailPreference("news")); MSG_NewsHostChanged(&context, WFE_MailPreference("news")); } }

class NativePrompt : public CDialog {
public:
    CString label, value;
    bool password;
    NativePrompt(const char *message, const char *initial, bool secret, CWnd *parent)
        : CDialog(IDD_NATIVE_PROMPT, parent), label(message ? message : ""), value(initial ? initial : ""), password(secret) { }
    BOOL OnInitDialog() override { CDialog::OnInitDialog(); SetDlgItemText(ID_PROMPT_LABEL, label); SetDlgItemText(ID_PROMPT_VALUE, value); CEdit *edit = (CEdit *)GetDlgItem(ID_PROMPT_VALUE); if (password) edit->SetPasswordChar('*'); edit->SetSel(0,-1); edit->SetFocus(); return FALSE; }
    void OnOK() override { GetDlgItemText(ID_PROMPT_VALUE, value); CDialog::OnOK(); }
};
extern "C" char *WFE_NativePrompt(MWContext *context, const char *message, const char *initial, int password)
{
    if (smoke_mode) { smoke_error = message ? message : "Unexpected prompt"; return nullptr; }
    auto found = native_windows.find(context);
    NativePrompt prompt(message, initial, password != 0, found == native_windows.end() ? nullptr : CWnd::FromHandle(found->second));
    return prompt.DoModal() == IDOK ? _strdup(prompt.value) : nullptr;
}
extern "C" void WFE_MailChanged(MWContext *context, unsigned int changes)
{
    BrowserFrame *frame = frame_for(context);
    if (frame && frame->initialized && frame->mode) {
        frame->mail_changes |= changes;
        if (!frame->mail_update_pending) {
            frame->mail_update_pending = true;
            if (!frame->PostMessage(WM_MAIL_CHANGED)) frame->mail_update_pending = false;
        }
    }
}
void BrowserFrame::OnClose()
{
    CFrameWnd::OnClose();
}
void BrowserFrame::OnDestroy()
{
    KillTimer(1);
    KillTimer(2);
    if (initialized) { initialized = false; WFE_DestroyContext(&context); }
    browser_contexts.erase(&context);
    WFE_UnregisterNativeWindow(&context);
    auto found = std::find(browser_windows.begin(), browser_windows.end(), this);
    if (found != browser_windows.end()) browser_windows.erase(found);
    CFrameWnd::OnDestroy();
}

void BrowserView::Render(CDC &dc, int width, int height)
{
    dc.FillSolidRect(0, 0, width, height, background);
    drawing = &dc;
    drawn_images.clear();
    int saved = dc.SaveDC();
    dc.SetViewportOrg(-scroll_x, -scroll_y);
    if (backdrop) WFE_DrawBackdrop(dc.GetSafeHdc(), &frame->context, backdrop, width, height, scroll_x, scroll_y);
    LO_RefreshArea(&frame->context, scroll_x, scroll_y, width, height);
    dc.RestoreDC(saved);
    drawing = nullptr;
}
BOOL BrowserView::OnCommand(WPARAM command, LPARAM control)
{
    if (WFE_FormCommand(&frame->context, command, control)) return TRUE;
    return CWnd::OnCommand(command, control);
}
void BrowserView::OnPaint() { CPaintDC dc(this); CRect area; GetClientRect(area); if (frame->initialized) Render(dc, area.Width(), area.Height()); }
static int scroll_position(int current, UINT code, UINT position, int maximum, int page)
{
    if (code == SB_TOP) current = 0;
    else if (code == SB_BOTTOM) current = maximum;
    else if (code == SB_LINEUP) current -= 24;
    else if (code == SB_LINEDOWN) current += 24;
    else if (code == SB_PAGEUP) current -= page;
    else if (code == SB_PAGEDOWN) current += page;
    else if (code == SB_THUMBTRACK || code == SB_THUMBPOSITION) current = position;
    return (std::max)(0, (std::min)(current, (std::max)(0, maximum - page)));
}
void BrowserView::OnVScroll(UINT code, UINT position, CScrollBar *) { CRect area; GetClientRect(area); scroll_y = scroll_position(scroll_y, code, position, document_height, area.Height()); SetScrollPos(SB_VERT, scroll_y); WFE_FormPositionAll(&frame->context); Invalidate(); }
void BrowserView::OnHScroll(UINT code, UINT position, CScrollBar *) { CRect area; GetClientRect(area); scroll_x = scroll_position(scroll_x, code, position, document_width, area.Width()); SetScrollPos(SB_HORZ, scroll_x); WFE_FormPositionAll(&frame->context); Invalidate(); }
BOOL BrowserView::OnMouseWheel(UINT, short delta, CPoint) { OnVScroll(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0, nullptr); return TRUE; }
void BrowserView::OnLButtonUp(UINT, CPoint point)
{
    SetFocus();
    LO_Element *element = LO_XYToElement(&frame->context, point.x + scroll_x, point.y + scroll_y);
    if (element && element->type == LO_TEXT && element->lo_text.anchor_href) {
        char *address = (char *)element->lo_text.anchor_href->anchor;
        if (address) WFE_NavigateTarget(&frame->context, address, (const char *)element->lo_text.anchor_href->target);
    } else if (element && element->type == LO_IMAGE) {
        LO_ImageStruct *image = &element->lo_image;
        int x = point.x + scroll_x - image->x - image->x_offset - image->border_width;
        int y = point.y + scroll_y - image->y - image->y_offset - image->border_width;
        if (image->image_attr && (image->image_attr->attrmask & LO_ATTR_ISFORM)) {
            WFE_FormImageSubmit(&frame->context, image, x, y);
            return;
        }
        LO_AnchorData *anchor = image->anchor_href;
        if (image->image_attr && image->image_attr->usemap_name)
            anchor = LO_MapXYToAreaAnchor(&frame->context, image, x, y);
        if (anchor && anchor->anchor) {
            std::string target((const char *)anchor->anchor);
            if (image->image_attr && (image->image_attr->attrmask & LO_ATTR_ISMAP)) {
                char position[48]; sprintf(position, "?%d,%d", x, y); target += position;
            }
            WFE_NavigateTarget(&frame->context, target.c_str(), (const char *)anchor->target);
        }
    }
}

static HFONT text_font(LO_TextAttr *attribute)
{
    static const int heights[] = {10,10,12,16,18,24,32,48};
    int size = attribute ? (std::max)(0, (std::min)(7, (int)attribute->size)) : 3;
    int mask = attribute ? attribute->fontmask : 0;
    int attributes = attribute ? attribute->attrmask : 0;
    const char *face = attribute && attribute->font_face ? attribute->font_face : (mask & LO_FONT_FIXED ? "Consolas" : "Times New Roman");
    return CreateFontA(-heights[size], 0, 0, 0, mask & LO_FONT_BOLD ? FW_BOLD : FW_NORMAL,
                      !!(mask & LO_FONT_ITALIC), !!(attributes & LO_ATTR_UNDERLINE),
                      !!(attributes & LO_ATTR_STRIKEOUT), ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
}

extern "C" HWND WFE_DocumentWindow(MWContext *context)
{
    BrowserFrame *frame = frame_for(context);
    return frame ? frame->view.GetSafeHwnd() : nullptr;
}
extern "C" void FE_RaiseWindow(MWContext *context)
{
    while (context && context->grid_parent) context = context->grid_parent;
    auto found = native_windows.find(context);
    if (!smoke_mode && found != native_windows.end()) {
        ShowWindow(found->second, SW_RESTORE);
        SetForegroundWindow(found->second);
    }
}
extern "C" void WFE_BeginLoad(MWContext *context, const char *url)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || frame->mode || !url) return;
    frame->current_url = url;
    if (frame->address.GetSafeHwnd()) frame->address.SetWindowText(url);
    if (frame->resizing) return;
    if (!frame->history.empty()) frame->history.resize(frame->history_position + 1);
    frame->history.push_back(url);
    frame->history_position = frame->history.size() - 1;
    frame->completed = false;
    frame->view.drawn_runs = 0; frame->view.drawn_text.clear();
}
extern "C" MWContext *WFE_URLTarget(MWContext *context, URL_Struct *url)
{
    if (!url || !url->window_target || !*url->window_target) return context;
    std::string name(url->window_target);
    MWContext *target = XP_FindNamedContextInList(context, (char *)name.c_str());
    if (!target) {
        target = WFE_NewWindow(nullptr);
        if (target && name[0] != '_') { free(target->name); target->name = _strdup(name.c_str()); }
    }
    free(url->window_target); url->window_target = _strdup("");
    return target;
}
extern "C" void WFE_NavigateTarget(MWContext *context, const char *address, const char *target)
{
    URL_Struct *url = NET_CreateURLStruct(address, NET_DONT_RELOAD);
    if (!url) return;
    if (target && *target) url->window_target = _strdup(target);
    FE_GetURL(context, url);
}
extern "C" MWContext *WFE_NewGridWindow(MWContext *parent, void *history,
    int32 x, int32 y, int32 width, int32 height, char *address, char *name,
    int8 scrolling, NET_ReloadMethod reload, Bool no_edge)
{
    BrowserFrame *owner = frame_for(parent);
    if (!owner) return nullptr;
    BrowserFrame *frame = new BrowserFrame;
    frame->grid_parent = parent; frame->grid_name = name ? name : ""; frame->grid_scrolling = scrolling;
    DWORD style = WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | (no_edge ? 0 : WS_BORDER);
    if (!frame->Create(nullptr, "Netscape frame", style,
        CRect(x,y,x+(std::max)(width,1L),y+(std::max)(height,1L)), &owner->view, nullptr)) return nullptr;
    frame->view.MoveWindow(0, 0, (std::max)(width,1L), (std::max)(height,1L));
    if (history) SHIST_AddDocument(&frame->context, (History_entry *)history);
    URL_Struct *url = history ? SHIST_CreateURLStructFromHistoryEntry(&frame->context, (History_entry *)history)
                             : NET_CreateURLStruct(address ? address : "about:blank", NET_DONT_RELOAD);
    if (url) {
        url->force_reload = reload;
        memset(&url->savedData, 0, sizeof(url->savedData));
        FE_GetURL(&frame->context, url);
    }
    return &frame->context;
}
extern "C" void *WFE_FreeGridWindow(MWContext *context, XP_Bool save)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !context->is_grid_cell) return nullptr;
    XP_List *history = nullptr;
    if (save) {
        LO_DiscardDocument(context);
        history = context->hist.list_ptr;
        context->hist.list_ptr = nullptr;
        context->hist.cur_doc_ptr = nullptr;
    }
    frame->DestroyWindow();
    return history;
}
extern "C" void WFE_ResizeGridWindow(MWContext *context, int32 x, int32 y, int32 width, int32 height)
{
    BrowserFrame *frame = frame_for(context);
    if (frame && context->is_grid_cell) frame->MoveWindow(x, y, (std::max)(width, 1L), (std::max)(height, 1L));
}
extern "C" void WFE_LoadGridHistory(MWContext *context, void *history, NET_ReloadMethod reload)
{
    if (!history) return;
    URL_Struct *url = SHIST_CreateURLStructFromHistoryEntry(context, (History_entry *)history);
    if (url) { url->force_reload = reload; FE_GetURL(context, url); }
}
extern "C" HDC WFE_DrawingContext(MWContext *context)
{
    BrowserFrame *frame = frame_for(context);
    return frame && frame->view.drawing ? frame->view.drawing->GetSafeHdc() : nullptr;
}
extern "C" void WFE_SetBackdrop(MWContext *context, LO_ImageStruct *image)
{
    BrowserFrame *frame = frame_for(context);
    if (frame) frame->view.backdrop = image;
}
extern "C" void WFE_ClearBackdrop(MWContext *context, LO_ImageStruct *image)
{
    BrowserFrame *frame = frame_for(context);
    if (frame && frame->view.backdrop == image) frame->view.backdrop = nullptr;
}
extern "C" void WFE_RecordImage(MWContext *context, LO_ImageStruct *image)
{
    BrowserFrame *frame = frame_for(context);
    if (frame && std::find(frame->view.drawn_images.begin(), frame->view.drawn_images.end(), image) == frame->view.drawn_images.end())
        frame->view.drawn_images.push_back(image);
}

extern "C" void WFE_TextMetrics(MWContext *context, LO_TextStruct *text, LO_TextInfo *info)
{
    BrowserFrame *frame = frame_for(context);
    CClientDC dc(frame ? &frame->view : nullptr);
    HFONT font = text_font(text->text_attr);
    HGDIOBJ previous = SelectObject(dc, font);
    TEXTMETRICA metrics;
    SIZE size = {0,0};
    GetTextMetricsA(dc, &metrics);
    if (text->text && text->text_len > 0) GetTextExtentPoint32A(dc, (char *)text->text, text->text_len, &size);
    info->max_width = size.cx;
    info->ascent = (int16)metrics.tmAscent;
    info->descent = (int16)metrics.tmDescent;
    info->lbearing = 0;
    info->rbearing = (int16)size.cx;
    SelectObject(dc, previous);
    DeleteObject(font);
}
extern "C" void WFE_Text(MWContext *context, LO_TextStruct *text, int32 start, int32 end, int background)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !frame->view.drawing || !text->text) return;
    start = (std::max)(0L, start);
    end = (std::min)((long)text->text_len, end);
    if (end <= start) return;
    CDC &dc = *frame->view.drawing;
    HFONT font = text_font(text->text_attr);
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE offset = {0,0};
    if (start) GetTextExtentPoint32A(dc, (char *)text->text, start, &offset);
    LO_TextAttr *attribute = text->text_attr;
    if (attribute) {
        SetTextColor(dc, RGB(attribute->fg.red, attribute->fg.green, attribute->fg.blue));
        SetBkColor(dc, RGB(attribute->bg.red, attribute->bg.green, attribute->bg.blue));
    }
    SetBkMode(dc, background ? OPAQUE : TRANSPARENT);
    TextOutA(dc, text->x + text->x_offset + offset.cx, text->y + text->y_offset,
             (char *)text->text + start, end - start);
    SelectObject(dc, previous);
    DeleteObject(font);
    if (smoke_mode) {
        frame->view.drawn_text.append((char *)text->text + start, end - start);
        frame->view.drawn_text += ' ';
        ++frame->view.drawn_runs;
        if (text->anchor_href) {
            frame->view.link_text.assign((char *)text->text + start, end - start);
            frame->view.link_point = CPoint(text->x + text->x_offset + offset.cx + 1,
                                           text->y + text->y_offset + 3);
            frame->view.links[frame->view.link_text] = frame->view.link_point;
        }
    }
}
extern "C" void WFE_Rect(MWContext *context, int32 x, int32 y, int32 width, int32 height, unsigned long color, int fill)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !frame->view.drawing) return;
    CRect area(x,y,x+width,y+height);
    if (fill) frame->view.drawing->FillSolidRect(area, color);
    else { CBrush brush(color); frame->view.drawing->FrameRect(area, &brush); }
}
extern "C" void WFE_Status(MWContext *context, const char *message) { BrowserFrame *frame = frame_for(context); while (frame && frame->grid_parent) frame = frame_for(frame->grid_parent); if (frame && frame->status.GetSafeHwnd()) frame->status.SetPaneText(0, message ? message : ""); else WFE_ComposerStatus(context, message); }
extern "C" void WFE_Title(MWContext *context, const char *title) { BrowserFrame *frame = frame_for(context); if (frame) { std::string caption = std::string(title ? title : "Untitled") + " - Netscape 3.02"; frame->SetWindowText(caption.c_str()); } }
extern "C" void WFE_Invalidate(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame && frame->view.GetSafeHwnd()) frame->view.Invalidate(); }
extern "C" void WFE_WindowSize(MWContext *context, int32 *width, int32 *height) { BrowserFrame *frame = frame_for(context); CRect area(0,0,800,600); if (frame) frame->view.GetClientRect(area); *width = area.Width(); *height = area.Height(); }
extern "C" void WFE_DocumentSize(MWContext *context, int32 width, int32 height)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame) return;
    BrowserView &view = frame->view;
    view.document_width = width; view.document_height = height;
    if (context->is_grid_cell && frame->grid_scrolling == LO_SCROLL_NO) {
        view.ShowScrollBar(SB_BOTH, FALSE);
        return;
    }
    CRect area; view.GetClientRect(area);
    SCROLLINFO vertical = {sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS, 0, (int)(std::max)(0L,height-1), (UINT)area.Height(), view.scroll_y, 0};
    SCROLLINFO horizontal = {sizeof(SCROLLINFO), SIF_RANGE | SIF_PAGE | SIF_POS, 0, (int)(std::max)(0L,width-1), (UINT)area.Width(), view.scroll_x, 0};
    if (context->is_grid_cell && frame->grid_scrolling == LO_SCROLL_YES) {
        vertical.fMask |= SIF_DISABLENOSCROLL; horizontal.fMask |= SIF_DISABLENOSCROLL;
    }
    view.SetScrollInfo(SB_VERT, &vertical); view.SetScrollInfo(SB_HORZ, &horizontal);
}
extern "C" void WFE_Scroll(MWContext *context, int32 x, int32 y) { BrowserFrame *frame = frame_for(context); if (frame) { frame->view.scroll_x = x; frame->view.scroll_y = y; WFE_FormPositionAll(context); WFE_Invalidate(context); } }
extern "C" void WFE_Position(MWContext *context, int32 *x, int32 *y) { BrowserFrame *frame = frame_for(context); *x = frame ? frame->view.scroll_x : 0; *y = frame ? frame->view.scroll_y : 0; }
extern "C" void WFE_Background(MWContext *context, unsigned long color) { BrowserFrame *frame = frame_for(context); if (frame) frame->view.background = color; }
extern "C" void WFE_Unsupported(MWContext *context, const char *feature) { std::string message = std::string(feature) + " is not available in this Windows port yet."; OutputDebugStringA(message.c_str()); WFE_Status(context, message.c_str()); }
extern "C" int WFE_Message(MWContext *context, const char *message, int confirm) { if (smoke_mode) { smoke_error = message ? message : ""; return 0; } BrowserFrame *frame = frame_for(context); return MessageBoxA(frame ? frame->GetSafeHwnd() : nullptr, message ? message : "", "Netscape", confirm ? MB_YESNO | MB_ICONQUESTION : MB_OK | MB_ICONINFORMATION) == IDYES; }
extern "C" void WFE_Complete(MWContext *context, int status) { BrowserFrame *frame = frame_for(context); if (frame) { frame->completed = true; frame->resizing = false; frame->load_status = status; WFE_Invalidate(context); } }
extern "C" MWContext *WFE_NewWindow(const char *address)
{
    BrowserFrame *frame = new BrowserFrame;
    if (!frame->Create(nullptr, "Netscape 3.02", WS_OVERLAPPEDWINDOW, CRect(80,80,1100,800), nullptr, MAKEINTRESOURCE(IDR_MAIN))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAIN), TRUE);
    frame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAIN));
    browser_windows.push_back(frame);
    WFE_RegisterNativeWindow(&frame->context, frame->GetSafeHwnd());
    if (!AfxGetApp()->m_pMainWnd) AfxGetApp()->m_pMainWnd = frame;
    if (!smoke_mode) frame->ShowWindow(SW_SHOW);
    if (address) frame->Navigate(address);
    return &frame->context;
}
extern "C" void WFE_CloseWindow(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame) frame->SendMessage(WM_CLOSE); else WFE_ComposerClose(context); }
extern "C" MWContext *WFE_NewMailWindow(int is_news, const char *server)
{
    if (!WFE_MailURLDirectory()) { WFE_Message(nullptr, "Could not prepare the profile's Mail directory.", 0); return nullptr; }
    int mode = is_news ? 2 : 1;
    for (BrowserFrame *existing : browser_windows) {
        if (existing->mode == mode) { if (!smoke_mode) existing->SetForegroundWindow(); return &existing->context; }
    }
    if (is_news && server && *server) { WFE_SetMailPreference("news", server); WFE_ApplyMailPreferences(); }
    BrowserFrame *frame = new BrowserFrame;
    frame->mode = mode;
    if (!frame->Create(nullptr, is_news ? "Netscape News" : "Netscape Mail", WS_OVERLAPPEDWINDOW,
                       CRect(70,70,1200,850), nullptr, MAKEINTRESOURCE(IDR_MAIN))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAIN), TRUE);
    frame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAIN));
    browser_windows.push_back(frame);
    WFE_RegisterNativeWindow(&frame->context, frame->GetSafeHwnd());
    frame->InitializeMessaging(server);
    if (!smoke_mode) frame->ShowWindow(SW_SHOW);
    return &frame->context;
}

struct NativeTimer { TimeoutCallbackFunction callback; void *closure; ULONGLONG due; };
static std::map<UINT_PTR, NativeTimer> timers;
static void CALLBACK timer_callback(HWND, UINT, UINT_PTR identifier, DWORD)
{
    auto found = timers.find(identifier);
    if (found == timers.end() || GetTickCount64() < found->second.due) return;
    NativeTimer timer = found->second;
    KillTimer(nullptr, identifier);
    timers.erase(found);
    timer.callback(timer.closure);
}
extern "C" void *WFE_SetTimer(TimeoutCallbackFunction callback, void *closure, uint32 milliseconds) { UINT_PTR identifier = SetTimer(nullptr, 0, (std::max)(1UL, milliseconds), timer_callback); if (!identifier) return nullptr; timers[identifier] = {callback, closure, GetTickCount64() + milliseconds}; return reinterpret_cast<void *>(identifier); }
extern "C" void WFE_KillTimer(void *timer) { UINT_PTR identifier = reinterpret_cast<UINT_PTR>(timer); KillTimer(nullptr, identifier); timers.erase(identifier); }

extern "C" int WFE_RunSmokeTest(const char *report, const char *url)
{
    std::string path = std::string(WFE_ProfileDirectory()) + "\\frontend-smoke.html";
    FILE *file = fopen(path.c_str(), "wb");
    if (!file) return 2;
    fputs("<html><head><title>Native frontend smoke</title></head><body><h1>Windows core renderer</h1><p>Hello <b>Netscape</b> 3.02.</p><ul><li>Native list</li></ul><img src='placeholder.png'><a href='about:'>About this port</a></body></html>", file);
    fclose(file);
    MWContext *context = WFE_NewWindow(nullptr);
    if (!context) return 3;
    BrowserFrame *frame = frame_for(context);
    frame->Navigate(url ? url : path.c_str());
    ULONGLONG deadline = GetTickCount64() + 10000;
    while (!frame->completed && GetTickCount64() < deadline) {
        MSG message;
        while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessage(&message); }
        WFE_Pump();
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
    }
    CClientDC screen(&frame->view);
    CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1024, 768);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1024, 768);
    memory.SelectObject(previous);
    CString caption; frame->GetWindowText(caption);
    bool passed = frame->completed && frame->load_status >= 0 && frame->view.drawn_runs > 0
        && frame->view.drawn_text.find("Windows core renderer") != std::string::npos
        && frame->view.drawn_text.find("Netscape") != std::string::npos
        && caption.Find("Native frontend smoke") >= 0;
    const bool completed = frame->completed;
    const int load_status = frame->load_status;
    const unsigned drawn_runs = frame->view.drawn_runs;
    const std::string drawn_text = frame->view.drawn_text;
    BrowserFrame *survivor = frame_for(WFE_NewWindow(nullptr));
    frame->SendMessage(WM_CLOSE);
    bool survived = survivor && IsWindow(survivor->GetSafeHwnd())
        && AfxGetApp()->m_pMainWnd == survivor;
    MSG quit;
    if (PeekMessage(&quit, nullptr, WM_QUIT, WM_QUIT, PM_NOREMOVE)) survived = false;
    passed = passed && survived;
    if (survivor) survivor->SendMessage(WM_CLOSE);
    file = fopen(report, "wb");
    if (file) {
        fprintf(file, "passed=%d\ncompleted=%d\nstatus=%d\ntext_runs=%u\nwindow_survived=%d\ntitle=%s\ntext=%s\nerror=%s\n",
                passed, completed, load_status, drawn_runs, survived,
                (const char *)caption, drawn_text.c_str(), smoke_error.c_str());
        fclose(file);
    } else passed = false;
    DeleteFileA(path.c_str());
    return passed ? 0 : 4;
}

static void mail_smoke_pump()
{
    MSG message;
    for (int count = 0; count < 64 && PeekMessage(&message, nullptr, 0, 0, PM_REMOVE); ++count) {
        if (message.message == WM_QUIT) continue;
        TranslateMessage(&message); DispatchMessage(&message);
    }
    WFE_Pump();
    MsgWaitForMultipleObjects(0, nullptr, FALSE, 10, QS_ALLINPUT);
}

static void browser_smoke_wait(BrowserFrame *frame)
{
    ULONGLONG deadline = GetTickCount64() + 12000, quiet = 0;
    while (GetTickCount64() < deadline) {
        mail_smoke_pump();
        if (frame->completed && !XP_IsContextBusy(&frame->context)) {
            if (!quiet) quiet = GetTickCount64();
            if (GetTickCount64() - quiet >= 200) break;
        } else quiet = 0;
    }
}
static std::string frame_text(BrowserFrame *frame)
{
    if (!frame) return "";
    CClientDC screen(&frame->view); CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1800, 1200);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.drawn_text.clear(); frame->view.link_text.clear(); frame->view.links.clear();
    CRect area; frame->view.GetClientRect(area);
    frame->view.Render(memory, area.Width(), area.Height());
    memory.SelectObject(previous);
    return frame->view.drawn_text;
}
static BrowserFrame *named_frame(MWContext *root, const char *name)
{
    return frame_for(XP_FindNamedContextInList(root, (char *)name));
}
static bool click_frame_link(BrowserFrame *frame, const char *label)
{
    frame_text(frame);
    if (!frame) return false;
    for (const auto &link : frame->view.links) if (link.first.find(label) != std::string::npos) {
        CPoint point = link.second;
        frame->view.SendMessage(WM_LBUTTONUP, 0, MAKELPARAM(point.x, point.y));
        return true;
    }
    return false;
}
static int WFE_RunFramesSmoke(const char *report, const char *url)
{
    BrowserFrame *root = frame_for(WFE_NewWindow(nullptr));
    if (!root) return 2;
    root->Navigate(url); browser_smoke_wait(root);
    BrowserFrame *navigation = named_frame(&root->context, "navigation");
    BrowserFrame *content = named_frame(&root->context, "content");
    int count = XP_ListCount(root->context.grid_children);
    bool initial = count == 2 && frame_text(navigation).find("Navigation frame") != std::string::npos
        && frame_text(content).find("Initial content frame") != std::string::npos
        && content->view.drawn_images.size() == 1;
    bool form = false, header_target = false, targeted = false, resized = false, nested = false, parent_target = false, top_target = false;
    if (initial) {
        HWND query = WFE_FormFindControl(&content->context, "query", 0);
        HWND submit = WFE_FormFindControl(&content->context, "go", 0);
        if (query && submit) {
            SetWindowTextA(query, "frame & value"); SendMessage(submit, BM_CLICK, 0, 0);
            browser_smoke_wait(root);
            form = frame_text(content).find("Frame form submitted") != std::string::npos;
        }
        bool header_clicked = click_frame_link(navigation, "Header target"); browser_smoke_wait(root);
        std::string header = frame_text(content);
        header_target = header_clicked && header.find("Header transfer finished") != std::string::npos
            && frame_text(navigation).find("Navigation frame") != std::string::npos;
        bool clicked = click_frame_link(navigation, "Open target"); browser_smoke_wait(root);
        targeted = clicked && content == named_frame(&root->context, "content")
            && frame_text(content).find("Targeted content frame") != std::string::npos
            && frame_text(navigation).find("Navigation frame") != std::string::npos;
    }
    if (targeted) {
        CRect before; navigation->GetWindowRect(before);
        root->completed = false; root->MoveWindow(60,60,950,690);
        browser_smoke_wait(root);
        navigation = named_frame(&root->context, "navigation"); content = named_frame(&root->context, "content");
        CRect after; if (navigation) navigation->GetWindowRect(after);
        resized = navigation && content && before.Width() != after.Width()
            && frame_text(content).find("Targeted content frame") != std::string::npos;
        if (!resized) fprintf(stderr, "Frame resize: before=%d after=%d navigation=%p content=%p body=%s\n", before.Width(), after.Width(), navigation, content, frame_text(content).c_str());
    }
    if (resized) {
        bool clicked = click_frame_link(content, "Open nested"); browser_smoke_wait(root);
        nested = clicked && frame_text(named_frame(&root->context, "nestedTop")).find("Nested top frame") != std::string::npos
            && frame_text(named_frame(&root->context, "nestedBottom")).find("Nested bottom frame") != std::string::npos;
    }
    if (nested) {
        bool clicked = click_frame_link(named_frame(&root->context, "nestedTop"), "Parent target");
        browser_smoke_wait(root);
        content = named_frame(&root->context, "content");
        parent_target = clicked && content && frame_text(content).find("Parent target frame") != std::string::npos
            && named_frame(&root->context, "navigation") && XP_ListCount(content->context.grid_children) == 0;
    }
    if (parent_target) {
        bool clicked = click_frame_link(content, "Top target"); browser_smoke_wait(root);
        top_target = clicked && XP_ListCount(root->context.grid_children) == 0
            && frame_text(root).find("Top target document") != std::string::npos
            && frame_text(root).find("Top transfer finished") != std::string::npos;
    }
    root->Navigate("about:"); browser_smoke_wait(root);
    bool cleanup = XP_ListCount(root->context.grid_children) == 0 && browser_contexts.size() == 1;
    root->SendMessage(WM_CLOSE);
    cleanup = cleanup && browser_contexts.empty();
    FILE *output = fopen(report, "wb");
    if (!output) return 3;
    fprintf(output, "child_count=%d\ninitial_render=%d\nform=%d\nheader_target=%d\ntargeted=%d\nresized=%d\nnested=%d\nparent_target=%d\ntop_target=%d\ncleanup=%d\nerror=%s\n", count, initial, form, header_target, targeted, resized, nested, parent_target, top_target, cleanup, smoke_error.c_str());
    fclose(output);
    return initial && form && header_target && targeted && resized && nested && parent_target && top_target && cleanup ? 0 : 4;
}
static bool image_pixel(CDC &dc, LO_ImageStruct *image, int column, int row, COLORREF expected, int tolerance)
{
    int x = image->x + image->x_offset + image->border_width + ((column ? 3 : 1) * image->width) / 4;
    int y = image->y + image->y_offset + image->border_width + ((row ? 3 : 1) * image->height) / 4;
    COLORREF actual = dc.GetPixel(x, y);
    return abs((int)GetRValue(actual) - GetRValue(expected)) <= tolerance
        && abs((int)GetGValue(actual) - GetGValue(expected)) <= tolerance
        && abs((int)GetBValue(actual) - GetBValue(expected)) <= tolerance;
}
static int WFE_RunImageSmoke(const char *report, const char *url)
{
    BrowserFrame *frame = frame_for(WFE_NewWindow(nullptr));
    if (!frame) return 2;
    frame->Navigate(url);
    browser_smoke_wait(frame);
    CClientDC screen(&frame->view); CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1600, 1000);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1600, 1000);
    CString caption; frame->GetWindowText(caption); caption.Replace(" - Netscape 3.02", "");
    bool pixels = true, dimensions = false;
    int kinds = 0;
    const COLORREF quadrants[] = {RGB(255,0,0), RGB(0,255,0), RGB(0,0,255), RGB(255,255,0)};
    const COLORREF transparent[] = {RGB(0,255,255), RGB(255,0,0), RGB(0,0,255), RGB(0,255,255)};
    const COLORREF checker[] = {RGB(0,255,255), RGB(0,0,0), RGB(0,0,0), RGB(0,255,255)};
    for (LO_ImageStruct *image : frame->view.drawn_images) {
        const char *name = (const char *)image->image_url;
        const COLORREF *expected = nullptr;
        int tolerance = 0;
        if (name && strstr(name, "/quadrants.gif")) { expected = quadrants; kinds |= 1; if (image->width == 32 && image->height == 32) dimensions = true; }
        else if (name && strstr(name, "/quadrants.jpg")) { expected = quadrants; kinds |= 2; tolerance = 12; }
        else if (name && strstr(name, "/checker.xbm")) { expected = checker; kinds |= 4; }
        else if (name && strstr(name, "/transparent.gif")) { expected = transparent; kinds |= 8; }
        if (expected) for (int i = 0; i < 4; ++i) {
            bool match = image_pixel(memory, image, i & 1, i >> 1, expected[i], tolerance);
            if (!match) fprintf(stderr, "Image pixel mismatch: %s %ldx%ld quadrant %d\n", name, image->width, image->height, i);
            pixels = match && pixels;
        }
    }
    pixels = pixels && kinds == 15 && dimensions;
    int count = (int)frame->view.drawn_images.size();
    bool finished = frame->completed && frame->load_status >= 0 && frame->view.drawn_text.find("Images finished") != std::string::npos;
    memory.SelectObject(previous);
    /* Exercise shared cache reuse and closing a client while another survives. */
    frame->Navigate(url, false); browser_smoke_wait(frame);
    BrowserFrame *shared = frame_for(WFE_NewWindow(url));
    bool shared_survived = shared != nullptr;
    if (shared) { browser_smoke_wait(shared); shared->SendMessage(WM_CLOSE); }
    memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1600, 1000);
    shared_survived = shared_survived && frame->view.drawn_images.size() >= 5;
    for (LO_ImageStruct *image : frame->view.drawn_images)
        if (image->image_url && strstr((const char *)image->image_url, "/quadrants.gif"))
            shared_survived = image_pixel(memory, image, 0, 0, RGB(255,0,0), 0) && shared_survived;
    memory.SelectObject(previous);
    /* A deferred relayout must be canceled before its window/context is freed. */
    BrowserFrame *closing = frame_for(WFE_NewWindow(nullptr));
    size_t timer_count = timers.size();
    LO_SetImageInfo(&closing->context, 0, 2, 2);
    bool pending_cancelled = timers.size() == timer_count + 1;
    closing->SendMessage(WM_CLOSE);
    pending_cancelled = pending_cancelled && timers.size() == timer_count;
    mail_smoke_pump();
    frame->Navigate("about:", false); browser_smoke_wait(frame);
    frame->SendMessage(WM_CLOSE);
    FILE *output = fopen(report, "wb");
    if (!output) return 3;
    fprintf(output, "title=%s\nimage_count=%d\nimage_pixels_ok=%d\nnoerrors=%d\nshared_survived=%d\npending_cancelled=%d\n", (const char *)caption, count, pixels, finished, shared_survived, pending_cancelled);
    fclose(output);
    return pixels && finished && shared_survived && pending_cancelled ? 0 : 4;
}
static std::string control_text(HWND control)
{
    if (!control) return "";
    std::vector<char> text(GetWindowTextLengthA(control) + 1);
    GetWindowTextA(control, text.data(), (int)text.size());
    return text.data();
}
static bool fill_form(MWContext *context)
{
    HWND query = WFE_FormFindControl(context, "query", 0);
    HWND secret = WFE_FormFindControl(context, "secret", 0);
    HWND note = WFE_FormFindControl(context, "note", 0);
    HWND enabled = WFE_FormFindControl(context, "enabled", 0);
    HWND choice = WFE_FormFindControl(context, "choice", 1);
    HWND pick = WFE_FormFindControl(context, "pick", 0);
    HWND multi = WFE_FormFindControl(context, "multi", 0);
    if (!query || !secret || !note || !enabled || !choice || !pick || !multi) return false;
    SetWindowTextA(query, "native & value"); SetWindowTextA(secret, "password");
    SetWindowTextA(note, "line one\r\nline two");
    if (SendMessage(enabled, BM_GETCHECK, 0, 0) == BST_CHECKED) SendMessage(enabled, BM_CLICK, 0, 0);
    SendMessage(choice, BM_CLICK, 0, 0);
    SendMessage(pick, CB_SETCURSEL, 1, 0);
    SendMessage(multi, LB_SETSEL, FALSE, -1);
    SendMessage(multi, LB_SETSEL, TRUE, 0); SendMessage(multi, LB_SETSEL, TRUE, 2);
    return true;
}
static int WFE_RunFormsSmoke(const char *report, const char *url)
{
    BrowserFrame *frame = frame_for(WFE_NewWindow(nullptr));
    if (!frame) return 2;
    frame->Navigate(url); browser_smoke_wait(frame);
    /* Layout display creates the live native child controls even in hidden tests. */
    CClientDC screen(&frame->view); CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1600, 1000);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1600, 1000);
    MWContext *context = &frame->context;
    bool filled = fill_form(context);
    const int control_count = WFE_FormCount(context);
    HWND query = WFE_FormFindControl(context, "query", 0);
    RECT original = {}, scrolled = {};
    if (query) GetWindowRect(query, &original);
    WFE_Scroll(context, 0, 16);
    if (query) GetWindowRect(query, &scrolled);
    bool scrolling = query && scrolled.top == original.top - 16;
    WFE_Scroll(context, 0, 0);
    HWND reset = WFE_FormFindControl(context, "", 0);
    if (filled && reset) SendMessage(reset, BM_CLICK, 0, 0);
    bool reset_ok = filled && reset
        && control_text(WFE_FormFindControl(context, "query", 0)) == "default query"
        && control_text(WFE_FormFindControl(context, "secret", 0)) == "default secret"
        && control_text(WFE_FormFindControl(context, "note", 0)) == "default note"
        && SendMessage(WFE_FormFindControl(context, "enabled", 0), BM_GETCHECK, 0, 0) == BST_CHECKED
        && SendMessage(WFE_FormFindControl(context, "choice", 0), BM_GETCHECK, 0, 0) == BST_CHECKED
        && SendMessage(WFE_FormFindControl(context, "choice", 1), BM_GETCHECK, 0, 0) == BST_UNCHECKED
        && SendMessage(WFE_FormFindControl(context, "pick", 0), CB_GETCURSEL, 0, 0) == 0
        && SendMessage(WFE_FormFindControl(context, "multi", 0), LB_GETSEL, 1, 0) > 0
        && SendMessage(WFE_FormFindControl(context, "multi", 0), LB_GETSELCOUNT, 0, 0) == 1;
    bool refilled = fill_form(context);
    HWND submit = WFE_FormFindControl(context, "go", 0);
    frame->completed = false;
    bool activated = false;
    if (refilled && strstr(url, "/forms-image")) {
        for (LO_ImageStruct *image : frame->view.drawn_images) {
            if (image->image_attr && (image->image_attr->attrmask & LO_ATTR_ISFORM)) {
                int x = image->x + image->x_offset + image->border_width + 1;
                int y = image->y + image->y_offset + image->border_width + 1;
                frame->view.SendMessage(WM_LBUTTONUP, 0, MAKELPARAM(x, y));
                activated = true; break;
            }
        }
    } else if (refilled && submit) {
        if (strstr(url, "/forms-get")) {
            MSG enter = {}; enter.hwnd = query; enter.message = WM_KEYDOWN; enter.wParam = VK_RETURN;
            activated = frame->PreTranslateMessage(&enter) != FALSE;
        } else { SendMessage(submit, BM_CLICK, 0, 0); activated = true; }
    }
    if (activated) browser_smoke_wait(frame);
    frame->view.drawn_text.clear(); frame->view.Render(memory, 1600, 1000);
    bool submitted = activated && frame->completed && frame->load_status >= 0
        && frame->view.drawn_text.find("Native form received") != std::string::npos;
    bool cleaned = WFE_FormCount(context) == 0;
    memory.SelectObject(previous);
    frame->SendMessage(WM_CLOSE);
    FILE *output = fopen(report, "wb");
    if (!output) return 3;
    fprintf(output, "reset_ok=%d\nsubmitted=%d\ncontrol_count=%d\nscrolling=%d\ncleaned=%d\nerror=%s\n", reset_ok, submitted, control_count, scrolling, cleaned, smoke_error.c_str());
    fclose(output);
    return reset_ok && submitted && scrolling && cleaned && control_count >= 9 ? 0 : 4;
}
static int find_folder(MWContext *context, const char *name)
{
    MSG_FolderLine line;
    for (int i = 0; i < 100000 && MSG_GetFolderLine(context, i, &line, nullptr); ++i)
        if (line.name && strstr(line.name, name)) return i;
    return -1;
}
static int WFE_RunMailNewsSmoke(const char *report, int news, const char *server)
{
    WFE_SetMailPreference("email", "native@example.test");
    WFE_SetMailPreference("name", "Native Test");
    WFE_ApplyMailPreferences();
    MWContext *context = WFE_NewMailWindow(news, server);
    BrowserFrame *frame = frame_for(context);
    if (!frame || !context->msgframe) return 2;
    ULONGLONG deadline = GetTickCount64() + 12000;
    int folder = -1;
    while (GetTickCount64() < deadline && (folder = find_folder(context, news ? "test.native" : "Inbox")) < 0
           && !(frame->completed && frame->load_status < 0)) mail_smoke_pump();
    if (folder >= 0) {
        while (GetTickCount64() < deadline && NET_AreThereActiveConnectionsForWindow(context)) mail_smoke_pump();
        frame->SendMessage(WM_COMMAND, ID_MESSAGE_SHOW_ALL);
        frame->RefreshMailLists();
        frame->folders.SetItemState(-1, 0, LVIS_SELECTED);
        frame->folders.SetItemState(folder, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
    }
    MSG_MessageLine message;
    deadline = GetTickCount64() + 12000;
    while (folder >= 0 && GetTickCount64() < deadline && !MSG_GetThreadLine(context, 1, &message, nullptr)) mail_smoke_pump();
    frame->RefreshMailLists();
    std::string folders, subjects;
    for (int i = 0; i < frame->folders.GetItemCount(); ++i) { if (i) folders += '|'; folders += frame->folders.GetItemText(i, 0); }
    for (int i = 0; i < frame->messages.GetItemCount(); ++i) { if (i) subjects += '|'; subjects += frame->messages.GetItemText(i, 0); }
    const int folder_count = frame->folders.GetItemCount(), message_count = frame->messages.GetItemCount();
    std::string selected_subject;
    if (MSG_GetThreadLine(context, 0, &message, nullptr)) {
        selected_subject = message.subject ? message.subject : "";
        frame->completed = false; frame->view.drawn_text.clear(); frame->view.drawn_runs = 0;
        frame->messages.SetItemState(-1, 0, LVIS_SELECTED);
        frame->messages.SetItemState(0, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        deadline = GetTickCount64() + 12000;
        while (!frame->completed && GetTickCount64() < deadline) mail_smoke_pump();
    }
    CClientDC screen(&frame->view); CDC memory; memory.CreateCompatibleDC(&screen);
    CBitmap bitmap; bitmap.CreateCompatibleBitmap(&screen, 1024, 768);
    CBitmap *previous = memory.SelectObject(&bitmap);
    frame->view.Render(memory, 1024, 768);
    memory.SelectObject(previous);
    std::string reader = frame->view.drawn_text;
    std::replace(reader.begin(), reader.end(), '\n', ' ');
    std::replace(reader.begin(), reader.end(), '\r', ' ');
    bool read = frame->completed && frame->load_status >= 0 && frame->view.drawn_runs > 0;
    bool controls = frame->folders.GetSafeHwnd() && frame->messages.GetSafeHwnd() && frame->view.GetSafeHwnd();
    const int load_status = frame->load_status;
    int composer = read && WFE_ComposerSmoke(context, report, news) == 0;
    BrowserFrame *survivor = frame_for(WFE_NewWindow(nullptr));
    frame->SendMessage(WM_CLOSE);
    bool survived = survivor && IsWindow(survivor->GetSafeHwnd()) && AfxGetApp()->m_pMainWnd == survivor;
    if (survivor) survivor->SendMessage(WM_CLOSE);
    bool passed = controls && folder >= 0 && message_count >= 2 && read && composer && survived;
    FILE *output = fopen(report, "ab");
    if (!output) return 3;
    fprintf(output, "passed=%d\nmode=%s\nbackend=MSG\nfolder_count=%d\nmessage_count=%d\nfolders=%s\nsubjects=%s\nselected_subject=%s\nreader_text=%s\nstatus=%d\ncomposer=%d\nwindow_survived=%d\nerror=%s\n",
            passed, news ? "news" : "mail", folder_count, message_count, folders.c_str(), subjects.c_str(),
            selected_subject.c_str(), reader.c_str(), load_status, composer, survived, smoke_error.c_str());
    fclose(output);
    return passed ? 0 : 4;
}

static int WFE_RunComposerDeliverySmoke(const char *report, int news, const char *server)
{
    WFE_SetMailPreference("email", "native@example.test");
    WFE_SetMailPreference("name", "Native Test");
    if (!WFE_SetMailPreference(news ? "news" : "smtp", server)) return 2;
    WFE_ApplyMailPreferences();
    MWContext *source = WFE_NewMailWindow(0, nullptr);
    int status = WFE_ComposerDeliverSmoke(source, report, news);
    WFE_CloseWindow(source);
    return status;
}

class NetscapeApplication : public CWinApp {
public:
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_LISTVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        SetRegistryKey("NetscapeLegacy");
        CStringList arguments;
        if (strlen(m_lpCmdLine) >= 511) return FALSE;
        CCmdParse parser(m_lpCmdLine, arguments);
        if (!parser.ProcessCmdLine()) return FALSE;
        POSITION position = arguments.GetHeadPosition();
        CString first = position ? arguments.GetNext(position) : "about:";
        smoke_mode = first == "--smoke-test" || first == "--mailnews-smoke" || first == "--composer-delivery-smoke"
            || first == "--images-smoke" || first == "--forms-smoke" || first == "--frames-smoke";
        if (smoke_mode) {
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _DEBUG
            _CrtSetReportHook(debug_report);
            _set_invalid_parameter_handler(invalid_parameter);
#endif
        }
        if (!WFE_Init()) { m_nExitCode = 2; return FALSE; }
        backend_initialized = true;
        if (smoke_mode) {
            CString report = position ? arguments.GetNext(position) : "frontend-smoke.txt";
            CString url = position ? arguments.GetNext(position) : "";
            if (first == "--mailnews-smoke" || first == "--composer-delivery-smoke") {
                CString server = position ? arguments.GetNext(position) : "";
                m_nExitCode = first == "--mailnews-smoke" ? WFE_RunMailNewsSmoke(report, url == "news", server) : WFE_RunComposerDeliverySmoke(report, url == "news", server);
            } else if (first == "--images-smoke") m_nExitCode = WFE_RunImageSmoke(report, url);
            else if (first == "--forms-smoke") m_nExitCode = WFE_RunFormsSmoke(report, url);
            else if (first == "--frames-smoke") m_nExitCode = WFE_RunFramesSmoke(report, url);
            else m_nExitCode = WFE_RunSmokeTest(report, url.IsEmpty() ? nullptr : (const char *)url);
            WFE_Shutdown();
            backend_initialized = false;
            return FALSE;
        }
        if (first == "-mail" || first == "--mail" || first.Left(8) == "mailbox:") return WFE_NewMailWindow(0, nullptr) != nullptr;
        if (first == "-news" || first == "--news" || first.Left(5) == "news:") return WFE_NewMailWindow(1, nullptr) != nullptr;
        if (first == "--compose") { WFE_NewComposer(nullptr, 0); return WFE_ComposerWindowCount() != 0; }
        return WFE_NewWindow(first) != nullptr;
    }
    int ExitInstance() override {
        while (!browser_windows.empty()) browser_windows.back()->DestroyWindow();
        WFE_ComposerCloseAll();
        if (backend_initialized) WFE_Shutdown();
        for (const auto &timer : timers) KillTimer(nullptr, timer.first);
        timers.clear();
        int result = CWinApp::ExitInstance();
        return smoke_mode || m_nExitCode ? m_nExitCode : result;
    }
private:
    int m_nExitCode = 0;
    bool backend_initialized = false;
};
static NetscapeApplication application;
