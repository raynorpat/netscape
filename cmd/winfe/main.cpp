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
#include "menu_resource.h"
#include "window-menu.h"
#include "legacy/genedit.h"
#include "legacy/cmdparse.h"
#include "frontend.h"
#include "msgcom.h"
#include "libi18n.h"
#include "mailsettings.h"
#include "composer.h"
#include "composer_resource.h"
#include "images.h"
#include "forms.h"
#include "shist.h"
#include "downloads.h"
#include "history.h"
#include "bookmarks.h"
#include "chrome.h"
#include "mailui.h"
#include "find.h"
#include "browser-links.h"
#include "frame-divider.h"
#include "preferences.h"
#include "printing.h"
#ifdef EDITOR
#include "editor.h"
#include "editor_resource.h"
#include "editor-create.h"
#include "editor-publish.h"
#include "edt.h"
#endif
#include "glhist.h"
#include "merrors.h"
extern "C" {
/* OLE DB and the legacy database header both typedef DBTYPE. */
#define DBTYPE NetscapeDBType
#include "mcom_db.h"
#undef DBTYPE
#include "xp_file.h"
void GH_CollectGarbage(void);
#ifdef NETSCAPE_USE_BEARSSL
extern int MK_UNABLE_TO_CONNECT_TO_PROXY;
#endif
}

class BrowserFrame;
static bool original_browser_menu_smoke(BrowserFrame *,const char *,const char *);
static bool smoke_mode = false;
#ifdef NETSCAPE_USE_BEARSSL
static NETTLSInfo tls_smoke_info;
static MWContext *tls_smoke_context;
static unsigned tls_smoke_completed;
static std::string tls_smoke_key_cipher;
static bool tls_smoke_certificate_null;
#endif
static bool automatic_images = true;
static std::string smoke_error;
static std::vector<BrowserFrame *> browser_windows;
static std::map<MWContext *, BrowserFrame *> browser_contexts;
static std::map<MWContext *, HWND> native_windows;
static const UINT WM_MAIL_CHANGED = WM_APP + 30;
static const UINT WM_MESSAGE_SELECTION = WM_APP + 31;

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
    bool selecting=false;
    bool selection_dragged=false;
    bool suppress_release=false;
    CPoint selection_origin,selection_pointer;
    std::string hovered_link,hover_status;
    void ClearHover();
    FrameDivider divider;
    bool grid_document=false;
    void CancelInput();
    bool printing=false;
    std::vector<std::pair<int,int>> *print_spans=nullptr;
protected:
    BOOL OnCommand(WPARAM command, LPARAM control) override;
    afx_msg void OnPaint();
    afx_msg void OnSize(UINT type,int width,int height);
    afx_msg void OnVScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg void OnHScroll(UINT code, UINT position, CScrollBar *bar);
    afx_msg BOOL OnMouseWheel(UINT flags, short delta, CPoint point);
    afx_msg void OnLButtonDown(UINT flags,CPoint point);
    afx_msg void OnMouseMove(UINT flags,CPoint point);
    afx_msg void OnLButtonDblClk(UINT flags,CPoint point);
    afx_msg void OnSetFocus(CWnd *old);
    afx_msg void OnKillFocus(CWnd *next);
    afx_msg void OnLButtonUp(UINT flags, CPoint point);
    afx_msg void OnContextMenu(CWnd *window, CPoint point);
    afx_msg void OnCaptureChanged(CWnd *window);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnMouseLeave();
    afx_msg BOOL OnSetCursor(CWnd *window,UINT hit,UINT message);
    DECLARE_MESSAGE_MAP()
};

class BrowserFrame : public CFrameWnd {
public:
    BrowserView view;
    CGenericEdit address;
    ClassicButton go;
    ClassicChrome chrome;
    ClassicStatusBar status;
    bool status_visible = true, editing = false;
    bool new_document_copy=false;
    CListCtrl folders, messages;
    MailPaneUI mail_panes;
    MWContext context;
    GeneralPreferences document_preferences;
    MWContext *grid_parent = nullptr;
    MWContext *last_document = nullptr;
    std::string grid_name;
    int grid_scrolling = LO_SCROLL_AUTO;
    bool resizing = false;
    int mode = 0; /* browser, mail, news */
    bool updating_lists = false, selection_update_pending = false;
    std::vector<int> pending_message_selection;
    bool mail_update_pending = false, news_initial_groups_pending = true;
    unsigned int mail_changes = 0;
    bool initialized = false, completed = false;
    bool force_images = false;
    int load_status = 0;
    std::string current_url;
#ifdef NETSCAPE_USE_BEARSSL
    NETTLSInfo document_tls = {};
    std::string connection_details, connection_error;
#endif
    void Navigate(const char *url, bool remember = true);
    MWContext *FocusedDocument();
    void RefreshMailLists();
    void MailCaptionStatus();
    void InitializeMessaging(const char *server);
    BOOL PreTranslateMessage(MSG *message) override;
    BOOL OnCmdMsg(UINT id,int code,void *extra,AFX_CMDHANDLERINFO *info) override;
    void PostNcDestroy() override { delete this; }
protected:
    afx_msg int OnCreate(LPCREATESTRUCT create);
    afx_msg void OnSize(UINT type, int width, int height);
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnFocusLocation() {
        if(!chrome.location_visible) OnViewBar(ID_VIEW_LOCATION);
        address.SetFocus();address.SetSel(0,-1);
    }
    afx_msg void OnHome();
    afx_msg void OnFind();
    afx_msg void OnLoadImages();
    afx_msg void OnGoldCommand(UINT command);
    afx_msg void OnUpdateGold(CCmdUI *command);
    afx_msg void OnNewBrowser() { WFE_NewWindow("about:"); }
    afx_msg void OnSetHome();
    afx_msg void OnDirectory(UINT command);
    afx_msg void OnViewBar(UINT command);
    afx_msg void OnUpdateViewBar(CCmdUI *command);
    afx_msg void OnUpdateChrome(CCmdUI *command);
    afx_msg void OnUpdateMessage(CCmdUI *command);
    afx_msg void OnEditCommand(UINT command);
    afx_msg void OnUpdateEdit(CCmdUI *command);
    afx_msg void OnOpen();
    afx_msg void OnSavePage();
    afx_msg void OnHistory();
    afx_msg void OnBookmarks();
    afx_msg void OnAddBookmark();
    afx_msg void OnUpdateAddBookmark(CCmdUI *command);
    afx_msg void OnUpdateBack(CCmdUI *command);
    afx_msg void OnUpdateForward(CCmdUI *command);
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
    afx_msg void OnMessageStep(UINT command);
    afx_msg void OnHeaderMode(UINT command);
    afx_msg void OnMessageColumn(NMHDR *header,LRESULT *result);
    afx_msg void OnSettings();
    afx_msg void OnPreferences() { WFE_ShowGeneralPreferences(this); }
    afx_msg void OnMenuCommand(UINT command);
    afx_msg void OnUpdateMenu(CCmdUI *command);
    afx_msg void OnInitMenuPopup(CMenu *menu,UINT index,BOOL system);
    afx_msg void OnNewsCommand(UINT command);
    afx_msg void OnUpdateNews(CCmdUI *command);
    afx_msg void OnFolderClick(NMHDR *header,LRESULT *result);
    afx_msg void OnFolderKey(NMHDR *header,LRESULT *result);
    afx_msg void OnFolderDraw(NMHDR *header,LRESULT *result);
    afx_msg void OnFolderSelection(NMHDR *header, LRESULT *result);
    afx_msg void OnMessageSelection(NMHDR *header, LRESULT *result);
    afx_msg void OnFolderDoubleClick(NMHDR *header, LRESULT *result);
    afx_msg void OnMessageDoubleClick(NMHDR *header, LRESULT *result);
    afx_msg LRESULT OnMailChanged(WPARAM, LPARAM);
    afx_msg LRESULT OnMessageSelectionChanged(WPARAM,LPARAM);
    DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(BrowserFrame, CFrameWnd)
 ON_WM_CREATE()
 ON_WM_SIZE()
 ON_WM_TIMER()
 ON_WM_CLOSE()
 ON_WM_DESTROY()
 ON_WM_INITMENUPOPUP()
 ON_COMMAND_RANGE(42000,42999,OnGoldCommand)
 ON_UPDATE_COMMAND_UI_RANGE(42000,42999,OnUpdateGold)
 ON_COMMAND_RANGE(ID_BROWSER_EDIT,ID_BROWSER_EDIT,OnGoldCommand)
 ON_COMMAND_RANGE(35000,35149,OnMenuCommand)
 ON_UPDATE_COMMAND_UI_RANGE(35000,35149,OnUpdateMenu)
 ON_COMMAND_RANGE(48000,48199,OnMenuCommand)
 ON_UPDATE_COMMAND_UI_RANGE(48000,48199,OnUpdateMenu)
 ON_COMMAND_RANGE(36500,36599,OnMenuCommand)
 ON_UPDATE_COMMAND_UI_RANGE(36500,36599,OnUpdateMenu)
 ON_COMMAND_RANGE(37000,37999,OnMenuCommand)
 ON_UPDATE_COMMAND_UI_RANGE(37000,37999,OnUpdateMenu)
 ON_COMMAND(ID_BROWSER_NEW, OnNewBrowser)
 ON_COMMAND(ID_NAV_HOME, OnHome)
 ON_COMMAND(ID_FOCUS_LOCATION, OnFocusLocation)
 ON_COMMAND(ID_BROWSER_SET_HOME, OnSetHome)
 ON_COMMAND(ID_BROWSER_FIND, OnFind)
 ON_COMMAND(ID_BROWSER_IMAGES, OnLoadImages)
 ON_COMMAND_RANGE(ID_DIRECTORY_FIRST, ID_DIRECTORY_LAST, OnDirectory)
 ON_COMMAND_RANGE(ID_VIEW_TOOLBAR, ID_VIEW_STATUS, OnViewBar)
 ON_UPDATE_COMMAND_UI_RANGE(ID_VIEW_TOOLBAR, ID_VIEW_STATUS, OnUpdateViewBar)
 ON_UPDATE_COMMAND_UI_RANGE(ID_NAV_HOME, ID_BROWSER_FIND, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI(ID_NAV_STOP, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI(ID_NAV_RELOAD, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI(ID_BROWSER_SET_HOME, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI(ID_BROWSER_SAVE, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI(ID_BROWSER_HISTORY, OnUpdateChrome)
 ON_UPDATE_COMMAND_UI_RANGE(ID_MESSAGE_REPLY, ID_MESSAGE_SHOW_UNREAD, OnUpdateMessage)
 ON_COMMAND_RANGE(ID_EDIT_CLEAR, ID_EDIT_UNDO, OnEditCommand)
 ON_UPDATE_COMMAND_UI_RANGE(ID_EDIT_CLEAR, ID_EDIT_UNDO, OnUpdateEdit)
 ON_COMMAND_RANGE(ID_EDIT_REDO, ID_EDIT_REDO, OnEditCommand)
 ON_UPDATE_COMMAND_UI(ID_EDIT_REDO,OnUpdateEdit)
 ON_COMMAND(ID_BROWSER_OPEN, OnOpen)
 ON_COMMAND(ID_BROWSER_SAVE, OnSavePage)
 ON_COMMAND(ID_BROWSER_HISTORY, OnHistory)
 ON_COMMAND(ID_BOOKMARKS_SHOW, OnBookmarks)
 ON_COMMAND(ID_BOOKMARKS_ADD, OnAddBookmark)
 ON_UPDATE_COMMAND_UI(ID_BOOKMARKS_ADD, OnUpdateAddBookmark)
 ON_UPDATE_COMMAND_UI(ID_NAV_BACK, OnUpdateBack)
 ON_UPDATE_COMMAND_UI(ID_NAV_FORWARD, OnUpdateForward)
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
 ON_COMMAND_RANGE(ID_MESSAGE_PREVIOUS, ID_MESSAGE_NEXT, OnMessageStep)
 ON_COMMAND_RANGE(ID_MAIL_HEADERS_SOME, ID_MAIL_HEADERS_ALL, OnHeaderMode)
 ON_UPDATE_COMMAND_UI_RANGE(ID_MESSAGE_PREVIOUS, ID_MESSAGE_NEXT, OnUpdateMessage)
 ON_UPDATE_COMMAND_UI_RANGE(ID_MAIL_HEADERS_SOME, ID_MAIL_HEADERS_ALL, OnUpdateMessage)
 ON_NOTIFY(LVN_COLUMNCLICK, ID_MESSAGE_LIST, OnMessageColumn)
 ON_COMMAND(ID_MAIL_SETTINGS, OnSettings)
 ON_COMMAND(ID_BROWSER_PREFERENCES, OnPreferences)
 ON_COMMAND_RANGE(ID_NEWS_REPLY_POST, ID_NEWS_THREADED, OnNewsCommand)
 ON_UPDATE_COMMAND_UI_RANGE(ID_NEWS_REPLY_POST, ID_NEWS_THREADED, OnUpdateNews)
 ON_UPDATE_COMMAND_UI(ID_CLIENT_POST, OnUpdateNews)
 ON_NOTIFY(NM_CLICK, ID_FOLDER_LIST, OnFolderClick)
 ON_NOTIFY(LVN_KEYDOWN, ID_FOLDER_LIST, OnFolderKey)
 ON_NOTIFY(NM_CUSTOMDRAW, ID_FOLDER_LIST, OnFolderDraw)
 ON_NOTIFY(LVN_ITEMCHANGED, ID_FOLDER_LIST, OnFolderSelection)
 ON_NOTIFY(LVN_ITEMCHANGED, ID_MESSAGE_LIST, OnMessageSelection)
 ON_NOTIFY(NM_DBLCLK, ID_FOLDER_LIST, OnFolderDoubleClick)
 ON_NOTIFY(NM_DBLCLK, ID_MESSAGE_LIST, OnMessageDoubleClick)
 ON_MESSAGE(WM_MAIL_CHANGED, OnMailChanged)
 ON_MESSAGE(WM_MESSAGE_SELECTION, OnMessageSelectionChanged)
END_MESSAGE_MAP()
BEGIN_MESSAGE_MAP(BrowserView, CWnd)
 ON_WM_PAINT()
 ON_WM_SIZE()
 ON_WM_VSCROLL()
 ON_WM_HSCROLL()
 ON_WM_MOUSEWHEEL()
 ON_WM_LBUTTONDOWN()
 ON_WM_MOUSEMOVE()
 ON_WM_LBUTTONDBLCLK()
 ON_WM_SETFOCUS()
 ON_WM_KILLFOCUS()
 ON_WM_LBUTTONUP()
 ON_WM_CONTEXTMENU()
 ON_WM_CAPTURECHANGED()
 ON_WM_TIMER()
 ON_WM_MOUSELEAVE()
 ON_WM_SETCURSOR()
END_MESSAGE_MAP()

static BrowserFrame *frame_for(MWContext *context)
{
    auto found = browser_contexts.find(context);
    return found == browser_contexts.end() ? nullptr : found->second;
}
BOOL BrowserFrame::OnCmdMsg(UINT id,int code,void *extra,AFX_CMDHANDLERINFO *info)
{
    if(initialized && !info) {
        UINT print_id=id==ID_BROWSER_PRINT?57607:id==ID_MENU_PAGE_SETUP?57605:id==ID_MENU_PRINT_PREVIEW?57609:id;
        if(code==CN_COMMAND && WFE_PrintCommand(FocusedDocument(),print_id)) return TRUE;
        if(code==CN_UPDATE_COMMAND_UI && extra) {
            CCmdUI *ui=(CCmdUI *)extra;UINT original=ui->m_nID;ui->m_nID=print_id;
            bool handled=WFE_PrintUpdate(FocusedDocument(),ui);ui->m_nID=original;
            if(handled) return TRUE;
        }
    }
#ifdef EDITOR
    if(initialized && !info) {
        if(code==CN_COMMAND && WFE_EditorWorkflowCommand(&context,id)) return TRUE;
        if(code==CN_UPDATE_COMMAND_UI && extra && WFE_EditorWorkflowUpdate(&context,(CCmdUI *)extra)) return TRUE;
        if(code==CN_COMMAND && WFE_EditorPublishCommand(&context,id)) return TRUE;
        if(code==CN_UPDATE_COMMAND_UI && extra && WFE_EditorPublishUpdate(&context,(CCmdUI *)extra)) return TRUE;
    }
    if(editing && initialized && !info) {
        if(code==CN_COMMAND && WFE_EditorCommand(&context,id)) return TRUE;
        if(code==CN_UPDATE_COMMAND_UI && extra && WFE_EditorUpdate(&context,(CCmdUI *)extra)) return TRUE;
    }
#endif
    return CFrameWnd::OnCmdMsg(id,code,extra,info);
}
void BrowserFrame::OnGoldCommand(UINT command)
{
#ifdef EDITOR
    if(command==42279) WFE_NewEditorWindow(nullptr);
    else if(command==42151) {
        CFileDialog file(TRUE,"html",nullptr,OFN_FILEMUSTEXIST,"HTML documents|*.html;*.htm|All files|*.*||",this);
        if(file.DoModal()==IDOK) WFE_NewEditorWindow(file.GetPathName());
    } else if(command==42003 || command==ID_BROWSER_EDIT) WFE_NewEditorWindow(current_url.empty()?nullptr:current_url.c_str());
#endif
}
void BrowserFrame::OnUpdateGold(CCmdUI *command)
{
#ifdef EDITOR
    command->Enable(command->m_nID==42279 || command->m_nID==42151 || command->m_nID==42003);
#else
    command->Enable(FALSE);
#endif
}

static std::map<HWND,std::vector<std::pair<MWContext *,HWND>>> window_menu_entries;
static std::map<HWND,std::vector<int32>> go_menu_entries;
bool WFE_IsWindowMenu(CMenu *menu)
{
    return menu && (menu->GetMenuState(ID_MENU_WINDOW_ANCHOR,MF_BYCOMMAND)!=(UINT)-1 ||
        (menu->GetMenuState(ID_CLIENT_MAIL,MF_BYCOMMAND)!=(UINT)-1
        && menu->GetMenuState(ID_CLIENT_NEWS,MF_BYCOMMAND)!=(UINT)-1));
}
void WFE_PopulateWindowMenu(CMenu *menu,CWnd *owner)
{
    if(!menu || !owner) return;
    while(menu->GetMenuItemCount()>0) menu->DeleteMenu(0,MF_BYPOSITION);
    menu->AppendMenu(MF_STRING,ID_CLIENT_MAIL,"Netscape &Mail");
    menu->AppendMenu(MF_STRING,ID_CLIENT_NEWS,"Netscape &News");
    menu->AppendMenu(MF_SEPARATOR);
    menu->AppendMenu(MF_STRING|MF_GRAYED,ID_MENU_ADDRESS_BOOK,"&Address Book");
    bool editor_owner=false;
    for(auto frame:browser_windows) if(frame->GetSafeHwnd()==owner->GetSafeHwnd()) editor_owner=frame->editing;
    menu->AppendMenu(MF_STRING,editor_owner?ID_WINDOW_BOOKMARKS:ID_BOOKMARKS_SHOW,"&Bookmarks\tCtrl+B");
    menu->AppendMenu(MF_STRING,ID_BROWSER_HISTORY,"&History\tCtrl+H");
    menu->AppendMenu(MF_SEPARATOR);
    auto &entries=window_menu_entries[owner->GetSafeHwnd()];entries.clear();
    for(auto item:native_windows) {
        if(item.first->type!=MWContextBrowser && item.first->type!=MWContextMessageComposition && item.first->type!=MWContextEditor) continue;
        HWND handle=item.second;if(!::IsWindow(handle)||(::GetWindowLong(handle,GWL_STYLE)&WS_CHILD)) continue;
        char caption[512]={0};::GetWindowTextA(handle,caption,sizeof(caption));
        if(!*caption || entries.size()>=200) continue;
        CString caption_text=caption;
        if(item.first->type==MWContextMessageComposition) caption_text="Compose:Message Composition";
        else if(caption_text.Left(12)=="Netscape - [" && caption_text.Right(1)=="]") caption_text="Netscape:"+caption_text.Mid(12,caption_text.GetLength()-13);
        caption_text.Replace("&","&&");CString text;text.Format("&%u %s",(unsigned)entries.size(),(const char *)caption_text);text.Replace("\t"," ");
        menu->AppendMenu(MF_STRING|(handle==owner->GetSafeHwnd()?MF_CHECKED:0),48000+(UINT)entries.size(),text);
        entries.push_back(item);
    }
}
bool WFE_WindowCommand(CWnd *owner,UINT command)
{
    if(command>=48000 && command<48200 && owner) {
        auto list=window_menu_entries.find(owner->GetSafeHwnd());size_t index=command-48000;
        if(list!=window_menu_entries.end() && index<list->second.size()) {
            auto entry=list->second[index];auto live=native_windows.find(entry.first);
            if(live!=native_windows.end() && live->second==entry.second && ::IsWindow(entry.second)) {
                ::ShowWindow(entry.second,::IsIconic(entry.second)?SW_RESTORE:SW_SHOW);
                ::SetActiveWindow(entry.second);
                ::SetForegroundWindow(entry.second);
            }
        }
        return true;
    }
    if(command==ID_CLIENT_MAIL) { WFE_NewMailWindow(0,nullptr);return true; }
    if(command==ID_CLIENT_NEWS) { WFE_NewMailWindow(1,nullptr);return true; }
    if(command==ID_BROWSER_NEW) { WFE_NewWindow("about:");return true; }
    if(command==ID_BOOKMARKS_SHOW || command==ID_WINDOW_BOOKMARKS || command==ID_BROWSER_HISTORY) {
        MWContext *source=nullptr;
        for(auto frame:browser_windows) if(owner && frame->GetSafeHwnd()==owner->GetSafeHwnd() && !frame->mode && !frame->editing) {source=&frame->context;break;}
        if(!source) for(auto frame:browser_windows) if(!frame->grid_parent && !frame->mode && !frame->editing) {source=&frame->context;break;}
        if(!source) source=WFE_NewWindow("about:blank");
        if(source) { if(command==ID_BOOKMARKS_SHOW || command==ID_WINDOW_BOOKMARKS) WFE_BookmarksShow(source);else WFE_NavigateTarget(source,"about:global",nullptr); }
        return true;
    }
    return false;
}
bool WFE_WindowUpdateCommand(CWnd *owner,CCmdUI *command)
{
    UINT id=command->m_nID;
    if(id==ID_MENU_ADDRESS_BOOK) {command->Enable(FALSE);return true;}
    if((id>=48000&&id<48200) || id==ID_CLIENT_MAIL || id==ID_CLIENT_NEWS ||
       id==ID_BOOKMARKS_SHOW || id==ID_WINDOW_BOOKMARKS || id==ID_BROWSER_HISTORY || id==ID_BROWSER_NEW) {
        command->Enable(TRUE);return true;
    }
    return false;
}
void BrowserFrame::OnInitMenuPopup(CMenu *menu,UINT index,BOOL system)
{
    if(!system && WFE_IsWindowMenu(menu)) WFE_PopulateWindowMenu(menu,this);
    else if(!system && !mode && menu && menu->GetMenuState(ID_BOOKMARKS_ADD,MF_BYCOMMAND)!=(UINT)-1 && menu->GetMenuState(ID_BOOKMARKS_SHOW,MF_BYCOMMAND)!=(UINT)-1) WFE_BookmarksPopulateMenu(&context,menu);
    else if(!system && !mode && menu && menu->GetMenuState(ID_NAV_BACK,MF_BYCOMMAND)!=(UINT)-1 && menu->GetMenuState(ID_NAV_FORWARD,MF_BYCOMMAND)!=(UINT)-1) {
        while(menu->GetMenuItemCount()>5) menu->DeleteMenu(5,MF_BYPOSITION);
        auto &ids=go_menu_entries[GetSafeHwnd()];ids.clear();XP_List *history=SHIST_GetList(&context);
        History_entry *current=SHIST_GetCurrent(&context.hist);
        for(int i=XP_ListCount(history);i>=1 && ids.size()<100;--i) {
            History_entry *entry=(History_entry *)XP_ListGetObjectNum(history,i);if(!entry) continue;
            CString label=entry->title&&*entry->title?entry->title:entry->address;label.Replace("&","&&");label.Replace("\t"," ");
            menu->AppendMenu(MF_STRING|(entry==current?MF_CHECKED:0),36500+(UINT)ids.size(),label);ids.push_back(entry->unique_id);
        }
    }
    CFrameWnd::OnInitMenuPopup(menu,index,system);
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
    window_menu_entries.erase(closing);go_menu_entries.erase(closing);WFE_BookmarksForgetMenu(context);
    native_windows.erase(found);
    if (AfxGetApp()->m_pMainWnd && AfxGetApp()->m_pMainWnd->GetSafeHwnd() == closing
        && !native_windows.empty())
        AfxGetApp()->m_pMainWnd = CWnd::FromHandlePermanent(native_windows.begin()->second);
}

void BrowserView::OnSize(UINT type,int width,int height)
{
    CWnd::OnSize(type,width,height);
    if(frame && frame->mode && frame->initialized) {
        WFE_DocumentSize(&frame->context,document_width,document_height);
        scroll_x=GetScrollPos(SB_HORZ);scroll_y=GetScrollPos(SB_VERT);
        WFE_FormPositionAll(&frame->context);Invalidate();
    }
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
    static UINT indicators[] = {ID_STATUS_NAVIGATOR, ID_SEPARATOR, ID_STATUS_PROGRESS, ID_STATUS_MAIL};
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
    if (!chrome.Create(this, &address, &go, mode)) return -1;
    view.frame = this;
    if (!view.CreateEx(0, AfxRegisterWndClass(CS_DBLCLKS, LoadCursor(nullptr, IDC_ARROW)),
                       "Document", WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_VSCROLL | WS_HSCROLL,
                       CRect(0,40,800,600), this, 112)) return -1;
    if (!status.Create(this) || !status.SetIndicators(indicators, 4)) return -1;
    status.SetFont(ClassicUI::Font(this));
    status.SetPaneInfo(0, ID_STATUS_NAVIGATOR, SBPS_NORMAL, ClassicUI::Scale(this, 20));
    status.SetPaneInfo(1, ID_SEPARATOR, SBPS_STRETCH, 0);
    status.SetPaneInfo(2, ID_STATUS_PROGRESS, SBPS_NORMAL, ClassicUI::Scale(this, 80));
    status.SetPaneInfo(3, ID_STATUS_MAIL, SBPS_NORMAL, ClassicUI::Scale(this, 28));
    status.SetPaneText(0, "N"); status.SetPaneText(1, "Document: Done"); status.SetPaneText(3, "Mail");
    if (mode) {
        DWORD style = WS_CHILD | WS_VISIBLE | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS;
        if (!folders.Create(style, CRect(0,40,260,550), this, ID_FOLDER_LIST)
            || !messages.Create(style & ~LVS_SINGLESEL, CRect(265,40,800,250), this, ID_MESSAGE_LIST)) return -1;
        folders.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        messages.SetExtendedStyle(LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
        if(!mail_panes.Attach(this,&folders,&messages,&view,mode==2)) return -1;
        address.SetReadOnly(mode == 1);
        go.SetWindowText(mode == 2 ? "Connect" : "Refresh");
    }
    browser_contexts[&context] = this;
    document_preferences=WFE_GeneralPreferences();
    WFE_InitContext(&context, this);
#ifdef EDITOR
    if(editing) {
        context.type=MWContextEditor;context.is_editor=TRUE;
        chrome.toolbar_visible=chrome.location_visible=chrome.directory_visible=false;chrome.ShowWindow(SW_HIDE);
        if(!WFE_EditorCreateUI(&context,this)) return -1;
    }
#endif
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
    int top = chrome.Arrange(width);
#ifdef EDITOR
    if(editing) top=WFE_EditorArrange(&context,width);
#endif
    int footer = status_visible ? ClassicUI::Scale(this, 20) : 0;
    status.ShowWindow(status_visible ? SW_SHOW : SW_HIDE);
    if (status_visible) status.MoveWindow(0,(std::max)(0,height-footer),width,footer);
    int body = (std::max)(1,height-top-footer);
    if (mode && folders.GetSafeHwnd()) {
        mail_panes.Arrange(top,footer);
    } else view.MoveWindow(0, top, width, body);
    if (initialized) WFE_FormPositionAll(&context);
    if (initialized && mode == 0 && !current_url.empty()) SetTimer(2, 120, nullptr);
}
BOOL BrowserFrame::PreTranslateMessage(MSG *message)
{
    FocusedDocument();
    if(message->message==WM_KEYDOWN && message->wParam==VK_ESCAPE && view.divider.Active()) {view.CancelInput();return TRUE;}
#ifdef EDITOR
    if(editing && ::GetFocus()==view.GetSafeHwnd() && WFE_EditorKey(&context,message)) return TRUE;
#endif
    if (WFE_FormKey(&context, message->hwnd, message->message, message->wParam, message->lParam)) return TRUE;
    if(message->message==WM_KEYDOWN && message->wParam==VK_TAB && ::GetFocus()==view.GetSafeHwnd()) {
        chrome.FocusBar((GetKeyState(VK_SHIFT)&0x8000)!=0);return TRUE;
    }
    if (message->message == WM_KEYDOWN && message->wParam == VK_RETURN
        && GetFocus() == &address) { OnGo(); return TRUE; }
    return CFrameWnd::PreTranslateMessage(message);
}
void BrowserFrame::Navigate(const char *input, bool remember)
{
    (void)remember;
    force_images=false;
#ifdef EDITOR
    if(editing && !WFE_EditorCheckSave(&context)) return;
#endif
    WFE_HistorySavePosition(&context);
    current_url = as_url(input);
    if (address.GetSafeHwnd()) address.SetWindowText(current_url.c_str());
    chrome.Remember(current_url.c_str());
    completed = false;
    view.drawn_runs = 0;
    view.drawn_text.clear();
    WFE_Load(&context, current_url.c_str());
}
void BrowserFrame::OnTimer(UINT_PTR timer)
{
    if (timer == 1) {
        WFE_Pump(); chrome.UpdateCommands();
        bool loading = XP_IsContextBusy(&context) != 0;
        if (status.busy != loading) {
            status.busy = loading;
            if (!loading) {
                if(mode) MailCaptionStatus();
                else status.SetPaneText(1,
#ifdef NETSCAPE_USE_BEARSSL
                    !connection_error.empty() ? connection_error.c_str() :
#endif
                    load_status >= 0 ? "Document: Done" : load_status == MK_INTERRUPTED ? "Document: Stopped" : "Document: Error");
            }
            status.Invalidate();
        }
    }
    else if (timer == 2) {
        KillTimer(2);
        if (!initialized || current_url.empty()) return;
        if (resizing || XP_IsContextBusy(&context)) { SetTimer(2, 120, nullptr); return; }
#ifdef EDITOR
        if(editing) {if(EDT_HaveEditBuffer(&context) && !EDT_IsBlocked(&context)) EDT_RefreshLayout(&context);return;}
#endif
        History_entry *entry = SHIST_GetCurrent(&context.hist);
        if (!entry || entry->method != URL_GET_METHOD) return;
        WFE_HistorySavePosition(&context);
        URL_Struct *url = SHIST_CreateURLStructFromHistoryEntry(&context, entry);
        if (!url) return;
        resizing = true; completed = false;
        url->force_reload = NET_RESIZE_RELOAD;
        FE_GetURL(&context, url);
    } else CFrameWnd::OnTimer(timer);
}
MWContext *BrowserFrame::FocusedDocument()
{
    /* Walk from the focused control upwards: a parent frameset view also owns
     * each child HWND, so an arbitrary IsChild/map-order match is insufficient. */
    for(HWND focused=::GetFocus();focused;focused=::GetParent(focused)) {
        for(auto &item:browser_contexts) if(item.second->view.GetSafeHwnd()==focused) {
            MWContext *top=item.first;
            while(top->grid_parent) top=top->grid_parent;
            if(top==&context) { last_document=item.first;return item.first; }
        }
        if(focused==GetSafeHwnd()) break;
    }
    if(last_document && frame_for(last_document)) {
        MWContext *top=last_document;
        while(top->grid_parent) top=top->grid_parent;
        if(top==&context) return last_document;
    }
    last_document=nullptr;return &context;
}
void BrowserFrame::OnHome()
{
    const char *home=WFE_GeneralPreferences().home.c_str();
    if(mode) WFE_NewWindow(home);else Navigate(home);
}
void BrowserFrame::OnSetHome()
{
    if(current_url.empty()) return;
    GeneralPreferences value=WFE_GeneralPreferences();value.home=current_url;std::string error;
    if(!WFE_SaveGeneralPreferences(value,&error)) WFE_Message(&context,error.c_str(),0);
}
void WFE_GeneralPreferencesChanged()
{
    for(BrowserFrame *frame:browser_windows) if(!frame->grid_parent && frame->GetSafeHwnd()) {
        frame->chrome.SetPresentation(WFE_GeneralPreferences().toolbar);
        CRect area;frame->GetClientRect(area);frame->SendMessage(WM_SIZE,SIZE_RESTORED,MAKELPARAM(area.Width(),area.Height()));
    }
}
void BrowserFrame::OnFind() { WFE_FindShow(FocusedDocument()); }
extern "C" int WFE_LoadImagesAutomatically(MWContext *context)
{
    BrowserFrame *frame=frame_for(context);
    return !frame || automatic_images || frame->force_images;
}
void BrowserFrame::OnLoadImages()
{
    BrowserFrame *document=frame_for(FocusedDocument());if(!document) return;
    document->force_images=true;
    if(document->mode) {
        if(!document->current_url.empty()) {
            URL_Struct *url=NET_CreateURLStruct(document->current_url.c_str(),NET_RESIZE_RELOAD);
            if(url) FE_GetURL(&document->context,url);
        }
    }
    else {
        History_entry *entry=SHIST_GetCurrent(&document->context.hist);
        if(entry && entry->method==URL_GET_METHOD) {
            URL_Struct *url=SHIST_CreateURLStructFromHistoryEntry(&document->context,entry);
            if(url) {url->force_reload=NET_RESIZE_RELOAD;FE_GetURL(&document->context,url);}
        }
    }
}
void BrowserFrame::OnDirectory(UINT command)
{
    static const char *urls[] = {
        "http://home.netscape.com/home/whats-new.html", "http://home.netscape.com/home/whats-cool.html",
        "http://home.netscape.com/escapes/index.html", "http://home.netscape.com/home/internet-search.html",
        "http://home.netscape.com/home/internet-white-pages.html", "http://home.netscape.com/comprod/upgrades/index.html"
    };
    if(command>=ID_DIRECTORY_FIRST && command<=ID_DIRECTORY_LAST) {
        if(mode) WFE_NewWindow(urls[command-ID_DIRECTORY_FIRST]);else Navigate(urls[command-ID_DIRECTORY_FIRST]);
    }
}
void BrowserFrame::OnViewBar(UINT command)
{
    if(command==ID_VIEW_TOOLBAR) chrome.toolbar_visible=!chrome.toolbar_visible;
    if(command==ID_VIEW_LOCATION) chrome.location_visible=!chrome.location_visible;
    if(command==ID_VIEW_DIRECTORY) chrome.directory_visible=!chrome.directory_visible;
    if(command==ID_VIEW_STATUS) status_visible=!status_visible;
    CRect area;GetClientRect(area);OnSize(SIZE_RESTORED,area.Width(),area.Height());
    if(!chrome.location_visible) view.SetFocus();
    Invalidate();
}
void BrowserFrame::OnUpdateViewBar(CCmdUI *command)
{
    bool visible=command->m_nID==ID_VIEW_TOOLBAR ? chrome.toolbar_visible :
        command->m_nID==ID_VIEW_LOCATION ? chrome.location_visible :
        command->m_nID==ID_VIEW_DIRECTORY ? chrome.directory_visible : status_visible;
    command->SetCheck(visible);
}

struct NativeMessageCommand { UINT id;MSG_CommandType action;int modes; };
static const NativeMessageCommand original_message_commands[]={
    {ID_MENU_NEW_FOLDER,MSG_NewFolder,1},{ID_MENU_ADD_FOLDER,MSG_OpenFolder,1},
    {ID_MENU_NEXT_MESSAGE,MSG_NextMessage,3},{ID_MENU_PREVIOUS_MESSAGE,MSG_PreviousMessage,3},
    {ID_MENU_SAVE_MESSAGE,MSG_SaveMessagesAs,3},{ID_MENU_EMPTY_TRASH,MSG_EmptyTrash,1},
    {ID_MENU_COMPRESS_FOLDER,MSG_CompressFolder,1},{ID_MENU_DELETE_FOLDER,MSG_DeleteFolder,1},
    {ID_MENU_SELECT_THREAD,MSG_SelectThread,3},{ID_MENU_SELECT_FLAGGED,MSG_SelectMarkedMessages,3},
    {ID_MENU_SORT_AGAIN,MSG_ReSort,3},{ID_MENU_SORT_THREADED,MSG_ThreadMessages,3},
    {ID_MENU_SORT_ASCENDING,MSG_SortBackward,3},{ID_MENU_SORT_DATE,MSG_SortByDate,3},
    {ID_MENU_SORT_SUBJECT,MSG_SortBySubject,3},{ID_MENU_SORT_SENDER,MSG_SortBySender,3},
    {ID_MENU_SORT_NUMBER,MSG_SortByMessageNumber,3},{ID_MENU_WRAP_LINES,MSG_WrapLongLines,3},
    {ID_MENU_ROT13,MSG_Rot13Message,3},{ID_MENU_ATTACHMENTS_INLINE,MSG_AttachmentsInline,3},
    {ID_MENU_ATTACHMENTS_LINKS,MSG_AttachmentsAsLinks,3},{ID_MENU_FORWARD_QUOTED,MSG_ForwardMessageQuoted,3},
    {ID_MENU_MARK_UNREAD,MSG_MarkSelectedUnread,3},{ID_MENU_FLAG,MSG_MarkSelectedMessages,3},
    {ID_MENU_UNFLAG,MSG_UnmarkSelectedMessages,3},{ID_MENU_MOVE_FILE,MSG_SaveMessagesAsAndDelete,1},
    {ID_MENU_COPY_FILE,MSG_SaveMessagesAs,1},{ID_MENU_FIRST_UNREAD,MSG_FirstUnreadMessage,3},
    {ID_MENU_NEXT_UNREAD,MSG_NextUnreadMessage,3},{ID_MENU_PREVIOUS_UNREAD,MSG_PreviousUnreadMessage,3},
    {ID_MENU_FIRST_FLAGGED,MSG_FirstMarkedMessage,3},{ID_MENU_NEXT_FLAGGED,MSG_NextMarkedMessage,3},
    {ID_MENU_PREVIOUS_FLAGGED,MSG_PreviousMarkedMessage,3},{ID_MENU_HEADERS_BRIEF,MSG_ShowMicroHeaders,3},
    {ID_MENU_OPEN_NEWS_HOST,MSG_OpenNewsHost,2},{ID_MENU_REMOVE_NEWS_HOST,MSG_DeleteNewsHost,2},
    {ID_MENU_ADD_NEWSGROUP,MSG_AddNewsGroup,2},{ID_MENU_CANCEL_MESSAGE,MSG_CancelMessage,2},
    {ID_MENU_ACTIVE_NEWSGROUPS,MSG_ShowActiveNewsGroups,2},{ID_MENU_NEW_NEWSGROUPS,MSG_CheckNewNewsGroups,2},
    {ID_MENU_ADD_NEWEST,MSG_AddFromNewest,2},{ID_MENU_ADD_OLDEST,MSG_AddFromOldest,2}
};
static const NativeMessageCommand *original_message_command(UINT id)
{
    for(const auto &item:original_message_commands) if(item.id==id) return &item;
    return nullptr;
}
static bool original_filing_command(UINT id) {return id>=ID_MENU_MOVE_INBOX&&id<=ID_MENU_COPY_TRASH&&id!=ID_MENU_COPY_FILE;}
static const char *original_help_url(UINT id)
{
#include "res/original-links.inc"
    for(const auto &link:original_links) if(link.command==id) return link.url;
    return nullptr;
}
void BrowserFrame::OnUpdateMenu(CCmdUI *command)
{
    UINT id=command->m_nID;
    if(WFE_WindowUpdateCommand(this,command)) return;
    if(id>=37000 && id<38000) {command->Enable(!mode);return;}
    if(id>=36500 && id<36600) {auto entries=go_menu_entries.find(GetSafeHwnd());command->Enable(entries!=go_menu_entries.end()&&id-36500<entries->second.size());return;}
    if(const auto *item=original_message_command(id)) {
        XP_Bool enabled=FALSE;MSG_COMMAND_CHECK_STATE checked=MSG_Unchecked;
        bool available=mode && (item->modes&mode) && context.msgframe &&
            MSG_CommandStatus(&context,item->action,&enabled,&checked,nullptr,nullptr)>=0 && enabled;
        command->Enable(available);
        if(checked!=MSG_NotUsed) command->SetCheck(id==ID_MENU_SORT_ASCENDING ? checked!=MSG_Checked : checked==MSG_Checked);
        return;
    }
    if(original_filing_command(id)) { XP_Bool enabled=FALSE;command->Enable(mode==1&&MSG_CommandStatus(&context,MSG_MoveMessagesInto,&enabled,nullptr,nullptr,nullptr)>=0&&enabled);return; }
    bool enabled=false;
    BrowserFrame *focused=frame_for(FocusedDocument());
    if(id==ID_MENU_FIND_AGAIN) enabled=XP_DOCID(FocusedDocument())!=0;
    else if(id==ID_MENU_REFRESH) enabled=true;
    else if(id==ID_MENU_RELOAD_FRAME || id==ID_MENU_FRAME_SOURCE || id==ID_MENU_FRAME_INFO)
        enabled=focused && focused!=this && focused->grid_parent && !focused->current_url.empty();
    else if(id==ID_MENU_DOCUMENT_SOURCE || id==ID_MENU_DOCUMENT_INFO || id==ID_MENU_MAIL_DOCUMENT) enabled=!current_url.empty();
    else if(id==ID_MENU_AUTO_IMAGES) {enabled=true;command->SetCheck(automatic_images);}
    else if(id==ID_MENU_ENCODING_LATIN1) {enabled=true;command->SetCheck(TRUE);}
    else if(id==ID_MENU_EXIT || id==ID_MENU_HELP_PLUGINS || original_help_url(id)) enabled=true;
    command->Enable(enabled);
}
void BrowserFrame::OnMenuCommand(UINT command)
{
    if(selection_update_pending) OnMessageSelectionChanged(0,0);
    if(WFE_WindowCommand(this,command)) return;
    if(command>=37000 && command<38000) {if(!mode) WFE_BookmarksMenuCommand(&context,command);return;}
    if(command>=36500 && command<36600) {
        auto entries=go_menu_entries.find(GetSafeHwnd());size_t index=command-36500;
        if(entries!=go_menu_entries.end() && index<entries->second.size()) WFE_HistoryGo(&context,entries->second[index]);
        return;
    }
    if(const auto *item=original_message_command(command)) {
        XP_Bool enabled=FALSE;
        if(!mode || !(item->modes&mode) || !context.msgframe || MSG_CommandStatus(&context,item->action,&enabled,nullptr,nullptr,nullptr)<0 || !enabled) return;
        if(command==ID_MENU_ACTIVE_NEWSGROUPS || command==ID_MENU_NEW_NEWSGROUPS) news_initial_groups_pending=false;
        MSG_Command(&context,item->action);RefreshMailLists();return;
    }
    if(original_filing_command(command) && mode==1) {
        ULONG wanted=command==ID_MENU_MOVE_SENT?MSG_FOLDER_FLAG_SENTMAIL:
            (command==ID_MENU_MOVE_TRASH||command==ID_MENU_COPY_TRASH)?MSG_FOLDER_FLAG_TRASH:MSG_FOLDER_FLAG_INBOX;
        MSG_FolderLine folder;
        for(int i=0;MSG_GetFolderLine(&context,i,&folder,nullptr);++i) if(folder.flags&wanted) {
            const char *name=MSG_GetFolderName(&context,i);
            if(name) {if(command<=ID_MENU_MOVE_TRASH) MSG_MoveSelectedMessagesInto(&context,name);else MSG_CopySelectedMessagesInto(&context,name);}
            break;
        }
        RefreshMailLists();return;
    }
    BrowserFrame *document=(command==ID_MENU_FRAME_SOURCE||command==ID_MENU_FRAME_INFO||command==ID_MENU_RELOAD_FRAME)?frame_for(FocusedDocument()):this;
    if(!document) return;
    if(command==ID_MENU_FIND_AGAIN) WFE_FindAgain(FocusedDocument());
    else if(command==ID_MENU_REFRESH) {view.Invalidate();view.UpdateWindow();}
    else if(command==ID_MENU_RELOAD_FRAME) document->OnReload();
    else if(command==ID_MENU_DOCUMENT_SOURCE||command==ID_MENU_FRAME_SOURCE) {
        History_entry *entry=SHIST_GetCurrent(&document->context.hist);
        if(entry&&entry->method==URL_POST_METHOD) {WFE_Message(&context,"Source for a POST response is unavailable without resubmitting the form.",0);return;}
        if(!document->current_url.empty()) WFE_ViewSource(&document->context,document->current_url.c_str());
    } else if(command==ID_MENU_DOCUMENT_INFO||command==ID_MENU_FRAME_INFO) {
        CString title;document->GetWindowText(title);
        std::string text="Title: "+std::string(title)+"\nLocation: "+document->current_url+"\nCharacter set: Western (Latin-1)";
#ifdef NETSCAPE_USE_BEARSSL
        text+="\nConnection: "+document->connection_details;
#endif
        WFE_Message(&context,text.c_str(),0);
    } else if(command==ID_MENU_MAIL_DOCUMENT) {
        std::string target="mailto:?body=";char *escaped=NET_Escape(current_url.c_str(),URL_XALPHAS);
        if(escaped) {target+=escaped;free(escaped);WFE_Load(&context,target.c_str());}
    } else if(command==ID_MENU_AUTO_IMAGES) {
        std::string file=std::string(WFE_ProfileDirectory())+"/browser.ini";
        if(!WritePrivateProfileStringA("Browser","AutoImages",automatic_images?"0":"1",file.c_str())) {WFE_Message(&context,"Unable to save automatic image loading.",0);return;}
        automatic_images=!automatic_images;for(auto frame:browser_windows) frame->force_images=false;
    }
    else if(command==ID_MENU_HELP_PLUGINS) {if(mode) WFE_NewWindow("about:plugins");else Navigate("about:plugins");}
    else if(command==ID_MENU_EXIT) {
        std::vector<HWND> windows;for(auto item:native_windows) if(!(::GetWindowLong(item.second,GWL_STYLE)&WS_CHILD)) windows.push_back(item.second);
        for(HWND window:windows) if(::IsWindow(window)) ::PostMessage(window,WM_CLOSE,0,0);
    } else if(const char *url=original_help_url(command)) {if(mode) WFE_NewWindow(url);else Navigate(url);}
}
void BrowserFrame::OnUpdateChrome(CCmdUI *command)
{
    bool enabled=true;
    if(command->m_nID==ID_BROWSER_PRINT) enabled=false;
#ifndef EDITOR
    if(command->m_nID==ID_BROWSER_EDIT) enabled=false;
#endif
    if(command->m_nID==ID_BROWSER_IMAGES) {
        MWContext *document=FocusedDocument();History_entry *entry=SHIST_GetCurrent(&document->hist);
        enabled=XP_DOCID(document)!=0 && (mode || (entry && entry->method==URL_GET_METHOD));
    }
    if(command->m_nID==ID_NAV_STOP) enabled=XP_IsContextBusy(&context)!=0;
    if(command->m_nID==ID_NAV_RELOAD) enabled=!current_url.empty();
    if(command->m_nID==ID_BROWSER_SET_HOME) enabled=!mode&&!current_url.empty();
    if(command->m_nID==ID_BROWSER_SAVE) enabled=!mode&&SHIST_GetCurrent(&context.hist)!=nullptr;
    if(command->m_nID==ID_BROWSER_HISTORY) enabled=true;
    if(command->m_nID==ID_BROWSER_FIND) enabled=XP_DOCID(FocusedDocument())!=0;
    command->Enable(enabled);
}
void BrowserFrame::OnUpdateMessage(CCmdUI *command)
{
    if(command->m_nID==ID_MAIL_SETTINGS) { command->Enable(TRUE);return; }
    if(!mode || !initialized) { command->Enable(FALSE);return; }
    MSG_CommandType action=MSG_ReplyToSender;
    switch(command->m_nID) {
    case ID_MESSAGE_REPLY: action=MSG_ReplyToSender;break;
    case ID_MESSAGE_REPLY_ALL: action=MSG_ReplyToAll;break;
    case ID_MESSAGE_FORWARD: action=MSG_ForwardMessage;break;
    case ID_MESSAGE_GET: action=mode==1 ? MSG_GetNewMail : MSG_GetMoreMessages;break;
    case ID_MESSAGE_DELETE: if(mode!=1) { command->Enable(FALSE);return; } action=MSG_DeleteMessage;break;
    case ID_MESSAGE_READ: action=MSG_MarkSelectedRead;break;
    case ID_MESSAGE_PREVIOUS: action=mode==2?MSG_PreviousUnreadMessage:MSG_PreviousMessage;break;
    case ID_MESSAGE_NEXT: action=mode==2?MSG_NextUnreadMessage:MSG_NextMessage;break;
    case ID_MAIL_HEADERS_SOME: action=MSG_ShowSomeHeaders;break;
    case ID_MAIL_HEADERS_ALL: action=MSG_ShowAllHeaders;break;
    case ID_MESSAGE_SEND_QUEUED: action=MSG_DeliverQueuedMessages;break;
    case ID_MESSAGE_SHOW_ALL: action=MSG_ShowAllMessages;break;
    case ID_MESSAGE_SHOW_UNREAD: action=MSG_ShowOnlyUnreadMessages;break;
    default: command->Enable(FALSE);return;
    }
    XP_Bool selectable=FALSE;
    MSG_COMMAND_CHECK_STATE checked=MSG_Unchecked;
    int result=MSG_CommandStatus(&context,action,&selectable,&checked,nullptr,nullptr);
    command->Enable(result>=0&&selectable);
    if(command->m_nID==ID_MAIL_HEADERS_SOME||command->m_nID==ID_MAIL_HEADERS_ALL) command->SetCheck(checked==MSG_Checked);
}
void BrowserFrame::OnUpdateEdit(CCmdUI *command)
{
    HWND focused=::GetFocus();char kind[32]={0};GetClassNameA(focused,kind,sizeof(kind));
    bool editable=!_stricmp(kind,"Edit");
    bool writable=editable&&!(::GetWindowLong(focused,GWL_STYLE)&ES_READONLY);
    if(mode && !editable && (command->m_nID==ID_EDIT_UNDO || command->m_nID==ID_EDIT_REDO || command->m_nID==ID_EDIT_SELECT_ALL)) {
        XP_Bool available=FALSE;MSG_CommandType action=command->m_nID==ID_EDIT_UNDO?MSG_Undo:command->m_nID==ID_EDIT_REDO?MSG_Redo:MSG_SelectAllMessages;
        command->Enable(MSG_CommandStatus(&context,action,&available,nullptr,nullptr,nullptr)>=0&&available);return;
    }
    DWORD start=0,end=0;if(editable) ::SendMessage(focused,EM_GETSEL,(WPARAM)&start,(LPARAM)&end);
    bool selected=editable ? start!=end : LO_HaveSelection(FocusedDocument())!=0;
    bool enabled=false;
    switch(command->m_nID) {
    case ID_EDIT_COPY: enabled=selected;break;
    case ID_EDIT_CUT: case ID_EDIT_CLEAR: enabled=writable&&selected;break;
    case ID_EDIT_PASTE: enabled=writable&&IsClipboardFormatAvailable(CF_TEXT);break;
    case ID_EDIT_UNDO: enabled=writable&&::SendMessage(focused,EM_CANUNDO,0,0);break;
    case ID_EDIT_SELECT_ALL: enabled=editable||XP_DOCID(FocusedDocument())!=0;break;
    }
    command->Enable(enabled);
}
void BrowserFrame::OnEditCommand(UINT command)
{
    HWND focused=::GetFocus(); char name[32]={0};GetClassNameA(focused,name,sizeof(name));
    if(mode && _stricmp(name,"Edit") && (command==ID_EDIT_UNDO||command==ID_EDIT_REDO||command==ID_EDIT_SELECT_ALL)) {
        MSG_Command(&context,command==ID_EDIT_UNDO?MSG_Undo:command==ID_EDIT_REDO?MSG_Redo:MSG_SelectAllMessages);RefreshMailLists();return;
    }
    if(!_stricmp(name,"Edit")) {
        UINT message=command==ID_EDIT_COPY ? WM_COPY : command==ID_EDIT_CUT ? WM_CUT :
            command==ID_EDIT_PASTE ? WM_PASTE : command==ID_EDIT_UNDO ? WM_UNDO :
            command==ID_EDIT_SELECT_ALL ? EM_SETSEL : WM_CLEAR;
        ::SendMessage(focused,message,0,command==ID_EDIT_SELECT_ALL ? -1 : 0);return;
    }
    MWContext *document=FocusedDocument();
    if(command==ID_EDIT_SELECT_ALL) { LO_SelectAll(document);WFE_Invalidate(document); }
    if(command==ID_EDIT_COPY) {
        char *text=(char *)LO_GetSelectionText(document);
        if(text && OpenClipboard()) {
            HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,strlen(text)+1);
            if(data) {
                void *memory=GlobalLock(data);
                if(memory) { memcpy(memory,text,strlen(text)+1);GlobalUnlock(data);EmptyClipboard();if(!SetClipboardData(CF_TEXT,data)) GlobalFree(data); }
                else GlobalFree(data);
            }
            CloseClipboard();
        }
        free(text);
    }
}

void BrowserFrame::OnGo() { CString text; address.GetWindowText(text); if (mode == 1) { MSG_MailDirectoryChanged(&context); RefreshMailLists(); } else if (mode == 2) { if (!text.IsEmpty() && WFE_SetMailPreference("news", text)) { WFE_ApplyMailPreferences(); news_initial_groups_pending=true;MSG_NewsHostChanged(&context, text); RefreshMailLists(); } } else if (!text.IsEmpty()) Navigate(text); }
void BrowserFrame::OnOpen() { CFileDialog dialog(TRUE, "html", nullptr, OFN_FILEMUSTEXIST, "Documents|*.html;*.htm;*.txt|All files|*.*||", this); if (dialog.DoModal() == IDOK) { if(mode) WFE_NewWindow(dialog.GetPathName());else Navigate(dialog.GetPathName()); } }
void BrowserFrame::OnSavePage() { WFE_SavePage(&context, nullptr); }
void BrowserFrame::OnHistory() { if(mode) WFE_NewWindow("about:global");else Navigate("about:global"); }
void BrowserFrame::OnBookmarks() { WFE_BookmarksShow(&context); }
void BrowserFrame::OnAddBookmark() { WFE_BookmarksAddPage(&context); }
void BrowserFrame::OnUpdateAddBookmark(CCmdUI *command) {
    History_entry *entry = SHIST_GetCurrent(&context.hist);
    command->Enable(context.type == MWContextBrowser && entry && entry->method != URL_POST_METHOD);
}
void BrowserFrame::OnStop() { XP_InterruptContext(&context); }
void BrowserFrame::OnReload() { force_images=false;if(mode) {if(!current_url.empty()) {URL_Struct *url=NET_CreateURLStruct(current_url.c_str(),NET_NORMAL_RELOAD);if(url) FE_GetURL(&context,url);}}else WFE_HistoryMove(&context, 0); }
void BrowserFrame::OnBack() { WFE_HistoryMove(&context, -1); }
void BrowserFrame::OnForward() { WFE_HistoryMove(&context, 1); }
void BrowserFrame::OnUpdateBack(CCmdUI *command) { command->Enable(WFE_HistoryCanMove(&context, -1)); }
void BrowserFrame::OnUpdateForward(CCmdUI *command) { command->Enable(WFE_HistoryCanMove(&context, 1)); }
void BrowserFrame::OnAbout() { if(mode) WFE_NewWindow("about:");else Navigate("about:"); }
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
        CString name(' ', mode==2?0:(int)folder.depth * 2);
        if (mode!=2 && (folder.flags & MSG_FOLDER_FLAG_DIRECTORY)) name += folder.flags & MSG_FOLDER_FLAG_ELIDED ? "+ " : "- ";
        name += folder.name ? folder.name : "";
        folders.InsertItem(i, name, mail_panes.FolderImage(folder.flags));
        folders.SetItemData(i,((DWORD_PTR)folder.depth<<16)|folder.flags);
        if (!(folder.flags & MSG_FOLDER_FLAG_DIRECTORY)) {
            CString value;
            if(mode==2) {
                value=folder.unseen>=0?"":"???";if(folder.unseen>=0) value.Format("%d",folder.unseen);
                folders.SetItemText(i,2,value);
                LVITEM subscription={};subscription.mask=LVIF_IMAGE;subscription.iItem=i;subscription.iSubItem=1;
                subscription.iImage=mail_panes.SubscriptionImage(folder.flags);folders.SetItem(&subscription);
            } else {
                if(folder.unseen>=0) value.Format("%d",folder.unseen);folders.SetItemText(i,1,value);
                value.Empty();if(folder.total>=0) value.Format("%d",folder.total);folders.SetItemText(i,2,value);
            }
        }
        if (folder.flags & MSG_FOLDER_FLAG_SELECTED) folder_selection = i;
    }
    MSG_MessageLine message;
    for (int i = 0; i < 100000 && MSG_GetThreadLine(&context, i, &message, nullptr); ++i) {
        CString subject(' ', (int)message.depth * 2);
        if (message.flags & MSG_FLAG_HAS_RE) subject += "Re: ";
        subject += message.subject ? display_header(&context, message.subject) : CString("(no subject)");
        messages.InsertItem(i, display_header(&context,message.from),mail_panes.MessageImage(message.flags));
        LVITEM status_icon={};status_icon.mask=LVIF_IMAGE;status_icon.iItem=i;status_icon.iSubItem=MailPaneUI::FlagColumn;
        status_icon.iImage=(message.flags&MSG_FLAG_MARKED)?MailPaneUI::FlagImage:I_IMAGENONE;messages.SetItem(&status_icon);
        messages.SetItemText(i, MailPaneUI::SubjectColumn, subject);
        messages.SetItemText(i, MailPaneUI::DateColumn, MSG_FormatDate(&context, message.date));
        if (message.flags & MSG_FLAG_SELECTED) {message_selection=i;messages.SetItemState(i,LVIS_SELECTED,LVIS_SELECTED);}
    }
    if (folder_selection >= 0 && folder_selection < folders.GetItemCount()) {
        folders.SetItemState(folder_selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        folders.EnsureVisible(folder_selection,FALSE);
    }
    if (message_selection >= 0 && message_selection < messages.GetItemCount()) {
        messages.SetItemState(message_selection, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
        messages.EnsureVisible(message_selection,FALSE);
    }
    folders.SetRedraw(TRUE); messages.SetRedraw(TRUE);
    folders.Invalidate(); messages.Invalidate();
    updating_lists = false;
    mail_panes.FitColumns();
    MailCaptionStatus();
}
void BrowserFrame::MailCaptionStatus()
{
    if(!mode || !initialized || !context.msgframe) return;
    int selected=messages.GetNextItem(-1,LVNI_SELECTED);
    CString subject=selected>=0 ? messages.GetItemText(selected,MailPaneUI::SubjectColumn) : CString("");
    CString title=mode==2 ? "Netscape News" : "Netscape Mail";
    if(!subject.IsEmpty()) { title+=" - [";title+=subject;title+=']'; } else if(mode==2) title+=" - [news:]"; SetWindowText(title);
    int folder=folders.GetNextItem(-1,LVNI_SELECTED);MSG_FolderLine line;
    if(folder>=0 && MSG_GetFolderLine(&context,folder,&line,nullptr)) {
        CString text;
        if(mode==1) { text=WFE_MailDirectory();text+='\\'; }
        text+=line.name?line.name:"";
        if(!(line.flags&MSG_FOLDER_FLAG_DIRECTORY)&&line.total>=0) { CString count;count.Format("  %d messages",line.total);text+=count; }
        if(!(line.flags&MSG_FOLDER_FLAG_DIRECTORY)&&line.unseen>=0) { CString unread;unread.Format(" (%d unread)",line.unseen);text+=unread; }
        status.SetPaneText(1,text);
    }
}
LRESULT BrowserFrame::OnMailChanged(WPARAM, LPARAM)
{
    unsigned int changes = mail_changes;
    mail_changes = 0; mail_update_pending = false;
    if ((changes & 8) && mode == 2 && context.msgframe && news_initial_groups_pending
        && !NET_AreThereActiveConnectionsForWindow(&context)) {
        news_initial_groups_pending=false;
        bool has_groups=false;MSG_FolderLine line;
        for(int i=0;MSG_GetFolderLine(&context,i,&line,nullptr);++i)
            if(!(line.flags&MSG_FOLDER_FLAG_DIRECTORY)) { has_groups=true;break; }
        if(!has_groups && MSG_GetToggleStatus(&context,MSG_ShowAllNewsGroups)!=MSG_Checked)
            MSG_Command(&context,MSG_ShowAllNewsGroups);
    }
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
void BrowserFrame::OnMessageSelection(NMHDR *header,LRESULT *result)
{
    *result=0;NMLISTVIEW *change=(NMLISTVIEW *)header;
    if(updating_lists || !initialized || !(change->uChanged&LVIF_STATE) ||
       !((change->uOldState^change->uNewState)&LVIS_SELECTED)) return;
    pending_message_selection.clear();
    for(int row=messages.GetNextItem(-1,LVNI_SELECTED);row>=0;row=messages.GetNextItem(row,LVNI_SELECTED)) pending_message_selection.push_back(row);
    if(!selection_update_pending) {selection_update_pending=true;PostMessage(WM_MESSAGE_SELECTION);}
}
LRESULT BrowserFrame::OnMessageSelectionChanged(WPARAM,LPARAM)
{
    if(!selection_update_pending || !initialized || !context.msgframe) return 0;
    selection_update_pending=false;
    std::vector<int> selected=pending_message_selection;pending_message_selection.clear();
    completed=false;view.drawn_text.clear();view.drawn_runs=0;
    if(selected.empty() && messages.GetItemCount()>0) MSG_UnselectMessage(&context,0);
    MSG_Command(&context,MSG_UnselectAllMessages);
    if(!selected.empty()) {
        MSG_SelectMessage(&context,selected[0],TRUE);
        for(size_t i=1;i<selected.size();++i) MSG_ToggleSelectMessage(&context,selected[i]);
    }
    RefreshMailLists();view.Invalidate();return 0;
}
void BrowserFrame::OnFolderDoubleClick(NMHDR *, LRESULT *result) { *result = 0; int item = folders.GetNextItem(-1, LVNI_SELECTED); MSG_FolderLine line; if (item >= 0 && MSG_GetFolderLine(&context,item,&line,nullptr) && (line.flags&MSG_FOLDER_FLAG_DIRECTORY)) { MSG_ToggleFolderExpansion(&context, item); RefreshMailLists(); } }
void BrowserFrame::OnMessageDoubleClick(NMHDR *, LRESULT *result) { *result = 0; int item = messages.GetNextItem(-1, LVNI_SELECTED); if (item >= 0) { MSG_ToggleThreadExpansion(&context, item); RefreshMailLists(); } }
void BrowserFrame::OnMail() { WFE_NewMailWindow(0, nullptr); }
void BrowserFrame::OnNews() { WFE_NewMailWindow(1, nullptr); }
void BrowserFrame::OnCompose() { if(mode==2) MSG_Command(&context,MSG_PostNew);else WFE_NewComposer(&context,0); }
void BrowserFrame::OnPost() { if (mode == 2) MSG_Command(&context, MSG_PostNew); else WFE_NewComposer(&context, 1); }
void BrowserFrame::OnReply() { if (mode) MSG_Command(&context, MSG_ReplyToSender); }
void BrowserFrame::OnReplyAll() { if (mode) MSG_Command(&context, MSG_ReplyToAll); }
void BrowserFrame::OnForwardMessage() { if (mode) MSG_Command(&context, MSG_ForwardMessage); }
void BrowserFrame::OnGetMail() { if (mode == 1) MSG_Command(&context, MSG_GetNewMail); else if (mode == 2) MSG_Command(&context, MSG_GetMoreMessages); }
void BrowserFrame::OnSendQueued() { if (mode) MSG_Command(&context, MSG_DeliverQueuedMessages); }
void BrowserFrame::OnShowAllMessages() { if (mode) MSG_Command(&context, MSG_ShowAllMessages); }
void BrowserFrame::OnShowUnreadMessages() { if (mode) MSG_Command(&context, MSG_ShowOnlyUnreadMessages); }
void BrowserFrame::OnDeleteMessage() { if (mode == 1) MSG_Command(&context, MSG_DeleteMessage); }
void BrowserFrame::OnMarkRead() { if (mode) MSG_Command(&context, MSG_MarkSelectedRead); }

void BrowserFrame::OnMessageStep(UINT command)
{
    if(!mode || !context.msgframe) return;
    MSG_CommandType action=mode==2 ? (command==ID_MESSAGE_NEXT?MSG_NextUnreadMessage:MSG_PreviousUnreadMessage) : (command==ID_MESSAGE_NEXT?MSG_NextMessage:MSG_PreviousMessage);
    XP_Bool enabled=FALSE;
    if(MSG_CommandStatus(&context,action,&enabled,nullptr,nullptr,nullptr)<0 || !enabled) return;
    completed=false;MSG_Command(&context,action);RefreshMailLists();
}
void BrowserFrame::OnHeaderMode(UINT command)
{
    if(mode) MSG_Command(&context,command==ID_MAIL_HEADERS_ALL?MSG_ShowAllHeaders:MSG_ShowSomeHeaders);
}
void BrowserFrame::OnMessageColumn(NMHDR *header,LRESULT *result)
{
    *result=0;if(!mode) return;int column=((NMLISTVIEW*)header)->iSubItem;
    if(column==MailPaneUI::PriorityColumn || column==MailPaneUI::FlagColumn) return;
    MSG_CommandType action=column==0?MSG_SortBySender:column==MailPaneUI::SubjectColumn?MSG_SortBySubject:MSG_SortByDate;
    if(MSG_GetToggleStatus(&context,action)==MSG_Checked) MSG_Command(&context,MSG_SortBackward);
    else MSG_Command(&context,action);
    RefreshMailLists();
    bool reverse=MSG_GetToggleStatus(&context,MSG_SortBackward)==MSG_Checked;
    for(int i=0;i<messages.GetHeaderCtrl()->GetItemCount();++i) {
        HDITEM item={};item.mask=HDI_FORMAT;messages.GetHeaderCtrl()->GetItem(i,&item);
        item.fmt&=~(HDF_SORTUP|HDF_SORTDOWN);if(i==column) item.fmt|=reverse?HDF_SORTDOWN:HDF_SORTUP;
        messages.GetHeaderCtrl()->SetItem(i,&item);
    }
}

static MSG_CommandType news_command(UINT command)
{
    switch(command) {
    case ID_CLIENT_POST:return MSG_PostNew;
    case ID_NEWS_REPLY_POST:return MSG_PostReply;
    case ID_NEWS_REPLY_BOTH:return MSG_PostAndMailReply;
    case ID_NEWS_MAIL_NEW:return MSG_MailNew;
    case ID_NEWS_MARK_THREAD:return MSG_MarkThreadRead;
    case ID_NEWS_MARK_GROUP:return MSG_MarkAllRead;
    case ID_NEWS_SUBSCRIBED:return MSG_ShowSubscribedNewsGroups;
    case ID_NEWS_ALL_GROUPS:return MSG_ShowAllNewsGroups;
    case ID_NEWS_THREADED:return MSG_ThreadMessages;
    default:return MSG_SubscribeSelectedGroups;
    }
}
void BrowserFrame::OnUpdateNews(CCmdUI *command)
{
    if(command->m_nID==ID_CLIENT_POST && mode!=2) { command->Enable(TRUE);return; }
    if(mode!=2 || !initialized || !context.msgframe) { command->Enable(FALSE);return; }
    if(command->m_nID==ID_NEWS_SUBSCRIBE) {
        int row=folders.GetNextItem(-1,LVNI_SELECTED);MSG_FolderLine line;
        bool group=row>=0&&MSG_GetFolderLine(&context,row,&line,nullptr)&&!(line.flags&MSG_FOLDER_FLAG_DIRECTORY);
        command->Enable(group);command->SetCheck(group&&(line.flags&MSG_FOLDER_FLAG_SUBSCRIBED));return;
    }
    XP_Bool enabled=FALSE;MSG_COMMAND_CHECK_STATE checked=MSG_Unchecked;
    int result=MSG_CommandStatus(&context,news_command(command->m_nID),&enabled,&checked,nullptr,nullptr);
    command->Enable(result>=0&&enabled);
    if(command->m_nID==ID_NEWS_SUBSCRIBED||command->m_nID==ID_NEWS_ALL_GROUPS||command->m_nID==ID_NEWS_THREADED) command->SetCheck(checked==MSG_Checked);
}
void BrowserFrame::OnNewsCommand(UINT command)
{
    if(mode!=2||!context.msgframe) return;
    if(command==ID_NEWS_SUBSCRIBE) {
        int row=folders.GetNextItem(-1,LVNI_SELECTED);MSG_FolderLine line;
        if(row>=0&&MSG_GetFolderLine(&context,row,&line,nullptr)&&!(line.flags&MSG_FOLDER_FLAG_DIRECTORY)) MSG_ToggleSubscribed(&context,row);
    } else {
        XP_Bool enabled=FALSE;MSG_CommandType action=news_command(command);
        if(MSG_CommandStatus(&context,action,&enabled,nullptr,nullptr,nullptr)<0||!enabled) return;
        if(command==ID_NEWS_SUBSCRIBED||command==ID_NEWS_ALL_GROUPS) news_initial_groups_pending=false;
        MSG_Command(&context,action);
    }
    RefreshMailLists();
}
void BrowserFrame::OnFolderDraw(NMHDR *header,LRESULT *result) { mail_panes.DrawNewsFolder((NMLVCUSTOMDRAW*)header,result); }
void BrowserFrame::OnFolderClick(NMHDR *header,LRESULT *result)
{
    *result=0;if(mode!=2||updating_lists||!context.msgframe) return;
    NMITEMACTIVATE *hit=(NMITEMACTIVATE*)header;if(hit->iItem<0) return;
    MSG_FolderLine line;if(!MSG_GetFolderLine(&context,hit->iItem,&line,nullptr)) return;
    if(hit->iSubItem==1 && !(line.flags&MSG_FOLDER_FLAG_DIRECTORY)) MSG_ToggleSubscribed(&context,hit->iItem);
    else if(hit->iSubItem==0 && mail_panes.NewsExpansionHit(hit->iItem,hit->ptAction)) MSG_ToggleFolderExpansion(&context,hit->iItem);
    else return;
    RefreshMailLists();
}
void BrowserFrame::OnFolderKey(NMHDR *header,LRESULT *result)
{
    *result=0;if(mode!=2||updating_lists||!context.msgframe) return;
    int row=folders.GetNextItem(-1,LVNI_SELECTED);MSG_FolderLine line;
    if(row<0||!MSG_GetFolderLine(&context,row,&line,nullptr)) return;
    WORD key=((NMLVKEYDOWN*)header)->wVKey;
    if(key==VK_SPACE && !(line.flags&MSG_FOLDER_FLAG_DIRECTORY)) MSG_ToggleSubscribed(&context,row);
    else if((line.flags&MSG_FOLDER_FLAG_DIRECTORY) && ((key==VK_LEFT&&!(line.flags&MSG_FOLDER_FLAG_ELIDED)) || (key==VK_RIGHT&&(line.flags&MSG_FOLDER_FLAG_ELIDED)))) MSG_ToggleFolderExpansion(&context,row);
    else return;
    RefreshMailLists();
}

class MailSettingsDialog : public ClassicDialog {
public:
    explicit MailSettingsDialog(CWnd *parent) : ClassicDialog(IDD_MAIL_SETTINGS, parent) { }
    BOOL OnInitDialog() override {
        ClassicDialog::OnInitDialog();
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
void BrowserFrame::OnSettings() { MailSettingsDialog dialog(this); if (dialog.DoModal() == IDOK && mode == 2 && *WFE_MailPreference("news")) { address.SetWindowText(WFE_MailPreference("news")); news_initial_groups_pending=true;MSG_NewsHostChanged(&context, WFE_MailPreference("news")); } }

extern "C" void WFE_ShowMailSettings(MWContext *context)
{
    if(BrowserFrame *frame=frame_for(context)) {frame->SendMessage(WM_COMMAND,ID_MAIL_SETTINGS);return;}
    auto window=native_windows.find(context);
    MailSettingsDialog dialog(window==native_windows.end()?nullptr:CWnd::FromHandle(window->second));dialog.DoModal();
}

class NativePrompt : public ClassicDialog {
public:
    CString label, value;
    bool password;
    NativePrompt(const char *message, const char *initial, bool secret, CWnd *parent)
        : ClassicDialog(IDD_NATIVE_PROMPT, parent), label(message ? message : ""), value(initial ? initial : ""), password(secret) { }
    BOOL OnInitDialog() override { ClassicDialog::OnInitDialog(); SetDlgItemText(ID_PROMPT_LABEL, label); SetDlgItemText(ID_PROMPT_VALUE, value); CEdit *edit = (CEdit *)GetDlgItem(ID_PROMPT_VALUE); if (password) edit->SetPasswordChar('*'); edit->SetSel(0,-1); edit->SetFocus(); return FALSE; }
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
#ifdef EDITOR
    if(editing && !WFE_EditorCheckSave(&context)) return;
#endif
    CFrameWnd::OnClose();
}
void BrowserFrame::OnDestroy()
{
    KillTimer(1);
    KillTimer(2);
    view.CancelInput();
#ifdef EDITOR
    if(editing) {WFE_EditorPublishForget(&context);WFE_EditorDestroyUI(&context);WFE_EditorForget(&context);}
#endif
    if (initialized) { initialized = false; WFE_DestroyContext(&context); }
    browser_contexts.erase(&context);
    WFE_UnregisterNativeWindow(&context);
    auto found = std::find(browser_windows.begin(), browser_windows.end(), this);
    if (found != browser_windows.end()) browser_windows.erase(found);
    CFrameWnd::OnDestroy();
}

void BrowserView::Render(CDC &dc, int width, int height)
{
    bool empty_news=frame->mode==2 && (!frame->context.msgframe || MSG_GetNumSelectedMessages(&frame->context)==0);
    dc.FillSolidRect(0, 0, width, height, empty_news?RGB(255,255,255):background);
    if(empty_news) { drawn_text.clear();drawn_runs=0;return; }
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
void BrowserView::OnLButtonDown(UINT flags,CPoint point)
{
#ifdef EDITOR
    if(frame->editing && EDT_HaveEditBuffer(&frame->context) && !EDT_IsBlocked(&frame->context)) {
        SetFocus();selecting=true;SetCapture();
        if(flags&MK_SHIFT) EDT_ExtendSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);
        else EDT_StartSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);
        return;
    }
    if(frame->editing) return;
#endif
    if(!frame->initialized) return;
    suppress_release=false;
    LO_Element *element=LO_XYToElement(&frame->context,point.x+scroll_x,point.y+scroll_y);
    if(element && element->type==LO_EDGE) {
        ClearHover();SetFocus();
        if(divider.Begin(&frame->context,&element->lo_edge,CPoint(point.x+scroll_x,point.y+scroll_y))) SetCapture();
        return;
    }
    SetFocus();selection_origin=selection_pointer=point;
    selection_dragged=(flags&MK_SHIFT)!=0;selecting=true;SetCapture();
    if(flags&MK_SHIFT) LO_ExtendSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);
    else LO_StartSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);
    Invalidate();
}
void BrowserView::OnMouseMove(UINT flags,CPoint point)
{
    if(divider.Active()) {divider.Move(CPoint(point.x+scroll_x,point.y+scroll_y));return;}
#ifdef EDITOR
    if(frame->editing && selecting && (flags&MK_LBUTTON)) {EDT_ExtendSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);return;}
#endif
    if(selecting && (flags&MK_LBUTTON)) {
        selection_pointer=point;
        if(abs(point.x-selection_origin.x)>=GetSystemMetrics(SM_CXDRAG) || abs(point.y-selection_origin.y)>=GetSystemMetrics(SM_CYDRAG)) selection_dragged=true;
        if(selection_dragged) {
            LO_ExtendSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);Invalidate();
            CRect area;GetClientRect(area);
            if(!area.PtInRect(point)) SetTimer(71,60,nullptr);else KillTimer(71);
        }
        return;
    }
    CWnd::OnMouseMove(flags,point);
    if(!frame->initialized || frame->editing) return;
    BrowserLinkHit hit=WFE_BrowserLinkAt(&frame->context,point.x+scroll_x,point.y+scroll_y);
    if(hit.link!=hovered_link) {
        ClearHover();
        if(!hit.link.empty()) {
            BrowserFrame *owner=frame;while(owner && owner->grid_parent) owner=frame_for(owner->grid_parent);
            if(owner) hover_status=(const char *)owner->status.GetPaneText(1);
            hovered_link=hit.link;WFE_Status(&frame->context,hovered_link.c_str());
            TRACKMOUSEEVENT track={sizeof(track),TME_LEAVE,GetSafeHwnd(),0};TrackMouseEvent(&track);
        }
    }
    LO_Element *element=LO_XYToElement(&frame->context,point.x+scroll_x,point.y+scroll_y);
    LPCTSTR cursor=element && element->type==LO_EDGE && element->lo_edge.movable?
        (element->lo_edge.is_vertical?IDC_SIZEWE:IDC_SIZENS):!hit.link.empty()?IDC_HAND:hit.text?IDC_IBEAM:IDC_ARROW;
    ::SetCursor(::LoadCursor(nullptr,cursor));
}
void BrowserView::ClearHover()
{
    if(hovered_link.empty()) return;
    BrowserFrame *owner=frame;while(owner && owner->grid_parent) owner=frame_for(owner->grid_parent);
    if(owner && owner->status.GetSafeHwnd() && owner->status.GetPaneText(1)==hovered_link.c_str()) owner->status.SetPaneText(1,hover_status.c_str());
    hovered_link.clear();hover_status.clear();
}
void BrowserView::OnMouseLeave() {ClearHover();}
BOOL BrowserView::OnSetCursor(CWnd *window,UINT hit,UINT message)
{
    if(hit==HTCLIENT && frame->initialized && !frame->editing) {
        CPoint point;GetCursorPos(&point);ScreenToClient(&point);
        BrowserLinkHit link=WFE_BrowserLinkAt(&frame->context,point.x+scroll_x,point.y+scroll_y);
        LO_Element *element=LO_XYToElement(&frame->context,point.x+scroll_x,point.y+scroll_y);
        if(element && element->type==LO_EDGE && element->lo_edge.movable) {
            ::SetCursor(::LoadCursor(nullptr,element->lo_edge.is_vertical?IDC_SIZEWE:IDC_SIZENS));return TRUE;
        }
        ::SetCursor(::LoadCursor(nullptr,!link.link.empty()?IDC_HAND:link.text?IDC_IBEAM:IDC_ARROW));return TRUE;
    }
    return CWnd::OnSetCursor(window,hit,message);
}
void BrowserView::OnLButtonDblClk(UINT flags,CPoint point)
{
#ifdef EDITOR
    if(frame->editing && EDT_HaveEditBuffer(&frame->context)) {EDT_DoubleClick(&frame->context,point.x+scroll_x,point.y+scroll_y);return;}
    if(frame->editing) return;
#endif
    if(frame->initialized) {
        LO_Element *element=LO_XYToElement(&frame->context,point.x+scroll_x,point.y+scroll_y);
        if(element && element->type==LO_EDGE) return;
        SetFocus();LO_DoubleClick(&frame->context,point.x+scroll_x,point.y+scroll_y);
        selecting=true;selection_dragged=true;selection_pointer=point;SetCapture();Invalidate();
    }
}
void BrowserView::CancelInput()
{
    ClearHover();
    if(selecting || divider.Active()) suppress_release=true;
    divider.End(false);
    selecting=false;selection_dragged=false;KillTimer(71);
    if(::GetCapture()==GetSafeHwnd()) ReleaseCapture();
}
void BrowserView::OnCaptureChanged(CWnd *window)
{
    if(selecting || divider.Active()) suppress_release=true;
    divider.End(false);
    selecting=false;selection_dragged=false;KillTimer(71);
    CWnd::OnCaptureChanged(window);
}
void BrowserView::OnTimer(UINT_PTR timer)
{
    if(timer!=71) {CWnd::OnTimer(timer);return;}
    if(!selecting || !frame->initialized || ::GetCapture()!=GetSafeHwnd()) {CancelInput();return;}
    CRect area;GetClientRect(area);CPoint point=selection_pointer;
    if(point.y<0) OnVScroll(SB_LINEUP,0,nullptr);
    else if(point.y>=area.bottom) OnVScroll(SB_LINEDOWN,0,nullptr);
    if(point.x<0) OnHScroll(SB_LINEUP,0,nullptr);
    else if(point.x>=area.right) OnHScroll(SB_LINEDOWN,0,nullptr);
    point.x=(std::max)(0L,(std::min)(point.x,area.right-1));
    point.y=(std::max)(0L,(std::min)(point.y,area.bottom-1));
    LO_ExtendSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);Invalidate();
}
void BrowserView::OnSetFocus(CWnd *old)
{
    CWnd::OnSetFocus(old);
#ifdef EDITOR
    if(frame->editing) WFE_EditorRefreshCaret(&frame->context);
#endif
}
void BrowserView::OnKillFocus(CWnd *next)
{
#ifdef EDITOR
    if(frame->editing) HideCaret();
#endif
    CWnd::OnKillFocus(next);
}
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
void BrowserView::OnVScroll(UINT code, UINT position, CScrollBar *)
{
    CRect area; GetClientRect(area);
    scroll_y = scroll_position(scroll_y, code, position, document_height, area.Height());
    SetScrollPos(SB_VERT, scroll_y); WFE_FormPositionAll(&frame->context); Invalidate();
#ifdef EDITOR
    if(frame->editing) { EDT_WindowScrolled(&frame->context);WFE_EditorRefreshCaret(&frame->context); }
#endif
}
void BrowserView::OnHScroll(UINT code, UINT position, CScrollBar *)
{
    CRect area; GetClientRect(area);
    scroll_x = scroll_position(scroll_x, code, position, document_width, area.Width());
    SetScrollPos(SB_HORZ, scroll_x); WFE_FormPositionAll(&frame->context); Invalidate();
#ifdef EDITOR
    if(frame->editing) { EDT_WindowScrolled(&frame->context);WFE_EditorRefreshCaret(&frame->context); }
#endif
}
BOOL BrowserView::OnMouseWheel(UINT, short delta, CPoint) { OnVScroll(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0, nullptr); return TRUE; }
void BrowserView::OnLButtonUp(UINT flags, CPoint point)
{
    SetFocus();
    if(divider.Active()) {
        divider.Move(CPoint(point.x+scroll_x,point.y+scroll_y));divider.End(true);
        if(::GetCapture()==GetSafeHwnd()) ReleaseCapture();Invalidate();return;
    }
#ifdef EDITOR
    if(frame->editing) {if(selecting) {selecting=false;ReleaseCapture();EDT_EndSelection(&frame->context,point.x+scroll_x,point.y+scroll_y);}return;}
#endif
    /* Capture loss or a load may cancel the gesture before this release.
     * Keep that cancellation until a fresh mouse-down starts another click. */
    if(suppress_release) {suppress_release=false;CancelInput();return;}
    bool suppress_link=selecting && (selection_dragged || (flags&MK_SHIFT));
    if(selecting) {LO_EndSelection(&frame->context);selecting=false;CancelInput();Invalidate();}
    if(suppress_link) return;
    LO_Element *element = LO_XYToElement(&frame->context, point.x + scroll_x, point.y + scroll_y);
    if (element && element->type == LO_IMAGE) {
        LO_ImageStruct *image = &element->lo_image;
        int x = point.x + scroll_x - image->x - image->x_offset - image->border_width;
        int y = point.y + scroll_y - image->y - image->y_offset - image->border_width;
        if (image->image_attr && (image->image_attr->attrmask & LO_ATTR_ISFORM)) {
            WFE_FormImageSubmit(&frame->context, image, x, y);
            return;
        }
    }
    BrowserLinkHit hit=WFE_BrowserLinkAt(&frame->context,point.x+scroll_x,point.y+scroll_y);
    WFE_BrowserLinkCommand(&frame->context,hit,ID_BROWSER_OPEN_LINK);
}
void BrowserView::OnContextMenu(CWnd *, CPoint point)
{
#ifdef EDITOR
    if(frame->editing) return;
#endif
    SetFocus();
    if (point.x == -1 && point.y == -1) { point = CPoint(8,8); ClientToScreen(&point); }
    CPoint document = point; ScreenToClient(&document);
    BrowserLinkHit hit=WFE_BrowserLinkAt(&frame->context,document.x+scroll_x,document.y+scroll_y);
    CMenu menu; menu.CreatePopupMenu();
    menu.AppendMenu(MF_STRING|(WFE_HistoryCanMove(&frame->context,-1)?0:MF_GRAYED),ID_NAV_BACK,"&Back");
    menu.AppendMenu(MF_STRING|(WFE_HistoryCanMove(&frame->context,1)?0:MF_GRAYED),ID_NAV_FORWARD,"&Forward");
    menu.AppendMenu(MF_STRING,ID_NAV_RELOAD,"&Reload");menu.AppendMenu(MF_SEPARATOR);
    WFE_BrowserLinkMenu(menu,hit);
    menu.AppendMenu(MF_STRING, ID_BROWSER_SAVE, "Save &Page As...");
    char *selection=(char *)LO_GetSelectionText(&frame->context);
    menu.AppendMenu(MF_STRING|(selection&&*selection?0:MF_GRAYED),ID_EDIT_COPY,"&Copy");free(selection);
    MWContext *context=&frame->context;HWND window=GetSafeHwnd();
    UINT command = menu.TrackPopupMenu(TPM_RETURNCMD|TPM_RIGHTBUTTON,point.x,point.y,this);
    /* The popup's nested loop may have closed the owning browser. */
    BrowserFrame *owner=frame_for(context);if(!owner || !::IsWindow(window)) return;
    if(WFE_BrowserLinkCommand(context,hit,command)) return;
    if (command == ID_BROWSER_SAVE) WFE_SavePage(context, nullptr);
    else if (command == ID_BROWSER_SAVE_LINK) WFE_SaveLink(context, hit.link.c_str(), nullptr);
    else if (command == ID_BROWSER_SAVE_IMAGE) WFE_SaveLink(context, hit.image.c_str(), nullptr);
    else if(command) owner->SendMessage(WM_COMMAND,command);
}

static HFONT text_font(LO_TextAttr *attribute,const GeneralPreferences &preferences,bool link)
{
    static const int heights[] = {10,10,12,16,18,24,32,48};
    int size = attribute ? (std::max)(0, (std::min)(7, (int)attribute->size)) : 3;
    int mask = attribute ? attribute->fontmask : 0;
    int attributes = attribute ? attribute->attrmask : 0;
    const char *face = attribute && attribute->font_face ? attribute->font_face : (mask & LO_FONT_FIXED ? preferences.fixed.c_str() : preferences.proportional.c_str());
    int points=mask&LO_FONT_FIXED?preferences.fixed_size:preferences.proportional_size;
    return CreateFontA(-MulDiv(heights[size],points,12), 0, 0, 0, mask & LO_FONT_BOLD ? FW_BOLD : FW_NORMAL,
                      !!(mask & LO_FONT_ITALIC), !!(attributes & LO_ATTR_UNDERLINE) && (!link || preferences.underline),
                      !!(attributes & LO_ATTR_STRIKEOUT), ANSI_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH, face);
}

extern "C" HWND WFE_DocumentWindow(MWContext *context)
{
    BrowserFrame *frame = frame_for(context);
    return frame ? frame->view.GetSafeHwnd() : nullptr;
}
extern "C" void WFE_GridEdge(MWContext *context,LO_EdgeStruct *edge)
{
    BrowserFrame *frame=frame_for(context);if(!frame) return;
    if(frame->view.drawing) WFE_PaintFrameEdge(frame->view.drawing->GetSafeHdc(),edge);
    else WFE_Invalidate(context);
}
extern "C" void WFE_FreeGridEdge(MWContext *context,LO_EdgeStruct *edge)
{
    BrowserFrame *frame=frame_for(context);if(!frame) return;
    if(frame->view.divider.Active()) {
        frame->view.divider.Forget(edge);
        if(!frame->view.divider.Active()) frame->view.CancelInput();
    }
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
#ifdef NETSCAPE_USE_BEARSSL
static const char *tls_error_message(NETTLSError error)
{
    switch(error) {
    case NET_TLS_ERROR_HOSTNAME: return "HTTPS certificate hostname does not match this site.";
    case NET_TLS_ERROR_CERT_DATE: return "HTTPS certificate has expired or is not yet valid.";
    case NET_TLS_ERROR_CERT_UNTRUSTED: return "HTTPS certificate issuer is not trusted.";
    case NET_TLS_ERROR_CERT_PURPOSE: return "HTTPS certificate purpose does not permit server authentication.";
    case NET_TLS_ERROR_CERT_MALFORMED: return "HTTPS certificate is malformed or unsupported.";
    case NET_TLS_ERROR_CA_BUNDLE: return "HTTPS CA bundle configuration error.";
    case NET_TLS_ERROR_PROTOCOL: return "HTTPS protocol or cipher negotiation failed.";
    case NET_TLS_ERROR_HANDSHAKE: return "HTTPS handshake failed or timed out.";
    case NET_TLS_ERROR_TRUNCATED: return "HTTPS transfer was truncated before a clean completion.";
    case NET_TLS_ERROR_CANCELED: return "HTTPS transfer was canceled.";
    case NET_TLS_ERROR_IDENTITY: return "HTTPS requires an ASCII DNS hostname; IP literals are unsupported.";
    case NET_TLS_ERROR_ENTROPY: return "HTTPS could not obtain secure random data.";
    case NET_TLS_ERROR_TIME: return "HTTPS could not obtain the current time for certificate validation.";
    case NET_TLS_ERROR_MEMORY: return "HTTPS could not allocate connection memory.";
    case NET_TLS_ERROR_IO: return "HTTPS connection input or output failed.";
    default: return "HTTPS connection setup failed.";
    }
}
static std::string tls_connection_details(const NETTLSInfo &info)
{
    const char *cipher=nullptr;
    switch(info.cipher_suite) {
    case 0xCCA8: cipher="TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";break;
    case 0xCCA9: cipher="TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";break;
    case 0xC02F: cipher="TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256";break;
    case 0xC02B: cipher="TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";break;
    case 0xC030: cipher="TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384";break;
    case 0xC02C: cipher="TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";break;
    case 0xC013: cipher="TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA";break;
    case 0xC009: cipher="TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA";break;
    }
    char text[128];
    const char *version=info.version==0x0303 ? "TLS 1.2" : info.version==0x0302 ? "TLS 1.1" : info.version==0x0301 ? "TLS 1.0" : "Unknown TLS version";
    sprintf(text,"Authenticated connection: %s; cipher 0x%04X",version,info.cipher_suite);
    return std::string(text)+(cipher ? std::string("\n")+cipher : "");
}
extern "C" void WFE_TLSRequestComplete(MWContext *context, const URL_Struct *url, int status)
{
    if (!url) return;
    if (context==tls_smoke_context) {
        tls_smoke_info=url->tls_info;
        tls_smoke_key_cipher=url->key_cipher ? url->key_cipher : "";
        tls_smoke_certificate_null=url->certificate==nullptr;
    }
    ++tls_smoke_completed;
    BrowserFrame *frame=frame_for(context);
    if (!frame) return;
    frame->document_tls=url->tls_info;
    frame->connection_details="No authenticated live connection for this document.";
    frame->connection_error.clear();
    if (url->tls_info.error) {
        /* A rejected CONNECT response is a proxy error, before TLS starts. */
        if (status==MK_UNABLE_TO_CONNECT_TO_PROXY)
            frame->connection_error=url->error_msg ? url->error_msg : "Unable to connect to HTTPS proxy.";
        else frame->connection_error=tls_error_message(url->tls_info.error);
        if (url->tls_info.error==NET_TLS_ERROR_CA_BUNDLE && *WFE_TLSError())
            frame->connection_error+=" "+std::string(WFE_TLSError());
    } else if (status<0)
        frame->connection_error=url->error_msg ? url->error_msg : "Document load failed.";
    if (status<0 || url->tls_info.error) frame->document_tls.authenticated=0;
    else if (url->tls_info.authenticated) frame->connection_details=tls_connection_details(url->tls_info);
    if (!frame->connection_error.empty()) {
        frame->connection_details+="\n"+frame->connection_error;
        WFE_Status(context,frame->connection_error.c_str());
    }
}
#endif
extern "C" void WFE_BeginLoad(MWContext *context, const char *url)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame || !url) return;
    frame->view.CancelInput();
#ifdef NETSCAPE_USE_BEARSSL
    frame->document_tls=NETTLSInfo();
    frame->connection_details="No authenticated live connection for this document.";
    frame->connection_error.clear();
#endif
    if(frame->current_url!=url) frame->force_images=false;
    frame->document_preferences=WFE_GeneralPreferences();WFE_ImageInitContext(context);
    if(frame->mode) {frame->current_url=url;return;}
    WFE_HistorySavePosition(context);
    frame->current_url = url;
    BrowserFrame *owner=frame;
    while(owner->grid_parent) owner=frame_for(owner->grid_parent);
    if(owner && owner->status.GetSafeHwnd()) { owner->status.busy=true;owner->status.progress=0;owner->status.Invalidate(); }
    if (frame->address.GetSafeHwnd()) frame->address.SetWindowText(url);
    if (!context->is_grid_cell) frame->chrome.Remember(url);
    if (frame->resizing) return;
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
        /* Retain SHIST's saved form/grid data when recreating a child. */
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
    if(frame && frame->view.print_spans && image) {
        frame->view.print_spans->push_back({image->y+image->y_offset,image->height+2*image->border_width});
        return;
    }
    if(frame && frame->view.printing) return;
    if (frame && std::find(frame->view.drawn_images.begin(), frame->view.drawn_images.end(), image) == frame->view.drawn_images.end())
        frame->view.drawn_images.push_back(image);
}

extern "C" void WFE_TextMetrics(MWContext *context, LO_TextStruct *text, LO_TextInfo *info)
{
    BrowserFrame *frame = frame_for(context);
    CClientDC dc(frame ? &frame->view : nullptr);
    HFONT font = text_font(text->text_attr,frame?frame->document_preferences:WFE_GeneralPreferences(),text->anchor_href!=nullptr);
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
    if(frame->view.print_spans) {
        frame->view.print_spans->push_back({text->y+text->y_offset,text->height});return;
    }
    start = (std::max)(0L, start);
    end = (std::min)((long)text->text_len, end);
    if (end <= start) return;
    CDC &dc = *frame->view.drawing;
    HFONT font = text_font(text->text_attr,frame?frame->document_preferences:WFE_GeneralPreferences(),text->anchor_href!=nullptr);
    HGDIOBJ previous = SelectObject(dc, font);
    SIZE offset = {0,0};
    if (start) GetTextExtentPoint32A(dc, (char *)text->text, start, &offset);
    LO_TextAttr *attribute = text->text_attr;
    if (attribute) {
        SetTextColor(dc, RGB(attribute->fg.red, attribute->fg.green, attribute->fg.blue));
        SetBkColor(dc, RGB(attribute->bg.red, attribute->bg.green, attribute->bg.blue));
    }
    bool selection=!frame->view.printing && (text->ele_attrmask&LO_ELE_SELECTED)!=0;
    int first=selection?(std::max)((int)start,text->sel_start<0?0:(int)text->sel_start):(int)end;
    int last=selection?(std::min)((int)end,text->sel_end<0?(int)text->text_len:(int)text->sel_end+1):(int)end;
    auto draw=[&](int begin,int finish,bool selected) {
        if(finish<=begin) return;
        SIZE prefix={0,0};if(begin) GetTextExtentPoint32A(dc,(char *)text->text,begin,&prefix);
        SetTextColor(dc,selected?GetSysColor(COLOR_HIGHLIGHTTEXT):attribute?RGB(attribute->fg.red,attribute->fg.green,attribute->fg.blue):RGB(0,0,0));
        SetBkColor(dc,selected?GetSysColor(COLOR_HIGHLIGHT):attribute?RGB(attribute->bg.red,attribute->bg.green,attribute->bg.blue):RGB(255,255,255));
        SetBkMode(dc,background||selected?OPAQUE:TRANSPARENT);
        TextOutA(dc,text->x+text->x_offset+prefix.cx,text->y+text->y_offset,(char *)text->text+begin,finish-begin);
    };
    if(selection && first<last) {draw(start,first,false);draw(first,last,true);draw(last,end,false);}
    else draw(start,end,false);
    SelectObject(dc, previous);
    DeleteObject(font);
    if (smoke_mode && !frame->view.printing) {
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
    if(frame->view.print_spans) {frame->view.print_spans->push_back({y,height});return;}
    CRect area(x,y,x+width,y+height);
    if (fill) frame->view.drawing->FillSolidRect(area, color);
    else { CBrush brush(color); frame->view.drawing->FrameRect(area, &brush); }
}
extern "C" void WFE_Status(MWContext *context, const char *message) { BrowserFrame *frame = frame_for(context); while (frame && frame->grid_parent) frame = frame_for(frame->grid_parent); if (frame && frame->status.GetSafeHwnd()) frame->status.SetPaneText(1, message ? message : ""); else WFE_ComposerStatus(context, message); }
extern "C" void WFE_ChromeProgress(MWContext *context, int32 received, int32 length)
{
    BrowserFrame *frame=frame_for(context);
    while(frame && frame->grid_parent) frame=frame_for(frame->grid_parent);
    if(!frame || !frame->status.GetSafeHwnd()) return;
    int value=length>0 ? (int)((std::min)(100LL,(std::max)(0LL,((long long)received*100)/length))) : 0;
    if(value!=frame->status.progress) { frame->status.progress=value;frame->status.Invalidate(); }
}
extern "C" void WFE_Title(MWContext *context, const char *title) { BrowserFrame *frame = frame_for(context); if (frame) { std::string caption = std::string(frame->editing ? "Netscape Editor - [" : frame->mode==1 ? "Netscape Mail - [" : frame->mode==2 ? "Netscape News - [" : "Netscape - [") + (title ? title : "Untitled") + "]"; frame->SetWindowText(caption.c_str()); } }
extern "C" void WFE_Invalidate(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame && frame->view.GetSafeHwnd()) frame->view.Invalidate(); }
extern "C" void WFE_WindowSize(MWContext *context, int32 *width, int32 *height) { BrowserFrame *frame = frame_for(context); CRect area(0,0,800,600); if (frame) frame->view.GetClientRect(area); *width = area.Width(); *height = area.Height(); }
extern "C" void WFE_FullWindowSize(MWContext *context,int32 *width,int32 *height)
{
    /* As in XFE, framesets occupy the entire pane, without parent scrollbars.
     * Measuring before removing them changes the grid's stored dimensions and
     * makes LO discard resized frame geometry when restoring history. */
    BrowserFrame *frame=frame_for(context);
    if(frame) {
        frame->view.grid_document=true;frame->view.scroll_x=frame->view.scroll_y=0;
        frame->view.ShowScrollBar(SB_BOTH,FALSE);
    }
    WFE_WindowSize(context,width,height);
}
extern "C" void WFE_DocumentSize(MWContext *context, int32 width, int32 height)
{
    BrowserFrame *frame = frame_for(context);
    if (!frame) return;
    BrowserView &view = frame->view;
    view.document_width = width; view.document_height = height;
    if (view.grid_document || (context->is_grid_cell && frame->grid_scrolling == LO_SCROLL_NO)) {
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
extern "C" void WFE_Scroll(MWContext *context, int32 x, int32 y)
{
    BrowserFrame *frame = frame_for(context);
    if (frame) {
#ifdef EDITOR
        if(frame->editing) {
            CRect area;frame->view.GetClientRect(area);
            x=(std::max)(0L,(std::min)(x,(long)(std::max)(0,frame->view.document_width-area.Width())));
            y=(std::max)(0L,(std::min)(y,(long)(std::max)(0,frame->view.document_height-area.Height())));
        }
#endif
        frame->view.scroll_x = x; frame->view.scroll_y = y;
        WFE_FormPositionAll(context); WFE_Invalidate(context);
#ifdef EDITOR
        if(frame->editing) {
            frame->view.SetScrollPos(SB_HORZ,x);frame->view.SetScrollPos(SB_VERT,y);
            EDT_WindowScrolled(context);WFE_EditorRefreshCaret(context);
        }
#endif
    }
}
extern "C" void WFE_Position(MWContext *context, int32 *x, int32 *y) { BrowserFrame *frame = frame_for(context); *x = frame ? frame->view.scroll_x : 0; *y = frame ? frame->view.scroll_y : 0; }
extern "C" void WFE_ConfigureChrome(MWContext *context,const Chrome *chrome)
{
    BrowserFrame *frame=frame_for(context);if(!frame||!chrome) return;
    frame->chrome.toolbar_visible=chrome->show_button_bar!=0;
    frame->chrome.location_visible=chrome->show_url_bar!=0;
    frame->chrome.directory_visible=chrome->show_directory_buttons!=0;
    frame->status_visible=chrome->show_bottom_status_bar!=0;
    if(!chrome->show_menu) frame->SetMenu(nullptr);
    CRect area;frame->GetClientRect(area);frame->SendMessage(WM_SIZE,SIZE_RESTORED,MAKELPARAM(area.Width(),area.Height()));
    frame->DrawMenuBar();
}
extern "C" void WFE_DocumentAddress(MWContext *context,const char *address)
{
    BrowserFrame *frame=frame_for(context);if(!frame||!address) return;
    frame->current_url=address;
    if(!frame->mode && frame->address.GetSafeHwnd()) frame->address.SetWindowText(address);
}
extern "C" void WFE_DocumentDefaults(MWContext *context)
{
    BrowserFrame *frame=frame_for(context);if(!frame) return;
    frame->view.CancelInput();
    frame->view.grid_document=false;
    const GeneralPreferences &preferences=frame->document_preferences;
    COLORREF color=preferences.background_custom?preferences.colors[0]:RGB(255,255,255);
    frame->view.background=color;WFE_ImageBackground(context,GetRValue(color),GetGValue(color),GetBValue(color));
}
extern "C" void WFE_Background(MWContext *context, unsigned long color) { BrowserFrame *frame = frame_for(context); if (frame) frame->view.background = color; }
extern "C" void WFE_Unsupported(MWContext *context, const char *feature) { std::string message = std::string(feature) + " is not available in this Windows port yet."; OutputDebugStringA(message.c_str()); WFE_Status(context, message.c_str()); }
extern "C" int WFE_Message(MWContext *context, const char *message, int confirm) { if (smoke_mode) { smoke_error = message ? message : ""; return 0; } BrowserFrame *frame = frame_for(context); return MessageBoxA(frame ? frame->GetSafeHwnd() : nullptr, message ? message : "", "Netscape", confirm ? MB_YESNO | MB_ICONQUESTION : MB_OK | MB_ICONINFORMATION) == IDYES; }
extern "C" void WFE_Complete(MWContext *context, int status) { BrowserFrame *frame = frame_for(context); if (frame) { frame->completed = true; frame->resizing = false; frame->load_status = status; WFE_Invalidate(context); } }
extern "C" int WFE_IsPrinting(MWContext *context)
{BrowserFrame *frame=frame_for(context);return frame && frame->view.printing;}
extern "C" void WFE_RecordPrintSpan(MWContext *context,int y,int height)
{BrowserFrame *frame=frame_for(context);if(frame && frame->view.print_spans) frame->view.print_spans->push_back({y,height});}
bool WFE_PrintDocumentSize(MWContext *context,int *width,int *height)
{
    BrowserFrame *frame=frame_for(context);
    if(!frame || !frame->initialized || !width || !height || XP_IsContextBusy(context)) return false;
    /* Frames are printed as the focused document, not as a cropped frameset. */
    for(auto child:browser_windows) if(child->grid_parent==context) return false;
    CRect area;frame->view.GetClientRect(area);
    *width=(std::max)(frame->view.document_width,area.Width());
    *height=frame->view.document_height;
    return *width>0 && *height>0;
}
bool WFE_PrintDocumentPage(MWContext *context,HDC target,int top,int width,int height)
{
    BrowserFrame *frame=frame_for(context);
    if(!frame || !target || top<0 || width<=0 || height<=0 || width>1000000 || top>INT_MAX-height) return false;
    int saved=::SaveDC(target);if(!saved) return false;
    POINT origin={};GetWindowOrgEx(target,&origin);
    SetWindowOrgEx(target,origin.x,origin.y+top,nullptr);
    IntersectClipRect(target,0,top,width,top+height);
    CDC dc;dc.Attach(target);
    CDC *old=frame->view.drawing;bool printing=frame->view.printing;
    frame->view.drawing=&dc;frame->view.printing=true;
    if(!frame->view.print_spans) {
        dc.FillSolidRect(0,top,width,height,frame->view.background);
        if(frame->view.backdrop) WFE_DrawBackdrop(target,context,frame->view.backdrop,width,height,0,top);
    }
    LO_RefreshArea(context,0,top,width,height);
    frame->view.drawing=old;frame->view.printing=printing;
    dc.Detach();::RestoreDC(target,saved);return true;
}
int WFE_PrintPageHeight(MWContext *context,int top,int maximumHeight)
{
    BrowserFrame *frame=frame_for(context);int width=0,height=0;
    if(maximumHeight<=0 || !WFE_PrintDocumentSize(context,&width,&height)) return 0;
    std::vector<std::pair<int,int>> spans;
    CClientDC screen(&frame->view);CDC measure;measure.CreateCompatibleDC(&screen);
    auto old=frame->view.print_spans;frame->view.print_spans=&spans;
    bool measured=WFE_PrintDocumentPage(context,measure,top,width,maximumHeight);
    frame->view.print_spans=old;if(!measured) return 0;
    int bottom=top+maximumHeight;
    bool changed;
    do {
        changed=false;
        for(auto span:spans) if(span.second>0 && span.second<=maximumHeight && span.first>top &&
            span.first<bottom && (long long)span.first+span.second>bottom) {
            bottom=span.first;changed=true;
        }
    } while(changed);
    return bottom>top ? bottom-top : maximumHeight;
}

static void fit_reference_window(CWnd *window,int width,int height)
{
    MONITORINFO monitor={};monitor.cbSize=sizeof(monitor);CRect work;
    if(GetMonitorInfo(MonitorFromWindow(window->GetSafeHwnd(),MONITOR_DEFAULTTONEAREST),&monitor)) work=monitor.rcWork;
    else SystemParametersInfo(SPI_GETWORKAREA,0,&work,0);
    CRect bounds;window->GetWindowRect(bounds);
    width=(std::min)(ClassicUI::Scale(window,width),work.Width());height=(std::min)(ClassicUI::Scale(window,height),work.Height());
    int left=(std::max)(work.left,(std::min)(bounds.left,work.right-width));
    int top=(std::max)(work.top,(std::min)(bounds.top,work.bottom-height));
    window->SetWindowPos(nullptr,left,top,width,height,SWP_NOZORDER|SWP_NOACTIVATE);
}
extern "C" MWContext *WFE_NewWindow(const char *address)
{
    BrowserFrame *frame = new BrowserFrame;
    if (!frame->Create(nullptr, "Netscape 3.02", WS_OVERLAPPEDWINDOW, CRect(80,80,1100,800), nullptr, MAKEINTRESOURCE(IDR_MAIN))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAIN), TRUE);
    frame->LoadAccelTable(MAKEINTRESOURCE(IDR_MAIN));
    fit_reference_window(frame,1105,765);
    browser_windows.push_back(frame);
    WFE_RegisterNativeWindow(&frame->context, frame->GetSafeHwnd());
    if (!AfxGetApp()->m_pMainWnd) AfxGetApp()->m_pMainWnd = frame;
    if (!smoke_mode) frame->ShowWindow(SW_SHOW);
    if (address) frame->Navigate(address);
    return &frame->context;
}
extern "C" void WFE_CloseWindow(MWContext *context) { BrowserFrame *frame = frame_for(context); if (frame) frame->SendMessage(WM_CLOSE); else WFE_ComposerClose(context); }
#ifdef EDITOR
extern "C" const char *WFE_CurrentDocumentURL(MWContext *context)
{
    BrowserFrame *frame=frame_for(context);return frame?frame->current_url.c_str():"";
}
static MWContext *new_editor_window(const char *address,bool copy)
{
    if(!copy && address && *address) {
        std::string url=as_url(address);
        for(auto existing:browser_windows) if(existing->editing && !existing->new_document_copy && !existing->context.is_new_document && existing->current_url==url) {existing->ShowWindow(SW_SHOW);existing->SetForegroundWindow();return &existing->context;}
    }
    BrowserFrame *frame=new BrowserFrame;frame->editing=true;frame->new_document_copy=copy;
    if(!frame->Create(nullptr,"Netscape Editor - [file:///Untitled]",WS_OVERLAPPEDWINDOW,CRect(80,80,1185,845),nullptr,MAKEINTRESOURCE(IDR_EDITOR))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_EDITOR),TRUE);
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_EDITOR),FALSE);frame->LoadAccelTable(MAKEINTRESOURCE(IDR_EDITOR_ACCEL));
    fit_reference_window(frame,1105,765);browser_windows.push_back(frame);WFE_RegisterNativeWindow(&frame->context,frame->GetSafeHwnd());
    if(!smoke_mode) frame->ShowWindow(SW_SHOW);
    frame->Navigate(address&&*address?address:"about:editfilenew");return &frame->context;
}
extern "C" MWContext *WFE_NewEditorWindow(const char *address) {return new_editor_window(address,false);}
extern "C" MWContext *WFE_NewEditorDocument(const char *address) {return new_editor_window(address,true);}
extern "C" void WFE_EditorCopyLoaded(MWContext *context)
{
    BrowserFrame *frame=frame_for(context);
    if(frame && frame->new_document_copy && EDT_HaveEditBuffer(context)) {
        frame->new_document_copy=false;context->is_new_document=TRUE;
        EDT_SetDirtyFlag(context,TRUE);
        WFE_Status(context,"New document from template. Use Save to choose a file name.");
    }
}
#endif
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
                       CRect(70,70,870,670), nullptr, MAKEINTRESOURCE(is_news?IDR_NEWS:IDR_MAIL))) return nullptr;
    frame->SetIcon(AfxGetApp()->LoadIcon(IDR_MAIN), TRUE);
    frame->LoadAccelTable(MAKEINTRESOURCE(is_news?IDR_NEWS_ACCEL:IDR_MAIL_ACCEL));
    fit_reference_window(frame,805,605);
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
static void chrome_client_size(BrowserFrame *frame,int width,int height)
{
    CRect outer(0,0,width,height);
    AdjustWindowRectEx(&outer,frame->GetStyle(),TRUE,frame->GetExStyle());
    frame->SetWindowPos(nullptr,0,0,outer.Width(),outer.Height(),SWP_NOMOVE|SWP_NOZORDER|SWP_NOACTIVATE);
}
static bool chrome_capture(CWnd *frame,const char *path,bool full=false)
{
    CRect area;if(full) { frame->GetWindowRect(area);area.OffsetRect(-area.left,-area.top); } else frame->GetClientRect(area);
    BITMAPINFO info={};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=area.Width();info.bmiHeader.biHeight=-area.Height();
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    CClientDC screen(frame);CDC memory;memory.CreateCompatibleDC(&screen);
    void *pixels=nullptr;HBITMAP bitmap=CreateDIBSection(screen,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
    if(!bitmap) return false;
    HGDIOBJ old=SelectObject(memory,bitmap);
    frame->RedrawWindow(nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW);
    bool captured=PrintWindow(frame->GetSafeHwnd(),memory,full ? 0 : PW_CLIENTONLY)!=0;
    DWORD bytes=area.Width()*area.Height()*4;
    BITMAPFILEHEADER file_header={};file_header.bfType=0x4d42;
    file_header.bfOffBits=sizeof(file_header)+sizeof(info.bmiHeader);file_header.bfSize=file_header.bfOffBits+bytes;
    FILE *file=fopen(path,"wb");bool written=false;
    if(file) {
        written=fwrite(&file_header,sizeof(file_header),1,file)==1 &&
            fwrite(&info.bmiHeader,sizeof(info.bmiHeader),1,file)==1 && fwrite(pixels,bytes,1,file)==1;
        written=fclose(file)==0 && written;
    }
    SelectObject(memory,old);DeleteObject(bitmap);return captured&&written;
}
static void dump_native_menu(FILE *file,const char *name,CMenu *menu,int depth)
{
    for(int i=0;menu && i<menu->GetMenuItemCount();++i) {
        CString label;menu->GetMenuString(i,label,MF_BYPOSITION);
        CMenu *child=menu->GetSubMenu(i);UINT flags=menu->GetMenuState(i,MF_BYPOSITION);
        int id=child?-1:(int)menu->GetMenuItemID(i);if(!child && (flags&MF_SEPARATOR)) id=0;
        fprintf(file,"%s\t%d\t%d\t%u\t%s\n",name,depth,id,flags,(const char *)label);
        if(child) dump_native_menu(file,name,child,depth+1);
    }
}
static bool dump_native_menus(const char *report)
{
    FILE *file=fopen((std::string(report)+".menus.tsv").c_str(),"wb");if(!file) return false;
    const char *names[]={"browser","mail","news","composer"};
    const int ids[]={IDR_MAIN,IDR_MAIL,IDR_NEWS,IDR_COMPOSER_MENU};bool loaded=true;
    for(int i=0;i<4;++i) {CMenu menu;loaded=menu.LoadMenu(ids[i])&&loaded;dump_native_menu(file,names[i],&menu,0);}
#ifdef EDITOR
    CMenu editor;loaded=editor.LoadMenu(IDR_EDITOR)&&loaded;dump_native_menu(file,"editor",&editor,0);
#endif
    return fclose(file)==0&&loaded;
}
static bool original_image_menu_smoke(BrowserFrame *frame,const char *report,const char *base)
{
    if(!automatic_images) frame->SendMessage(WM_COMMAND,ID_MENU_AUTO_IMAGES);
    frame->SendMessage(WM_COMMAND,ID_MENU_AUTO_IMAGES);
    frame->Navigate((std::string(base)+"auto-first").c_str());browser_smoke_wait(frame);
    bool off=!WFE_LoadImagesAutomatically(&frame->context);
    frame->SendMessage(WM_COMMAND,ID_BROWSER_IMAGES);browser_smoke_wait(frame);
    bool forced=WFE_LoadImagesAutomatically(&frame->context)!=0;
    frame->Navigate((std::string(base)+"auto-second").c_str());browser_smoke_wait(frame);
    bool next=!WFE_LoadImagesAutomatically(&frame->context);
    frame->SendMessage(WM_COMMAND,ID_MENU_AUTO_IMAGES);
    frame->Navigate((std::string(base)+"auto-second").c_str());browser_smoke_wait(frame);
    bool enabled=WFE_LoadImagesAutomatically(&frame->context)!=0;
    FILE *file=fopen(report,"ab");if(!file) return false;
    bool passed=off&&forced&&next&&enabled;
    fprintf(file,"menu_auto_images=%d\nauto_off=%d\nauto_force_current=%d\nauto_next_off=%d\nauto_on=%d\n",passed,off,forced,next,enabled);fclose(file);
    return passed;
}
static int WFE_RunChromeSmoke(const char *report,const char *url,const char *snapshot)
{
    if(!dump_native_menus(report)) return 2;
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,900,650);frame->ShowWindow(SW_SHOWNOACTIVATE);
    frame->Navigate(url);browser_smoke_wait(frame);frame_text(frame);frame->chrome.UpdateCommands();
    bool complete=frame->completed && frame->load_status>=0;
    CString labels=frame->chrome.Labels(), menu_labels;
    CMenu *menu=frame->GetMenu();
    for(int i=0;menu && i<menu->GetMenuItemCount();++i) {
        CString text;menu->GetMenuString(i,text,MF_BYPOSITION);text.Replace("&","");
        if(!menu_labels.IsEmpty()) menu_labels+='|';menu_labels+=text;
    }
    auto enabled=[frame](UINT id){ CWnd *b=frame->chrome.Button(id);return b&&b->IsWindowEnabled(); };
    bool edit_enabled=false;
#ifdef EDITOR
    edit_enabled=true;
#endif
    bool state=!enabled(ID_NAV_BACK)&&!enabled(ID_NAV_FORWARD)&&enabled(ID_NAV_HOME)&&enabled(ID_NAV_RELOAD)&&
        enabled(ID_BROWSER_EDIT)==edit_enabled&&enabled(ID_BROWSER_IMAGES)&&enabled(ID_BROWSER_PRINT)&&enabled(ID_BROWSER_FIND)&&!enabled(ID_NAV_STOP);
    bool find=WFE_FindDialogSmoke(&frame->context,"Classic toolbar needle")!=0;
    LO_Element *first=nullptr,*last=nullptr;int32 a=0,b=0;
    LO_GetSelectionEndpoints(&frame->context,&first,&last,&a,&b);
    find=find&&first&&last&&WFE_FindText(&frame->context,"Classic toolbar needle",0,0,1)&&
        !WFE_FindText(&frame->context,"missing classic text",0,0,0);
    std::string next=url;next+="chrome-next";
    frame->address.SetWindowText(next.c_str());frame->address.SetFocus();
    MSG enter={};enter.hwnd=frame->address.GetSafeHwnd();enter.message=WM_KEYDOWN;enter.wParam=VK_RETURN;
    bool location=frame->PreTranslateMessage(&enter)!=FALSE;
    browser_smoke_wait(frame);frame_text(frame);frame->chrome.UpdateCommands();
    CString caption;frame->GetWindowText(caption);
    location=location&&frame->current_url==next&&caption.Find("Classic chrome B")>=0;
    bool history=enabled(ID_NAV_BACK)&&!enabled(ID_NAV_FORWARD);
    frame->SendMessage(WM_COMMAND,ID_NAV_BACK);browser_smoke_wait(frame);frame->chrome.UpdateCommands();
    history=history&&enabled(ID_NAV_FORWARD);
    CRect before,hidden,restored;frame->view.GetWindowRect(before);bool toggles=true;
    for(UINT command=ID_VIEW_TOOLBAR;command<=ID_VIEW_STATUS;++command) frame->SendMessage(WM_COMMAND,command);
    frame->view.GetWindowRect(hidden);
    toggles=hidden.Height()>before.Height()&&!frame->chrome.location.IsWindowVisible()&&!frame->status.IsWindowVisible();
    for(UINT command=ID_VIEW_TOOLBAR;command<=ID_VIEW_STATUS;++command) frame->SendMessage(WM_COMMAND,command);
    frame->view.GetWindowRect(restored);toggles=toggles&&restored.Height()==before.Height();
    chrome_client_size(frame,340,500);
    CRect client,view;frame->GetClientRect(client);frame->view.GetWindowRect(view);frame->ScreenToClient(view);
    bool narrow=client.PtInRect(view.TopLeft())&&view.right<=client.right&&view.bottom<=client.bottom&&view.Width()>=120&&view.Height()>=80;
    chrome_client_size(frame,900,650);browser_smoke_wait(frame);
    bool capture=chrome_capture(frame,snapshot);
    std::string full_capture=std::string(snapshot)+".window.bmp";
    chrome_capture(frame,full_capture.c_str(),true);
    bool dropdown=(frame->chrome.location.GetStyle()&CBS_DROPDOWN)!=0;
    BrowserFrame *about=frame_for(WFE_NewWindow("about:"));
    bool about_page=false;
    if(about) {
        chrome_client_size(about,655,410);about->ShowWindow(SW_SHOWNOACTIVATE);browser_smoke_wait(about);
        std::string text=frame_text(about);
        about_page=text.find("Netscape Navigator")!=std::string::npos&&text.find("3.02")!=std::string::npos&&about->view.drawn_images.size()==1&&WFE_AboutOutstanding()==0;
#ifdef EDITOR
        about_page=about_page&&text.find("3.02Gold")!=std::string::npos;
#endif
        chrome_capture(about,(std::string(snapshot)+".about.bmp").c_str(),true);
        about->SendMessage(WM_CLOSE);
    }
    bool passed=complete&&state&&find&&location&&history&&toggles&&narrow&&capture&&about_page;
    FILE *file=fopen(report,"wb");
    if(file) {
#ifdef EDITOR
        fputs("edition=gold\n",file);
#else
        fputs("edition=standard\n",file);
#endif
        fprintf(file,"passed=%d\ncompleted=%d\ntoolbar_state=%d\nlocation_enter=%d\nhistory_state=%d\nfind_text=%d\nview_toggles=%d\nnarrow_layout=%d\nlocation_dropdown=%d\ntoolbar_labels=%s\nmenu_labels=%s\ntoolbar_count=%u\ndirectory_count=6\nstatus_panes=4\nnarrow_client=%d,%d\nnarrow_view=%d,%d,%d,%d\ncapture=%d\nabout_page=%d\n",
            passed,complete,state,location,history,find,toggles,narrow,dropdown,(const char*)labels,(const char*)menu_labels,
            (unsigned)frame->chrome.ButtonCount(),client.Width(),client.Height(),view.left,view.top,view.right,view.bottom,capture,about_page);
        fclose(file);
    } else passed=false;
    passed=original_image_menu_smoke(frame,report,url)&&passed;
    passed=original_browser_menu_smoke(frame,report,url)&&passed;
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}

static int WFE_UIGallery(const char *directory)
{
    CreateDirectoryA(directory,nullptr);
    std::string folder=directory;
    BrowserFrame *browser=frame_for(WFE_NewWindow("about:"));if(!browser) return 2;
    chrome_client_size(browser,655,410);browser->ShowWindow(SW_SHOWNOACTIVATE);browser_smoke_wait(browser);
    bool saved=chrome_capture(browser,(folder+"/browser.bmp").c_str(),true);
    MWContext *mail=WFE_NewMailWindow(0,nullptr);
    if(BrowserFrame *frame=frame_for(mail)) { frame->ShowWindow(SW_SHOWNOACTIVATE);frame->chrome.UpdateCommands();saved=chrome_capture(frame,(folder+"/mail.bmp").c_str(),true)&&saved; }
    WFE_SetMailPreference("email","preview@example.invalid");
    WFE_SetMailPreference("name","Netscape User"); WFE_ApplyMailPreferences();
    WFE_NewComposer(&browser->context,0);
    saved=WFE_ComposerWindowCount()!=0&&saved;
    for(auto &item:native_windows) if(item.first->type==MWContextMessageComposition) {
        CWnd *window=CWnd::FromHandle(item.second);window->ShowWindow(SW_SHOWNOACTIVATE);
        saved=chrome_capture(window,(folder+"/composer.bmp").c_str(),true)&&saved;break;
    }
    WFE_BookmarksShow(&browser->context);
    for(auto &item:native_windows) if(item.first->type==MWContextBookmarks) {
        CWnd *window=CWnd::FromHandle(item.second);saved=chrome_capture(window,(folder+"/bookmarks.bmp").c_str(),true)&&saved;break;
    }
    class SettingsCapture : public MailSettingsDialog {
        std::string path;
    public:
        SettingsCapture(CWnd *owner,const std::string &file):MailSettingsDialog(owner),path(file) { }
        BOOL OnInitDialog() override {
            MailSettingsDialog::OnInitDialog();ShowWindow(SW_SHOWNOACTIVATE);
            chrome_capture(this,path.c_str(),true);PostMessage(WM_COMMAND,IDCANCEL);return TRUE;
        }
    } settings(browser,folder+"/settings.bmp");
    settings.DoModal();
    class PromptCapture : public NativePrompt {
        std::string path;
    public:
        PromptCapture(CWnd *owner,const std::string &file):NativePrompt("Bookmark name:","Netscape Navigator",false,owner),path(file) { }
        BOOL OnInitDialog() override {
            NativePrompt::OnInitDialog();ShowWindow(SW_SHOWNOACTIVATE);
            chrome_capture(this,path.c_str(),true);PostMessage(WM_COMMAND,IDCANCEL);return FALSE;
        }
    } prompt(browser,folder+"/prompt.bmp");
    prompt.DoModal();
    WFE_ComposerCloseAll();
    if(mail) WFE_CloseWindow(mail);
    WFE_CloseWindow(&browser->context);
    return saved ? 0 : 1;
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
        CPoint point = link.second - CPoint(frame->view.scroll_x, frame->view.scroll_y);
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
    bool form = false, header_target = false, targeted = false, grid_history = false, frame_find = false, resized = false, nested = false, parent_target = false, top_target = false;
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
        root->ShowWindow(SW_SHOWNOACTIVATE); content->view.SetFocus();
        root->FocusedDocument();
        root->chrome.Button(ID_BROWSER_FIND)->SetFocus();
        MWContext *focused=root->FocusedDocument();
        frame_find=focused==&content->context && WFE_FindDialogSmoke(focused,"Targeted content frame");
        root->SendMessage(WM_COMMAND, ID_NAV_BACK); browser_smoke_wait(root);
        content = named_frame(&root->context, "content");
        bool back = frame_text(content).find("Header transfer finished") != std::string::npos;
        root->SendMessage(WM_COMMAND, ID_NAV_FORWARD); browser_smoke_wait(root);
        content = named_frame(&root->context, "content");
        grid_history = back && frame_text(content).find("Targeted content frame") != std::string::npos;
        navigation = named_frame(&root->context, "navigation");
        CRect before; if (navigation) navigation->GetWindowRect(before);
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
    fprintf(output, "child_count=%d\ninitial_render=%d\nform=%d\nheader_target=%d\ntargeted=%d\ngrid_history=%d\nframe_find=%d\nresized=%d\nnested=%d\nparent_target=%d\ntop_target=%d\ncleanup=%d\nerror=%s\n", count, initial, form, header_target, targeted, grid_history, frame_find, resized, nested, parent_target, top_target, cleanup, smoke_error.c_str());
    fclose(output);
    return initial && form && header_target && targeted && grid_history && frame_find && resized && nested && parent_target && top_target && cleanup ? 0 : 4;
}
static std::string control_text(HWND control);
static int WFE_RunHistorySmoke(const char *report, const char *url)
{
    BrowserFrame *frame = frame_for(WFE_NewWindow(nullptr));
    if (!frame) return 2;
    MWContext *context = &frame->context;
    frame->Navigate(url); browser_smoke_wait(frame);
    frame_text(frame);
    GH_SetGlobalHistoryTimeout(3600);
    bool global_recent = GH_CheckGlobalHistory((char *)url) == 1;
    History_entry *first = SHIST_GetCurrent(&context->hist);
    int32 first_id = first ? first->unique_id : 0;
    bool initial_state = !WFE_HistoryCanMove(context, -1) && !WFE_HistoryCanMove(context, 1);
    HWND query = WFE_FormFindControl(context, "query", 0);
    HWND pick = WFE_FormFindControl(context, "pick", 0);
    if (query) SetWindowTextA(query, "history & value");
    if (pick) SendMessage(pick, CB_SETCURSEL, 1, 0);
    WFE_Scroll(context, 0, 80);
    bool clicked = click_frame_link(frame, "Next page"); browser_smoke_wait(frame);
    bool page_b = frame_text(frame).find("History B") != std::string::npos;
    frame->SendMessage(WM_COMMAND, ID_NAV_BACK); browser_smoke_wait(frame);
    bool back_a = frame_text(frame).find("History A") != std::string::npos;
    bool restored = control_text(WFE_FormFindControl(context, "query", 0)) == "history & value"
        && SendMessage(WFE_FormFindControl(context, "pick", 0), CB_GETCURSEL, 0, 0) == 1;
    bool scroll_restored = frame->view.scroll_y > 0;
    bool back_state = !WFE_HistoryCanMove(context, -1) && WFE_HistoryCanMove(context, 1);
    frame->SendMessage(WM_COMMAND, ID_NAV_FORWARD); browser_smoke_wait(frame);
    bool forward_b = frame_text(frame).find("History B") != std::string::npos;
    frame->SendMessage(WM_COMMAND, ID_NAV_BACK); browser_smoke_wait(frame);
    bool third = click_frame_link(frame, "Third page"); browser_smoke_wait(frame);
    bool page_c = frame_text(frame).find("History C") != std::string::npos;
    bool truncated = third && page_c && !WFE_HistoryCanMove(context, 1) && context->hist.num_entries == 2;
    int count = context->hist.num_entries;
    frame->SendMessage(WM_COMMAND, ID_NAV_RELOAD); browser_smoke_wait(frame);
    bool reloaded = frame_text(frame).find("History C") != std::string::npos && context->hist.num_entries == count;
    bool dialog = first_id && WFE_HistoryDialogSmoke(context, first_id);
    if (dialog) { browser_smoke_wait(frame); dialog = frame_text(frame).find("History A") != std::string::npos; }
    std::string post_page(url); post_page.resize(post_page.find_last_of('/') + 1); post_page += "history-c.html";
    frame->Navigate(post_page.c_str()); browser_smoke_wait(frame); frame_text(frame);
    HWND post = WFE_FormFindControl(context, "go", 0);
    if (post) { SendMessage(post, BM_CLICK, 0, 0); browser_smoke_wait(frame); }
    bool post_loaded = frame_text(frame).find("History POST") != std::string::npos;
    frame->SendMessage(WM_COMMAND, ID_NAV_RELOAD); browser_smoke_wait(frame);
    bool post_cancelled = post_loaded && frame->load_status == MK_INTERRUPTED;
    bool global_view = WFE_HistoryDialogSmoke(context, -1) != 0;
    if (global_view) {
        browser_smoke_wait(frame);
        std::string text;
        for (int y = 0; y <= frame->view.document_height && y < 100000; y += 400) {
            WFE_Scroll(context, 0, y); text += frame_text(frame);
            if (text.find(url) != std::string::npos) break;
        }
        WFE_Scroll(context, 0, 0);
        SYSTEMTIME now; GetLocalTime(&now); char year[8]; sprintf(year, "%u", now.wYear);
        global_view = text.find(url) != std::string::npos && text.find(year) != std::string::npos;
        if (!global_view) fprintf(stderr, "Global history rendering did not expose its saved URL/date.\n");
    }
    frame->SendMessage(WM_CLOSE);
    FILE *output = fopen(report, "wb");
    if (!output) return 3;
    bool traversal = clicked && page_b && back_a && forward_b;
    bool state = initial_state && back_state;
    fprintf(output, "traversal=%d\nform_restored=%d\nscroll_restored=%d\nforward_truncated=%d\nreload=%d\ncommand_state=%d\nhistory_dialog=%d\nglobal_recent=%d\nglobal_view=%d\npost_cancelled=%d\nerror=%s\n", traversal, restored, scroll_restored, truncated, reloaded, state, dialog, global_recent, global_view, post_cancelled, smoke_error.c_str());
    fclose(output);
    return traversal && restored && scroll_restored && truncated && reloaded && state && dialog && global_recent && global_view && post_cancelled ? 0 : 4;
}
static int WFE_RunHistoryReopenSmoke(const char *report, const char *url)
{
    GH_SetGlobalHistoryTimeout(3600);
    bool persisted = GH_CheckGlobalHistory((char *)url) == 1;
    GH_FreeGlobalHistory();
    char *filename = WH_FileName("", xpGlobalHistory);
    DB *database = filename ? dbopen(filename, O_RDWR, 0600, DB_HASH, nullptr) : nullptr;
    free(filename);
    const std::string expired = std::string(url) + "?expired", malformed = std::string(url) + "?malformed";
    const std::string collected = std::string(url) + "?collected";
    bool dates = database != nullptr;
    uint32 old = (uint32)(time(nullptr) - 7200);
    unsigned char invalid[2] = {1,2};
    if (database) {
        for (const std::string &name : {expired, collected}) {
            DBT key = {(void *)name.c_str(), name.size()+1}, value = {&old, sizeof(old)};
            dates = database->put(database, &key, &value, 0) == 0 && dates;
        }
        DBT key = {(void *)malformed.c_str(), malformed.size()+1}, value = {invalid, sizeof(invalid)};
        dates = database->put(database, &key, &value, 0) == 0 && dates;
        database->close(database);
    }
    GH_InitGlobalHistory();
    dates = GH_CheckGlobalHistory((char *)url) == 1 && GH_CheckGlobalHistory((char *)expired.c_str()) == -1
        && GH_CheckGlobalHistory((char *)malformed.c_str()) == -1 && dates;
    GH_CollectGarbage(); GH_SaveGlobalHistory(); GH_FreeGlobalHistory();
    filename = WH_FileName("", xpGlobalHistory);
    database = filename ? dbopen(filename, O_RDONLY, 0600, DB_HASH, nullptr) : nullptr;
    free(filename);
    if (database) {
        DBT key = {(void *)collected.c_str(), collected.size()+1}, value = {};
        dates = database->get(database, &key, &value, 0) != 0 && dates;
        key.data = (void *)url; key.size = strlen(url)+1;
        dates = database->get(database, &key, &value, 0) == 0 && value.size == sizeof(uint32) && dates;
        database->close(database);
    } else dates = false;
    GH_InitGlobalHistory();
    FILE *output = fopen(report, "wb");
    if (!output) return 3;
    fprintf(output, "persisted=%d\ndates=%d\n", persisted, dates); fclose(output);
    return persisted && dates ? 0 : 4;
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
static int ui_column(CListCtrl &list,const char *name)
{
    for(int i=0;i<list.GetHeaderCtrl()->GetItemCount();++i) {
        char text[128]={0};LVCOLUMN column={};column.mask=LVCF_TEXT;column.pszText=text;column.cchTextMax=sizeof(text);
        list.GetColumn(i,&column);if(!strcmp(text,name)) return i;
    }
    return -1;
}
static CRect ui_bounds(CWnd &owner,CWnd &child) { CRect r;child.GetWindowRect(r);owner.ScreenToClient(r);return r; }
static CString ui_menu(CFrameWnd *frame)
{
    CString names;CMenu *menu=frame->GetMenu();
    for(int i=0;menu&&i<menu->GetMenuItemCount();++i) { CString text;menu->GetMenuString(i,text,MF_BYPOSITION);text.Replace("&","");if(i) names+='|';names+=text; }
    return names;
}
static bool ui_drag(CWnd *owner,int id,CPoint target)
{
    CWnd *divider=owner->GetDlgItem(id);if(!divider) return false;
    CRect area;divider->GetClientRect(area);CPoint point=area.CenterPoint();
    divider->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
    owner->ClientToScreen(&target);CPoint screen=target;divider->ScreenToClient(&target);
    divider->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,MAKELPARAM(target.x,target.y));
    target=screen;divider->ScreenToClient(&target);
    divider->SendMessage(WM_LBUTTONUP,0,MAKELPARAM(target.x,target.y));return true;
}
#include "menu-smoke.inc"
#ifdef EDITOR
#include "editor-smoke.inc"
#endif

static int WFE_RunMailUISmoke(const char *report,const char *snapshot)
{
    WFE_SetMailPreference("email","preview@example.invalid");WFE_ApplyMailPreferences();
    BrowserFrame *frame=frame_for(WFE_NewMailWindow(0,nullptr));if(!frame) return 2;
    chrome_client_size(frame,800,600);frame->ShowWindow(SW_SHOWNOACTIVATE);
    int inbox=find_folder(&frame->context,"Inbox");
    if(inbox>=0) {
        frame->folders.SetItemState(-1,0,LVIS_SELECTED);
        frame->folders.SetItemState(inbox,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    ULONGLONG deadline=GetTickCount64()+8000;MSG_MessageLine line;
    while(GetTickCount64()<deadline&&!MSG_GetThreadLine(&frame->context,1,&line,nullptr)) mail_smoke_pump();
    frame->RefreshMailLists();
    int subject_col=ui_column(frame->messages,"Subject"),sender_col=ui_column(frame->messages,"Sender"),hello=-1;
    CString subjects;
    for(int i=0;i<frame->messages.GetItemCount();++i) {
        CString text=frame->messages.GetItemText(i,subject_col);if(i) subjects+='|';subjects+=text;
        if(text.Find("Hello!")>=0) hello=i;
    }
    if(hello>=0) { frame->messages.SetItemState(-1,0,LVIS_SELECTED);frame->messages.SetItemState(hello,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);browser_smoke_wait(frame); }
    bool previous_next=false,reader_scroll=false;
    if(hello>=0) {
        frame->SendMessage(WM_COMMAND,hello ? ID_MESSAGE_PREVIOUS : ID_MESSAGE_NEXT);browser_smoke_wait(frame);
        int selected=frame->messages.GetNextItem(-1,LVNI_SELECTED);
        previous_next=selected>=0&&selected!=hello;
        CRect before=ui_bounds(*frame,frame->folders);
        ui_drag(frame,125,CPoint(10,before.top+70));
        SCROLLINFO scroll={sizeof(SCROLLINFO),SIF_PAGE|SIF_RANGE};frame->view.GetScrollInfo(SB_VERT,&scroll);
        CRect page;frame->view.GetClientRect(page);
        reader_scroll=frame->view.document_height>page.Height()&&scroll.nPage==(UINT)page.Height();
        ui_drag(frame,125,CPoint(10,before.bottom+2));
        frame->SendMessage(WM_COMMAND,hello ? ID_MESSAGE_NEXT : ID_MESSAGE_PREVIOUS);browser_smoke_wait(frame);
        previous_next=previous_next&&frame->messages.GetNextItem(-1,LVNI_SELECTED)==hello;
    }
    frame->chrome.UpdateCommands();
    std::string reader=frame_text(frame);std::replace(reader.begin(),reader.end(),'\n',' ');std::replace(reader.begin(),reader.end(),'\r',' ');
    CString selected_subject=hello>=0?frame->messages.GetItemText(hello,subject_col):"";
    CString selected_sender=hello>=0?frame->messages.GetItemText(hello,sender_col):"";
    CString caption,status_text;frame->GetWindowText(caption);frame->status.GetPaneText(1,status_text);
    CString columns;
    for(int i=0;i<frame->messages.GetHeaderCtrl()->GetItemCount();++i) { char text[80]={0};LVCOLUMN c={};c.mask=LVCF_TEXT;c.pszText=text;c.cchTextMax=80;frame->messages.GetColumn(i,&c);if(i) columns+='|';columns+=*text?text:"Status"; }
    CRect client;frame->GetClientRect(client);CRect folders=ui_bounds(*frame,frame->folders),messages=ui_bounds(*frame,frame->messages),view=ui_bounds(*frame,frame->view);
    bool reader_header=reader.find("user@example.invalid")!=std::string::npos;
    bool reader_body=reader.find("Hi User")!=std::string::npos&&reader.find("Just a test.")!=std::string::npos;
    frame->messages.SetFocus();
    bool captured=chrome_capture(frame,snapshot);chrome_capture(frame,(std::string(snapshot)+".window.bmp").c_str(),true);
    bool splits=ui_drag(frame,124,CPoint(5000,folders.top+10))&&ui_drag(frame,125,CPoint(10,5000));
    CRect clamped=ui_bounds(*frame,frame->view);splits=splits&&clamped.Height()>=50&&clamped.Width()>=120;
    ui_drag(frame,124,CPoint(170,folders.top+10));ui_drag(frame,125,CPoint(10,folders.top+80));
    chrome_client_size(frame,340,430);
    CRect narrow_client;frame->GetClientRect(narrow_client);
    CRect nf=ui_bounds(*frame,frame->folders),nm=ui_bounds(*frame,frame->messages),nv=ui_bounds(*frame,frame->view);
    bool narrow=nv.left==0&&nv.right<=narrow_client.right&&nv.Width()>=120&&nv.Height()>=70&&nm.right<=narrow_client.right;
    int icons=frame->folders.GetImageList(LVSIL_SMALL)?frame->folders.GetImageList(LVSIL_SMALL)->GetImageCount():0;
    bool full=view.left==folders.left&&view.right==messages.right&&view.top>=folders.bottom;
    bool top=folders.top==messages.top&&folders.bottom==messages.bottom&&folders.Height()<client.Height()/2;
    bool done=frame->completed&&frame->load_status>=0;
    bool passed=done&&full&&top&&reader_header&&reader_body&&previous_next&&reader_scroll&&splits&&narrow&&captured&&icons>0;
    FILE *file=fopen(report,"wb");
    if(file) {
        fprintf(file,"passed=%d\ncompleted=%d\ntoolbar_count=%u\ntoolbar_labels=%s\nmenu_labels=%s\nlocation_visible=%d\ndirectory_visible=%d\ncolumns=%s\nfolder_icons=%d\nselected_subject=%s\nselected_sender=%s\ncaption=%s\nsubjects=%s\nreader_header=%d\nreader_body=%d\nreader_text=%s\nprevious_next=%d\nreader_scroll=%d\nsplitter_clamps=%d\nnarrow_layout=%d\nstatus_text=%s\nclient=%d,%d\nfolders=%d,%d,%d,%d\nmessages=%d,%d,%d,%d\nreader=%d,%d,%d,%d\nnarrow_client=%d,%d\nnarrow_folders=%d,%d,%d,%d\nnarrow_messages=%d,%d,%d,%d\nnarrow_reader=%d,%d,%d,%d\n",
            passed,done,(unsigned)frame->chrome.ButtonCount(),(const char*)frame->chrome.Labels(),(const char*)ui_menu(frame),frame->chrome.location.IsWindowVisible(),frame->chrome.directory_visible,(const char*)columns,icons,(const char*)selected_subject,(const char*)selected_sender,(const char*)caption,(const char*)subjects,reader_header,reader_body,reader.c_str(),previous_next,reader_scroll,splits,narrow,(const char*)status_text,
            client.Width(),client.Height(),folders.left,folders.top,folders.right,folders.bottom,messages.left,messages.top,messages.right,messages.bottom,view.left,view.top,view.right,view.bottom,narrow_client.Width(),narrow_client.Height(),nf.left,nf.top,nf.right,nf.bottom,nm.left,nm.top,nm.right,nm.bottom,nv.left,nv.top,nv.right,nv.bottom);
        fclose(file);
    } else passed=false;
    chrome_client_size(frame,800,600);
    passed=original_mail_menu_smoke(frame,report)&&passed;
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}

static bool news_ui_compose(BrowserFrame *source,UINT command,bool news,bool mail)
{
    source->chrome.UpdateCommands();CWnd *button=source->chrome.Button(command);
    if(!button||!button->IsWindowEnabled()) return false;
    button->SendMessage(BM_CLICK);
    MWContext *context=nullptr;HWND window=nullptr;
    for(auto &entry:native_windows) if(entry.first->type==MWContextMessageComposition) { context=entry.first;window=entry.second;break; }
    if(!context) return false;
    char groups[512]={0},to[512]={0};
    ::GetWindowTextA(::GetDlgItem(window,IDC_COMPOSE_FIRST+6),groups,sizeof(groups));
    ::GetWindowTextA(::GetDlgItem(window,IDC_COMPOSE_FIRST+2),to,sizeof(to));
    bool result=(news ? strstr(groups,"news.announce.newusers")!=nullptr : !*groups)
        && (mail ? strstr(to,"sender@example.invalid")!=nullptr : !*to);
    WFE_ComposerClose(context);
    ULONGLONG deadline=GetTickCount64()+2000;
    while(WFE_ComposerWindowCount()&&GetTickCount64()<deadline) mail_smoke_pump();
    return result&&WFE_ComposerWindowCount()==0;
}
static void news_ui_click(CWnd *window,CPoint point)
{
    /* Native list mouse-down may wait in DragDetect; queue its release first. */
    window->PostMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
    window->PostMessage(WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
    mail_smoke_pump();
}

static int WFE_RunNewsUISmoke(const char *report,const char *snapshot,const char *server)
{
    WFE_SetMailPreference("email","preview@example.invalid");WFE_ApplyMailPreferences();
    BrowserFrame *frame=frame_for(WFE_NewMailWindow(1,server));if(!frame) return 2;
    chrome_client_size(frame,800,600);frame->ShowWindow(SW_SHOWNOACTIVATE);
    ULONGLONG deadline=GetTickCount64()+10000;
    int group=-1;
    while(GetTickCount64()<deadline) {
        mail_smoke_pump();
        if(!NET_AreThereActiveConnectionsForWindow(&frame->context)) {
            MSG_FolderLine visible;
            for(int i=0;i<1000&&MSG_GetFolderLine(&frame->context,i,&visible,nullptr);++i)
                if((visible.flags&MSG_FOLDER_FLAG_DIRECTORY)&&(visible.flags&MSG_FOLDER_FLAG_ELIDED)) MSG_ToggleFolderExpansion(&frame->context,i);
        }
        group=find_folder(&frame->context,"news.announce.newusers");
        if(group>=0&&!NET_AreThereActiveConnectionsForWindow(&frame->context)) break;
    }
    MSG_FolderLine candidate;
    for(int i=0;i<1000&&MSG_GetFolderLine(&frame->context,i,&candidate,nullptr);++i)
        if(!(candidate.flags&MSG_FOLDER_FLAG_DIRECTORY)&&!(candidate.flags&MSG_FOLDER_FLAG_SUBSCRIBED)) MSG_ToggleSubscribed(&frame->context,i);
    group=find_folder(&frame->context,"news.announce.newusers");
    frame->folders.SetItemState(-1,0,LVIS_SELECTED);
    frame->folders.SetItemState(group,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    frame->folders.SetFocus();
    frame->folders.SendMessage(WM_KEYDOWN,VK_SPACE,0);
    MSG_GetFolderLine(&frame->context,group,&candidate,nullptr);bool subscription_key=!(candidate.flags&MSG_FOLDER_FLAG_SUBSCRIBED);
    frame->folders.SendMessage(WM_KEYDOWN,VK_SPACE,0);
    MSG_GetFolderLine(&frame->context,group,&candidate,nullptr);subscription_key=subscription_key&&(candidate.flags&MSG_FOLDER_FLAG_SUBSCRIBED);
    CRect sub;
    frame->folders.GetSubItemRect(group,1,LVIR_BOUNDS,sub);
    news_ui_click(&frame->folders,sub.CenterPoint());
    MSG_GetFolderLine(&frame->context,group,&candidate,nullptr);bool subscription_mouse=!(candidate.flags&MSG_FOLDER_FLAG_SUBSCRIBED);
    news_ui_click(&frame->folders,sub.CenterPoint());
    MSG_GetFolderLine(&frame->context,group,&candidate,nullptr);subscription_mouse=subscription_mouse&&(candidate.flags&MSG_FOLDER_FLAG_SUBSCRIBED);
    frame->folders.SetItemState(-1,0,LVIS_SELECTED);
    frame->folders.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    frame->SendMessage(WM_COMMAND,ID_NEWS_SUBSCRIBED);
    frame->RefreshMailLists();group=find_folder(&frame->context,"news.announce.newusers");
    frame->folders.SetItemState(-1,0,LVIS_SELECTED);
    frame->folders.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    frame->folders.SetFocus();
    frame->folders.SendMessage(WM_KEYDOWN,VK_LEFT,0);
    MSG_GetFolderLine(&frame->context,0,&candidate,nullptr);bool tree_key=(candidate.flags&MSG_FOLDER_FLAG_ELIDED)!=0&&frame->folders.GetItemCount()==1;
    frame->folders.SendMessage(WM_KEYDOWN,VK_RIGHT,0);frame->RefreshMailLists();
    MSG_GetFolderLine(&frame->context,0,&candidate,nullptr);tree_key=tree_key&&!(candidate.flags&MSG_FOLDER_FLAG_ELIDED)&&frame->folders.GetItemCount()==4;
    CRect root;
    frame->folders.GetItemRect(0,root,LVIR_BOUNDS);CPoint glyph(9,root.CenterPoint().y);
    news_ui_click(&frame->folders,glyph);MSG_GetFolderLine(&frame->context,0,&candidate,nullptr);bool tree_mouse=(candidate.flags&MSG_FOLDER_FLAG_ELIDED)!=0;
    news_ui_click(&frame->folders,glyph);MSG_GetFolderLine(&frame->context,0,&candidate,nullptr);tree_mouse=tree_mouse&&!(candidate.flags&MSG_FOLDER_FLAG_ELIDED);
    frame->chrome.UpdateCommands();
    bool empty_commands=!frame->chrome.Button(ID_NEWS_REPLY_POST)->IsWindowEnabled()&&!frame->chrome.Button(ID_MESSAGE_REPLY)->IsWindowEnabled()
        &&!frame->chrome.Button(ID_NEWS_MARK_THREAD)->IsWindowEnabled()&&!frame->chrome.Button(ID_NEWS_MARK_GROUP)->IsWindowEnabled();
    group=find_folder(&frame->context,"news.announce.newusers");
    CString directory_status=frame->status.GetPaneText(1);
    bool directory_counts=directory_status.Find("messages")<0&&directory_status.Find("unread")<0;
    BrowserFrame *browser=frame_for(WFE_NewWindow(nullptr));
    class NewsCommandState:public CCmdUI { public:bool enabled=false;void Enable(BOOL value=TRUE) override {enabled=value!=FALSE;} } post_state;
    post_state.m_nID=ID_CLIENT_POST;if(browser) browser->OnCmdMsg(ID_CLIENT_POST,CN_UPDATE_COMMAND_UI,&post_state,nullptr);
    bool browser_post=post_state.enabled;if(browser) browser->SendMessage(WM_CLOSE);
    CRect client;frame->GetClientRect(client);
    CRect folders=ui_bounds(*frame,frame->folders),messages=ui_bounds(*frame,frame->messages),reader=ui_bounds(*frame,frame->view);
    CString folder_columns,article_columns;
    for(int list=0;list<2;++list) {
        CListCtrl &control=list?frame->messages:frame->folders;CString &labels=list?article_columns:folder_columns;
        for(int i=0;i<control.GetHeaderCtrl()->GetItemCount();++i) {
            char label[100]={0};LVCOLUMN c={};c.mask=LVCF_TEXT;c.pszText=label;c.cchTextMax=100;control.GetColumn(i,&c);
            if(i) labels+='|';labels+=*label?label:"Status";
        }
    }
    CString tree;MSG_FolderLine folder;
    for(int i=0;MSG_GetFolderLine(&frame->context,i,&folder,nullptr);++i) { if(i) tree+='|';tree+=folder.name; }
    HWND logo=::FindWindowEx(frame->chrome.GetSafeHwnd(),nullptr,nullptr,"Netscape");CRect logo_rect;
    if(logo) { ::GetWindowRect(logo,&logo_rect);frame->ScreenToClient(logo_rect); }
    bool logo_visible=logo&&::IsWindowVisible(logo);
    bool captured=chrome_capture(frame,snapshot);chrome_capture(frame,(std::string(snapshot)+".window.bmp").c_str(),true);
    frame->SendMessage(WM_COMMAND,ID_MESSAGE_SHOW_ALL);
    if(group>=0) {
        frame->folders.SetItemState(-1,0,LVIS_SELECTED);
        frame->folders.SetItemState(group,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    }
    NMITEMACTIVATE leaf={};leaf.hdr.hwndFrom=frame->folders.GetSafeHwnd();leaf.hdr.idFrom=ID_FOLDER_LIST;leaf.hdr.code=NM_DBLCLK;
    frame->SendMessage(WM_NOTIFY,ID_FOLDER_LIST,(LPARAM)&leaf);
    bool leaf_guard=MSG_GetFolderLine(&frame->context,group,&candidate,nullptr)&&!(candidate.flags&MSG_FOLDER_FLAG_DIRECTORY);
    MSG_MessageLine article;deadline=GetTickCount64()+10000;
    while(GetTickCount64()<deadline&&!MSG_GetThreadLine(&frame->context,1,&article,nullptr)) mail_smoke_pump();
    frame->RefreshMailLists();
    int article_rows=frame->messages.GetItemCount();
    if(frame->messages.GetItemCount()) { frame->messages.SetItemState(-1,0,LVIS_SELECTED);frame->completed=false;frame->messages.SetItemState(0,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);browser_smoke_wait(frame); }
    CClientDC screen(&frame->view);CDC memory;memory.CreateCompatibleDC(&screen);CBitmap bitmap;bitmap.CreateCompatibleBitmap(&screen,700,460);
    CBitmap *old=memory.SelectObject(&bitmap);frame->view.drawn_text.clear();frame->view.Render(memory,700,460);memory.SelectObject(old);
    bool read=frame->completed&&frame->load_status>=0&&frame->view.drawn_text.find("decoded body.")!=std::string::npos;
    bool retained=MSG_GetToggleStatus(&frame->context,MSG_ShowSubscribedNewsGroups)==MSG_Checked&&frame->folders.GetItemCount()==4;
    chrome_capture(frame,(std::string(snapshot)+".article.window.bmp").c_str(),true);
    MSG_SetAutoQuoteReply(FALSE);
    bool post=news_ui_compose(frame,ID_CLIENT_POST,true,false);
    bool mail=news_ui_compose(frame,ID_NEWS_MAIL_NEW,false,false);
    bool reply_post=news_ui_compose(frame,ID_NEWS_REPLY_POST,true,false);
    bool reply_mail=news_ui_compose(frame,ID_MESSAGE_REPLY,false,true);
    bool reply_both=news_ui_compose(frame,ID_NEWS_REPLY_BOTH,true,true);
    MSG_Command(&frame->context,MSG_MarkSelectedUnread);
    frame->chrome.UpdateCommands();frame->chrome.Button(ID_NEWS_MARK_THREAD)->SendMessage(BM_CLICK);
    MSG_GetThreadLine(&frame->context,0,&article,nullptr);bool mark_thread=(article.flags&MSG_FLAG_READ)!=0;
    frame->chrome.UpdateCommands();frame->chrome.Button(ID_NEWS_MARK_GROUP)->SendMessage(BM_CLICK);
    bool mark_group=frame->messages.GetItemCount()==2;
    for(int i=0;MSG_GetThreadLine(&frame->context,i,&article,nullptr);++i) if(!(article.flags&MSG_FLAG_READ)) mark_group=false;
    bool splits=ui_drag(frame,124,CPoint(5000,folders.top+10))&&ui_drag(frame,125,CPoint(10,5000));
    CRect clamped=ui_bounds(*frame,frame->view);splits=splits&&clamped.Width()>=120&&clamped.Height()>=50;
    chrome_client_size(frame,340,430);CRect narrow_client;frame->GetClientRect(narrow_client);
    CRect nv=ui_bounds(*frame,frame->view),nf=ui_bounds(*frame,frame->folders),nm=ui_bounds(*frame,frame->messages);
    bool narrow=nv.left==0&&nv.right==narrow_client.right&&nv.Height()>=70&&nf.Width()>0&&nm.Width()>0;
    FILE *file=fopen(report,"wb");
    if(file) {
        fprintf(file,"toolbar_labels=%s\nmenu_labels=%s\nfolder_columns=%s\narticle_columns=%s\ntree=%s\nreader_body=%d\nsnapshot=%d\nlocation_visible=%d\ndirectory_visible=%d\nclient=%d,%d\nfolders=%d,%d,%d,%d\nmessages=%d,%d,%d,%d\nreader=%d,%d,%d,%d\n",
            (const char*)frame->chrome.Labels(),(const char*)ui_menu(frame),(const char*)folder_columns,(const char*)article_columns,(const char*)tree,read,captured,frame->chrome.location.IsWindowVisible(),frame->chrome.directory_visible,
            client.Width(),client.Height(),folders.left,folders.top,folders.right,folders.bottom,messages.left,messages.top,messages.right,messages.bottom,reader.left,reader.top,reader.right,reader.bottom);
        fprintf(file,"empty_commands=%d\ntree_keyboard=%d\ntree_mouse=%d\nsubscription_keyboard=%d\nsubscription_mouse=%d\nview_retained=%d\npost_command=%d\nmail_command=%d\nreply_post=%d\nreply_mail=%d\nreply_both=%d\nmark_thread=%d\nmark_group=%d\nnarrow_layout=%d\nsplitter_clamps=%d\n",empty_commands,tree_key,tree_mouse,subscription_key,subscription_mouse,retained,post,mail,reply_post,reply_mail,reply_both,mark_thread,mark_group,narrow,splits);
        fprintf(file,"directory_counts=%d\nbrowser_post=%d\nleaf_guard=%d\n",directory_counts,browser_post,leaf_guard);
        fprintf(file,"logo_visible=%d\nlogo_bounds=%d,%d,%d,%d\n",logo_visible,logo_rect.left,logo_rect.top,logo_rect.right,logo_rect.bottom);
        fclose(file);
    }
    frame->SendMessage(WM_CLOSE);return file&&read&&captured?0:1;
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
    for (int i = 0; i < frame->messages.GetItemCount(); ++i) { if (i) subjects += '|'; subjects += frame->messages.GetItemText(i, MailPaneUI::SubjectColumn); }
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

static bool open_preferred_startup()
{
    const GeneralPreferences &preferences=WFE_GeneralPreferences();bool opened=false;
    if(preferences.startup&1) opened=WFE_NewWindow(preferences.blank?"about:blank":preferences.home.c_str())!=nullptr;
    if(preferences.startup&2) opened=WFE_NewMailWindow(0,nullptr)!=nullptr||opened;
    if(preferences.startup&4) opened=WFE_NewMailWindow(1,nullptr)!=nullptr||opened;
    return opened;
}
static int WFE_RunPreferencesSmoke(const char *report,const char *snapshot,const char *url,bool reopen)
{
    bool store=true;
    if(!reopen) store=WFE_PreferencesStoreSmoke((std::string(report)+".store").c_str())==0;
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,850,580);frame->ShowWindow(SW_SHOWNOACTIVATE);
    int ui=reopen?0:WFE_PreferencesUISmoke(frame,report,snapshot);
    GeneralPreferences accepted=WFE_GeneralPreferences();
    bool persisted=accepted.toolbar==1&&accepted.startup==1&&accepted.blank&&!accepted.underline
        &&accepted.days==17&&accepted.languages.size()==2&&accepted.languages[0]=="fr"&&accepted.languages[1]=="en";
    CRect text_button;frame->chrome.Button(ID_NAV_HOME)->GetWindowRect(text_button);
    bool toolbar=frame->chrome.Presentation()==1&&text_button.Height()<30;
    if(!reopen) chrome_capture(frame,(std::string(snapshot)+".text.bmp").c_str());
    std::string error;GeneralPreferences probe=accepted;probe.toolbar=0;
    bool pictures=WFE_SaveGeneralPreferences(probe,&error);CRect picture_button;frame->chrome.Button(ID_NAV_HOME)->GetWindowRect(picture_button);
    pictures=pictures&&frame->chrome.Presentation()==0&&picture_button.Width()<text_button.Width();
    if(!reopen) chrome_capture(frame,(std::string(snapshot)+".pictures.bmp").c_str());
    probe.toolbar=2;bool both=WFE_SaveGeneralPreferences(probe,&error);CRect both_button;frame->chrome.Button(ID_NAV_HOME)->GetWindowRect(both_button);
    both=both&&frame->chrome.Presentation()==2&&both_button.Height()>text_button.Height();
    if(!reopen) chrome_capture(frame,(std::string(snapshot)+".both.bmp").c_str());
    probe.toolbar=1;probe.proportional="Arial";probe.proportional_size=14;probe.fixed_size=11;
    probe.background_custom=true;probe.text_custom=true;probe.link_custom=true;probe.override_colors=true;
    probe.colors[0]=RGB(242,230,200);probe.colors[1]=RGB(0,64,0);probe.colors[2]=RGB(170,0,0);
    probe.incremental=false;probe.dither=2;
    bool saved=WFE_SaveGeneralPreferences(probe,&error);
    if(url&&*url) { frame->Navigate(url);browser_smoke_wait(frame); }
    std::string content=frame_text(frame);
    HFONT font=text_font(nullptr,frame->document_preferences,false);LOGFONT actual={};GetObject(font,sizeof(actual),&actual);DeleteObject(font);
    bool fonts=!strcmp(actual.lfFaceName,"Arial")&&actual.lfHeight==-19;
    bool page=frame->completed&&frame->load_status>=0&&content.find("Preferences fixture")!=std::string::npos;
    bool images=frame->view.drawn_images.size()==1&&!WFE_PreferenceIncrementalImages()&&WFE_PreferenceImageDither()==2;
    if(!reopen) chrome_capture(frame,(std::string(snapshot)+".page.bmp").c_str());
    bool restored=WFE_SaveGeneralPreferences(accepted,&error);
    size_t before=browser_windows.size();bool launched=open_preferred_startup();
    BrowserFrame *startup=browser_windows.size()>before?browser_windows.back():nullptr;
    bool startup_ok=launched&&startup&&startup->mode==0&&startup->current_url=="about:blank";
    if(startup) startup->SendMessage(WM_CLOSE);
    bool passed=store&&!ui&&persisted&&toolbar&&pictures&&both&&saved&&fonts&&page&&images&&restored&&startup_ok;
    FILE *file=fopen(report,reopen?"wb":"ab");
    if(file) { fprintf(file,"root_passed=%d\nstore=%d\npersisted=%d\ntext_toolbar=%d\npictures_toolbar=%d\nboth_toolbar=%d\nfont_effect=%d\npage_loaded=%d\nimage_effect=%d\nstartup_effect=%d\nrestored=%d\nroot_error=%s\n",passed,store,persisted,toolbar,pictures,both,fonts,page,images,startup_ok,restored,error.c_str());fclose(file); }
    else passed=false;
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}

static int WFE_RunPrintingSmoke(const char *report,const char *url)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(url));if(!frame) return 2;
    browser_smoke_wait(frame);
    int x=frame->view.scroll_x,y=frame->view.scroll_y;
    bool passed=WFE_PrintSmoke(&frame->context,report)!=0;
    passed=passed && frame->view.scroll_x==x && frame->view.scroll_y==y;
    bool editor_preserved=true;
#ifdef EDITOR
    BrowserFrame *editor=frame_for(WFE_NewEditorWindow(url));editor_preserved=editor!=nullptr;
    if(editor) {
        browser_smoke_wait(editor);MWContext *context=&editor->context;
        editor_preserved=EDT_HaveEditBuffer(context)!=FALSE;
        if(editor_preserved) {
            EDT_EndOfDocument(context,FALSE);EDT_BeginOfLine(context,TRUE);EDT_SetDirtyFlag(context,TRUE);
            LO_Element *first=nullptr,*last=nullptr,*after_first=nullptr,*after_last=nullptr;int32 a=0,b=0,after_a=0,after_b=0;
            LO_GetSelectionEndpoints(context,&first,&last,&a,&b);
            int sx=editor->view.scroll_x,sy=editor->view.scroll_y,width=0,height=0;
            HDC emf=CreateEnhMetaFileA(nullptr,nullptr,nullptr,nullptr);
            editor_preserved=emf && WFE_PrintDocumentSize(context,&width,&height) &&
                WFE_PrintDocumentPage(context,emf,0,width,(std::min)(height,800));
            if(emf) {HENHMETAFILE picture=CloseEnhMetaFile(emf);if(picture) DeleteEnhMetaFile(picture);else editor_preserved=false;}
            LO_GetSelectionEndpoints(context,&after_first,&after_last,&after_a,&after_b);
            editor_preserved=editor_preserved && EDT_DirtyFlag(context) && sx==editor->view.scroll_x && sy==editor->view.scroll_y &&
                first==after_first && last==after_last && a==after_a && b==after_b;
            EDT_SetDirtyFlag(context,FALSE);
        }
        editor->SendMessage(WM_CLOSE);
    }
#endif
    FILE *file=fopen(report,"ab");
    if(file) {fprintf(file,"view_preserved=%d\neditor_preserved=%d\n",frame->view.scroll_x==x&&frame->view.scroll_y==y,editor_preserved);fclose(file);}
    passed=passed&&editor_preserved;
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}

#ifdef EDITOR
static int WFE_RunWorkflowSmoke(const char *report,const char *url,const char *destination)
{
    BrowserFrame *frame=frame_for(WFE_NewEditorWindow(url));if(!frame) return 2;
    browser_smoke_wait(frame);
    int result=destination ? WFE_EditorPublishSmoke(&frame->context,report,destination) :
        (WFE_EditorWorkflowSmoke(&frame->context,report)?0:1);
    if(EDT_HaveEditBuffer(&frame->context)) EDT_SetDirtyFlag(&frame->context,FALSE);
    frame->SendMessage(WM_CLOSE);return result;
}
#endif

#include "interaction-smoke.inc"
#include "link-smoke.inc"
#include "divider-smoke.inc"

#include "background-smoke.inc"

#include "applications-smoke.inc"
#ifdef NETSCAPE_USE_BEARSSL
#include "tls-smoke.inc"
#endif

class NetscapeApplication : public CWinApp {
public:
    BOOL ProcessMessageFilter(int code,LPMSG message) override {
        /* A native popup can retain a queued message for its just-closed
         * owner. MFC otherwise creates a CWnd for that invalid HWND and
         * dereferences it while finding its top-level frame. */
        if(message && message->hwnd && !::IsWindow(message->hwnd)) return FALSE;
        return CWinApp::ProcessMessageFilter(code,message);
    }
    BOOL InitInstance() override {
        CWinApp::InitInstance();
        INITCOMMONCONTROLSEX controls = {sizeof(controls), ICC_TAB_CLASSES | ICC_LISTVIEW_CLASSES | ICC_PROGRESS_CLASS | ICC_TREEVIEW_CLASSES};
        InitCommonControlsEx(&controls);
        SetRegistryKey("NetscapeLegacy");
        CStringList arguments;
        if (strlen(m_lpCmdLine) >= 511) return FALSE;
        CCmdParse parser(m_lpCmdLine, arguments);
        if (!parser.ProcessCmdLine()) return FALSE;
        POSITION position = arguments.GetHeadPosition();
        bool explicit_command=position!=nullptr;
        CString first = position ? arguments.GetNext(position) : "about:";
        smoke_mode = first == "--tls-smoke-test" || first == "--applications-smoke" || first == "--applications-reopen-smoke" || first == "--background-smoke" || first == "--background-reopen-smoke" || first == "--dividers-smoke" || first == "--links-smoke" || first == "--interaction-smoke" || first == "--printing-smoke" || first == "--smoke-test" || first == "--mailnews-smoke" || first == "--composer-delivery-smoke"
            || first == "--images-smoke" || first == "--forms-smoke" || first == "--frames-smoke" || first == "--downloads-smoke" || first == "--history-smoke" || first == "--history-reopen-smoke" || first == "--bookmarks-smoke" || first == "--chrome-smoke" || first == "--ui-gallery" || first == "--preferences-ui-smoke" || first == "--preferences-reopen-smoke" || first == "--news-ui-smoke" || first == "--mail-ui-smoke" || first == "--compose-ui-smoke"
#ifdef EDITOR
            || first == "--editor-smoke"
            || first == "--editor-workflows-smoke" || first == "--editor-publish-smoke"
#endif
            ;
        if (smoke_mode) {
            SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#ifdef _DEBUG
            _CrtSetReportHook(debug_report);
            _set_invalid_parameter_handler(invalid_parameter);
#endif
        }
        if(first=="--ui-gallery" && (!getenv("NETSCAPE_PROFILE_DIR") || !*getenv("NETSCAPE_PROFILE_DIR"))) { m_nExitCode=2;return FALSE; }
        if (!WFE_Init()) { m_nExitCode = 2; return FALSE; }
        backend_initialized = true;
        automatic_images=GetPrivateProfileIntA("Browser","AutoImages",1,(std::string(WFE_ProfileDirectory())+"/browser.ini").c_str())!=0;
        if (smoke_mode) {
            CString report = position ? arguments.GetNext(position) : "frontend-smoke.txt";
            CString url = position ? arguments.GetNext(position) : "";
#ifdef EDITOR
            if(first=="--editor-smoke") m_nExitCode=WFE_RunEditorSmoke(report,url);
            else if(first=="--editor-workflows-smoke") m_nExitCode=WFE_RunWorkflowSmoke(report,url,nullptr);
            else if(first=="--editor-publish-smoke") {
                CString destination=position?arguments.GetNext(position):"";
                m_nExitCode=WFE_RunWorkflowSmoke(report,url,destination);
            }
            else
#endif
#ifdef NETSCAPE_USE_BEARSSL
            if(first=="--tls-smoke-test") {
                CString bundle=position?arguments.GetNext(position):"";
                CString proxy=position?arguments.GetNext(position):"";
                CString scenario=position?arguments.GetNext(position):"";
                m_nExitCode=WFE_RunTLSSmoke(report,url,bundle,proxy,scenario);
            } else
#endif
            if(first=="--applications-smoke" || first=="--applications-reopen-smoke") {CString program=position?arguments.GetNext(position):"",temporary=position?arguments.GetNext(position):"";m_nExitCode=WFE_RunApplicationsSmoke(report,url,program,temporary,first=="--applications-reopen-smoke");}
            else if(first=="--background-smoke" || first=="--background-reopen-smoke") {CString image=position?arguments.GetNext(position):"";m_nExitCode=WFE_RunBackgroundSmoke(report,url,image,first=="--background-reopen-smoke");}
            else if(first=="--dividers-smoke") m_nExitCode=WFE_RunDividerSmoke(report,url);
            else if(first=="--links-smoke") m_nExitCode=WFE_RunLinkSmoke(report,url);
            else if(first=="--interaction-smoke") m_nExitCode=WFE_RunInteractionSmoke(report,url);
            else if(first=="--printing-smoke") m_nExitCode=WFE_RunPrintingSmoke(report,url);
            else if(first=="--preferences-ui-smoke"||first=="--preferences-reopen-smoke") { CString page=position?arguments.GetNext(position):"";m_nExitCode=WFE_RunPreferencesSmoke(report,url,page,first=="--preferences-reopen-smoke"); }
            else if(first=="--news-ui-smoke") { CString server=position?arguments.GetNext(position):"";m_nExitCode=WFE_RunNewsUISmoke(report,url,server); }
            else if(first=="--mail-ui-smoke") { m_nExitCode=WFE_RunMailUISmoke(report,url); }
            else if(first=="--compose-ui-smoke") {
                WFE_SetMailPreference("email","preview@example.invalid");WFE_ApplyMailPreferences();
                MWContext *source=WFE_NewWindow("about:blank");
                m_nExitCode=WFE_ComposerUISmoke(source,report,url);
                CString quote_source=position?arguments.GetNext(position):"";
                if(!m_nExitCode&&!quote_source.IsEmpty()) m_nExitCode=WFE_ComposerQuoteSmoke(report,quote_source);
                if(source) WFE_CloseWindow(source);
            }
            else if (first == "--ui-gallery") { m_nExitCode=WFE_UIGallery(report); }
            else if (first == "--mailnews-smoke" || first == "--composer-delivery-smoke") {
                CString server = position ? arguments.GetNext(position) : "";
                m_nExitCode = first == "--mailnews-smoke" ? WFE_RunMailNewsSmoke(report, url == "news", server) : WFE_RunComposerDeliverySmoke(report, url == "news", server);
            } else if (first == "--images-smoke") m_nExitCode = WFE_RunImageSmoke(report, url);
            else if (first == "--forms-smoke") m_nExitCode = WFE_RunFormsSmoke(report, url);
            else if (first == "--frames-smoke") m_nExitCode = WFE_RunFramesSmoke(report, url);
            else if (first == "--history-smoke") m_nExitCode = WFE_RunHistorySmoke(report, url);
            else if (first == "--history-reopen-smoke") m_nExitCode = WFE_RunHistoryReopenSmoke(report, url);
            else if (first == "--chrome-smoke") {
                CString snapshot = position ? arguments.GetNext(position) : "";
                m_nExitCode = WFE_RunChromeSmoke(report, url, snapshot);
            }
            else if (first == "--bookmarks-smoke") {
                MWContext *source = WFE_NewWindow("about:blank");
                if (source) browser_smoke_wait(frame_for(source));
                m_nExitCode = source ? WFE_BookmarksSmoke(source, report, url) : 2;
                if (source) WFE_CloseWindow(source);
            }
            else if (first == "--downloads-smoke") {
                CString directory = position ? arguments.GetNext(position) : "";
                MWContext *source = WFE_NewWindow(nullptr);
                m_nExitCode = WFE_DownloadSmoke(source, report, url, directory);
                WFE_CloseWindow(source);
            }
            else m_nExitCode = WFE_RunSmokeTest(report, url.IsEmpty() ? nullptr : (const char *)url);
            /* Close fixture-created editors before tearing down their engines. */
            while(!browser_windows.empty()) browser_windows.back()->DestroyWindow();
            WFE_ComposerCloseAll();m_pMainWnd=nullptr;
            WFE_Shutdown();
            backend_initialized = false;
            return FALSE;
        }
        if(!explicit_command) return open_preferred_startup();
#ifdef EDITOR
        if(first=="--edit" || first=="--editor" || first=="-edit") {
            CString page=position?arguments.GetNext(position):"";
            return WFE_NewEditorWindow(page.IsEmpty()?nullptr:(const char *)page)!=nullptr;
        }
#endif
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
