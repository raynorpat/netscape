/* Native General Preferences, following the Navigator 3.02 Win32 pages. */
#include <afxwin.h>
#include <afxcmn.h>
#include <afxdlgs.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include "classic.h"
#include "preferences.h"
#include "applications.h"
#include "preferences_resource.h"

extern "C" void GH_ClearGlobalHistory(void);
extern "C" const char *WFE_ProfileDirectory(void);

class GeneralPreferencesDialog;
class PreferencesPage : public CDialog {
public:
    PreferencesPage(GeneralPreferencesDialog *owner, int number);
    GeneralPreferencesDialog *owner;
    int number;
protected:
    BOOL OnCommand(WPARAM, LPARAM) override;
    BOOL OnNotify(WPARAM, LPARAM, LRESULT *) override;
    BOOL PreTranslateMessage(MSG *) override { return FALSE; }
    void OnOK() override {}
    void OnCancel() override;
    afx_msg BOOL OnEraseBkgnd(CDC *dc);
    afx_msg HBRUSH OnCtlColor(CDC *, CWnd *, UINT);
    afx_msg void OnDrawItem(int, LPDRAWITEMSTRUCT);
    DECLARE_MESSAGE_MAP()
};

static const char *preference_tabs[] = {
    "Appearance", "Fonts", "Colors", "Images", "Apps", "Helpers", "Language", "Security"
};
static const char *language_names[] = {
    "Chinese [zh]", "Chinese/China [zh-CN]", "Chinese/Taiwan [zh-TW]",
    "Dutch [nl]", "English [en]", "English/United Kingdom [en-GB]",
    "English/United States [en-US]", "French [fr]", "French/Canada [fr-CA]",
    "French/France [fr-FR]", "German [de]", "Italian [it]", "Japanese [ja]",
    "Korean [ko]", "Portuguese/Brazil [pt-BR]", "Spanish [es]",
    "Spanish/Spain [es-ES]", "Swedish [sv]"
};
static const char *language_tags[] = {
    "zh", "zh-CN", "zh-TW", "nl", "en", "en-GB", "en-US", "fr", "fr-CA",
    "fr-FR", "de", "it", "ja", "ko", "pt-BR", "es", "es-ES", "sv"
};
struct HelperDescription { const char *type, *action, *extensions; };
static const HelperDescription helper_types[] = {
    {"text/html", "Browser", "html,htm"},
    {"text/plain", "Browser", "txt,text"},
    {"image/gif", "Browser", "gif"},
    {"image/jpeg", "Browser", "jpg,jpeg"},
    {"image/x-xbitmap", "Browser", "xbm"},
    {"application/octet-stream", "Save to Disk", "*"}
};

class GeneralPreferencesDialog : public ClassicDialog {
public:
    explicit GeneralPreferencesDialog(CWnd *parent, bool diagnostic = false)
        : ClassicDialog(IDD_GENERAL_PREFERENCES, parent), values(WFE_GeneralPreferences()),
          testing(diagnostic), accepted(false), rejected(false), expire_now(false),
          help_count(0), current(0) {}
    GeneralPreferences values;
    bool testing, accepted, rejected, expire_now;
    int help_count, current;
    std::string error;
    CTabCtrl tabs;
    std::unique_ptr<PreferencesPage> pages[8];
    CBrush swatches[4];

    CWnd *Control(int page, int id) { return pages[page]->GetDlgItem(id); }
    void Check(int page, int id, bool checked) { pages[page]->CheckDlgButton(id, checked ? BST_CHECKED : BST_UNCHECKED); }
    bool Checked(int page, int id) { return pages[page]->IsDlgButtonChecked(id) == BST_CHECKED; }
    CString Text(int page, int id) { CString s; pages[page]->GetDlgItemText(id, s); return s; }
    void SetText(int page, int id, const char *text) { pages[page]->SetDlgItemText(id, text); }
    CWnd *Add(int page, const char *kind, const char *label, DWORD style,
              int x, int y, int w, int h, int id = -1, DWORD extra = 0) {
        HWND control = ::CreateWindowExA(extra, kind, label, WS_CHILD | WS_VISIBLE | style,
            ClassicUI::Scale(this,MulDiv(x,453,467)), ClassicUI::Scale(this,MulDiv(y,330,337)), ClassicUI::Scale(this,MulDiv(w,453,467)), ClassicUI::Scale(this,MulDiv(h,330,337)),
            pages[page]->GetSafeHwnd(), (HMENU)(INT_PTR)id, AfxGetInstanceHandle(), nullptr);
        return CWnd::FromHandle(control);
    }
    void Label(int p, const char *s, int x, int y, int w, int h = 16, int id = -1) { Add(p,"STATIC",s,SS_LEFT,x,y,w,h,id); }
    void Group(int p, const char *s, int x, int y, int w, int h) { Add(p,"BUTTON",s,BS_GROUPBOX,x,y,w,h); }
    void Button(int p, const char *s, int x, int y, int w, int h, int id, bool enabled = true) {
        Add(p,"BUTTON",s,BS_PUSHBUTTON|WS_TABSTOP,x,y,w,h,id)->EnableWindow(enabled);
    }
    void Radio(int p, const char *s, int x, int y, int w, int id, bool first = false) {
        Add(p,"BUTTON",s,BS_AUTORADIOBUTTON|WS_TABSTOP|(first?WS_GROUP:0),x,y,w,18,id);
    }
    void Tick(int p, const char *s, int x, int y, int w, int id) {
        Add(p,"BUTTON",s,BS_AUTOCHECKBOX|WS_TABSTOP|WS_GROUP,x,y,w,18,id);
    }
    void Edit(int p, const char *s, int x, int y, int w, int id, bool enabled = true) {
        Add(p,"EDIT",s,WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,x,y,w,20,id,WS_EX_CLIENTEDGE)->EnableWindow(enabled);
    }
    void SelectPage(int index) {
        if(index<0 || index>7) return;
        pages[current]->ShowWindow(SW_HIDE); current=index; tabs.SetCurSel(index);
        pages[current]->ShowWindow(SW_SHOW);
    }
    void SetSwatches() {
        for(int i=0;i<4;i++) {
            swatches[i].DeleteObject(); swatches[i].CreateSolidBrush(values.colors[i]);
            if(pages[2]) Control(2,IDC_PREF_SWATCH_BG+i)->Invalidate();
        }
    }
    void UpdateEnabled() {
        Control(0,IDC_PREF_HOME_URL)->EnableWindow(Checked(0,IDC_PREF_HOME));
        Control(0,IDC_PREF_DAYS)->EnableWindow(Checked(0,IDC_PREF_EXPIRE));
    }
    void UpdateBackground() {
        bool image=Checked(2,IDC_PREF_BG_IMAGE);
        Control(2,IDC_PREF_BG_FILE)->EnableWindow(image);Control(2,IDC_PREF_BG_BROWSE)->EnableWindow(image);
    }
    void RefreshAccepted() {
        CListBox *list=(CListBox *)Control(6,IDC_PREF_ACCEPTED); list->ResetContent();
        for(const auto &tag:values.languages) {
            const char *name=tag.c_str();
            for(int j=0;j<18;j++) if(!_stricmp(tag.c_str(),language_tags[j])) name=language_names[j];
            list->AddString(name);
        }
    }
    void UpdateHelper() {
        CListCtrl *list=(CListCtrl *)Control(5,IDC_PREF_HELPERS);
        int row=list->GetNextItem(-1,LVNI_SELECTED); if(row<0) row=0;
        CString mime=helper_types[row].type; int slash=mime.Find('/');
        SetText(5,IDC_PREF_HELPER_TYPE,mime.Left(slash));
        SetText(5,IDC_PREF_HELPER_SUBTYPE,mime.Mid(slash+1));
        SetText(5,IDC_PREF_HELPER_EXT,helper_types[row].extensions);
        Check(5,IDC_PREF_HELPER_BROWSER,row!=5); Check(5,IDC_PREF_HELPER_SAVE,row==5);
    }
    bool PageCommand(int page, WPARAM w, LPARAM);
    void ShowError(const std::string &message) { error=message; if(!testing) MessageBox(message.c_str(),"Preferences",MB_OK|MB_ICONEXCLAMATION); }
    void ChooseFont(bool fixed);
    bool ReadControls();
    BOOL PreTranslateMessage(MSG *message) override;
protected:
    BOOL OnInitDialog() override;
    void OnOK() override;
    void OnCancel() override {
        rejected=true; if(testing) ShowWindow(SW_HIDE); else ClassicDialog::OnCancel();
    }
    afx_msg void OnTab(NMHDR *, LRESULT *result) { SelectPage(tabs.GetCurSel()); *result=0; }
    afx_msg void OnHelp() {
        ++help_count;
        if(!testing) MessageBox("Choose how Netscape starts and displays documents, images, and links.\n\n"
            "Language entries are sent in the order shown in the Accept List.\n"
            "Background image files supply the default, unless a document supplies its own background.\n"
            "Always Use My Colors also overrides document backgrounds.\n"
            "Apps accepts executable paths without command templates. Terminal applications receive host and port,\n"
            "with -l username when supplied. Blank View Source uses Netscape. Temporary Directory must be writable.\n"
            "Helper association editing is not available in this version.\n\n"
            "OK saves all pages. Cancel discards changes. Expire Now takes effect when you choose OK.",
            "Preferences Help",MB_OK|MB_ICONINFORMATION);
    }
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(GeneralPreferencesDialog,ClassicDialog)
    ON_NOTIFY(TCN_SELCHANGE,IDC_PREF_TABS,OnTab)
    ON_BN_CLICKED(IDC_PREF_HELP,OnHelp)
END_MESSAGE_MAP()
BEGIN_MESSAGE_MAP(PreferencesPage,CDialog)
    ON_WM_ERASEBKGND()
    ON_WM_CTLCOLOR()
    ON_WM_DRAWITEM()
END_MESSAGE_MAP()
PreferencesPage::PreferencesPage(GeneralPreferencesDialog *dialog,int index)
    : CDialog(IDD_PREFERENCES_PAGE,dialog), owner(dialog), number(index) {}
BOOL PreferencesPage::OnCommand(WPARAM w,LPARAM l) {
    return owner->PageCommand(number,w,l) ? TRUE : CDialog::OnCommand(w,l);
}
BOOL PreferencesPage::OnNotify(WPARAM w,LPARAM l,LRESULT *result) {
    NMHDR *notification=(NMHDR *)l;
    if(number==5 && notification->idFrom==IDC_PREF_HELPERS && notification->code==LVN_ITEMCHANGED) {
        owner->UpdateHelper();*result=0;return TRUE;
    }
    return CDialog::OnNotify(w,l,result);
}
void PreferencesPage::OnCancel() { owner->SendMessage(WM_COMMAND,IDCANCEL); }
BOOL PreferencesPage::OnEraseBkgnd(CDC *dc) { CRect r;GetClientRect(r);dc->FillSolidRect(r,ClassicUI::Face);return TRUE; }
HBRUSH PreferencesPage::OnCtlColor(CDC *dc,CWnd *window,UINT type) {
    int id=window->GetDlgCtrlID();
    if(number==2 && id>=IDC_PREF_SWATCH_BG && id<=IDC_PREF_SWATCH_VISITED)
        return (HBRUSH)owner->swatches[id-IDC_PREF_SWATCH_BG].GetSafeHandle();
    return ClassicUI::ControlColor(dc,window,type);
}
void PreferencesPage::OnDrawItem(int id,LPDRAWITEMSTRUCT item) {
    if(number==2 && id>=IDC_PREF_SWATCH_BG && id<=IDC_PREF_SWATCH_VISITED) {
        CDC dc;dc.Attach(item->hDC);CRect area(item->rcItem);
        dc.FillSolidRect(area,owner->values.colors[id-IDC_PREF_SWATCH_BG]);
        dc.Draw3dRect(area,RGB(0,0,0),RGB(0,0,0));dc.Detach();return;
    }
    CDialog::OnDrawItem(id,item);
}

BOOL GeneralPreferencesDialog::OnInitDialog()
{
    ClassicDialog::OnInitDialog();
    CRect outer(0,0,ClassicUI::Scale(this,486),ClassicUI::Scale(this,403));
    AdjustWindowRectEx(&outer,GetStyle(),FALSE,GetExStyle());
    SetWindowPos(nullptr,0,0,outer.Width(),outer.Height(),SWP_NOMOVE|SWP_NOZORDER);
    tabs.SubclassDlgItem(IDC_PREF_TABS,this);
    tabs.MoveWindow(ClassicUI::Scale(this,7),ClassicUI::Scale(this,4),ClassicUI::Scale(this,473),ClassicUI::Scale(this,364));
    for(int i=0;i<8;i++) {
        tabs.InsertItem(i,preference_tabs[i]);
        pages[i].reset(new PreferencesPage(this,i)); pages[i]->Create(IDD_PREFERENCES_PAGE,this);
        pages[i]->MoveWindow(ClassicUI::Scale(this,16),ClassicUI::Scale(this,29),ClassicUI::Scale(this,453),ClassicUI::Scale(this,330));
    }
    GetDlgItem(IDOK)->MoveWindow(ClassicUI::Scale(this,244),ClassicUI::Scale(this,373),ClassicUI::Scale(this,75),ClassicUI::Scale(this,23));
    GetDlgItem(IDCANCEL)->MoveWindow(ClassicUI::Scale(this,325),ClassicUI::Scale(this,373),ClassicUI::Scale(this,75),ClassicUI::Scale(this,23));
    GetDlgItem(IDC_PREF_HELP)->MoveWindow(ClassicUI::Scale(this,406),ClassicUI::Scale(this,373),ClassicUI::Scale(this,75),ClassicUI::Scale(this,23));

    Group(0,"Toolbars",7,13,451,62);
    Label(0,"Show Main Toolbar as:",21,35,132);
    Radio(0,"Pictures",153,30,94,IDC_PREF_PICTURES,true);
    Radio(0,"Text",253,30,70,IDC_PREF_TEXT);
    Radio(0,"Pictures and Text",329,30,122,IDC_PREF_BOTH);
    Label(0,"(Character and paragraph formatting toolbars in Editor windows are always just pictures.)",24,57,421,14);
    Group(0,"Startup",7,94,451,114);
    Label(0,"On Startup Launch:",21,114,150);
    Tick(0,"Netscape Browser",129,130,120,IDC_PREF_BROWSER);
    Tick(0,"Netscape Mail",249,130,103,IDC_PREF_MAIL);
    Tick(0,"Netscape News",351,130,102,IDC_PREF_NEWS);
    Label(0,"Browser Starts With:",21,162,109);
    Radio(0,"Blank Page",129,156,110,IDC_PREF_BLANK,true);
    Radio(0,"Home Page Location:",249,156,183,IDC_PREF_HOME);
    Edit(0,values.home.c_str(),129,179,305,IDC_PREF_HOME_URL);
    Group(0,"Link Styles",7,229,451,93);
    Label(0,"Links are:",21,253,86);
    Tick(0,"Underlined",112,248,155,IDC_PREF_UNDERLINE);
    Label(0,"Followed Links:",21,286,94);
    Radio(0,"Never Expire",112,281,96,IDC_PREF_NEVER,true);
    Radio(0,"Expire After:",208,281,77,IDC_PREF_EXPIRE);
    char days[20]; sprintf(days,"%d",values.days); Edit(0,days,285,280,27,IDC_PREF_DAYS);
    Label(0,"Days",315,286,37); Button(0,"Expire Now",355,279,63,23,IDC_PREF_EXPIRE_NOW);
    Check(0,IDC_PREF_PICTURES+values.toolbar,true);
    Check(0,IDC_PREF_BROWSER,(values.startup&1)!=0); Check(0,IDC_PREF_MAIL,(values.startup&2)!=0); Check(0,IDC_PREF_NEWS,(values.startup&4)!=0);
    Check(0,values.blank?IDC_PREF_BLANK:IDC_PREF_HOME,true); Check(0,IDC_PREF_UNDERLINE,values.underline);
    Check(0,values.expire?IDC_PREF_EXPIRE:IDC_PREF_NEVER,true); UpdateEnabled();

    Group(1,"Fonts and Encodings",10,16,451,119);
    Label(1,"For the Encoding:",16,42,126);
    Add(1,"COMBOBOX","",CBS_DROPDOWNLIST|WS_TABSTOP|WS_VSCROLL,142,37,126,100,IDC_PREF_ENCODING);
    Control(1,IDC_PREF_ENCODING)->SendMessage(CB_ADDSTRING,0,(LPARAM)"Latin1");
    Control(1,IDC_PREF_ENCODING)->SendMessage(CB_SETCURSEL,0); Control(1,IDC_PREF_ENCODING)->EnableWindow(FALSE);
    Label(1,"Use the Proportional Font:",31,70,135); Label(1,values.proportional.c_str(),166,70,160,16,IDC_PREF_PROP_NAME);
    Label(1,"Use the Fixed Font:",31,96,135); Label(1,values.fixed.c_str(),166,96,160,16,IDC_PREF_FIXED_NAME);
    Button(1,"Choose Font...",331,60,83,23,IDC_PREF_PROP_FONT); Button(1,"Choose Font...",331,86,83,23,IDC_PREF_FIXED_FONT);

    Group(2,"Colors",10,16,451,288);
    Label(2,"Color is used to highlight some types of text.  Ideally, text colors are in good contrast\nwith the document background.",21,37,426,36);
    const char *color_labels[]={"Links:","Followed Links:","Text:"};
    const int color_indices[]={2,3,1};
    for(int i=0;i<3;i++) {
        int y=82+i*30, n=color_indices[i]; Label(2,color_labels[i],21,y+3,100);
        Tick(2,"Custom",121,y,65,IDC_PREF_LINK_CUSTOM+i);
        Add(2,"STATIC","",SS_OWNERDRAW,195,y+4,54,14,IDC_PREF_SWATCH_BG+n);
        Button(2,"Choose Color...",339,y,83,23,IDC_PREF_CHOOSE_BG+n);
    }
    Label(2,"Background:",21,177,101); Radio(2,"Default",121,171,75,IDC_PREF_BG_DEFAULT,true);
    Radio(2,"Custom",196,171,76,IDC_PREF_BG_CUSTOM);
    Add(2,"STATIC","",SS_OWNERDRAW,280,175,54,14,IDC_PREF_SWATCH_BG);
    Button(2,"Choose Color...",339,171,83,23,IDC_PREF_CHOOSE_BG);
    Radio(2,"Image File:",121,200,75,IDC_PREF_BG_IMAGE);
    Edit(2,values.background_file.c_str(),195,201,139,IDC_PREF_BG_FILE); Button(2,"Browse...",339,200,83,23,IDC_PREF_BG_BROWSE);
    Label(2,"Sometimes a document will provide its own colors and background.",17,247,429);
    Tick(2,"Always Use My Colors, Overriding Document",121,262,324,IDC_PREF_OVERRIDE);
    Check(2,IDC_PREF_LINK_CUSTOM,values.link_custom); Check(2,IDC_PREF_VISITED_CUSTOM,values.visited_custom);
    Check(2,IDC_PREF_TEXT_CUSTOM,values.text_custom); Check(2,values.background_image?IDC_PREF_BG_IMAGE:values.background_custom?IDC_PREF_BG_CUSTOM:IDC_PREF_BG_DEFAULT,true);
    UpdateBackground();
    Check(2,IDC_PREF_OVERRIDE,values.override_colors); SetSwatches();

    Group(3,"Images",10,16,449,150);
    Label(3,"These determine the methods Netscape uses to display images.",22,40,420);
    Label(3,"Choosing Colors:",22,66,103);
    Radio(3,"Automatic",124,61,155,IDC_PREF_AUTOMATIC,true); Radio(3,"Dither",124,82,155,IDC_PREF_DITHER);
    Radio(3,"Substitute Colors",124,103,179,IDC_PREF_SUBSTITUTE);
    Label(3,"Display Images:",22,138,103); Radio(3,"While Loading",124,133,126,IDC_PREF_WHILE_LOADING,true);
    Radio(3,"After Loading",251,133,165,IDC_PREF_AFTER_LOADING);
    Check(3,IDC_PREF_AUTOMATIC+values.dither,true); Check(3,values.incremental?IDC_PREF_WHILE_LOADING:IDC_PREF_AFTER_LOADING,true);

    Group(4,"Supporting Applications",10,16,449,190);
    const char *app_labels[]={"Telnet Application:","TN3270 Application:","View Source:","Temporary Directory:"};
    const char *app_values[]={values.telnet_application.c_str(),values.tn3270_application.c_str(),values.source_application.c_str(),
        values.temporary_directory.empty()?WFE_DefaultTemporaryDirectory():values.temporary_directory.c_str()};
    for(int i=0;i<4;i++) {
        int y=37+i*34; Label(4,app_labels[i],21,y+3,104);
        Edit(4,app_values[i],124,y,256,7500+i);
        if(i<3) Button(4,"Browse...",388,y,59,23,7510+i);
    }
    Label(4,"Leave View Source blank to use Netscape's source viewer.",21,179,420);

    Label(5,"File type",10,14,190); Label(5,"Action",216,14,115); Label(5,"Extensions",350,14,100);
    Add(5,WC_LISTVIEWA,"",LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS|LVS_NOCOLUMNHEADER|WS_TABSTOP|WS_BORDER,10,32,442,140,IDC_PREF_HELPERS,WS_EX_CLIENTEDGE);
    CListCtrl *helpers=(CListCtrl *)Control(5,IDC_PREF_HELPERS);
    helpers->InsertColumn(0,"File type",LVCFMT_LEFT,ClassicUI::Scale(this,205));
    helpers->InsertColumn(1,"Action",LVCFMT_LEFT,ClassicUI::Scale(this,115));
    helpers->InsertColumn(2,"Extensions",LVCFMT_LEFT,ClassicUI::Scale(this,104));
    helpers->SetBkColor(RGB(255,255,255)); helpers->SetTextBkColor(RGB(255,255,255));
    for(int i=0;i<6;i++) { helpers->InsertItem(i,helper_types[i].type);helpers->SetItemText(i,1,helper_types[i].action);helpers->SetItemText(i,2,helper_types[i].extensions); }
    Label(5,"File / MIME Type:",10,182,95); Label(5,"",105,182,205,16,IDC_PREF_HELPER_TYPE);
    Button(5,"Create New Type...",352,175,100,23,7620,false);
    Label(5,"Subtype:",10,204,95); Label(5,"",105,204,315,16,IDC_PREF_HELPER_SUBTYPE);
    Label(5,"File Extensions:",10,226,95); Edit(5,"",105,222,347,IDC_PREF_HELPER_EXT,false);
    Label(5,"Action:",10,252,95); Radio(5,"View in Browser",105,247,115,IDC_PREF_HELPER_BROWSER,true);
    Radio(5,"Save to Disk",218,247,112,IDC_PREF_HELPER_SAVE); Radio(5,"Unknown: Prompt User",326,247,138,7621);
    Radio(5,"Launch the Application:",105,269,205,7622); Edit(5,"",162,299,218,7623,false);
    Button(5,"Browse...",393,297,59,23,7624,false);
    Control(5,IDC_PREF_HELPER_BROWSER)->EnableWindow(FALSE); Control(5,IDC_PREF_HELPER_SAVE)->EnableWindow(FALSE);
    Control(5,7621)->EnableWindow(FALSE); Control(5,7622)->EnableWindow(FALSE);
    helpers->SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);UpdateHelper();

    Group(6,"Accept Language",10,18,445,300);
    Label(6,"Language/Region",34,56,170); Label(6,"User define:",274,47,87);
    Edit(6,"",358,42,87,IDC_PREF_LANG_CUSTOM); Button(6,"v",408,71,26,27,IDC_PREF_LANG_CUSTOM_ADD);
    Label(6,"Accept List:",273,96,170);
    Add(6,"LISTBOX","",LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP,27,94,170,204,IDC_PREF_AVAILABLE,WS_EX_CLIENTEDGE);
    Add(6,"LISTBOX","",LBS_NOTIFY|WS_VSCROLL|WS_BORDER|WS_TABSTOP,273,120,169,173,IDC_PREF_ACCEPTED,WS_EX_CLIENTEDGE);
    for(int i=0;i<18;i++) Control(6,IDC_PREF_AVAILABLE)->SendMessage(LB_ADDSTRING,0,(LPARAM)language_names[i]);
    Control(6,IDC_PREF_AVAILABLE)->SendMessage(LB_SETCURSEL,0);
    Button(6,">",214,142,26,26,IDC_PREF_LANG_ADD); Button(6,"<",214,206,26,26,IDC_PREF_LANG_REMOVE);
    RefreshAccepted();
    Group(7,"HTTPS",10,18,445,220);
    Tick(7,"Allow obsolete TLS 1.0 and TLS 1.1",24,46,409,IDC_PREF_LEGACY_TLS);
    Check(7,IDC_PREF_LEGACY_TLS,values.allow_legacy_tls);
    Label(7,"CA file (full absolute path; blank uses the supplied CA bundle):",24,91,410);
    Edit(7,values.ca_bundle.c_str(),24,113,317,IDC_PREF_CA_BUNDLE);
    Button(7,"Browse...",350,111,83,23,IDC_PREF_CA_BROWSE);
    Label(7,"CA file changes take effect after restarting Netscape.",24,150,410);
    Label(7,"Protocol changes apply to new connections. Certificates are always checked.",24,181,410,32);
    for(int i=0;i<8;i++) { ClassicUI::Apply(pages[i].get());pages[i]->ShowWindow(i==0?SW_SHOW:SW_HIDE); }
    ClassicUI::Apply(this); CenterWindow(); tabs.SetFocus(); return FALSE;
}

void GeneralPreferencesDialog::ChooseFont(bool fixed)
{
    LOGFONT font={};
    const std::string &face=fixed?values.fixed:values.proportional;
    strncpy(font.lfFaceName,face.c_str(),LF_FACESIZE-1);
    CClientDC dc(this);
    font.lfHeight=-MulDiv(fixed?values.fixed_size:values.proportional_size,dc.GetDeviceCaps(LOGPIXELSY),72);
    font.lfCharSet=ANSI_CHARSET;
    CFontDialog picker(&font,CF_SCREENFONTS|CF_INITTOLOGFONTSTRUCT|CF_FORCEFONTEXIST|CF_NOVERTFONTS|CF_LIMITSIZE|(fixed?CF_FIXEDPITCHONLY:0),nullptr,this);
    picker.m_cf.nSizeMin=6;picker.m_cf.nSizeMax=72;
    if(picker.DoModal()!=IDOK) return;
    if(fixed) { values.fixed=picker.GetFaceName();values.fixed_size=picker.GetSize()/10;SetText(1,IDC_PREF_FIXED_NAME,values.fixed.c_str()); }
    else { values.proportional=picker.GetFaceName();values.proportional_size=picker.GetSize()/10;SetText(1,IDC_PREF_PROP_NAME,values.proportional.c_str()); }
}

bool GeneralPreferencesDialog::PageCommand(int page,WPARAM w,LPARAM)
{
    int id=LOWORD(w), notification=HIWORD(w);
    if(page==0 && notification==BN_CLICKED) {
        if(id==IDC_PREF_BLANK || id==IDC_PREF_HOME || id==IDC_PREF_NEVER || id==IDC_PREF_EXPIRE) UpdateEnabled();
        if(id==IDC_PREF_EXPIRE_NOW) { expire_now=true; Control(0,id)->EnableWindow(FALSE); }
    }
    if(page==1 && notification==BN_CLICKED && (id==IDC_PREF_PROP_FONT || id==IDC_PREF_FIXED_FONT)) { ChooseFont(id==IDC_PREF_FIXED_FONT); return true; }
    if(page==4 && notification==BN_CLICKED && id>=7510 && id<=7512) {
        int field=7500+id-7510;
        CFileDialog picker(TRUE,"exe",Text(4,field),OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,
            "Windows applications|*.exe|All files|*.*||",this);
        if(picker.DoModal()==IDOK) SetText(4,field,picker.GetPathName());return true;
    }
    if(page==2 && notification==BN_CLICKED) {
        if(id==IDC_PREF_BG_IMAGE || id==IDC_PREF_BG_DEFAULT || id==IDC_PREF_BG_CUSTOM) UpdateBackground();
        if(id==IDC_PREF_BG_BROWSE) {
            CFileDialog picker(TRUE,nullptr,Text(2,IDC_PREF_BG_FILE),OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,
                "Images|*.gif;*.jpg;*.jpeg;*.jpe;*.xbm|All files|*.*||",this);
            if(picker.DoModal()==IDOK) SetText(2,IDC_PREF_BG_FILE,picker.GetPathName());return true;
        }
    }
    if(page==2 && notification==BN_CLICKED && id>=IDC_PREF_CHOOSE_BG && id<=IDC_PREF_CHOOSE_VISITED) {
        int n=id-IDC_PREF_CHOOSE_BG; CColorDialog picker(values.colors[n],CC_FULLOPEN,this);
        if(picker.DoModal()==IDOK) { values.colors[n]=picker.GetColor();SetSwatches(); }
        return true;
    }
    if(page==7 && notification==BN_CLICKED && id==IDC_PREF_CA_BROWSE) {
        CFileDialog picker(TRUE,"pem",Text(7,IDC_PREF_CA_BUNDLE),OFN_FILEMUSTEXIST|OFN_HIDEREADONLY|OFN_NOCHANGEDIR,
            "CA bundles|*.pem;*.crt|All files|*.*||",this);
        if(picker.DoModal()==IDOK) SetText(7,IDC_PREF_CA_BUNDLE,picker.GetPathName());return true;
    }
    if(page==6) {
        if(id==IDC_PREF_LANG_ADD || (id==IDC_PREF_AVAILABLE && notification==LBN_DBLCLK)) {
            int index=(int)Control(6,IDC_PREF_AVAILABLE)->SendMessage(LB_GETCURSEL);
            if(index>=0 && index<18) {
                bool duplicate=false; for(const auto &tag:values.languages) if(!_stricmp(tag.c_str(),language_tags[index])) duplicate=true;
                if(!duplicate) { values.languages.push_back(language_tags[index]);RefreshAccepted(); }
            }
            return true;
        }
        if(id==IDC_PREF_LANG_REMOVE || (id==IDC_PREF_ACCEPTED && notification==LBN_DBLCLK)) {
            int index=(int)Control(6,IDC_PREF_ACCEPTED)->SendMessage(LB_GETCURSEL);
            if(index>=0 && index<(int)values.languages.size()) { values.languages.erase(values.languages.begin()+index);RefreshAccepted(); }
            return true;
        }
        if(id==IDC_PREF_LANG_CUSTOM_ADD) {
            CString token=Text(6,IDC_PREF_LANG_CUSTOM); token.TrimLeft();token.TrimRight();
            GeneralPreferences proposed=WFE_GeneralPreferences();
            proposed.languages=values.languages;proposed.languages.push_back((const char *)token);
            std::string problem;
            if(WFE_ValidateGeneralPreferences(proposed,&problem)) { values.languages=proposed.languages;RefreshAccepted();SetText(6,IDC_PREF_LANG_CUSTOM,""); }
            else { ShowError(problem);Control(6,IDC_PREF_LANG_CUSTOM)->SetFocus(); }
            return true;
        }
    }
    return false;
}

BOOL GeneralPreferencesDialog::PreTranslateMessage(MSG *message)
{
    if(message->message==WM_KEYDOWN) {
        CWnd *focus=GetFocus(); HWND handle=focus?focus->GetSafeHwnd():nullptr;
        if(message->wParam==VK_ESCAPE) { SendMessage(WM_COMMAND,IDCANCEL);return TRUE; }
        if((message->wParam==VK_TAB && (GetKeyState(VK_CONTROL)&0x8000)) ||
           (handle==tabs.GetSafeHwnd() && (message->wParam==VK_LEFT || message->wParam==VK_RIGHT))) {
            bool previous=message->wParam==VK_LEFT || (message->wParam==VK_TAB && (GetKeyState(VK_SHIFT)&0x8000));
            SelectPage((current+(previous?7:1))%8);tabs.SetFocus();return TRUE;
        }
        if(message->wParam==VK_RETURN) {
            if(handle==Control(6,IDC_PREF_LANG_CUSTOM)->GetSafeHwnd()) { Control(6,IDC_PREF_LANG_CUSTOM_ADD)->SendMessage(BM_CLICK);return TRUE; }
            char name[32]={}; if(handle) ::GetClassNameA(handle,name,sizeof(name));
            if(!_stricmp(name,"Button")) {
                DWORD kind=(DWORD)::GetWindowLongPtr(handle,GWL_STYLE)&BS_TYPEMASK;
                if(kind==BS_PUSHBUTTON || kind==BS_DEFPUSHBUTTON || kind==BS_OWNERDRAW) { ::SendMessage(handle,BM_CLICK,0,0);return TRUE; }
            }
            SendMessage(WM_COMMAND,IDOK);return TRUE;
        }
    }
    /* ClassicDialog forces every Enter key to OK; use native dialog navigation. */
    return CDialog::PreTranslateMessage(message);
}

bool GeneralPreferencesDialog::ReadControls()
{
    values.toolbar=Checked(0,IDC_PREF_PICTURES)?0:Checked(0,IDC_PREF_TEXT)?1:2;
    values.startup=(Checked(0,IDC_PREF_BROWSER)?1:0)|(Checked(0,IDC_PREF_MAIL)?2:0)|(Checked(0,IDC_PREF_NEWS)?4:0);
    values.blank=Checked(0,IDC_PREF_BLANK); values.home=(const char *)Text(0,IDC_PREF_HOME_URL);
    values.underline=Checked(0,IDC_PREF_UNDERLINE); values.expire=Checked(0,IDC_PREF_EXPIRE);
    CString text=Text(0,IDC_PREF_DAYS); char *end=nullptr; long days=strtol(text,&end,10);
    if(text.IsEmpty() || !end || *end || days<1 || days>24855) {
        SelectPage(0);ShowError("Enter a number of days between 1 and 24855.");Control(0,IDC_PREF_DAYS)->SetFocus();return false;
    }
    values.days=(int)days;
    values.link_custom=Checked(2,IDC_PREF_LINK_CUSTOM);values.visited_custom=Checked(2,IDC_PREF_VISITED_CUSTOM);
    values.text_custom=Checked(2,IDC_PREF_TEXT_CUSTOM);values.background_custom=Checked(2,IDC_PREF_BG_CUSTOM);
    values.override_colors=Checked(2,IDC_PREF_OVERRIDE);
    values.background_image=Checked(2,IDC_PREF_BG_IMAGE);values.background_file=(const char *)Text(2,IDC_PREF_BG_FILE);
    values.dither=Checked(3,IDC_PREF_AUTOMATIC)?0:Checked(3,IDC_PREF_DITHER)?1:2;
    values.incremental=Checked(3,IDC_PREF_WHILE_LOADING);
    values.allow_legacy_tls=Checked(7,IDC_PREF_LEGACY_TLS);
    values.ca_bundle=(const char *)Text(7,IDC_PREF_CA_BUNDLE);
    values.telnet_application=(const char *)Text(4,7500);values.tn3270_application=(const char *)Text(4,7501);
    values.source_application=(const char *)Text(4,7502);values.temporary_directory=(const char *)Text(4,7503);
    if(!_stricmp(values.temporary_directory.c_str(),WFE_DefaultTemporaryDirectory())) values.temporary_directory.clear();
    CString pending=Text(6,IDC_PREF_LANG_CUSTOM);pending.TrimLeft();pending.TrimRight();
    if(!pending.IsEmpty()) {
        SelectPage(6);ShowError("Add the language to the Accept List or clear User define before choosing OK.");Control(6,IDC_PREF_LANG_CUSTOM)->SetFocus();return false;
    }
    return true;
}
void GeneralPreferencesDialog::OnOK()
{
    error.clear(); if(!ReadControls()) return;
    std::string problem;
    if(!WFE_ValidateGeneralPreferences(values,&problem) || !WFE_SaveGeneralPreferences(values,&problem)) { ShowError(problem);return; }
    if(expire_now) GH_ClearGlobalHistory();
    accepted=true; if(testing) ShowWindow(SW_HIDE); else ClassicDialog::OnOK();
}

int WFE_ShowGeneralPreferences(CWnd *parent)
{
    GeneralPreferencesDialog dialog(parent); return (int)dialog.DoModal();
}

static bool preferences_capture(CWnd *dialog,const char *path)
{
    if(!path || !*path) return false;
    CRect area;dialog->GetWindowRect(area);
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=area.Width();info.bmiHeader.biHeight=-area.Height();
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    CClientDC screen(dialog);CDC memory;memory.CreateCompatibleDC(&screen);
    void *pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap) return false;
    HGDIOBJ old=SelectObject(memory,bitmap);
    dialog->RedrawWindow(nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW|RDW_FRAME);
    bool captured=PrintWindow(dialog->GetSafeHwnd(),memory,0)!=FALSE;
    BITMAPFILEHEADER header={};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(info.bmiHeader);
    DWORD bytes=area.Width()*area.Height()*4;header.bfSize=header.bfOffBits+bytes;
    FILE *file=fopen(path,"wb");bool saved=false;
    if(file) { saved=fwrite(&header,sizeof(header),1,file)==1 && fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1 && fwrite(pixels,bytes,1,file)==1;saved=fclose(file)==0 && saved; }
    SelectObject(memory,old);DeleteObject(bitmap);return captured && saved;
}

static bool same_preferences(const GeneralPreferences &a,const GeneralPreferences &b)
{
    return a.allow_legacy_tls==b.allow_legacy_tls && a.ca_bundle==b.ca_bundle && a.toolbar==b.toolbar && a.startup==b.startup && a.blank==b.blank && a.underline==b.underline &&
        a.expire==b.expire && a.days==b.days && a.home==b.home && a.proportional==b.proportional && a.fixed==b.fixed &&
        a.proportional_size==b.proportional_size && a.fixed_size==b.fixed_size &&
        a.text_custom==b.text_custom && a.link_custom==b.link_custom && a.visited_custom==b.visited_custom &&
        a.background_custom==b.background_custom && a.override_colors==b.override_colors &&
        a.background_image==b.background_image && a.background_file==b.background_file &&
        a.telnet_application==b.telnet_application && a.tn3270_application==b.tn3270_application &&
        a.source_application==b.source_application && a.temporary_directory==b.temporary_directory &&
        !memcmp(a.colors,b.colors,sizeof(a.colors)) && a.dither==b.dither && a.incremental==b.incremental && a.languages==b.languages;
}

static bool preferences_file(const std::string &path,std::string &bytes)
{
    FILE *file=fopen(path.c_str(),"rb");bytes.clear();if(!file) return false;
    char buffer[4096];size_t length;
    while((length=fread(buffer,1,sizeof(buffer),file))!=0) bytes.append(buffer,length);
    bool ok=ferror(file)==0;return fclose(file)==0 && ok;
}

int WFE_PreferencesUISmoke(CWnd *parent,const char *report,const char *snapshot)
{
    GeneralPreferences before=WFE_GeneralPreferences();
    std::string path=std::string(WFE_ProfileDirectory())+"\\browser.ini", disk_before,disk_after;
    bool existed=preferences_file(path,disk_before);
    GeneralPreferencesDialog cancel(parent,true);
    if(!cancel.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
    cancel.ShowWindow(SW_SHOW);cancel.UpdateWindow();
    bool captured=preferences_capture(&cancel,snapshot), labels=cancel.tabs.GetItemCount()==8, bounds=true;
    CRect client;cancel.GetClientRect(client);
    CRect original;cancel.GetWindowRect(original);
    for(int i=0;i<8;i++) {
        TCITEM item={};char text[64]={};item.mask=TCIF_TEXT;item.pszText=text;item.cchTextMax=sizeof(text);
        labels=labels && cancel.tabs.GetItem(i,&item) && !strcmp(text,preference_tabs[i]);
        cancel.SelectPage(i);CRect page;cancel.pages[i]->GetWindowRect(page);cancel.ScreenToClient(page);
        bounds=bounds && client.PtInRect(page.TopLeft()) && client.PtInRect(CPoint(page.right-1,page.bottom-1));
        CRect page_client;cancel.pages[i]->GetClientRect(page_client);
        for(CWnd *child=cancel.pages[i]->GetWindow(GW_CHILD);child;child=child->GetWindow(GW_HWNDNEXT)) {
            CRect control;child->GetWindowRect(control);cancel.pages[i]->ScreenToClient(control);
            bounds=bounds && page_client.PtInRect(control.TopLeft()) && page_client.PtInRect(CPoint(control.right-1,control.bottom-1));
        }
    }
    cancel.SelectPage(7);
    bool tls_controls=cancel.Control(7,IDC_PREF_CA_BUNDLE)->IsWindowVisible() &&
        cancel.Control(7,IDC_PREF_CA_BUNDLE)->IsWindowEnabled() &&
        cancel.Text(7,IDC_PREF_LEGACY_TLS)=="Allow obsolete TLS 1.0 and TLS 1.1";
    tls_controls=tls_controls && preferences_capture(&cancel,(std::string(snapshot)+".security.bmp").c_str());
    cancel.Check(7,IDC_PREF_LEGACY_TLS,!before.allow_legacy_tls);
    cancel.SetText(7,IDC_PREF_CA_BUNDLE,"must-not-apply.pem");
    cancel.SelectPage(0);
    bool appearance=cancel.Control(0,IDC_PREF_BOTH)->IsWindowVisible() && cancel.Control(0,IDC_PREF_EXPIRE_NOW)->IsWindowVisible();
    cancel.GetDlgItem(IDC_PREF_HELP)->SetFocus();
    MSG key={};key.hwnd=cancel.GetSafeHwnd();key.message=WM_KEYDOWN;key.wParam=VK_RETURN;
    cancel.PreTranslateMessage(&key);
    bool keyboard=cancel.help_count==1 && !cancel.accepted && cancel.IsWindowVisible();
    cancel.tabs.SetFocus();key.wParam=VK_RIGHT;cancel.PreTranslateMessage(&key);keyboard=keyboard && cancel.current==1;
    key.hwnd=cancel.tabs.GetSafeHwnd();key.wParam=VK_TAB;cancel.PreTranslateMessage(&key);
    keyboard=keyboard && ::GetFocus()!=cancel.tabs.GetSafeHwnd() && ::IsChild(cancel.GetSafeHwnd(),::GetFocus());
    cancel.SelectPage(0);cancel.Control(0,IDC_PREF_TEXT)->SendMessage(BM_CLICK);
    cancel.SetText(0,IDC_PREF_HOME_URL,"about:cancelled");
    cancel.Control(0,IDC_PREF_EXPIRE_NOW)->SendMessage(BM_CLICK);
    key.wParam=VK_ESCAPE;cancel.PreTranslateMessage(&key);
    bool exists_after=preferences_file(path,disk_after);
    bool cancelled=cancel.rejected && !cancel.accepted && same_preferences(before,WFE_GeneralPreferences()) &&
        existed==exists_after && disk_before==disk_after;
    cancel.DestroyWindow();

    GeneralPreferencesDialog save(parent,true);
    if(!save.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
    save.ShowWindow(SW_SHOW);
    save.SetText(0,IDC_PREF_DAYS,"not-a-number");save.SendMessage(WM_COMMAND,IDOK);
    bool validation=!save.accepted && save.IsWindowVisible() && !save.error.empty() && same_preferences(before,WFE_GeneralPreferences());
    save.SetText(0,IDC_PREF_DAYS,"17");
    save.Check(0,IDC_PREF_BROWSER,false);save.Check(0,IDC_PREF_MAIL,false);save.Check(0,IDC_PREF_NEWS,false);
    save.SendMessage(WM_COMMAND,IDOK);validation=validation && !save.accepted && !save.error.empty();
    save.Check(0,IDC_PREF_BROWSER,true);
    save.SelectPage(6);save.SetText(6,IDC_PREF_LANG_CUSTOM,"en\r\nInjected");
    save.Control(6,IDC_PREF_LANG_CUSTOM_ADD)->SendMessage(BM_CLICK);
    validation=validation && !save.accepted && !save.error.empty() && save.values.languages==before.languages;
    save.SetText(6,IDC_PREF_LANG_CUSTOM,"");
    while(save.Control(6,IDC_PREF_ACCEPTED)->SendMessage(LB_GETCOUNT)>0) {
        save.Control(6,IDC_PREF_ACCEPTED)->SendMessage(LB_SETCURSEL,0);save.Control(6,IDC_PREF_LANG_REMOVE)->SendMessage(BM_CLICK);
    }
    save.Control(6,IDC_PREF_AVAILABLE)->SendMessage(LB_SETCURSEL,7);save.Control(6,IDC_PREF_LANG_ADD)->SendMessage(BM_CLICK);
    save.Control(6,IDC_PREF_AVAILABLE)->SendMessage(LB_SETCURSEL,4);save.Control(6,IDC_PREF_LANG_ADD)->SendMessage(BM_CLICK);
    save.SelectPage(0);save.Control(0,IDC_PREF_TEXT)->SendMessage(BM_CLICK);save.Control(0,IDC_PREF_BLANK)->SendMessage(BM_CLICK);
    save.Control(0,IDC_PREF_EXPIRE)->SendMessage(BM_CLICK);save.Check(0,IDC_PREF_UNDERLINE,false);save.SetText(0,IDC_PREF_HOME_URL,"about:blank");
    save.SelectPage(3);save.Control(3,IDC_PREF_AFTER_LOADING)->SendMessage(BM_CLICK);save.SelectPage(0);save.SelectPage(3);
    bool retained=save.Checked(3,IDC_PREF_AFTER_LOADING);save.Control(3,IDC_PREF_WHILE_LOADING)->SendMessage(BM_CLICK);
    /* A real destination handle without delete sharing makes atomic replacement fail. */
    if(GetFileAttributesA(path.c_str())==INVALID_FILE_ATTRIBUTES) {
        std::string problem;WFE_SaveGeneralPreferences(before,&problem);
    }
    save.Check(7,IDC_PREF_LEGACY_TLS,!before.allow_legacy_tls);
    preferences_file(path,disk_before);
    HANDLE locked=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    bool save_failure=false;
    if(locked!=INVALID_HANDLE_VALUE) {
        save.SendMessage(WM_COMMAND,IDOK);
        save_failure=!save.accepted && save.IsWindowVisible() && !save.error.empty() && same_preferences(before,WFE_GeneralPreferences());
        CloseHandle(locked);
        save_failure=save_failure && preferences_file(path,disk_after) && disk_before==disk_after;
    }
    save.Check(7,IDC_PREF_LEGACY_TLS,true);
    save.SendMessage(WM_COMMAND,IDOK);
    const GeneralPreferences &result=WFE_GeneralPreferences();
    bool saved=save.accepted && result.allow_legacy_tls && result.ca_bundle.empty() && result.toolbar==1 && result.startup==1 && result.blank && !result.underline && result.days==17 &&
        result.languages.size()==2 && result.languages[0]=="fr" && result.languages[1]=="en";
    std::string last_error=save.error;save.DestroyWindow();
    GeneralPreferencesDialog reopen(parent,true);bool reopened=reopen.Create(IDD_GENERAL_PREFERENCES,parent)!=FALSE;
    if(reopened) { reopened=reopen.Checked(7,IDC_PREF_LEGACY_TLS) && reopen.Text(7,IDC_PREF_CA_BUNDLE).IsEmpty() && reopen.Checked(0,IDC_PREF_TEXT) && reopen.Checked(0,IDC_PREF_BLANK) && reopen.Text(0,IDC_PREF_DAYS)=="17" && reopen.values.languages==result.languages;reopen.DestroyWindow(); }
    bool menu=parent && parent->GetMenu() && parent->GetMenu()->GetMenuState(33030,MF_BYCOMMAND)!=(UINT)-1;
    FILE *output=fopen(report,"wb");if(!output) return 2;
    fprintf(output,"tls_controls=%d\nmenu=%d\ndialog=1\ntabs=%d\nappearance=%d\nlayout=%d\nkeyboard=%d\ncancel=%d\nvalidation=%d\npage_retention=%d\nsave_failure=%d\nsave=%d\nreopen=%d\ncapture=%d\nwidth=%d\nheight=%d\nerror=%s\n",
        tls_controls,menu,labels,appearance,bounds,keyboard,cancelled,validation,retained,save_failure,saved,reopened,captured,original.Width(),original.Height(),last_error.c_str());
    fclose(output);
    return tls_controls && menu && labels && appearance && bounds && keyboard && cancelled && validation && retained && save_failure && saved && reopened && captured ? 0 : 1;
}

#include "preferences-background-smoke.inc"

#include "preferences-apps-smoke.inc"

/* Focused TLS scenarios use the same native controls and Save/Cancel handlers. */
int WFE_TLSPreferencesUISmoke(CWnd *parent,const char *action,const char *bundle)
{
    const GeneralPreferences before=WFE_GeneralPreferences();
    std::string path=std::string(WFE_ProfileDirectory())+"\\browser.ini",old_bytes,new_bytes;
    if(!preferences_file(path,old_bytes)) return 0;
    GeneralPreferencesDialog dialog(parent,true);
    if(!dialog.Create(IDD_GENERAL_PREFERENCES,parent)) return 0;
    dialog.SelectPage(7);dialog.ShowWindow(SW_SHOW);dialog.UpdateWindow();
    bool ca=!strcmp(action,"policy-ca");
    bool failed=!strcmp(action,"policy-failed"),cancel=!strcmp(action,"policy-cancel");
    dialog.Check(7,IDC_PREF_LEGACY_TLS,ca ? !before.allow_legacy_tls : false);
    if(ca || failed || cancel) dialog.SetText(7,IDC_PREF_CA_BUNDLE,bundle);
    if(!strcmp(action,"policy-invalid")) dialog.SetText(7,IDC_PREF_CA_BUNDLE,path.c_str());
    HANDLE locked=INVALID_HANDLE_VALUE;
    if(failed) locked=CreateFileA(path.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,
        nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);
    dialog.SendMessage(WM_COMMAND,cancel ? IDCANCEL : IDOK);
    if(locked!=INVALID_HANDLE_VALUE) CloseHandle(locked);
    bool ok;
    if(cancel || failed || !strcmp(action,"policy-invalid")) {
        ok=!dialog.accepted && (cancel ? dialog.rejected : !dialog.error.empty()) &&
            (!failed || locked!=INVALID_HANDLE_VALUE) && same_preferences(before,WFE_GeneralPreferences()) &&
            preferences_file(path,new_bytes) && old_bytes==new_bytes;
    } else {
        const GeneralPreferences &after=WFE_GeneralPreferences();
        ok=dialog.accepted && after.allow_legacy_tls==(ca ? !before.allow_legacy_tls : false) &&
            after.ca_bundle==(ca ? bundle : before.ca_bundle);
    }
    dialog.DestroyWindow();return ok ? 1 : 0;
}
