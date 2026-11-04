#include <afxwin.h>
#include <algorithm>
#include <map>
#include <string>
#include "find.h"
#include "classic.h"

enum { IDC_FIND_TEXT = 4600, IDC_FIND_CASE, IDC_FIND_BACKWARDS,
       IDC_FIND_WRAP, IDC_FIND_RESULT };

struct FindState {
    int32 document = 0;
    std::string term;
    bool match_case = false, backwards = false, wrap = true;
};
static std::map<MWContext *, FindState> find_states;

static void remove_closed_contexts()
{
    for (auto entry = find_states.begin(); entry != find_states.end();) {
        if (!XP_IsContextInList(entry->first)) entry = find_states.erase(entry);
        else ++entry;
    }
}

static int32 reveal_axis(HWND window, int bar, int32 point, int32 current, int32 page)
{
    int32 position = current;
    if (point < current || point >= current + page) position = point - page / 2;
    SCROLLINFO range = { sizeof(SCROLLINFO), SIF_RANGE };
    if (GetScrollInfo(window, bar, &range)) {
        int32 maximum = (std::max)(0L, (int32)range.nMax + 1 - page);
        position = (std::min)(position, maximum);
    }
    return (std::max)(0L, position);
}

extern "C" int WFE_FindText(MWContext *context, const char *term,
                             int match_case, int backwards, int wrap)
{
    remove_closed_contexts();
    if (!context || !XP_IsContextInList(context) || !term || !*term) return 0;
    HWND window = WFE_DocumentWindow(context);
    if (!IsWindow(window)) return 0;
    FindState &previous = find_states[context];
    bool continuing = previous.document == XP_DOCID(context) &&
                      previous.term == term && previous.match_case == !!match_case;
    previous.document = XP_DOCID(context);
    previous.term = term;
    previous.match_case = !!match_case;
    previous.backwards = !!backwards;
    previous.wrap = !!wrap;

    /* Selection belongs to the current layout. Never retain LO_Element pointers
     * across modal message pumping, navigation, or a resize that relays out text. */
    LO_Element *start = nullptr, *end = nullptr;
    int32 start_pos = 0, end_pos = 0;
    if (continuing) LO_GetSelectionEndpoints(context, &start, &end, &start_pos, &end_pos);
    bool from_selection = start != nullptr;
    std::string query(term);
    Bool found = LO_FindText(context, &query[0], &start, &start_pos, &end, &end_pos,
                            match_case != 0, backwards == 0);
    if (!found && wrap && from_selection) {
        start = end = nullptr;
        start_pos = end_pos = 0;
        found = LO_FindText(context, &query[0], &start, &start_pos, &end, &end_pos,
                            match_case != 0, backwards == 0);
    }
    if (!found) return 0;

    int32 x = 0, y = 0, current_x, current_y, width, height;
    LO_SelectText(context, start, start_pos, end, end_pos, &x, &y);
    if (start->type == LO_TEXT) {
        /* LO_SelectText returns the element origin, not the matched character.
         * Include the prefix width so matches in long unbroken lines are visible. */
        LO_TextStruct prefix = start->lo_text;
        prefix.text_len = (int16)(std::min)(start_pos, (int32)prefix.text_len);
        LO_TextInfo metrics = {};
        WFE_TextMetrics(context, &prefix, &metrics);
        x += prefix.x_offset + metrics.max_width;
        y += prefix.y_offset;
    }
    WFE_Position(context, &current_x, &current_y);
    WFE_WindowSize(context, &width, &height);
    x = reveal_axis(window, SB_HORZ, x, current_x, width);
    y = reveal_axis(window, SB_VERT, y, current_y, height);
    WFE_Scroll(context, x, y);
    SetScrollPos(window, SB_HORZ, x, TRUE);
    SetScrollPos(window, SB_VERT, y, TRUE);
    return 1;
}

class FindDialog : public CDialog {
public:
    MWContext *context;
    bool automatic;
    int found = 0;
    std::string initial;
    CStatic label, result;
    CEdit text;
    ClassicButton next, close;
    CButton match_case, backwards, wrap;
    FindDialog(MWContext *source, const char *test_term)
        : context(source), automatic(test_term != nullptr), initial(test_term ? test_term : "") { }
protected:
    BOOL OnInitDialog() override {
        CDialog::OnInitDialog();
        SetWindowText("Find in Page");
        CFont *font = ClassicUI::Font(this);
        SetFont(font);
        auto rect = [this](int left, int top, int right, int bottom) {
            return CRect(ClassicUI::Scale(this,left), ClassicUI::Scale(this,top),
                         ClassicUI::Scale(this,right), ClassicUI::Scale(this,bottom));
        };
        CRect outer = rect(0,0,400,164);
        AdjustWindowRectEx(&outer, GetStyle(), FALSE, GetExStyle());
        SetWindowPos(nullptr, 0, 0, outer.Width(), outer.Height(), SWP_NOMOVE | SWP_NOZORDER);
        label.Create("Fi&nd what:", WS_CHILD | WS_VISIBLE, rect(12,17,83,35), this);
        text.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_BORDER | ES_AUTOHSCROLL,
                    rect(86,12,294,36), this, IDC_FIND_TEXT);
        next.Create("Find &Next", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                    rect(305,12,388,38), this, IDOK);
        close.Create("Cancel", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                     rect(305,46,388,72), this, IDCANCEL);
        match_case.Create("Match &case", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                          rect(86,46,286,66), this, IDC_FIND_CASE);
        backwards.Create("Search &backwards", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                         rect(86,72,286,92), this, IDC_FIND_BACKWARDS);
        wrap.Create("&Wrap around", WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_AUTOCHECKBOX,
                    rect(86,98,286,118), this, IDC_FIND_WRAP);
        result.Create("", WS_CHILD | WS_VISIBLE, rect(12,134,388,154), this, IDC_FIND_RESULT);
        ClassicUI::Apply(this);
        text.LimitText(1024);
        auto saved = find_states.find(context);
        if (!automatic && saved != find_states.end()) {
            initial = saved->second.term;
            match_case.SetCheck(saved->second.match_case);
            backwards.SetCheck(saved->second.backwards);
            wrap.SetCheck(saved->second.wrap);
        } else wrap.SetCheck(BST_CHECKED);
        text.SetWindowText(initial.c_str());
        next.EnableWindow(!initial.empty());
        CenterWindow();
        text.SetFocus();
        text.SetSel(0, -1);
        if (automatic) PostMessage(WM_COMMAND, IDOK);
        return FALSE;
    }
    BOOL PreTranslateMessage(MSG *message) override {
        if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN) {
            if(GetFocus()==&close) OnCancel();else OnOK();return TRUE;
        }
        return CDialog::PreTranslateMessage(message);
    }
    void OnOK() override {
        if (!XP_IsContextInList(context)) { EndDialog(IDCANCEL); return; }
        CString query;
        text.GetWindowText(query);
        if (query.IsEmpty()) { if (automatic) EndDialog(IDCANCEL); return; }
        found = WFE_FindText(context, query, match_case.GetCheck() == BST_CHECKED,
                             backwards.GetCheck() == BST_CHECKED, wrap.GetCheck() == BST_CHECKED);
        result.SetWindowText(found ? "" : "Text not found.");
        if (automatic) EndDialog(IDOK);
    }
    BOOL OnCommand(WPARAM command, LPARAM control) override {
        if (LOWORD(command) == IDC_FIND_TEXT && HIWORD(command) == EN_CHANGE) {
            if (next.GetSafeHwnd()) next.EnableWindow(text.GetWindowTextLength() != 0);
            if (result.GetSafeHwnd()) result.SetWindowText("");
            return TRUE;
        }
        return CDialog::OnCommand(command, control);
    }
    afx_msg BOOL OnEraseBkgnd(CDC *dc) {
        CRect area; GetClientRect(area); dc->FillSolidRect(area, ClassicUI::Face); return TRUE;
    }
    afx_msg HBRUSH OnCtlColor(CDC *dc, CWnd *window, UINT type) {
        return ClassicUI::ControlColor(dc, window, type);
    }
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(FindDialog, CDialog)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

static int show_find(MWContext *context, const char *automatic)
{
    remove_closed_contexts();
    if (!context || !XP_IsContextInList(context)) return 0;
    HWND owner = WFE_DocumentWindow(context);
    if (!IsWindow(owner)) return 0;
    struct DialogTemplate { DLGTEMPLATE dialog; WORD menu, window_class, title; } layout = {};
    layout.dialog.style = WS_POPUP | WS_CAPTION | WS_SYSMENU | DS_MODALFRAME;
    layout.dialog.cx = 270; layout.dialog.cy = 110;
    FindDialog dialog(context, automatic);
    if (!dialog.InitModalIndirect(&layout.dialog, CWnd::FromHandle(GetAncestor(owner, GA_ROOT)))) return 0;
    dialog.DoModal();
    if(dialog.found && XP_IsContextInList(context) && IsWindow(owner)) ::SetFocus(owner);
    return dialog.found;
}

extern "C" void WFE_FindShow(MWContext *context) { show_find(context, nullptr); }
extern "C" int WFE_FindDialogSmoke(MWContext *context, const char *term)
{
    return term ? show_find(context, term) : 0;
}
