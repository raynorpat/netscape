#include "chrome.h"
#include "resource.h"
#include "preferences.h"
#include <algorithm>

struct ChromeCommand { UINT id; const char *label; UINT bitmap; const char *tip; };
static const ChromeCommand browser_tools[] = {
    {ID_NAV_BACK,"Back",IDB_CLASSIC_BACK,"Back one page"},
    {ID_NAV_FORWARD,"Forward",IDB_CLASSIC_FORWARD,"Forward one page"},
    {ID_NAV_HOME,"Home",IDB_CLASSIC_HOME,"Open the home page"},
    {ID_NAV_RELOAD,"Reload",IDB_CLASSIC_RELOAD,"Reload the current page"},
    {ID_BROWSER_IMAGES,"Images",IDB_CLASSIC_IMAGES,"Images are loaded automatically"},
    {ID_BROWSER_OPEN,"Open",IDB_CLASSIC_OPEN,"Open a local document"},
    {ID_BROWSER_PRINT,"Print",IDB_CLASSIC_PRINT,"Native printing is not yet available"},
    {ID_BROWSER_FIND,"Find",IDB_CLASSIC_FIND,"Find text in the document"},
    {ID_NAV_STOP,"Stop",IDB_CLASSIC_STOP,"Stop loading"}
};
static const ChromeCommand mail_tools[] = {
    {ID_MESSAGE_GET,"Get Mail",IDB_CLASSIC_GETMAIL,"Get new mail / news"},
    {ID_MESSAGE_DELETE,"Delete",IDB_CLASSIC_DELETE,"Delete this message"},
    {ID_CLIENT_COMPOSE,"To:Mail",IDB_CLASSIC_COMPOSE,"Compose a message"},
    {ID_MESSAGE_REPLY,"Re:Mail",IDB_CLASSIC_REPLY,"Reply to sender"},
    {ID_MESSAGE_REPLY_ALL,"Re:All",IDB_CLASSIC_REPLYALL,"Reply to all recipients"},
    {ID_MESSAGE_FORWARD,"Forward",IDB_CLASSIC_FORWARDMSG,"Forward this message"},
    {ID_MESSAGE_PREVIOUS,"Previous",IDB_CLASSIC_PREVIOUS,"Previous message"},
    {ID_MESSAGE_NEXT,"Next",IDB_CLASSIC_NEXT,"Next message"},
    {ID_BROWSER_PRINT,"Print",IDB_CLASSIC_PRINT,"Native printing is not yet available"},
    {ID_NAV_STOP,"Stop",IDB_CLASSIC_STOP,"Stop loading"}
};
static const ChromeCommand news_tools[] = {
    {ID_CLIENT_POST,"To:News",IDB_CLASSIC_POST,"Post a new article"},
    {ID_NEWS_MAIL_NEW,"To:Mail",IDB_CLASSIC_COMPOSE,"Compose a mail message"},
    {ID_MESSAGE_REPLY,"Re:Mail",IDB_CLASSIC_REPLY,"Reply to the sender by mail"},
    {ID_NEWS_REPLY_POST,"Re:News",IDB_CLASSIC_POST_REPLY,"Reply to the newsgroup"},
    {ID_NEWS_REPLY_BOTH,"Re:Both",IDB_CLASSIC_POST_BOTH,"Reply by news and mail"},
    {ID_MESSAGE_FORWARD,"Forward",IDB_CLASSIC_FORWARDMSG,"Forward this article by mail"},
    {ID_MESSAGE_PREVIOUS,"Previous",IDB_CLASSIC_PREVIOUS,"Previous unread article"},
    {ID_MESSAGE_NEXT,"Next",IDB_CLASSIC_NEXT,"Next unread article"},
    {ID_NEWS_MARK_THREAD,"Thread",IDB_CLASSIC_MARK_THREAD,"Mark this thread read"},
    {ID_NEWS_MARK_GROUP,"Group",IDB_CLASSIC_MARK_GROUP,"Mark this group read"},
    {ID_BROWSER_PRINT,"Print",IDB_CLASSIC_PRINT,"Native printing is not yet available"},
    {ID_NAV_STOP,"Stop",IDB_CLASSIC_STOP,"Stop loading"}
};
static const char *directory_labels[] = {"What's New?","What's Cool?","Destinations","Net Search","People","Software"};

BEGIN_MESSAGE_MAP(ClassicChrome, CWnd)
 ON_WM_ERASEBKGND()
 ON_WM_CTLCOLOR()
END_MESSAGE_MAP()

bool ClassicChrome::Create(CWnd *owner, CGenericEdit *address, ClassicButton *go_button, int window_mode)
{
    mode = window_mode; edit = address; go = go_button;
    if(mode) { location_visible=false;directory_visible=false; }
    if (!CreateEx(0, AfxRegisterWndClass(0,LoadCursor(nullptr,IDC_ARROW),nullptr), "Navigation toolbar",
                   WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS, CRect(0,0,1,1),owner,ID_CLASSIC_CHROME)) return false;
    tips.Create(this, TTS_ALWAYSTIP); tips.Activate(TRUE);
    const ChromeCommand *commands = mode==2 ? news_tools : mode ? mail_tools : browser_tools;
    size_t count = mode==2 ? _countof(news_tools) : mode ? _countof(mail_tools) : _countof(browser_tools);
    for (size_t i=0; i<count; ++i) {
        std::unique_ptr<ClassicButton> button(new ClassicButton);
        if (!button->Create(commands[i].label,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,CRect(0,0,1,1),this,commands[i].id)) return false;
        button->SetBitmapResource(commands[i].bitmap);
        tips.AddTool(button.get(),commands[i].tip);
        buttons.push_back(std::move(button));
    }
    if (!label.Create(mode == 1 ? "Mail folder:" : mode == 2 ? "News server:" : "Location:",
                       WS_CHILD|WS_VISIBLE|SS_CENTERIMAGE,CRect(0,0,1,1),this)) return false;
    if (!location.Create(WS_CHILD|WS_VISIBLE|WS_TABSTOP|WS_VSCROLL|CBS_DROPDOWN|CBS_AUTOHSCROLL,
                           CRect(0,0,1,200),this,ID_LOCATION_COMBO)) return false;
    COMBOBOXINFO info = {sizeof(info)};
    if (!::GetComboBoxInfo(location.GetSafeHwnd(),&info) || !edit->SubclassWindow(info.hwndItem)) return false;
    edit->SetDlgCtrlID(ID_ADDRESS);
    edit->LimitText(32767);
    if (!go->Create(mode == 2 ? "Connect" : "Refresh",WS_CHILD|WS_TABSTOP|BS_OWNERDRAW,CRect(0,0,1,1),this,ID_NAV_GO)) return false;
    if (mode) go->ShowWindow(SW_SHOW);
    if (!logo.Create("Netscape",WS_CHILD|WS_VISIBLE,CRect(0,0,30,30),this)) return false;
    logo.bitmap.LoadBitmap(IDB_CLASSIC_LOGO);
    size_t shortcut_count = mode ? 3 : 6;
    const char *mail_labels[] = {"All Messages","Unread","Account..."};
    const UINT mail_ids[] = {ID_MESSAGE_SHOW_ALL,ID_MESSAGE_SHOW_UNREAD,ID_MAIL_SETTINGS};
    for (size_t i=0; i<shortcut_count; ++i) {
        std::unique_ptr<ClassicButton> button(new ClassicButton);
        if (!button->Create(mode ? mail_labels[i] : directory_labels[i],WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                            CRect(0,0,1,1),this,mode ? mail_ids[i] : ID_DIRECTORY_FIRST+(UINT)i)) return false;
        shortcuts.push_back(std::move(button));
    }
    ClassicUI::Apply(this);
    SetPresentation(WFE_GeneralPreferences().toolbar);
    return true;
}

int ClassicChrome::Arrange(int width)
{
    if(!toolbar_visible && !location_visible && !directory_visible) {ShowWindow(SW_HIDE);return 0;}
    ShowWindow(SW_SHOW);
    auto s = [this](int n){ return ClassicUI::Scale(this,n); };
    int y=s(3), button_width=s(presentation==0 ? 30 : mode ? 42 : 48), button_height=s(presentation==0 ? 26 : presentation==1 ? 24 : 42);
    int x=s(4), row=0, limit=(std::max)(button_width+s(4),width-s(mode?40:4));
    for (size_t i=0;i<buttons.size();++i) {
        bool group=mode==2?(i==2||i==6||i==8||i==10):mode==1?(i==1||i==3||i==6||i==8):(i==3||i==8);
        if(group && presentation==2) x+=s(mode==2?4:mode==1?8:12);
        if(x+button_width>limit) {x=s(4);++row;}
        buttons[i]->ShowWindow(toolbar_visible ? SW_SHOW : SW_HIDE);
        buttons[i]->MoveWindow(x,y+row*button_height,button_width-s(2),button_height-s(2));
        x+=button_width;
    }
    if (toolbar_visible) y += (row+1)*button_height+s(3);
    int logo_top=mode ? s(3) : y, logo_size=s(mode ? 32 : 44), right=(std::max)(s(100),width-logo_size-s(5));
    label.ShowWindow(location_visible ? SW_SHOW : SW_HIDE);
    location.ShowWindow(location_visible ? SW_SHOW : SW_HIDE);
    go->ShowWindow(location_visible && mode ? SW_SHOW : SW_HIDE);
    if (location_visible) {
        int label_width=s(mode ? 68 : 57), go_width=mode ? s(66) : 0;
        label.MoveWindow(s(5),y,label_width,s(21));
        location.MoveWindow(label_width+s(5),y,(std::max)(s(20),right-label_width-s(8)-go_width),s(180));
        if(mode) go->MoveWindow(right-go_width,y,go_width-s(2),s(21));
        y += s(23);
    }
    int short_columns=(std::max)(1,(right-s(8))/s(76));
    for (size_t i=0;i<shortcuts.size();++i) {
        shortcuts[i]->ShowWindow(directory_visible ? SW_SHOW : SW_HIDE);
        shortcuts[i]->MoveWindow(s(5)+(int(i)%short_columns)*s(76),y+(int(i)/short_columns)*s(20),s(75),s(19));
    }
    if (directory_visible) y += ((int(shortcuts.size())+short_columns-1)/short_columns)*s(20);
    bool show_logo=mode ? toolbar_visible : location_visible || directory_visible;
    logo.ShowWindow(show_logo ? SW_SHOW : SW_HIDE);
    if(show_logo) {
        logo.MoveWindow(width-logo_size-s(4),logo_top,logo_size,logo_size);
        y=(std::max)(y,logo_top+logo_size);
    }
    y+=s(3);
    MoveWindow(0,0,(std::max)(1,width),y);
    Invalidate();
    return y;
}

void ClassicChrome::SetPresentation(int style)
{
    presentation=style>=0&&style<=2?style:2;
    for(auto &button:buttons) button->SetPresentation(presentation);
}
void ClassicChrome::UpdateCommands()
{
    CCmdUI command;
    command.m_pOther = nullptr;
    for(auto &button:buttons) {
        command.m_nID=button->GetDlgCtrlID(); command.m_pOther=button.get(); command.m_bEnableChanged=FALSE;
        GetParent()->OnCmdMsg(command.m_nID,CN_UPDATE_COMMAND_UI,&command,nullptr);
    }
}
void ClassicChrome::Remember(const char *address)
{
    if(!address || !*address || !location.GetSafeHwnd()) return;
    int existing=location.FindStringExact(-1,address);
    if(existing>=0) location.DeleteString(existing);
    location.InsertString(0,address);
    while(location.GetCount()>32) location.DeleteString(32);
}
CString ClassicChrome::Labels() const
{
    CString result;
    for(auto &button:buttons) { CString text; button->GetWindowText(text); if(!result.IsEmpty()) result+='|';result+=text; }
    return result;
}
CWnd *ClassicChrome::Button(UINT command) { for(auto &b:buttons) if((UINT)b->GetDlgCtrlID()==command) return b.get();return nullptr; }
BOOL ClassicChrome::OnCommand(WPARAM command, LPARAM control)
{
    if(LOWORD(command)==ID_LOCATION_COMBO) {
        if(HIWORD(command)==CBN_SELENDOK) {
            int row=location.GetCurSel();
            if(row>=0) { CString text; location.GetLBText(row,text); edit->SetWindowText(text);GetParent()->SendMessage(WM_COMMAND,ID_NAV_GO); }
        }
        return TRUE;
    }
    return (BOOL)GetParent()->SendMessage(WM_COMMAND,command,control);
}
void ClassicChrome::FocusBar(bool backwards)
{
    std::vector<CWnd*> order;
    for(auto &button:buttons) if(button->IsWindowVisible()&&button->IsWindowEnabled()) order.push_back(button.get());
    if(location.IsWindowVisible()) order.push_back(edit);
    if(go->IsWindowVisible()&&go->IsWindowEnabled()) order.push_back(go);
    for(auto &button:shortcuts) if(button->IsWindowVisible()&&button->IsWindowEnabled()) order.push_back(button.get());
    if(order.empty()) return;
    HWND focused=::GetFocus();
    auto current=std::find_if(order.begin(),order.end(),[focused](CWnd *window){return window->GetSafeHwnd()==focused;});
    if(current==order.end()) { (backwards ? order.back() : order.front())->SetFocus();return; }
    int next=(int)(current-order.begin())+(backwards ? -1 : 1);
    if(next<0 || next>=(int)order.size()) { if(CWnd *view=GetParent()->GetDlgItem(112)) view->SetFocus(); }
    else order[next]->SetFocus();
}
BOOL ClassicChrome::PreTranslateMessage(MSG *message)
{
    if(tips.GetSafeHwnd()) tips.RelayEvent(message);
    if(message->message==WM_KEYDOWN && message->wParam==VK_TAB) {
        location.ShowDropDown(FALSE);FocusBar((GetKeyState(VK_SHIFT)&0x8000)!=0);return TRUE;
    }
    if(message->message==WM_KEYDOWN && message->wParam==VK_RETURN && ::GetFocus()!=edit->GetSafeHwnd()) {
        HWND focused=::GetFocus();char kind[32]={0};GetClassNameA(focused,kind,sizeof(kind));
        if(!_stricmp(kind,"Button")) { ::SendMessage(focused,BM_CLICK,0,0);return TRUE; }
    }
    return CWnd::PreTranslateMessage(message);
}
BOOL ClassicChrome::OnEraseBkgnd(CDC *dc) { CRect area;GetClientRect(area);dc->FillSolidRect(area,ClassicUI::Face);return TRUE; }
HBRUSH ClassicChrome::OnCtlColor(CDC *dc,CWnd *window,UINT type) { return ClassicUI::ControlColor(dc,window,type); }

BEGIN_MESSAGE_MAP(ClassicStatusBar,CStatusBar)
 ON_WM_PAINT()
 ON_WM_LBUTTONUP()
END_MESSAGE_MAP()
void ClassicStatusBar::OnPaint()
{
    CPaintDC dc(this); CRect area;GetClientRect(area);dc.FillSolidRect(area,ClassicUI::Face);
    CFont *old=dc.SelectObject(ClassicUI::Font(this));dc.SetBkMode(TRANSPARENT);dc.SetTextColor(RGB(0,0,0));
    for(int i=0;i<GetStatusBarCtrl().GetParts(0,nullptr);++i) {
        CRect pane;GetItemRect(i,pane);dc.Draw3dRect(pane,RGB(128,128,128),RGB(255,255,255));pane.DeflateRect(3,2);
        if(i==2) { if(busy) { pane.right=pane.left+(pane.Width()*(progress>0 ? progress : 25))/100;dc.FillSolidRect(pane,RGB(0,0,128)); } }
        else if(i==0 || i==3) {
            CBitmap icon;icon.LoadBitmap(i==0 ? IDB_CLASSIC_HOME : IDB_CLASSIC_GETMAIL);
            BITMAP data;icon.GetBitmap(&data);CDC memory;memory.CreateCompatibleDC(&dc);
            CBitmap *saved=memory.SelectObject(&icon);int size=ClassicUI::Scale(this,12);
            TransparentBlt(dc,pane.left,pane.CenterPoint().y-size/2,size,size,memory,0,0,data.bmWidth,data.bmHeight,ClassicUI::Face);
            memory.SelectObject(saved);
        } else { CString text;GetPaneText(i,text);dc.DrawText(text,pane,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS); }
    }
    dc.SelectObject(old);
}
void ClassicStatusBar::OnLButtonUp(UINT flags,CPoint point)
{
    CRect pane;GetItemRect(0,pane);if(pane.PtInRect(point)) GetParent()->SendMessage(WM_COMMAND,ID_NAV_HOME);
    GetItemRect(3,pane);if(pane.PtInRect(point)) GetParent()->SendMessage(WM_COMMAND,ID_CLIENT_MAIL);
    CStatusBar::OnLButtonUp(flags,point);
}

BEGIN_MESSAGE_MAP(ClassicLogo,CStatic)
 ON_WM_PAINT()
END_MESSAGE_MAP()
void ClassicLogo::OnPaint()
{
    CPaintDC dc(this);CRect area;GetClientRect(area);dc.FillSolidRect(area,ClassicUI::Face);
    if(!bitmap.GetSafeHandle()) return;
    BITMAP data;bitmap.GetBitmap(&data);CDC memory;memory.CreateCompatibleDC(&dc);
    CBitmap *old=memory.SelectObject(&bitmap);dc.SetStretchBltMode(COLORONCOLOR);
    dc.StretchBlt(0,0,area.Width(),area.Height(),&memory,0,0,data.bmWidth,data.bmHeight,SRCCOPY);
    memory.SelectObject(old);
}
