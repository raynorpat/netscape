#include <afxwin.h>
#include <afxdlgs.h>
#include <afxcmn.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <shlwapi.h>
#include "composer.h"
#include "classic.h"
#include "chrome.h"
#include "composer_resource.h"
#include "resource.h"
#include "menu_resource.h"
#include "window-menu.h"
#include "mailsettings.h"
#include "legacy/genedit.h"
extern "C" {
#include "msgcom.h"
#include "xp_file.h"
#include "shist.h"
void WFE_RegisterNativeWindow(MWContext *, HWND);
void WFE_UnregisterNativeWindow(MWContext *);
/* Original libmsg mailbox writer, declared in lib/libmsg/msg.h. */
int msg_DoFCC(MWContext *, const char *, XP_FileType, const char *, XP_FileType,
              const char *, const char *);
}

static const int header_count = 9;
static const char *header_names[header_count] = {
    "From:", "Reply-To:", "To:", "Cc:", "Bcc:", "Fcc:",
    "Newsgroups:", "Followup-To:", "Subject:"
};
static const MSG_HEADER_SET header_masks[header_count] = {
    MSG_FROM_HEADER_MASK, MSG_REPLY_TO_HEADER_MASK, MSG_TO_HEADER_MASK,
    MSG_CC_HEADER_MASK, MSG_BCC_HEADER_MASK, MSG_FCC_HEADER_MASK,
    MSG_NEWSGROUPS_HEADER_MASK, MSG_FOLLOWUP_TO_HEADER_MASK,
    MSG_SUBJECT_HEADER_MASK
};
static const MSG_CommandType header_commands[header_count] = {
    MSG_ShowFrom, MSG_ShowReplyTo, MSG_ShowTo, MSG_ShowCC, MSG_ShowBCC,
    MSG_ShowFCC, MSG_ShowPostTo, MSG_ShowFollowupTo, MSG_ShowSubject
};

class ComposerStatusBar : public ClassicStatusBar {
public:
    void SetWindowText(const char *text) { SetPaneText(1, text ? text : ""); }
};

class ComposerFrame;
static std::map<MWContext *, ComposerFrame *> composers;
static std::map<MWContext *, bool *> delivery_observers;

static ComposerFrame *composer_for(MWContext *context)
{
    auto item = composers.find(context);
    return item == composers.end() ? nullptr : item->second;
}

/* Windows edit controls require CRLF, while quoting may supply bare LF. */
static std::string edit_text(const char *text)
{
    std::string result;
    if (text) for (const char *p = text; *p; ++p) {
        if (*p == '\n' && (p == text || p[-1] != '\r')) result += '\r';
        result += *p;
    }
    return result;
}

class ComposerFrame : public CFrameWnd {
public:
    MWContext context = {};
    CStatic labels[header_count], attachment_text;
    ComposerStatusBar status;
    ClassicLogo logo;
    CGenericEdit fields[header_count], body;
    ClassicButton send, later, draft, attach, cancel, headers, remove, quote, address, stop;
    ClassicButton mail_to, cc_to, attachment_button;
    MSG_HEADER_SET visible = 0x3ff;
    MSG_HEADER_SET displayed_headers = MSG_TO_HEADER_MASK | MSG_CC_HEADER_MASK |
        MSG_SUBJECT_HEADER_MASK | MSG_ATTACHMENTS_HEADER_MASK;
    bool compact = true, compact_news = false, headers_initialized = false;
    bool wrap = true, deferred = false;
    bool initialized = false, changing = false, dirty = false;
    bool destroying = false, backend_closed = false, delivery = false;
    bool completing = false;
    bool saving = false, draft_done = false, draft_ok = false;
    std::string draft_path;
    void Arrange();
    void SyncHeaders();
    void UpdateButtons();
    void Deliver(MSG_CommandType command);
    void SaveDraft();
    bool SaveBody(const char *path);
    void RefreshAttachments();
    bool AttachPath(const char *path);
    void SetHeader(int index, const char *text);
    CString Header(int index) { CString text; fields[index].GetWindowText(text); return text; }
    BOOL PreTranslateMessage(MSG *message) override;
    BOOL OnCmdMsg(UINT id, int code, void *extra, AFX_CMDHANDLERINFO *info) override;
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnGetMinMaxInfo(MINMAXINFO *info);
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSend() { Deliver(deferred ? MSG_SendMessageLater : MSG_SendMessage); }
    afx_msg void OnLater() { Deliver(MSG_SendMessageLater); }
    afx_msg void OnDraft() { SaveDraft(); }
    afx_msg void OnAttach();
    afx_msg void OnRemove();
    afx_msg void OnHeaders();
    afx_msg void OnQuote();
    afx_msg void OnStop();
    afx_msg void OnMailTo() { fields[2].SetFocus(); }
    afx_msg void OnCcTo() { fields[3].SetFocus(); }
    afx_msg void OnHeaderCommand(UINT id);
    afx_msg void OnEditCommand(UINT id);
    afx_msg void OnSaveAs();
    afx_msg void OnPasteQuotation();
    afx_msg void OnWrap();
    afx_msg void OnDelivery(UINT id);
    afx_msg void OnAttachments();
    afx_msg void OnSettings() { WFE_ShowMailSettings(&context); }
    afx_msg void OnInitMenuPopup(CMenu *menu, UINT index, BOOL system);
    afx_msg void OnUpdateCommand(CCmdUI *command);
    afx_msg void OnHeaderChanged(UINT id);
    afx_msg void OnBodyChanged();
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(ComposerFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_GETMINMAXINFO()
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_WM_INITMENUPOPUP()
 ON_COMMAND(IDC_COMPOSE_SEND, OnSend)
 ON_COMMAND(IDC_COMPOSE_LATER, OnLater)
 ON_COMMAND(IDC_COMPOSE_DRAFT, OnDraft)
 ON_COMMAND(IDC_COMPOSE_ATTACH, OnAttach)
 ON_COMMAND(IDC_COMPOSE_REMOVE, OnRemove)
 ON_COMMAND(IDC_COMPOSE_CANCEL, OnClose)
 ON_COMMAND(IDC_COMPOSE_HEADERS, OnHeaders)
 ON_COMMAND(IDC_COMPOSE_QUOTE, OnQuote)
 ON_COMMAND(IDC_COMPOSE_STOP, OnStop)
 ON_COMMAND(IDC_COMPOSE_MAILTO, OnMailTo)
 ON_COMMAND(IDC_COMPOSE_CC, OnCcTo)
 ON_COMMAND(IDC_COMPOSE_ATTACHMENT_BUTTON, OnAttach)
 ON_COMMAND(ID_COMPOSE_SAVE_AS, OnSaveAs)
 ON_COMMAND(ID_COMPOSE_PASTE_QUOTATION, OnPasteQuotation)
 ON_COMMAND(ID_COMPOSE_WRAP, OnWrap)
 ON_COMMAND(ID_COMPOSE_ATTACHMENTS, OnAttachments)
 ON_COMMAND_RANGE(ID_COMPOSE_IMMEDIATE, ID_COMPOSE_DEFERRED, OnDelivery)
 ON_COMMAND(ID_MAIL_SETTINGS, OnSettings)
 ON_UPDATE_COMMAND_UI(ID_MAIL_SETTINGS, OnUpdateCommand)
 ON_UPDATE_COMMAND_UI_RANGE(ID_COMPOSE_SAVE_AS, ID_COMPOSE_ATTACHMENTS, OnUpdateCommand)
 ON_COMMAND_RANGE(IDC_COMPOSE_HEADER_FIRST, IDC_COMPOSE_HEADER_LAST, OnHeaderCommand)
 ON_COMMAND_RANGE(IDC_COMPOSE_UNDO, IDC_COMPOSE_SELECT_ALL, OnEditCommand)
 ON_UPDATE_COMMAND_UI_RANGE(IDC_COMPOSE_SEND, IDC_COMPOSE_HEADER_LAST, OnUpdateCommand)
 ON_CONTROL_RANGE(EN_CHANGE, IDC_COMPOSE_FIRST, IDC_COMPOSE_FIRST + header_count - 1, OnHeaderChanged)
 ON_CONTROL(EN_CHANGE, IDC_COMPOSE_BODY, OnBodyChanged)
END_MESSAGE_MAP()

int ComposerFrame::OnCreate(LPCREATESTRUCT create)
{
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    CFont *font = ClassicUI::Font(this);
    ClassicButton *buttons[] = {&send, &quote, &attach, &address, &stop,
        &later, &draft, &cancel, &headers, &remove, &mail_to, &cc_to, &attachment_button};
    const char *names[] = {"Send", "Quote", "Attach", "Address", "Stop",
        "Send Later", "Save Draft", "Cancel", "All Headers", "Remove All", "Mail To:", "Cc:", "Attachment:"};
    UINT ids[] = {IDC_COMPOSE_SEND, IDC_COMPOSE_QUOTE, IDC_COMPOSE_ATTACH,
        IDC_COMPOSE_ADDRESS, IDC_COMPOSE_STOP, IDC_COMPOSE_LATER, IDC_COMPOSE_DRAFT,
        IDC_COMPOSE_CANCEL, IDC_COMPOSE_HEADERS, IDC_COMPOSE_REMOVE,
        IDC_COMPOSE_MAILTO, IDC_COMPOSE_CC, IDC_COMPOSE_ATTACHMENT_BUTTON};
    for (int i = 0; i < _countof(buttons); ++i) {
        DWORD style = WS_CHILD | WS_TABSTOP | BS_OWNERDRAW;
        if (i < 5 || i >= 10) style |= WS_VISIBLE;
        if (!buttons[i]->Create(names[i], style, CRect(0,0,0,0), this, ids[i])) return -1;
        buttons[i]->SetFont(font);
    }
    send.SetBitmapResource(IDB_CLASSIC_SEND);  /* Original message toolbar artwork. */
    attach.SetBitmapResource(IDB_CLASSIC_ATTACH);
    quote.SetBitmapResource(IDB_CLASSIC_QUOTE);
    address.SetBitmapResource(IDB_CLASSIC_ADDRESS);
    stop.SetBitmapResource(IDB_CLASSIC_STOP);
    if (!logo.Create("Netscape", WS_CHILD | WS_VISIBLE, CRect(0,0,0,0), this, IDC_COMPOSE_LOGO)) return -1;
    logo.bitmap.LoadBitmap(IDB_CLASSIC_LOGO);
    for (int i = 0; i < header_count; ++i) {
        if (!labels[i].Create(header_names[i], WS_CHILD | WS_VISIBLE,
                               CRect(0,0,0,0), this)) return -1;
        if (!fields[i].Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                               CRect(0,0,0,0), this, IDC_COMPOSE_FIRST + i)) return -1;
        labels[i].SetFont(font);
        fields[i].SetFont(font);
        fields[i].LimitText(32767);
    }
    if (!attachment_text.Create("", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS | SS_SUNKEN,
                                 CRect(0,0,0,0), this, IDC_COMPOSE_ATTACHMENT) ||
        !body.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL |
                     ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                     CRect(0,0,0,0), this, IDC_COMPOSE_BODY)) return -1;
    UINT indicators[] = {ID_STATUS_NAVIGATOR, ID_SEPARATOR, ID_STATUS_PROGRESS, ID_STATUS_MAIL};
    if (!status.Create(this) || !status.SetIndicators(indicators, _countof(indicators))) return -1;
    status.SetDlgCtrlID(IDC_COMPOSE_STATUS);
    status.SetPaneInfo(0, ID_STATUS_NAVIGATOR, SBPS_NORMAL, ClassicUI::Scale(this, 20));
    status.SetPaneInfo(1, ID_SEPARATOR, SBPS_STRETCH, 0);
    status.SetPaneInfo(2, ID_STATUS_PROGRESS, SBPS_NORMAL, ClassicUI::Scale(this, 80));
    status.SetPaneInfo(3, ID_STATUS_MAIL, SBPS_NORMAL, ClassicUI::Scale(this, 22));
    status.SetWindowText("Ready");
    attachment_text.SetFont(font);
    body.LimitText(0x7ffffffe);
    status.SetFont(font);
    ClassicUI::Apply(this);
    body.SetFont(CFont::FromHandle((HFONT)GetStockObject(ANSI_FIXED_FONT)));
    WFE_InitContext(&context, this);
    context.type = MWContextMessageComposition;
    initialized = true;
    composers[&context] = this;
    WFE_RegisterNativeWindow(&context, GetSafeHwnd());
    SetTimer(1, 20, nullptr);
    return 0;
}

void ComposerFrame::Arrange()
{
    if (!body.GetSafeHwnd()) return;
    CRect area; GetClientRect(&area);
    auto s = [this](int value) { return ClassicUI::Scale(this, value); };
    CButton *buttons[] = {&send, &quote, &attach, &address, &stop};
    int x = s(4), y = s(3), button_width = s(44);
    for (int i = 0; i < _countof(buttons); ++i) {
        buttons[i]->MoveWindow(x, y, button_width - s(2), s(38));
        x += button_width;
    }
    logo.MoveWindow((std::max)(x + s(4), area.Width() - s(43)), s(2), s(40), s(40));
    y = s(49);
    /* The backend's header mask and every edit field are retained even when
       the compact front end hides advanced headers. */
    int order[] = {2, 3, 8, 0, 1, 4, 5, 6, 7};
    for (int row = 0; row < header_count; ++row) {
        int i = order[row];
        bool show = (displayed_headers & header_masks[i]) != 0;
        bool button_label = i == 2 || i == 3;
        labels[i].ShowWindow(show && !button_label ? SW_SHOW : SW_HIDE);
        fields[i].ShowWindow(show ? SW_SHOW : SW_HIDE);
        if (i == 2) mail_to.ShowWindow(show ? SW_SHOW : SW_HIDE);
        if (i == 3) cc_to.ShowWindow(show ? SW_SHOW : SW_HIDE);
        if (show) {
            labels[i].MoveWindow(s(8), y + s(3), s(76), s(18));
            if (i == 2) mail_to.MoveWindow(s(5), y, s(78), s(22));
            if (i == 3) cc_to.MoveWindow(s(5), y, s(78), s(22));
            fields[i].MoveWindow(s(88), y, (std::max)(area.Width() - s(94), s(20)), s(22));
            y += s(25);
        }
    }
    bool attachments = (displayed_headers & MSG_ATTACHMENTS_HEADER_MASK) != 0;
    attachment_button.ShowWindow(attachments ? SW_SHOW : SW_HIDE);
    attachment_text.ShowWindow(attachments ? SW_SHOW : SW_HIDE);
    if (attachments) {
        attachment_button.MoveWindow(s(5), y, s(78), s(22));
        attachment_text.MoveWindow(s(88), y, (std::max)(area.Width() - s(94), s(20)), s(22));
        y += s(28);
    }
    int status_top = (std::max)(0, area.Height() - s(21));
    body.MoveWindow(s(5), y, (std::max)(area.Width() - s(10), s(20)),
                    (std::max)(status_top - y - s(3), s(20)));
    status.MoveWindow(0, status_top, area.Width(), s(21));
    Invalidate(FALSE);
}

BOOL ComposerFrame::OnEraseBkgnd(CDC *dc)
{
    CRect area; GetClientRect(&area);
    dc->FillSolidRect(area, RGB(192,192,192));
    int y = ClassicUI::Scale(this, 45);
    dc->FillSolidRect(0, y, area.Width(), 1, RGB(128,128,128));
    dc->FillSolidRect(0, y + 1, area.Width(), 1, RGB(255,255,255));
    return TRUE;
}

HBRUSH ComposerFrame::OnCtlColor(CDC *dc, CWnd *window, UINT type)
{
    return ClassicUI::ControlColor(dc, window, type);
}

void ComposerFrame::OnSize(UINT type, int width, int height)
{
    CFrameWnd::OnSize(type, width, height);
    Arrange();
}

void ComposerFrame::OnGetMinMaxInfo(MINMAXINFO *info)
{
    CFrameWnd::OnGetMinMaxInfo(info);
    info->ptMinTrackSize.x = ClassicUI::Scale(this, 360);
    info->ptMinTrackSize.y = ClassicUI::Scale(this, 440);
}

BOOL ComposerFrame::PreTranslateMessage(MSG *message)
{
    if (message->message == WM_KEYDOWN && message->wParam == VK_TAB &&
        !(GetKeyState(VK_CONTROL) & 0x8000)) {
        HWND next = ::GetNextDlgTabItem(GetSafeHwnd(), message->hwnd,
                                        (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        if (next) { ::SetFocus(next); return TRUE; }
    }
    return CFrameWnd::PreTranslateMessage(message);
}

BOOL ComposerFrame::OnCmdMsg(UINT id, int code, void *extra, AFX_CMDHANDLERINFO *info)
{
    if (code == CN_COMMAND && !info && WFE_WindowCommand(this, id)) return TRUE;
    if (code == CN_UPDATE_COMMAND_UI && extra &&
        WFE_WindowUpdateCommand(this, (CCmdUI *)extra)) return TRUE;
    return CFrameWnd::OnCmdMsg(id, code, extra, info);
}

void ComposerFrame::OnInitMenuPopup(CMenu *menu, UINT index, BOOL system)
{
    if (!system && WFE_IsWindowMenu(menu)) WFE_PopulateWindowMenu(menu, this);
    CFrameWnd::OnInitMenuPopup(menu, index, system);
}

void ComposerFrame::SetHeader(int index, const char *text)
{
    bool previous = changing; changing = true;
    fields[index].SetWindowText(text ? text : "");
    if (context.msg_cframe) MSG_SetHeaderContents(&context, header_masks[index], text ? text : "");
    changing = previous;
}

void ComposerFrame::SyncHeaders()
{
    if (!context.msg_cframe) return;
    for (int i = 0; i < header_count; ++i) {
        CString value = Header(i);
        char *expanded = MSG_UpdateHeaderContents(&context, header_masks[i], value);
        SetHeader(i, expanded ? expanded : (const char *)value);
        if (expanded) XP_FREE(expanded);
    }
}

void ComposerFrame::OnHeaderChanged(UINT id)
{
    if (changing || !context.msg_cframe) return;
    int index = id - IDC_COMPOSE_FIRST;
    CString text = Header(index);
    MSG_SetHeaderContents(&context, header_masks[index], text);
    dirty = true;
}

void ComposerFrame::OnBodyChanged()
{
    if (changing || !context.msg_cframe) return;
    MSG_MessageBodyEdited(&context);
    dirty = true;
}

static bool command_enabled(MWContext *context, MSG_CommandType command)
{
    XP_Bool enabled = FALSE, plural = FALSE;
    MSG_COMMAND_CHECK_STATE checked = MSG_NotUsed;
    const char *name = nullptr;
    return context->msg_cframe && MSG_CommandStatus(context, command, &enabled,
                                                    &checked, &name, &plural) >= 0 && enabled;
}

void ComposerFrame::UpdateButtons()
{
    if (destroying) return;
    bool ready = !saving && !backend_closed && context.msg_cframe;
    bool available = ready && command_enabled(&context, MSG_SendMessage);
    send.EnableWindow(ready && command_enabled(&context, deferred ? MSG_SendMessageLater : MSG_SendMessage));
    later.EnableWindow(ready && command_enabled(&context, MSG_SendMessageLater));
    draft.EnableWindow(available);
    attach.EnableWindow(available);
    attachment_button.EnableWindow(available);
    mail_to.EnableWindow(available);
    cc_to.EnableWindow(available);
    quote.EnableWindow(ready && !MSG_QuoteInProgress(&context) && command_enabled(&context, MSG_QuoteMessage));
    /* This native front end has no address picker. */
    address.EnableWindow(FALSE);
    stop.EnableWindow(!saving && !backend_closed && (XP_IsContextStoppable(&context) || MSG_QuoteInProgress(&context)));
    status.busy = XP_IsContextBusy(&context) || MSG_QuoteInProgress(&context);
    status.Invalidate(FALSE);
    remove.EnableWindow(available && MSG_GetAttachmentList(&context) != nullptr);
    headers.EnableWindow(ready);
    for (int i = 0; i < header_count; ++i) fields[i].EnableWindow(available);
    body.EnableWindow(available);
}

void ComposerFrame::OnHeaders()
{
    if (!context.msg_cframe || saving) return;
    compact = !compact;
    displayed_headers = compact ? MSG_TO_HEADER_MASK | MSG_CC_HEADER_MASK |
        MSG_SUBJECT_HEADER_MASK | MSG_ATTACHMENTS_HEADER_MASK |
        (compact_news ? MSG_NEWSGROUPS_HEADER_MASK : 0) : 0x3ff;
    if (!compact) {
        XP_Bool enabled = FALSE, plural = FALSE;
        MSG_COMMAND_CHECK_STATE checked = MSG_NotUsed;
        const char *name = nullptr;
        MSG_CommandStatus(&context, MSG_ShowAllHeaders, &enabled, &checked, &name, &plural);
        if (enabled && checked != MSG_Checked) MSG_Command(&context, MSG_ShowAllHeaders);
    }
    Arrange();
}

void ComposerFrame::OnHeaderCommand(UINT id)
{
    if (!context.msg_cframe || saving) return;
    int index = id - IDC_COMPOSE_HEADER_FIRST;
    bool shown = (displayed_headers & header_masks[index]) != 0;
    bool backend_shown = (visible & header_masks[index]) != 0;
    compact = false;
    displayed_headers ^= header_masks[index];
    /* A hidden compact row can already be set in the backend. Only toggle
       the backend if its state differs from the requested individual row. */
    if (shown == backend_shown) MSG_Command(&context, header_commands[index]);
    Arrange();
}

void ComposerFrame::OnAttachments()
{
    if (!context.msg_cframe || saving) return;
    displayed_headers ^= MSG_ATTACHMENTS_HEADER_MASK;
    if ((displayed_headers & MSG_ATTACHMENTS_HEADER_MASK) != (visible & MSG_ATTACHMENTS_HEADER_MASK))
        MSG_Command(&context, MSG_ShowAttachments);
    Arrange();
}

void ComposerFrame::OnQuote()
{
    if (saving || backend_closed || MSG_QuoteInProgress(&context) || !command_enabled(&context, MSG_QuoteMessage)) return;
    MWContext *target = &context;
    MSG_Command(target, MSG_QuoteMessage);
    ComposerFrame *frame = composer_for(target);
    if (frame && !frame->backend_closed && target->msg_cframe) frame->dirty = true;
}

void ComposerFrame::OnStop()
{
    if (saving || backend_closed || (!XP_IsContextStoppable(&context) && !MSG_QuoteInProgress(&context))) return;
    /* Interrupt callbacks can run synchronously. They must not treat this
       cancelled delivery as a completed message and close its editor. */
    delivery = false;
    MWContext *target = &context;
    MSG_InterruptQuoting(target);
    ComposerFrame *frame = composer_for(target);
    if (frame && !frame->backend_closed && target->msg_cframe && XP_IsContextStoppable(target))
        XP_InterruptContext(target);
    frame = composer_for(target);
    if (frame && !frame->backend_closed) {
        frame->status.SetWindowText("Stopped");
        frame->UpdateButtons();
    }
}

void ComposerFrame::OnEditCommand(UINT id)
{
    CWnd *focus = GetFocus();
    if (!focus || !focus->IsWindowEnabled() || focus->GetParent() != this ||
        !focus->IsKindOf(RUNTIME_CLASS(CEdit))) return;
    if (id != IDC_COMPOSE_COPY && id != IDC_COMPOSE_SELECT_ALL &&
        (saving || backend_closed || !command_enabled(&context, MSG_SendMessage))) return;
    UINT messages[] = {EM_UNDO, WM_CUT, WM_COPY, WM_PASTE, EM_SETSEL};
    focus->SendMessage(messages[id - IDC_COMPOSE_UNDO], 0, id == IDC_COMPOSE_SELECT_ALL ? -1 : 0);
}

void ComposerFrame::OnPasteQuotation()
{
    if (saving || backend_closed || !command_enabled(&context, MSG_SendMessage) ||
        !IsClipboardFormatAvailable(CF_TEXT) || !::OpenClipboard(GetSafeHwnd())) return;
    HANDLE data = GetClipboardData(CF_TEXT);
    const char *text = data ? (const char *)GlobalLock(data) : nullptr;
    std::string quoted;
    if (text) {
        bool line_start = true;
        for (const char *p = text; *p; ++p) {
            if (line_start) quoted += "> ";
            quoted += *p;
            line_start = *p == '\n';
        }
        GlobalUnlock(data);
    }
    CloseClipboard();
    if (!quoted.empty()) {
        body.SetFocus();
        body.ReplaceSel(edit_text(quoted.c_str()).c_str(), TRUE);
    }
}

void ComposerFrame::OnWrap()
{
    if (saving || backend_closed || !command_enabled(&context, MSG_SendMessage)) return;
    /* Windows EDIT determines horizontal wrapping when the HWND is created.
       Recreate only that control, preserving its text, font and selection. */
    CString text; body.GetWindowText(text);
    int start = 0, end = 0; body.GetSel(start, end);
    int first_line = body.GetFirstVisibleLine();
    bool focused = GetFocus() == &body;
    HFONT font = (HFONT)body.SendMessage(WM_GETFONT);
    DWORD style = body.GetStyle();
    CRect rect; body.GetWindowRect(&rect); ScreenToClient(&rect);
    bool new_wrap = !wrap;
    if (new_wrap) style &= ~(ES_AUTOHSCROLL | WS_HSCROLL);
    else style |= ES_AUTOHSCROLL | WS_HSCROLL;
    HWND replacement = ::CreateWindowEx(body.GetExStyle(), "EDIT", "", style & ~WS_VISIBLE,
        rect.left, rect.top, rect.Width(), rect.Height(), GetSafeHwnd(),
        nullptr, AfxGetInstanceHandle(), nullptr);
    if (!replacement) return;
    bool was_changing = changing; changing = true;
    body.DestroyWindow();
    body.SubclassWindow(replacement);
    body.SetDlgCtrlID(IDC_COMPOSE_BODY);
    wrap = new_wrap;
    body.LimitText(0x7ffffffe);
    body.SendMessage(WM_SETFONT, (WPARAM)font, TRUE);
    body.SetWindowText(text); body.SetSel(start, end);
    body.LineScroll(first_line - body.GetFirstVisibleLine());
    changing = was_changing;
    body.ShowWindow(SW_SHOW);
    if (focused) body.SetFocus();
    Arrange();
}

void ComposerFrame::OnDelivery(UINT id)
{
    if (saving || backend_closed || !context.msg_cframe) return;
    deferred = id == ID_COMPOSE_DEFERRED;
    send.SetWindowText(deferred ? "Send Later" : "Send");
    UpdateButtons();
}

bool ComposerFrame::SaveBody(const char *path)
{
    CString text; body.GetWindowText(text);
    CFile file;
    CFileException error;
    if (!file.Open(path, CFile::modeCreate | CFile::modeWrite | CFile::typeBinary, &error)) {
        status.SetWindowText("Could not save the message text.");
        return false;
    }
    try {
        file.Write((const char *)text, text.GetLength());
        file.Close();
    } catch (CFileException *failure) {
        failure->Delete(); file.Abort();
        status.SetWindowText("Could not save the message text.");
        return false;
    }
    /* A body export does not save recipients or attachments. Keep the draft
       dirty so closing still protects all unsaved composition information. */
    status.SetWindowText("Message text saved.");
    return true;
}

void ComposerFrame::OnSaveAs()
{
    if (saving || backend_closed || !command_enabled(&context, MSG_SendMessage)) return;
    CFileDialog dialog(FALSE, "txt", "message.txt", OFN_OVERWRITEPROMPT | OFN_HIDEREADONLY,
                       "Text files (*.txt)|*.txt|All files (*.*)|*.*||", this);
    dialog.m_ofn.lpstrTitle = "Save Message As";
    if (dialog.DoModal() == IDOK) SaveBody(dialog.GetPathName());
}

void ComposerFrame::OnUpdateCommand(CCmdUI *command)
{
    UINT id = command->m_nID;
    bool ready = !saving && !backend_closed && context.msg_cframe;
    bool enabled = ready && command_enabled(&context, MSG_SendMessage);
    if (id == IDC_COMPOSE_CANCEL) enabled = !saving;
    else if (id == IDC_COMPOSE_SEND) {
        enabled = ready && command_enabled(&context, deferred ? MSG_SendMessageLater : MSG_SendMessage);
        command->SetText(deferred ? "Send &Later\tCtrl+Enter" : "&Send Now\tCtrl+Enter");
    }
    else if (id == ID_MAIL_SETTINGS) enabled = ready;
    else if (id == ID_COMPOSE_PASTE_QUOTATION) enabled = enabled && IsClipboardFormatAvailable(CF_TEXT);
    else if (id == ID_COMPOSE_WRAP) command->SetCheck(wrap);
    else if (id == ID_COMPOSE_IMMEDIATE || id == ID_COMPOSE_DEFERRED) {
        enabled = ready;
        command->SetRadio(id == ID_COMPOSE_DEFERRED ? deferred : !deferred);
    } else if (id == ID_COMPOSE_ATTACHMENTS) {
        enabled = ready;
        command->SetCheck((displayed_headers & MSG_ATTACHMENTS_HEADER_MASK) != 0);
    }
    else if (id == IDC_COMPOSE_ADDRESS) enabled = false;
    else if (id == IDC_COMPOSE_STOP) enabled = !saving && !backend_closed && (XP_IsContextStoppable(&context) || MSG_QuoteInProgress(&context));
    else if (id == IDC_COMPOSE_QUOTE) enabled = ready && !MSG_QuoteInProgress(&context) && command_enabled(&context, MSG_QuoteMessage);
    else if (id == IDC_COMPOSE_LATER) enabled = ready && command_enabled(&context, MSG_SendMessageLater);
    else if (id == IDC_COMPOSE_REMOVE) enabled = enabled && MSG_GetAttachmentList(&context) != nullptr;
    else if (id == IDC_COMPOSE_HEADERS) { enabled = ready; command->SetCheck(displayed_headers == 0x3ff); }
    else if (id >= IDC_COMPOSE_HEADER_FIRST && id <= IDC_COMPOSE_HEADER_LAST) {
        int index = id - IDC_COMPOSE_HEADER_FIRST;
        enabled = ready;
        command->SetCheck((displayed_headers & header_masks[index]) != 0);
    } else if (id >= IDC_COMPOSE_UNDO && id <= IDC_COMPOSE_SELECT_ALL) {
        CWnd *focus = GetFocus();
        enabled = enabled && focus && focus->GetParent() == this && focus->IsKindOf(RUNTIME_CLASS(CEdit));
        if (enabled && id == IDC_COMPOSE_UNDO) enabled = focus->SendMessage(EM_CANUNDO) != 0;
        if (enabled && id == IDC_COMPOSE_PASTE) enabled = IsClipboardFormatAvailable(CF_TEXT) != FALSE;
        if (enabled && (id == IDC_COMPOSE_CUT || id == IDC_COMPOSE_COPY)) {
            DWORD start = 0, end = 0;
            focus->SendMessage(EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            enabled = start != end;
        }
    }
    command->Enable(enabled);
}

void ComposerFrame::OnTimer(UINT_PTR timer)
{
    if (timer != 1) { CFrameWnd::OnTimer(timer); return; }
    UpdateButtons();
    WFE_Pump();
}

void ComposerFrame::Deliver(MSG_CommandType command)
{
    if (saving || !context.msg_cframe || !command_enabled(&context, command)) return;
    SyncHeaders();
    status.SetWindowText(command == MSG_SendMessage ? "Sending message..." : "Queuing message...");
    delivery = true;
    MWContext *target = &context;
    MSG_Command(target, command);
    /* A cancelled preflight returns without starting delivery. Do not let
       later attachment completion close this unsent composition. Callbacks
       may already have destroyed the backend or its native window. */
    ComposerFrame *frame = composer_for(target);
    if (frame && !frame->backend_closed && target->msg_cframe &&
        command_enabled(target, command)) {
        frame->delivery = false;
        frame->status.SetWindowText("Ready");
    }
}

void ComposerFrame::RefreshAttachments()
{
    char *text = MSG_GetAttachmentString(&context);
    attachment_text.SetWindowText(text && *text ? text : "");
    if (text) XP_FREE(text);
}

void ComposerFrame::OnAttach()
{
    if (saving || !command_enabled(&context, MSG_SendMessage)) return;
    CFileDialog dialog(TRUE, nullptr, nullptr, OFN_FILEMUSTEXIST | OFN_HIDEREADONLY,
                        "All files|*.*||", this);
    if (dialog.DoModal() != IDOK) return;
    AttachPath(dialog.GetPathName());
}

bool ComposerFrame::AttachPath(const char *path)
{
    char url[4096];
    DWORD size = sizeof(url);
    if (FAILED(UrlCreateFromPathA(path, url, &size, 0))) {
        status.SetWindowText("Could not convert attachment path to a file URL.");
        return false;
    }
    std::vector<MSG_AttachmentData> list;
    const MSG_AttachmentData *old = MSG_GetAttachmentList(&context);
    size_t count = 0;
    if (old) while (old[count].url) ++count;
    /* MSG_SetAttachmentList frees its old strings before copying the new
       list, so every field in retained entries needs independent storage. */
    std::vector<std::string> strings;
    strings.reserve(count * 8);
    for (size_t i = 0; i < count; ++i) {
        MSG_AttachmentData copy = old[i];
        const char **values[] = {&copy.url, &copy.desired_type, &copy.real_type,
            &copy.real_encoding, &copy.real_name, &copy.description,
            &copy.x_mac_type, &copy.x_mac_creator};
        for (int field = 0; field < 8; ++field) if (*values[field]) {
            strings.push_back(*values[field]);
            *values[field] = strings.back().c_str();
        }
        list.push_back(copy);
    }
    MSG_AttachmentData added = {};
    added.url = url;
    list.push_back(added);
    list.push_back(MSG_AttachmentData{});
    MSG_SetAttachmentList(&context, list.data());
    RefreshAttachments();
    dirty = true;
    return true;
}

void ComposerFrame::OnRemove()
{
    if (saving || !command_enabled(&context, MSG_SendMessage)) return;
    MSG_SetAttachmentList(&context, nullptr);
    RefreshAttachments();
    dirty = true;
}

static void draft_written(MWContext *context, void *, int result, const char *file)
{
    ComposerFrame *frame = composer_for(context);
    bool copied = false;
    if (result >= 0 && file && frame) {
        /* The generator owns the encoding. The frontend only copies its file. */
        CString bcc = frame->Header(4), fcc = frame->Header(5);
        copied = msg_DoFCC(context, file, xpFileToPost, "Drafts", xpMailFolder,
                           bcc, fcc) >= 0;
    }
    if (result >= 0 && file) {
        XP_FileRemove(file, xpFileToPost);
        XP_FREE((void *)file);
    }
    if (!frame) return;
    frame->saving = false;
    frame->draft_done = true;
    frame->draft_ok = copied;
    if (copied) {
        frame->dirty = false;
        frame->status.SetWindowText(("Draft saved: " + frame->draft_path).c_str());
    } else frame->status.SetWindowText(result < 0 && file ? file : "Could not save the draft file.");
    frame->UpdateButtons();
}

void ComposerFrame::SaveDraft()
{
    if (saving || !command_enabled(&context, MSG_SendMessage)) return;
    SyncHeaders();
    const char *directory = WFE_MailDirectory();
    if (!directory) {
        status.SetWindowText("Could not prepare the profile mail directory.");
        return;
    }
    draft_path = std::string(directory) + "\\Drafts";
    CString value[header_count];
    for (int i = 0; i < header_count; ++i) value[i] = Header(i);
    char *text = nullptr; uint32 length = 0; MSG_FontCode *fonts = nullptr;
    if (FE_GetMessageBody(&context, &text, &length, &fonts) < 0) return;
    saving = true; draft_done = false; draft_ok = false;
    status.SetWindowText("Saving draft...");
    UpdateButtons();
    /* This core predates a Save Draft command. dont_deliver creates a MIME
       file only. msg_DoFCC writes it as an ordinary message in Drafts and
       retains the private Bcc using the original mailbox writer. */
    MSG_StartMessageDelivery(&context, nullptr, value[0], value[1], value[2],
        value[3], value[4], value[5], value[6], value[7], FE_UsersOrganization(),
        nullptr, nullptr, value[8], nullptr, nullptr, FALSE, TRUE, FALSE,
        FALSE, FALSE, TEXT_PLAIN, text, length, MSG_GetAttachmentList(&context), draft_written);
    FE_DoneWithMessageBody(&context, text, length);
}

void ComposerFrame::OnClose()
{
    if (destroying) return;
    if (!backend_closed && saving) {
        status.SetWindowText("Please wait for the draft to finish saving.");
        return;
    }
    if (!backend_closed && (dirty || delivery) &&
        MessageBox(delivery ? "Cancel delivery and close this message?" : "Discard changes to this message?",
                    "Netscape", MB_OKCANCEL | MB_ICONQUESTION) != IDOK) return;
    DestroyWindow();
}

void ComposerFrame::OnDestroy()
{
    destroying = true;
    KillTimer(1);
    if (initialized) {
        /* Cleanup calls FE_DestroyMailCompositionContext back with a cleared
           msg_cframe. destroying prevents a second window destruction. */
        WFE_DestroyContext(&context);
        initialized = false;
    }
    composers.erase(&context);
    WFE_UnregisterNativeWindow(&context);
    CFrameWnd::OnDestroy();
}

extern "C" MWContext *FE_MakeMessageCompositionContext(MWContext *)
{
    ComposerFrame *frame = new ComposerFrame;
    /* The reference 3.02 window is 534 by 953 at 96 DPI. Keep its tall
       composition area when space permits, and fit the current work area. */
    CRect bounds(90,70,624,1023);
    if (!frame->Create(nullptr, "Netscape - [Message Composition]", WS_OVERLAPPEDWINDOW,
                        bounds, nullptr, MAKEINTRESOURCE(IDR_COMPOSER_MENU))) return nullptr;
    MONITORINFO monitor = {}; monitor.cbSize = sizeof(monitor);
    CRect work;
    if (GetMonitorInfo(MonitorFromWindow(frame->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &monitor))
        work = monitor.rcWork;
    else SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
    int width = (std::min)(ClassicUI::Scale(frame, 534), work.Width());
    int height = (std::min)(ClassicUI::Scale(frame, 953), work.Height());
    int left = (std::max)(work.left, (std::min)(bounds.left, work.right - width));
    int top = (std::max)(work.top, (std::min)(bounds.top, work.bottom - height));
    frame->SetWindowPos(nullptr, left, top, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
    frame->LoadAccelTable(MAKEINTRESOURCE(IDR_COMPOSER_ACCEL));
    return &frame->context;
}

extern "C" void FE_InitializeMailCompositionContext(MWContext *context,
    const char *from, const char *reply_to, const char *to, const char *cc,
    const char *bcc, const char *fcc, const char *newsgroups,
    const char *followup_to, const char *subject, const char *)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame) return;
    const char *values[] = {from, reply_to, to, cc, bcc, fcc, newsgroups, followup_to, subject};
    for (int i = 0; i < header_count; ++i) frame->SetHeader(i, values[i]);
    frame->RefreshAttachments();
    frame->dirty = false;
}

extern "C" void FE_InsertMessageCompositionText(MWContext *context, const char *text, XP_Bool leave_cursor)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame || !text) return;
    int start, end;
    frame->body.GetSel(start, end);
    frame->changing = true;
    frame->body.ReplaceSel(edit_text(text).c_str(), TRUE);
    if (leave_cursor) frame->body.SetSel(start, start);
    frame->changing = false;
}

extern "C" void FE_RaiseMailCompositionWindow(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame) return;
    frame->Arrange();
    frame->UpdateButtons();
    frame->ShowWindow(SW_SHOWNORMAL);
    frame->SetForegroundWindow();
    if (frame->visible & MSG_NEWSGROUPS_HEADER_MASK) frame->fields[6].SetFocus();
    else frame->fields[2].SetFocus();
}

extern "C" void FE_DestroyMailCompositionContext(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame || frame->destroying) return;
    /* Only successful MSG_MailCompositionAllConnectionsComplete destroys
       its composition. A user close or transport failure is not success. */
    auto observer = delivery_observers.find(context);
    if (frame->completing && observer != delivery_observers.end()) *observer->second = true;
    frame->backend_closed = true;
    frame->PostMessage(WM_CLOSE);
}

extern "C" void FE_MsgShowHeaders(MWContext *context, MSG_HEADER_SET mask)
{
    ComposerFrame *frame = composer_for(context);
    if (frame) {
        if (!frame->headers_initialized) {
            frame->compact_news = (mask & MSG_NEWSGROUPS_HEADER_MASK) != 0;
            if (frame->compact_news) frame->displayed_headers |= MSG_NEWSGROUPS_HEADER_MASK;
            frame->headers_initialized = true;
        }
        frame->visible = mask;
        frame->Arrange();
    }
}

extern "C" int FE_GetMessageBody(MWContext *context, char **body, uint32 *length, MSG_FontCode **fonts)
{
    if (body) *body = nullptr;
    if (length) *length = 0;
    if (fonts) *fonts = nullptr;
    ComposerFrame *frame = composer_for(context);
    if (!frame || !body || !length) return -1;
    CString text; frame->body.GetWindowText(text);
    *body = XP_STRDUP((const char *)text);
    if (!*body) return -1;
    *length = text.GetLength();
    return 0;
}

extern "C" void FE_DoneWithMessageBody(MWContext *, char *body, uint32)
{
    if (body) XP_FREE(body);
}

extern "C" char *FE_PromptMessageSubject(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame) return nullptr;
    char *subject = FE_Prompt(context, "Please enter a subject for this message:", "(no subject)");
    if (subject) frame->SetHeader(8, subject);
    return subject;
}

extern "C" void WFE_NewComposer(MWContext *source, int is_news)
{
    MWContext *context = MSG_ComposeMessage(source, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, is_news ? "" : nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, FALSE, FALSE);
    if (!context) WFE_Status(source, "Could not create message. Set a valid mail identity in Mail Settings.");
}

extern "C" void WFE_ComposerStatus(MWContext *context, const char *message)
{
    ComposerFrame *frame = composer_for(context);
    if (frame && !frame->destroying) frame->status.SetWindowText(message ? message : "");
}

extern "C" void WFE_ComposerComplete(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame || frame->destroying || !frame->delivery || !context->msg_cframe) return;
    frame->delivery = false;
    frame->completing = true;
    MSG_MailCompositionAllConnectionsComplete(context);
    /* Completion may clear msg_cframe and ask the frontend to close. */
    frame = composer_for(context);
    if (frame) frame->completing = false;
}

extern "C" int WFE_ComposerClose(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame) return 0;
    FE_DestroyMailCompositionContext(context);
    return 1;
}

extern "C" void WFE_ComposerCloseAll(void)
{
    while (!composers.empty()) {
        ComposerFrame *frame = composers.begin()->second;
        frame->backend_closed = true;
        frame->DestroyWindow();
    }
}

extern "C" int WFE_ComposerWindowCount(void) { return (int)composers.size(); }

static void smoke_pump()
{
    WFE_Pump();
    MSG message;
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        if (message.message == WM_QUIT) { PostQuitMessage((int)message.wParam); break; }
        if (!AfxGetApp()->PreTranslateMessage(&message)) {
            TranslateMessage(&message); DispatchMessage(&message);
        }
    }
    Sleep(1);
}

static ULONGLONG file_size(const std::string &path)
{
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) return 0;
    return ((ULONGLONG)data.nFileSizeHigh << 32) | data.nFileSizeLow;
}

static bool smoke_wait_ready(MWContext *context)
{
    DWORD started = GetTickCount();
    while (composer_for(context) && !command_enabled(context, MSG_SendMessage) &&
           GetTickCount() - started < 5000) smoke_pump();
    return composer_for(context) && command_enabled(context, MSG_SendMessage);
}

static bool smoke_save_draft(MWContext *context)
{
    ComposerFrame *frame = composer_for(context);
    if (!frame) return false;
    frame->SaveDraft();
    DWORD started = GetTickCount();
    while (composer_for(context) && !frame->draft_done && GetTickCount() - started < 10000) smoke_pump();
    frame = composer_for(context);
    return frame && frame->draft_done && frame->draft_ok;
}

extern "C" int WFE_ComposerSmoke(MWContext *source, const char *report, int is_news)
{
    MWContext *context = MSG_ComposeMessage(source, nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, is_news ? "netscape.test" : nullptr,
        nullptr, nullptr, "Native composer smoke", nullptr, nullptr,
        nullptr, nullptr, nullptr, FALSE, FALSE);
    ComposerFrame *frame = composer_for(context);
    if (!frame) return 1;
    MSG_Command(context, MSG_ShowAllHeaders);
    frame->SetHeader(1, "reply@example.test");
    frame->SetHeader(2, is_news ? "" : "recipient@example.test");
    frame->SetHeader(3, "copy@example.test");
    frame->SetHeader(4, "blind@example.test");
    frame->SetHeader(5, "");
    if (is_news) frame->SetHeader(7, "netscape.test");
    /* The untouched body causes libmsg's empty-message confirmation. The
       smoke frontend declines it, so no transport may be started. */
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_SEND);
    frame = composer_for(context);
    bool preflight_cancelled = frame && !frame->delivery && context->msg_cframe;
    if (!frame) return 1;
    frame->body.SetWindowText("Native composer body\r\nSecond line.\r\n");
    MSG_MessageBodyEdited(context);
    char *body = nullptr; uint32 length = 0; MSG_FontCode *fonts = nullptr;
    bool roundtrip = FE_GetMessageBody(context, &body, &length, &fonts) == 0 &&
        body && std::string(body, length) == "Native composer body\r\nSecond line.\r\n";
    FE_DoneWithMessageBody(context, body, length);
    frame->SetHeader(2, "");
    frame->SetHeader(6, "");
    bool unfinished = smoke_save_draft(context);
    frame->SetHeader(2, is_news ? "" : "recipient@example.test");
    frame->SetHeader(6, is_news ? "netscape.test" : "");
    frame->SetHeader(4, "");
    bool cleared = frame->Header(4).IsEmpty();
    frame->SetHeader(4, "blind@example.test");
    std::string attachment_path = std::string(WFE_ProfileDirectory()) +
        (is_news ? "\\composer-news-attachment.txt" : "\\composer-mail-attachment.txt");
    FILE *attachment = fopen(attachment_path.c_str(), "wb");
    bool attachment_ok = false, attachment_cleared = false;
    if (attachment) {
        fputs("Native composer attachment\r\n", attachment);
        fclose(attachment);
        attachment_ok = frame->AttachPath(attachment_path.c_str()) && smoke_wait_ready(context);
        if (attachment_ok) {
            const MSG_AttachmentData *list = MSG_GetAttachmentList(context);
            attachment_ok = list && list[0].url && !list[1].url;
            frame->SendMessage(WM_COMMAND, IDC_COMPOSE_REMOVE);
            attachment_cleared = MSG_GetAttachmentList(context) == nullptr && smoke_wait_ready(context);
            attachment_ok = attachment_ok && attachment_cleared &&
                frame->AttachPath(attachment_path.c_str()) && smoke_wait_ready(context);
        }
    }
    bool survived_attachment = composer_for(context) && !composer_for(context)->delivery;
    bool saved = smoke_save_draft(context);
    frame = composer_for(context);
    bool survived_draft = frame && !frame->delivery;
    std::string path = saved ? frame->draft_path : "";
    bool encoded = false;
    if (saved) {
        FILE *file = fopen(path.c_str(), "rb");
        if (file) {
            std::string contents; char block[2048]; size_t count;
            while ((count = fread(block, 1, sizeof(block), file)) != 0) contents.append(block, count);
            fclose(file);
            encoded = contents.find("Subject: Native composer smoke") != std::string::npos &&
                contents.find("reply@example.test") != std::string::npos &&
                contents.find("blind@example.test") != std::string::npos &&
                contents.find("Native composer body") != std::string::npos &&
                contents.find("multipart/mixed") != std::string::npos &&
                contents.find("Native composer attachment") != std::string::npos;
        }
    }
    const char *mail_directory = WFE_MailDirectory();
    std::string outbox = mail_directory ? std::string(mail_directory) + "\\Outbox" : "";
    ULONGLONG before = file_size(outbox);
    bool queued = false;
    if (frame && saved && encoded) {
        /* Deferred Delivery must change the real Send command's transport. */
        frame->SendMessage(WM_COMMAND, ID_COMPOSE_DEFERRED);
        frame->SendMessage(WM_COMMAND, IDC_COMPOSE_SEND);
        DWORD started = GetTickCount();
        while (composer_for(context) && GetTickCount() - started < 15000) smoke_pump();
        queued = file_size(outbox) > before;
        frame = composer_for(context);
    }
    if (frame) {
        frame->backend_closed = true;
        frame->DestroyWindow();
    }
    FILE *output = report ? fopen(report, "a") : nullptr;
    if (output) {
        fprintf(output, "composer_%s_body_roundtrip=%d\ncomposer_%s_draft_saved=%d\ncomposer_%s_mime_headers=%d\ncomposer_%s_draft=%s\ncomposer_%s_queued=%d\n",
            is_news ? "news" : "mail", roundtrip, is_news ? "news" : "mail", saved,
            is_news ? "news" : "mail", encoded, is_news ? "news" : "mail", path.c_str(),
            is_news ? "news" : "mail", queued);
        fprintf(output, "composer_%s_unfinished_draft=%d\ncomposer_%s_header_clear=%d\ncomposer_%s_attachment=%d\ncomposer_%s_attachment_clear=%d\n",
            is_news ? "news" : "mail", unfinished, is_news ? "news" : "mail", cleared,
            is_news ? "news" : "mail", attachment_ok, is_news ? "news" : "mail", attachment_cleared);
        fprintf(output, "composer_%s_preflight_cancel=%d\ncomposer_%s_cancel_attachment_survived=%d\ncomposer_%s_cancel_draft_survived=%d\n",
            is_news ? "news" : "mail", preflight_cancelled,
            is_news ? "news" : "mail", survived_attachment,
            is_news ? "news" : "mail", survived_draft);
        fclose(output);
    }
    return preflight_cancelled && survived_attachment && survived_draft &&
        roundtrip && unfinished && cleared && attachment_ok && attachment_cleared &&
        saved && encoded && queued ? 0 : 1;
}

static CRect compose_control_rect(ComposerFrame *frame, int id)
{
    CRect rect(0,0,0,0);
    CWnd *control = frame->GetDlgItem(id);
    if (control && control->IsWindowVisible()) {
        control->GetWindowRect(&rect); frame->ScreenToClient(&rect);
    }
    return rect;
}

static bool compose_capture(ComposerFrame *frame, const char *path, bool client = true)
{
    if (!path || !*path) return false;
    CRect area;
    if (client) frame->GetClientRect(&area); else frame->GetWindowRect(&area);
    HDC window = ::GetDC(frame->GetSafeHwnd());
    HDC memory = CreateCompatibleDC(window);
    BITMAPINFO info = {};
    info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = area.Width(); info.bmiHeader.biHeight = -area.Height();
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void *pixels = nullptr;
    HBITMAP bitmap = CreateDIBSection(window, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
    bool captured = false;
    if (bitmap && memory) {
        HGDIOBJ old = SelectObject(memory, bitmap);
        captured = PrintWindow(frame->GetSafeHwnd(), memory, client ? PW_CLIENTONLY : 0) != FALSE;
        if (captured) {
            FILE *output = fopen(path, "wb");
            captured = output != nullptr;
            if (output) {
                DWORD bytes = area.Width() * area.Height() * 4;
                BITMAPFILEHEADER header = {};
                header.bfType = 0x4d42;
                header.bfOffBits = sizeof(header) + sizeof(BITMAPINFOHEADER);
                header.bfSize = header.bfOffBits + bytes;
                captured = fwrite(&header, sizeof(header), 1, output) == 1 &&
                    fwrite(&info.bmiHeader, sizeof(BITMAPINFOHEADER), 1, output) == 1 &&
                    fwrite(pixels, bytes, 1, output) == 1;
                fclose(output);
            }
        }
        SelectObject(memory, old);
    }
    if (bitmap) DeleteObject(bitmap);
    if (memory) DeleteDC(memory);
    ::ReleaseDC(frame->GetSafeHwnd(), window);
    return captured;
}

extern "C" int WFE_ComposerUISmoke(MWContext *source, const char *report, const char *snapshot)
{
    MWContext *context = MSG_ComposeMessage(source, nullptr, nullptr,
        "recipient@example.test", "copy@example.test", "blind@example.test",
        nullptr, nullptr, nullptr, nullptr, "Hello", nullptr, nullptr,
        nullptr, nullptr, nullptr, FALSE, FALSE);
    ComposerFrame *frame = composer_for(context);
    if (!frame) return 1;
    CRect initial_bounds; frame->GetWindowRect(&initial_bounds);
    MONITORINFO initial_monitor = {}; initial_monitor.cbSize = sizeof(initial_monitor);
    bool original_size = GetMonitorInfo(MonitorFromWindow(frame->GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &initial_monitor) != FALSE;
    if (original_size) {
        CRect work(initial_monitor.rcWork);
        original_size = initial_bounds.Width() == (std::min)(ClassicUI::Scale(frame, 534), work.Width()) &&
            initial_bounds.Height() == (std::min)(ClassicUI::Scale(frame, 953), work.Height()) &&
            initial_bounds.left >= work.left && initial_bounds.top >= work.top &&
            initial_bounds.right <= work.right && initial_bounds.bottom <= work.bottom;
    }
    frame->body.SetWindowText("Hello,\r\n\r\nThis is a message composed in Netscape.\r\n\r\nRegards,\r\nPat\r\n");
    frame->SetHeader(1, "reply@example.test");
    frame->SetHeader(5, "Sent");
    frame->ShowWindow(SW_SHOWNORMAL); frame->UpdateWindow();
    for (int i = 0; i < 5; ++i) smoke_pump();
    /* Exercise the original 3.02 commands through normal frame routing. */
    CMenu *file_menu = frame->GetMenu() ? frame->GetMenu()->GetSubMenu(0) : nullptr;
    CString save_caption;
    if (file_menu) file_menu->GetMenuString(ID_COMPOSE_SAVE_AS, save_caption, MF_BYCOMMAND);
    bool original_save_as = save_caption == "Save &As...\tCtrl+S";
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_ATTACHMENTS);
    bool attachment_toggle = !frame->attachment_text.IsWindowVisible();
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_ATTACHMENTS);
    attachment_toggle = attachment_toggle && frame->attachment_text.IsWindowVisible();
    CString original_body; frame->body.GetWindowText(original_body);
    bool paste_quotation = false;
    if (::OpenClipboard(frame->GetSafeHwnd())) {
        const char text[] = "First line\r\nSecond line\r\n";
        HGLOBAL data = GlobalAlloc(GMEM_MOVEABLE, sizeof(text));
        if (data) {
            void *bytes = GlobalLock(data);
            if (bytes) {
                memcpy(bytes, text, sizeof(text)); GlobalUnlock(data);
                EmptyClipboard();
                if (!SetClipboardData(CF_TEXT, data)) GlobalFree(data);
            } else GlobalFree(data);
        }
        CloseClipboard();
        frame->body.SetWindowText(""); frame->body.SetFocus();
        frame->SendMessage(WM_COMMAND, ID_COMPOSE_PASTE_QUOTATION);
        CString quoted; frame->body.GetWindowText(quoted);
        paste_quotation = quoted == "> First line\r\n> Second line\r\n";
        frame->body.SetWindowText(original_body);
    }
    frame->body.SetSel(7, 12);
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_WRAP);
    bool unwrapped = (frame->body.GetStyle() & ES_AUTOHSCROLL) != 0;
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_WRAP);
    int selection_start = 0, selection_end = 0;
    frame->body.GetSel(selection_start, selection_end);
    CString wrapped_body; frame->body.GetWindowText(wrapped_body);
    bool wrap_toggle = unwrapped && !(frame->body.GetStyle() & ES_AUTOHSCROLL) &&
        selection_start == 7 && selection_end == 12 && wrapped_body == original_body;
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_DEFERRED);
    CString send_text; frame->send.GetWindowText(send_text);
    bool deferred_delivery = frame->deferred && send_text == "Send Later";
    frame->SendMessage(WM_COMMAND, ID_COMPOSE_IMMEDIATE);
    frame->send.GetWindowText(send_text);
    bool immediate_delivery = !frame->deferred && send_text == "Send";
    std::string body_path = snapshot && *snapshot ? std::string(snapshot) + ".txt" : "";
    bool body_saved = !body_path.empty() && frame->SaveBody(body_path.c_str());
    if (body_saved) {
        FILE *saved = fopen(body_path.c_str(), "rb");
        std::vector<char> bytes(original_body.GetLength() + 1);
        size_t length = saved ? fread(bytes.data(), 1, bytes.size(), saved) : 0;
        if (saved) fclose(saved);
        body_saved = length == (size_t)original_body.GetLength() &&
            !memcmp(bytes.data(), (const char *)original_body, length) && frame->dirty;
        DeleteFile(body_path.c_str());
    }
    std::string menu_labels, toolbar_labels;
    CMenu *menu = frame->GetMenu();
    if (menu) for (int i = 0; i < menu->GetMenuItemCount(); ++i) {
        CString text; menu->GetMenuString(i, text, MF_BYPOSITION); text.Remove('&');
        if (i) menu_labels += '|'; menu_labels += (const char *)text;
    }
    int toolbar_ids[] = {IDC_COMPOSE_SEND, IDC_COMPOSE_QUOTE, IDC_COMPOSE_ATTACH, IDC_COMPOSE_ADDRESS, IDC_COMPOSE_STOP};
    int toolbar_count = 0;
    CRect toolbar(0,0,0,0);
    for (int i = 0; i < 5; ++i) {
        CWnd *control = frame->GetDlgItem(toolbar_ids[i]);
        if (control && control->IsWindowVisible()) {
            CString text; control->GetWindowText(text);
            if (toolbar_count) toolbar_labels += '|'; toolbar_labels += (const char *)text;
            CRect rect = compose_control_rect(frame, toolbar_ids[i]);
            if (toolbar_count) toolbar.UnionRect(toolbar, rect); else toolbar = rect;
            ++toolbar_count;
        }
    }
    CRect client; frame->GetClientRect(&client);
    CRect full = client;
    CRect editor = compose_control_rect(frame, IDC_COMPOSE_BODY);
    CRect mail_to = compose_control_rect(frame, IDC_COMPOSE_FIRST + 2);
    CRect cc = compose_control_rect(frame, IDC_COMPOSE_FIRST + 3);
    CRect subject = compose_control_rect(frame, IDC_COMPOSE_FIRST + 8);
    CRect attachment = compose_control_rect(frame, IDC_COMPOSE_ATTACHMENT);
    CWnd *to_button = frame->GetDlgItem(IDC_COMPOSE_MAILTO), *cc_button = frame->GetDlgItem(IDC_COMPOSE_CC), *attachment_button = frame->GetDlgItem(IDC_COMPOSE_ATTACHMENT_BUTTON);
    CString to_label, cc_label, attachment_label;
    if (to_button) to_button->GetWindowText(to_label);
    if (cc_button) cc_button->GetWindowText(cc_label);
    if (attachment_button) attachment_button->GetWindowText(attachment_label);
    bool compact_labels = to_label == "Mail To:" && cc_label == "Cc:" && attachment_label == "Attachment:";
    bool compact = !frame->fields[0].IsWindowVisible() && !frame->fields[1].IsWindowVisible() &&
        !frame->fields[4].IsWindowVisible() && !frame->fields[5].IsWindowVisible() &&
        mail_to.top >= toolbar.bottom && cc.top > mail_to.top && subject.top > cc.top &&
        attachment.top > subject.top && editor.top >= attachment.bottom &&
        editor.Height() > client.Height() / 2;
    frame->SyncHeaders();
    bool retained = frame->Header(4) == "blind@example.test" &&
        frame->Header(1) == "reply@example.test" && frame->Header(5) == "Sent";
    /* A compact header can be hidden while its backend mask is already set. */
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADER_FIRST + 4);
    bool bcc_toggle = frame->fields[4].IsWindowVisible() && frame->Header(4) == "blind@example.test";
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADERS);
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADERS);
    bool advanced = frame->fields[4].IsWindowVisible() && retained;
    /* Conversely, compact Cc stays visible after an advanced user hides it. */
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADER_FIRST + 3);
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADERS);
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADER_FIRST + 3);
    bool cc_toggle = !frame->fields[3].IsWindowVisible() && frame->Header(3) == "copy@example.test";
    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_HEADERS);
    frame->Arrange(); frame->UpdateWindow();
    bool captured = compose_capture(frame, snapshot);
    bool window_captured = snapshot && *snapshot &&
        compose_capture(frame, (std::string(snapshot) + ".window.bmp").c_str(), false);
    UINT commands[] = {IDC_COMPOSE_DRAFT, IDC_COMPOSE_ATTACH, IDC_COMPOSE_CANCEL};
    bool menu_commands[3] = {};
    for (int c = 0; menu && c < 3; ++c)
        for (int i = 0; i < menu->GetMenuItemCount(); ++i) {
            CMenu *sub = menu->GetSubMenu(i);
            if (sub && sub->GetMenuState(commands[c], MF_BYCOMMAND) != (UINT)-1) menu_commands[c] = true;
        }
    CWnd *bar = frame->GetDlgItem(IDC_COMPOSE_STATUS);
    int parts = bar ? (int)bar->SendMessage(SB_GETPARTS) : 0;
    frame->SetWindowPos(nullptr, 0, 0, ClassicUI::Scale(frame, 480), ClassicUI::Scale(frame, 500),
                         SWP_NOMOVE | SWP_NOZORDER);
    frame->Arrange(); frame->GetClientRect(&client);
    CRect narrow = compose_control_rect(frame, IDC_COMPOSE_BODY);
    bool narrow_ok = narrow.Width() > 0 && narrow.Height() > 0 && narrow.left >= 0 &&
        narrow.right <= client.right && narrow.bottom <= client.bottom &&
        frame->Header(4) == "blind@example.test";
    CString title; frame->GetWindowText(title);
    bool titled = title == "Netscape - [Message Composition]";
    CWnd *address = frame->GetDlgItem(IDC_COMPOSE_ADDRESS), *quote = frame->GetDlgItem(IDC_COMPOSE_QUOTE), *stop = frame->GetDlgItem(IDC_COMPOSE_STOP);
    bool address_disabled = address && !address->IsWindowEnabled();
    bool quote_backend = quote && (quote->IsWindowEnabled() != FALSE) == command_enabled(context, MSG_QuoteMessage);
    bool stop_idle = stop && !stop->IsWindowEnabled();
    bool passed = original_size && original_save_as && attachment_toggle && paste_quotation && wrap_toggle &&
        deferred_delivery && immediate_delivery && body_saved &&
        titled && compact_labels && address_disabled && quote_backend && stop_idle && menu_labels == "File|Edit|View|Options|Window" &&
        toolbar_labels == "Send|Quote|Attach|Address|Stop" && toolbar_count == 5 &&
        compact && advanced && bcc_toggle && cc_toggle && retained && narrow_ok && parts >= 2 && captured && window_captured &&
        menu_commands[0] && menu_commands[1] && menu_commands[2];
    FILE *output = report ? fopen(report, "a") : nullptr;
    if (output) {
        fprintf(output, "original_size=%d\ninitial_outer=%d,%d\n", original_size, initial_bounds.Width(), initial_bounds.Height());
        fprintf(output, "original_save_as=%d\nattachment_toggle=%d\npaste_quotation=%d\nwrap_toggle=%d\n", original_save_as, attachment_toggle, paste_quotation, wrap_toggle);
        fprintf(output, "deferred_delivery=%d\nimmediate_delivery=%d\nbody_saved=%d\n", deferred_delivery, immediate_delivery, body_saved);
        fprintf(output, "menu_labels=%s\ntoolbar_labels=%s\ntoolbar_count=%d\n", menu_labels.c_str(), toolbar_labels.c_str(), toolbar_count);
        fprintf(output, "compact_headers=%d\nadvanced_headers=%d\nhidden_headers_retained=%d\n", compact, advanced, retained);
        fprintf(output, "compact_labels=%d\n", compact_labels);
        fprintf(output, "bcc_toggle=%d\ncc_toggle=%d\n", bcc_toggle, cc_toggle);
        fprintf(output, "window_snapshot=%d\n", window_captured);
        fprintf(output, "composition_title=%d\naddress_disabled=%d\nquote_backend=%d\nstop_idle=%d\n", titled, address_disabled, quote_backend, stop_idle);
        fprintf(output, "draft_command=%d\nattachment_command=%d\ncancel_command=%d\nstatus_panes=%d\n", menu_commands[0], menu_commands[1], menu_commands[2], parts);
        fprintf(output, "client=%d,%d\ntoolbar=%d,%d,%d,%d\neditor=%d,%d,%d,%d\n", full.Width(), full.Height(), toolbar.left, toolbar.top, toolbar.right, toolbar.bottom, editor.left, editor.top, editor.right, editor.bottom);
        const char *names[] = {"mail_to", "cc", "subject", "attachment"};
        CRect rows[] = {mail_to, cc, subject, attachment};
        for (int i = 0; i < 4; ++i) fprintf(output, "%s=%d,%d,%d,%d\n", names[i], rows[i].left, rows[i].top, rows[i].right, rows[i].bottom);
        fprintf(output, "narrow_client=%d,%d\nnarrow_editor=%d,%d,%d,%d\nnarrow_layout=%d\nsnapshot=%d\npassed=%d\ncompleted=1\n", client.Width(), client.Height(), narrow.left, narrow.top, narrow.right, narrow.bottom, narrow_ok, captured, passed);
        fclose(output);
    }
    frame->backend_closed = true; frame->DestroyWindow();
    return passed ? 0 : 1;
}

static bool loopback_endpoint(const char *endpoint)
{
    if (!endpoint) return false;
    const char *colon = strchr(endpoint, ':');
    if (!colon || colon == endpoint || !colon[1]) return false;
    std::string host(endpoint, colon - endpoint);
    if (_stricmp(host.c_str(), "localhost") && host != "127.0.0.1") return false;
    unsigned port = 0;
    for (const char *p = colon + 1; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
        port = port * 10 + (*p - '0');
        if (port > 65535) return false;
    }
    return port != 0;
}

extern "C" int WFE_ComposerQuoteSmoke(const char *report, const char *url)
{
    bool quote_dirty = false, quote_stop_active = false, quote_stop_survived = false, quote_retry = false;
    std::string address = url ? url : "";
    size_t slash = address.find('/', 7);
    std::string endpoint = address.size() > 7 ? address.substr(7, slash == std::string::npos ? slash : slash - 7) : "";
    bool guarded = address.compare(0, 7, "http://") == 0 && loopback_endpoint(endpoint.c_str());
    if (guarded) {
        /* Seed a source history entry without fetching it as an attachment. */
        MWContext source = {};
        WFE_InitContext(&source, nullptr);
        URL_Struct *entry_url = NET_CreateURLStruct(url, NET_DONT_RELOAD);
        History_entry *entry = entry_url ? SHIST_CreateHistoryEntry(entry_url, (char *)"Quote fixture") : nullptr;
        if (entry) SHIST_AddDocument(&source, entry);
        if (entry_url) NET_FreeURLStruct(entry_url);
        MWContext *context = entry ? MSG_ComposeMessage(&source, nullptr, nullptr,
            "recipient@example.test", nullptr, "blind@example.test", nullptr,
            nullptr, nullptr, nullptr, "Quote fixture", nullptr, nullptr,
            nullptr, nullptr, nullptr, FALSE, FALSE) : nullptr;
        ComposerFrame *frame = composer_for(context);
        if (frame) {
            frame->body.SetWindowText("Quote fixture body\r\n");
            frame->body.SetSel(-1, -1);
            frame->dirty = false;
            frame->SendMessage(WM_COMMAND, IDC_COMPOSE_QUOTE);
            DWORD started = GetTickCount();
            /* Permit the server to receive the slow first request. */
            while (composer_for(context) && MSG_QuoteInProgress(context) && GetTickCount() - started < 150) smoke_pump();
            frame = composer_for(context);
            quote_dirty = frame && frame->dirty;
            if (frame) frame->UpdateButtons();
            quote_stop_active = frame && MSG_QuoteInProgress(context) && frame->stop.IsWindowEnabled();
            if (quote_stop_active) frame->SendMessage(WM_COMMAND, IDC_COMPOSE_STOP);
            started = GetTickCount();
            while (composer_for(context) && MSG_QuoteInProgress(context) && GetTickCount() - started < 5000) smoke_pump();
            frame = composer_for(context);
            if (frame) {
                CString body; frame->body.GetWindowText(body);
                quote_stop_survived = quote_stop_active && context->msg_cframe &&
                    !MSG_QuoteInProgress(context) && !frame->backend_closed &&
                    body == "Quote fixture body\r\n" && frame->Header(2) == "recipient@example.test" &&
                    frame->Header(4) == "blind@example.test";
                if (quote_stop_survived) {
                    frame->SendMessage(WM_COMMAND, IDC_COMPOSE_QUOTE);
                    started = GetTickCount();
                    while (composer_for(context) && MSG_QuoteInProgress(context) && GetTickCount() - started < 10000) smoke_pump();
                    frame = composer_for(context);
                    if (frame) {
                        frame->body.GetWindowText(body);
                        quote_retry = !MSG_QuoteInProgress(context) && body.Find("Quoted reference text") >= 0 &&
                            frame->Header(4) == "blind@example.test";
                    }
                }
            }
            frame = composer_for(context);
            if (frame) { frame->backend_closed = true; frame->DestroyWindow(); }
        }
        WFE_DestroyContext(&source);
    }
    FILE *output = report ? fopen(report, "a") : nullptr;
    if (output) {
        fprintf(output, "quote_guard=%d\nquote_dirty=%d\nquote_stop_active=%d\nquote_stop_survived=%d\nquote_retry=%d\n",
            guarded, quote_dirty, quote_stop_active, quote_stop_survived, quote_retry);
        fclose(output);
    }
    return guarded && quote_dirty && quote_stop_active && quote_stop_survived && quote_retry ? 0 : 1;
}

extern "C" int WFE_ComposerDeliverSmoke(MWContext *source, const char *report, int is_news)
{
    const char *protocol = is_news ? "nntp" : "smtp";
    std::string endpoint = WFE_MailPreference(is_news ? "news" : "smtp");
    bool guarded = loopback_endpoint(endpoint.c_str());
    bool delivered = false;
    bool stop_active = false, stop_survived = false, stop_attachment = false, stop_retry = false;
    const char *error = guarded ? "could_not_create_composer" : "endpoint_must_be_loopback_with_port";
    if (guarded) {
        WFE_ApplyMailPreferences();
        std::string news_url = is_news ? "news://" + endpoint + "/" : "";
        MWContext *context = MSG_ComposeMessage(source, nullptr, nullptr,
            is_news ? nullptr : "recipient@example.test", nullptr, nullptr,
            nullptr, is_news ? "test.native" : nullptr, nullptr, nullptr,
            "Native delivery smoke", nullptr, nullptr, nullptr,
            is_news ? news_url.c_str() : nullptr, nullptr, FALSE, FALSE);
        ComposerFrame *frame = composer_for(context);
        if (frame) {
            /* Clear inherited/default copies, attachments and recipients so
               this test has exactly one loopback transport destination. */
            for (int i = 1; i < header_count - 1; ++i) frame->SetHeader(i, "");
            frame->SetHeader(is_news ? 6 : 2, is_news ? "test.native" : "recipient@example.test");
            MSG_SetAttachmentList(context, nullptr);
            frame->RefreshAttachments();
            frame->body.SetWindowText("Native delivery body\r\n");
            MSG_MessageBodyEdited(context);
            std::string attachment_path = std::string(WFE_ProfileDirectory()) + "\\delivery-stop-attachment.txt";
            FILE *attachment = fopen(attachment_path.c_str(), "wb");
            bool attachment_ready = false;
            if (attachment) {
                fputs("Attachment retained across native Stop.\r\n", attachment);
                fclose(attachment);
                attachment_ready = frame->AttachPath(attachment_path.c_str()) && smoke_wait_ready(context);
            }
            if (attachment_ready) {
                const MSG_AttachmentData *list = MSG_GetAttachmentList(context);
                std::string attachment_url = list && list[0].url ? list[0].url : "";
                frame->SendMessage(WM_COMMAND, IDC_COMPOSE_SEND);
                frame = composer_for(context);
                stop_active = frame && frame->delivery && XP_IsContextStoppable(context);
                if (stop_active) frame->SendMessage(WM_COMMAND, IDC_COMPOSE_STOP);
                bool ready = smoke_wait_ready(context);
                frame = composer_for(context);
                if (frame) {
                    CString body; frame->body.GetWindowText(body);
                    stop_survived = stop_active && ready && context->msg_cframe &&
                        !frame->delivery && !frame->backend_closed && body == "Native delivery body\r\n";
                    list = MSG_GetAttachmentList(context);
                    stop_attachment = !attachment_url.empty() && list && list[0].url &&
                        attachment_url == list[0].url && !list[1].url;
                }
            }
            delivery_observers[context] = &delivered;
            if (stop_survived && stop_attachment && smoke_wait_ready(context)) {
                /* Same WM_COMMAND route used by the native Send button. */
                frame->SendMessage(WM_COMMAND, IDC_COMPOSE_SEND);
                DWORD started = GetTickCount();
                while (composer_for(context) && GetTickCount() - started < 15000) smoke_pump();
                stop_retry = delivered;
                error = delivered ? "" : "backend_delivery_did_not_complete";
            } else error = "stop_did_not_preserve_composition";
            delivery_observers.erase(context);
            frame = composer_for(context);
            if (frame) {
                frame->backend_closed = true;
                frame->DestroyWindow();
            }
        }
    }
    FILE *output = report ? fopen(report, "a") : nullptr;
    if (output) {
        fprintf(output, "protocol=%s\nendpoint=%s\ndelivery_guard=%d\ndelivered=%d\ndelivery_error=%s\n",
            protocol, endpoint.c_str(), guarded, delivered, error);
        fprintf(output, "stop_active=%d\nstop_survived=%d\nstop_attachment=%d\nstop_retry=%d\n",
            stop_active, stop_survived, stop_attachment, stop_retry);
        fclose(output);
    }
    return delivered && stop_active && stop_survived && stop_attachment && stop_retry ? 0 : 1;
}
