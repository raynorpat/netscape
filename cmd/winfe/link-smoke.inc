/* Exercise real popup menus, including their nested native message loop. */
static const char *link_popup_label=nullptr;
static bool link_popup_found=false;
static UINT link_popup_attempts=0;
static HWND link_popup_close=nullptr;
struct LinkClipboardHold {HANDLE ready;bool opened=false;};
static DWORD WINAPI link_hold_clipboard(void *data)
{
    LinkClipboardHold *hold=(LinkClipboardHold *)data;
    HWND window=CreateWindowExA(0,"STATIC","",0,0,0,0,0,HWND_MESSAGE,nullptr,nullptr,nullptr);
    hold->opened=window && ::OpenClipboard(window)!=FALSE;SetEvent(hold->ready);
    if(hold->opened) {Sleep(120);CloseClipboard();}
    if(window) DestroyWindow(window);return 0;
}
static void CALLBACK link_popup_timer(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND popup=nullptr;
    EnumThreadWindows(GetCurrentThreadId(),[](HWND window,LPARAM data)->BOOL {
        char name[32]={};GetClassNameA(window,name,sizeof(name));
        if(!strcmp(name,"#32768")) {*reinterpret_cast<HWND *>(data)=window;return FALSE;}
        return TRUE;
    },reinterpret_cast<LPARAM>(&popup));
    if(!popup) {if(++link_popup_attempts>40) {KillTimer(nullptr,timer);EndMenu();}return;}
    if(link_popup_close) {
        KillTimer(nullptr,timer);::DestroyWindow(link_popup_close);EndMenu();return;
    }
    HMENU menu=reinterpret_cast<HMENU>(::SendMessage(popup,0x01e1,0,0)); /* MN_GETHMENU */
    int selected=-1;bool found=false;
    for(int i=0;i<GetMenuItemCount(menu);++i) {
        MENUITEMINFOA info={sizeof(info)};info.fMask=MIIM_FTYPE|MIIM_STATE;
        GetMenuItemInfoA(menu,i,TRUE,&info);
        if(info.fType&MFT_SEPARATOR) continue;
        char label[160]={};GetMenuStringA(menu,i,label,sizeof(label),MF_BYPOSITION);
        std::string normalized;for(char c:std::string(label)) if(c!='&') normalized+=c;
        if(normalized==link_popup_label) {found=(info.fState&(MFS_DISABLED|MFS_GRAYED))==0;selected=i;break;}
    }
    KillTimer(nullptr,timer);link_popup_found=found;
    if(!found) {EndMenu();return;}
    ::SendMessage(popup,0x01e5,selected,0); /* MN_SELECTITEM: exact native menu position */
    ::PostMessage(popup,WM_KEYDOWN,VK_RETURN,0);
}
static bool link_popup(BrowserFrame *frame,CPoint point,const char *label)
{
    link_popup_label=label;link_popup_found=false;link_popup_attempts=0;
    UINT_PTR timer=::SetTimer(nullptr,0,50,link_popup_timer);
    frame->view.ClientToScreen(&point);
    frame->view.SendMessage(WM_CONTEXTMENU,(WPARAM)frame->view.GetSafeHwnd(),MAKELPARAM(point.x,point.y));
    ::KillTimer(nullptr,timer);return link_popup_found;
}
static int WFE_RunLinkSmoke(const char *report,const char *base)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,800,560);frame->ShowWindow(SW_SHOW);
    frame->Navigate((std::string(base)+"links").c_str());browser_smoke_wait(frame);frame_text(frame);
    CPoint text=interaction_point(frame,"Targeted text link",3);
    CString previous=frame->status.GetPaneText(1);
    interaction_mouse(frame->view,WM_MOUSEMOVE,text);
    bool hover=frame->status.GetPaneText(1)==(std::string(base)+"opened?x=1&y=two").c_str();
    bool hand=::GetCursor()==::LoadCursor(nullptr,IDC_HAND);
    frame->view.SendMessage(WM_MOUSELEAVE);
    bool leave=frame->status.GetPaneText(1)==previous;
    interaction_mouse(frame->view,WM_MOUSEMOVE,text);
    frame->status.SendMessage(WM_SETTEXT,0,(LPARAM)"Temporary menu help");
    frame->view.SendMessage(WM_MOUSELEAVE);
    leave=leave&&frame->status.GetPaneText(1)=="Temporary menu help";
    frame->status.SendMessage(WM_SETTEXT,0,(LPARAM)"");
    leave=leave&&frame->status.GetPaneText(1)==previous;
    bool copy=link_popup(frame,text,"Copy Link Location");
    interaction_record(report,"copylink",interaction_clipboard(frame));
    size_t windows=browser_windows.size();
    bool new_window=link_popup(frame,text,"Open in New Window");
    if(browser_windows.size()==windows+1) {
        BrowserFrame *opened=browser_windows.back();browser_smoke_wait(opened);
        new_window=new_window&&opened->current_url==std::string(base)+"opened?x=1&y=two"&&frame->current_url==std::string(base)+"links";
        opened->SendMessage(WM_CLOSE);
    } else new_window=false;
    bool image_copy=false,map_copy=false,server_copy=false,view_image=false;
    if(frame->view.drawn_images.size()==3) {
        LO_ImageStruct *image=frame->view.drawn_images[0];
        CPoint point(image->x+image->x_offset+image->border_width+5,image->y+image->y_offset+image->border_width+6);
        image_copy=link_popup(frame,point,"Copy Image Location");interaction_record(report,"copyimage",interaction_clipboard(frame));
        image=frame->view.drawn_images[1];point=CPoint(image->x+image->x_offset+image->border_width+5,image->y+image->y_offset+image->border_width+6);
        map_copy=link_popup(frame,point,"Copy Link Location");interaction_record(report,"copymap",interaction_clipboard(frame));
        image=frame->view.drawn_images[2];point=CPoint(image->x+image->x_offset+image->border_width+5,image->y+image->y_offset+image->border_width+6);
        server_copy=link_popup(frame,point,"Copy Link Location");interaction_record(report,"copyservermap",interaction_clipboard(frame));
        image=frame->view.drawn_images[0];point=CPoint(image->x+image->x_offset+image->border_width+5,image->y+image->y_offset+image->border_width+6);
        view_image=link_popup(frame,point,"View Image");browser_smoke_wait(frame);
        view_image=view_image&&frame->current_url==std::string(base)+"pixel.gif";
    }
    frame->Navigate((std::string(base)+"linkframes").c_str());browser_smoke_wait(frame);
    BrowserFrame *child=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("linkSource")));
    BrowserFrame *target=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("linkTarget")));
    bool targeted=false,child_hover=false;
    if(child&&target) {
        browser_smoke_wait(child);browser_smoke_wait(target);
        CPoint point=interaction_point(child,"Targeted text link",3);
        interaction_mouse(child->view,WM_MOUSEMOVE,point);
        child_hover=frame->status.GetPaneText(1)==(std::string(base)+"opened?x=1&y=two").c_str();
        targeted=link_popup(child,point,"Open this Link");browser_smoke_wait(target);
        targeted=targeted&&target->current_url==std::string(base)+"opened?x=1&y=two"&&child->current_url==std::string(base)+"links";
    }
    LinkClipboardHold hold={CreateEvent(nullptr,TRUE,FALSE,nullptr)};
    HANDLE worker=CreateThread(nullptr,0,link_hold_clipboard,&hold,0,nullptr);
    bool busy_clipboard=false;
    if(worker && WaitForSingleObject(hold.ready,2000)==WAIT_OBJECT_0 && hold.opened) {
        BrowserLinkHit hit;hit.link=std::string(base)+"busy-copy";
        WFE_BrowserLinkCommand(&frame->context,hit,ID_BROWSER_COPY_LINK);
        WaitForSingleObject(worker,2000);
        interaction_record(report,"busycopy",interaction_clipboard(frame));busy_clipboard=true;
    }
    if(worker) {WaitForSingleObject(worker,2000);CloseHandle(worker);}CloseHandle(hold.ready);
    BrowserFrame *closing=frame_for(WFE_NewWindow(nullptr));bool close_safe=false;
    if(closing) {
        closing->Navigate((std::string(base)+"links").c_str());browser_smoke_wait(closing);closing->ShowWindow(SW_SHOW);
        link_popup_close=closing->GetSafeHwnd();
        link_popup(closing,interaction_point(closing,"Targeted text link",3),"");
        close_safe=!::IsWindow(link_popup_close);link_popup_close=nullptr;
    }
    bool passed=hover&&hand&&leave&&copy&&new_window&&image_copy&&map_copy&&server_copy&&view_image&&targeted&&child_hover&&close_safe&&busy_clipboard;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fprintf(file,"passed=%d\nhover=%d\nhand=%d\nleave=%d\ncopy=%d\nnew_window=%d\nimage_copy=%d\nmap_copy=%d\nserver_copy=%d\nview_image=%d\ntargeted=%d\nchild_hover=%d\nclose_safe=%d\n",passed,hover,hand,leave,copy,new_window,image_copy,map_copy,server_copy,view_image,targeted,child_hover,close_safe);
    fclose(file);frame->SendMessage(WM_CLOSE);return passed?0:1;
}
