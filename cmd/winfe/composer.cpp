#include <afxwin.h>
#include <afxdlgs.h>
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <shlwapi.h>
#include "composer.h"
#include "classic.h"
#include "resource.h"
#include "mailsettings.h"
#include "legacy/genedit.h"
extern "C" {
#include "msgcom.h"
#include "xp_file.h"
void WFE_RegisterNativeWindow(MWContext *, HWND);
void WFE_UnregisterNativeWindow(MWContext *);
/* Original libmsg mailbox writer, declared in lib/libmsg/msg.h. */
int msg_DoFCC(MWContext *, const char *, XP_FileType, const char *, XP_FileType,
              const char *, const char *);
}

enum {
    IDC_COMPOSE_FIRST = 4100, IDC_COMPOSE_BODY = 4110,
    IDC_COMPOSE_SEND = 4120, IDC_COMPOSE_LATER, IDC_COMPOSE_DRAFT,
    IDC_COMPOSE_ATTACH, IDC_COMPOSE_CANCEL, IDC_COMPOSE_HEADERS,
    IDC_COMPOSE_REMOVE, IDC_COMPOSE_ATTACHMENT
};
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
    CStatic labels[header_count], attachment_label, attachment_text, status;
    CGenericEdit fields[header_count], body;
    ClassicButton send, later, draft, attach, cancel, headers, remove;
    MSG_HEADER_SET visible = 0x3ff;
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
    void RefreshAttachments();
    bool AttachPath(const char *path);
    void SetHeader(int index, const char *text);
    CString Header(int index) { CString text; fields[index].GetWindowText(text); return text; }
    BOOL PreTranslateMessage(MSG *message) override;
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnClose();
    afx_msg void OnDestroy();
    afx_msg void OnSend() { Deliver(MSG_SendMessage); }
    afx_msg void OnLater() { Deliver(MSG_SendMessageLater); }
    afx_msg void OnDraft() { SaveDraft(); }
    afx_msg void OnAttach();
    afx_msg void OnRemove();
    afx_msg void OnHeaders() { if (context.msg_cframe) MSG_Command(&context, MSG_ShowAllHeaders); }
    afx_msg void OnHeaderChanged(UINT id);
    afx_msg void OnBodyChanged();
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(ComposerFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_COMMAND(IDC_COMPOSE_SEND, OnSend)
 ON_COMMAND(IDC_COMPOSE_LATER, OnLater)
 ON_COMMAND(IDC_COMPOSE_DRAFT, OnDraft)
 ON_COMMAND(IDC_COMPOSE_ATTACH, OnAttach)
 ON_COMMAND(IDC_COMPOSE_REMOVE, OnRemove)
 ON_COMMAND(IDC_COMPOSE_CANCEL, OnClose)
 ON_COMMAND(IDC_COMPOSE_HEADERS, OnHeaders)
 ON_CONTROL_RANGE(EN_CHANGE, IDC_COMPOSE_FIRST, IDC_COMPOSE_FIRST + header_count - 1, OnHeaderChanged)
 ON_CONTROL(EN_CHANGE, IDC_COMPOSE_BODY, OnBodyChanged)
END_MESSAGE_MAP()

int ComposerFrame::OnCreate(LPCREATESTRUCT create)
{
    if (CFrameWnd::OnCreate(create) == -1) return -1;
    CFont *font = ClassicUI::Font(this);
    CButton *buttons[] = {&send, &later, &draft, &attach, &cancel, &headers};
    const char *names[] = {"Send", "Send Later", "Save Draft", "Attach...", "Cancel", "All Headers"};
    for (int i = 0; i < 6; ++i) {
        if (!buttons[i]->Create(names[i], WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                                CRect(0,0,0,0), this, IDC_COMPOSE_SEND + i)) return -1;
        buttons[i]->SetFont(font);
    }
    send.SetBitmapResource(IDB_CLASSIC_SEND);  /* Original message toolbar artwork. */
    attach.SetBitmapResource(IDB_CLASSIC_ATTACH);
    draft.SetBitmapResource(IDB_CLASSIC_COMPOSE);
    for (int i = 0; i < header_count; ++i) {
        if (!labels[i].Create(header_names[i], WS_CHILD | WS_VISIBLE,
                               CRect(0,0,0,0), this)) return -1;
        if (!fields[i].Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | ES_AUTOHSCROLL,
                               CRect(0,0,0,0), this, IDC_COMPOSE_FIRST + i)) return -1;
        labels[i].SetFont(font);
        fields[i].SetFont(font);
        fields[i].LimitText(32767);
    }
    if (!attachment_label.Create("Attachments:", WS_CHILD | WS_VISIBLE,
                                  CRect(0,0,0,0), this) ||
        !attachment_text.Create("None", WS_CHILD | WS_VISIBLE | SS_PATHELLIPSIS,
                                 CRect(0,0,0,0), this, IDC_COMPOSE_ATTACHMENT) ||
        !remove.Create("Remove All", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                         CRect(0,0,0,0), this, IDC_COMPOSE_REMOVE) ||
        !body.Create(WS_CHILD | WS_VISIBLE | WS_BORDER | WS_TABSTOP | WS_VSCROLL |
                     ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN,
                     CRect(0,0,0,0), this, IDC_COMPOSE_BODY) ||
        !status.Create("Ready", WS_CHILD | WS_VISIBLE | SS_LEFT,
                        CRect(0,0,0,0), this)) return -1;
    attachment_label.SetFont(font);
    attachment_text.SetFont(font);
    remove.SetFont(font);
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
    CButton *buttons[] = {&send, &later, &draft, &attach, &cancel, &headers};
    int x = s(4), y = s(4);
    for (int i = 0; i < 6; ++i) {
        if (i && x + s(88) > area.Width() - s(4)) { x = s(4); y += s(48); }
        buttons[i]->MoveWindow(x, y, s(88), s(46));
        x += s(90);
    }
    y += s(52);
    for (int i = 0; i < header_count; ++i) {
        bool show = (visible & header_masks[i]) != 0;
        labels[i].ShowWindow(show ? SW_SHOW : SW_HIDE);
        fields[i].ShowWindow(show ? SW_SHOW : SW_HIDE);
        if (show) {
            labels[i].MoveWindow(s(6), y + s(3), s(76), s(18));
            fields[i].MoveWindow(s(84), y, (std::max)(area.Width() - s(90), s(20)), s(21));
            y += s(24);
        }
    }
    attachment_label.MoveWindow(s(6), y + s(3), s(76), s(18));
    attachment_text.MoveWindow(s(84), y + s(3), (std::max)(area.Width() - s(186), s(20)), s(18));
    remove.MoveWindow((std::max)(area.Width() - s(96), s(106)), y, s(90), s(22));
    y += s(27);
    body.MoveWindow(s(6), y, (std::max)(area.Width() - s(12), s(20)),
                    (std::max)(area.Height() - y - s(26), s(20)));
    status.MoveWindow(s(6), (std::max)(area.Height() - s(21), y + s(24)),
                      (std::max)(area.Width() - s(12), s(20)), s(18));
}

BOOL ComposerFrame::OnEraseBkgnd(CDC *dc)
{
    CRect area; GetClientRect(&area);
    dc->FillSolidRect(area, RGB(192,192,192));
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
    if (index == 8) SetWindowText(text.IsEmpty() ? "Compose Message - Netscape" : text + " - Netscape");
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
    send.EnableWindow(available);
    later.EnableWindow(ready && command_enabled(&context, MSG_SendMessageLater));
    draft.EnableWindow(available);
    attach.EnableWindow(available);
    remove.EnableWindow(available && MSG_GetAttachmentList(&context) != nullptr);
    headers.EnableWindow(ready);
    for (int i = 0; i < header_count; ++i) fields[i].EnableWindow(available);
    body.EnableWindow(available);
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
    attachment_text.SetWindowText(text && *text ? text : "None");
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
    if (!frame->Create(nullptr, "Compose Message - Netscape", WS_OVERLAPPEDWINDOW,
                        CRect(90,70,900,790))) return nullptr;
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
    if (frame) { frame->visible = mask; frame->Arrange(); }
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
        frame->Deliver(MSG_SendMessageLater);
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

extern "C" int WFE_ComposerDeliverSmoke(MWContext *source, const char *report, int is_news)
{
    const char *protocol = is_news ? "nntp" : "smtp";
    std::string endpoint = WFE_MailPreference(is_news ? "news" : "smtp");
    bool guarded = loopback_endpoint(endpoint.c_str());
    bool delivered = false;
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
            delivery_observers[context] = &delivered;
            if (smoke_wait_ready(context)) {
                /* Same WM_COMMAND route used by the native Send button. */
                frame->SendMessage(WM_COMMAND, IDC_COMPOSE_SEND);
                DWORD started = GetTickCount();
                while (composer_for(context) && GetTickCount() - started < 15000) smoke_pump();
                error = delivered ? "" : "backend_delivery_did_not_complete";
            } else error = "composer_did_not_become_ready";
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
        fclose(output);
    }
    return delivered ? 0 : 1;
}
