#include <afxwin.h>
#include <afxcmn.h>
#include "history.h"
#include "classic.h"
extern "C" {
#include "shist.h"
}

static MWContext *top_context(MWContext *context)
{
    while (context && context->grid_parent) context = context->grid_parent;
    return context;
}

extern "C" void WFE_HistorySavePosition(MWContext *context)
{
    if (!context || !SHIST_GetCurrent(&context->hist)) return;
    int32 x, y; WFE_Position(context, &x, &y);
    LO_Element *element = LO_XYToNearestElement(context, x, y);
    if (element) SHIST_SetPositionOfCurrentDoc(&context->hist, element->lo_any.ele_id);
}

static int load_entry(MWContext *context, History_entry *entry, bool reload)
{
    if (!context || !entry) return 0;
    WFE_HistorySavePosition(context);
    URL_Struct *url = SHIST_CreateURLStructFromHistoryEntry(context, entry);
    if (!url) return 0;
    if (reload) url->force_reload = NET_NORMAL_RELOAD;
    FE_GetURL(context, url);
    return 1;
}

extern "C" int WFE_HistoryMove(MWContext *context, int direction)
{
    if (!context || context->type != MWContextBrowser) return 0;
    if (!direction) return load_entry(context, SHIST_GetCurrent(&context->hist), true);
    context = top_context(context);
    if (direction < 0 && LO_BackInGrid(context)) return 1;
    if (direction > 0 && LO_ForwardInGrid(context)) return 1;
    return load_entry(context, direction < 0 ? SHIST_GetPrevious(context) : SHIST_GetNext(context), false);
}

extern "C" int WFE_HistoryCanMove(MWContext *context, int direction)
{
    context = top_context(context);
    if (!context || context->type != MWContextBrowser) return 0;
    return direction < 0 ? SHIST_CanGoBack(context) : SHIST_CanGoForward(context);
}
extern "C" int WFE_HistoryGo(MWContext *context,int32 id)
{
    context=top_context(context);if(!context || context->type!=MWContextBrowser) return 0;
    XP_List *entries=SHIST_GetList(context);
    for(int i=1;i<=XP_ListCount(entries);++i) {
        History_entry *entry=(History_entry *)XP_ListGetObjectNum(entries,i);
        if(entry && entry->unique_id==id) return load_entry(context,entry,false);
    }
    return 0;
}

/* Row identities survive nested dialog message pumping; backend pointers do not. */
class HistoryDialog : public CDialog {
public:
    MWContext *context;
    int32 selected = 0, automatic;
    bool global = false;
    CListCtrl entries;
    ClassicButton open, close, visited;
    explicit HistoryDialog(MWContext *source, int32 test_id = 0) : context(source), automatic(test_id) { }
protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetWindowText("History");
        CRect area; GetClientRect(area);
        auto s = [this](int value) { return ClassicUI::Scale(this, value); };
        entries.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS,
                       CRect(s(6),s(6),area.Width()-s(6),area.Height()-s(35)), this, 1001);
        entries.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        entries.InsertColumn(0, "Title", LVCFMT_LEFT, s(190));
        entries.InsertColumn(1, "Address", LVCFMT_LEFT, s(440));
        open.Create("Open", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    CRect(area.Width()-s(170),area.Height()-s(29),area.Width()-s(90),area.Height()-s(6)), this, IDOK);
        close.Create("Close", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                     CRect(area.Width()-s(86),area.Height()-s(29),area.Width()-s(6),area.Height()-s(6)), this, IDCANCEL);
        visited.Create("Visited pages", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                       CRect(s(6),area.Height()-s(29),s(118),area.Height()-s(6)), this, 1002);
        CFont *font = ClassicUI::Font(this);
        entries.SetFont(font); open.SetFont(font); close.SetFont(font); visited.SetFont(font);
        ClassicUI::Apply(this);
        entries.SetBkColor(RGB(255,255,255));
        entries.SetTextBkColor(RGB(255,255,255));
        entries.SetTextColor(RGB(0,0,0));
        SetDefID(IDOK);
        if (XP_IsContextInList(context)) {
            XP_List *list = SHIST_GetList(context);
            History_entry *current = SHIST_GetCurrent(&context->hist);
            for (int i = 1; i <= XP_ListCount(list); ++i) {
                History_entry *entry = (History_entry *)XP_ListGetObjectNum(list, i);
                int row = entries.InsertItem(i-1, entry->title && *entry->title ? entry->title : "Untitled");
                entries.SetItemText(row, 1, entry->address ? entry->address : "");
                entries.SetItemData(row, (DWORD_PTR)entry->unique_id);
                if (automatic ? entry->unique_id == automatic : entry == current) {
                    entries.SetItemState(row, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
                    entries.EnsureVisible(row, FALSE);
                }
            }
        }
        CenterWindow();
        if (automatic) PostMessage(WM_COMMAND, automatic == -1 ? 1002 : IDOK);
        return TRUE;
    }
    void OnOK() override {
        int row = entries.GetNextItem(-1, LVNI_SELECTED);
        if (row < 0) { if (automatic) OnCancel(); return; }
        selected = (int32)entries.GetItemData(row);
        CDialog::OnOK();
    }
    BOOL OnCommand(WPARAM command, LPARAM control) override {
        if (LOWORD(command) == 1002) { global = true; EndDialog(IDOK); return TRUE; }
        return CDialog::OnCommand(command, control);
    }
    BOOL OnNotify(WPARAM control, LPARAM notification, LRESULT *result) override {
        NMHDR *event = (NMHDR *)notification;
        if (control == 1001 && event && event->code == NM_DBLCLK) { OnOK(); *result = 0; return TRUE; }
        return CDialog::OnNotify(control, notification, result);
    }
    afx_msg BOOL OnEraseBkgnd(CDC *dc) {
        CRect area; GetClientRect(&area);
        dc->FillSolidRect(area, ClassicUI::Face());
        return TRUE;
    }
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type) {
        return ClassicUI::ControlColor(dc, window, type);
    }
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(HistoryDialog, CDialog)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

static int show_history(MWContext *context, int32 automatic)
{
    context = top_context(context);
    if (!context || !XP_IsContextInList(context)) return 0;
    struct DialogTemplate { DLGTEMPLATE dialog; WORD menu, window_class, title; } layout = {};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    layout.dialog.cx = 460; layout.dialog.cy = 240;
    HistoryDialog dialog(context, automatic);
    HWND owner = WFE_DocumentWindow(context);
    if (!dialog.InitModalIndirect(&layout.dialog, CWnd::FromHandle(GetAncestor(owner, GA_ROOT)))) return 0;
    if (dialog.DoModal() != IDOK || !XP_IsContextInList(context)) return 0;
    if (dialog.global) { WFE_NavigateTarget(context, "about:global", nullptr); return 1; }
    XP_List *list = SHIST_GetList(context);
    for (int i = 1; i <= XP_ListCount(list); ++i) {
        History_entry *entry = (History_entry *)XP_ListGetObjectNum(list, i);
        if (entry->unique_id == dialog.selected) return load_entry(context, entry, false);
    }
    WFE_Status(context, "That history entry is no longer available.");
    return 0;
}

extern "C" void WFE_HistoryShow(MWContext *context) { show_history(context, 0); }
extern "C" int WFE_HistoryDialogSmoke(MWContext *context, int32 id) { return show_history(context, id); }
