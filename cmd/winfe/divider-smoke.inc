static LO_EdgeStruct *divider_find(BrowserFrame *frame,bool vertical)
{
    CRect area;frame->view.GetClientRect(area);
    int limit=vertical?area.Width():area.Height();
    for(int n=0;n<limit;++n) {
        int x=vertical?n:area.Width()*3/4,y=vertical?area.Height()/4:n;
        LO_Element *element=LO_XYToElement(&frame->context,x,y);
        if(element && element->type==LO_EDGE && !!element->lo_edge.is_vertical==vertical) return &element->lo_edge;
    }
    return nullptr;
}
static CPoint divider_point(LO_EdgeStruct *edge)
{return CPoint(edge->x+edge->x_offset+(edge->is_vertical?edge->width/2:edge->width/3),edge->y+edge->y_offset+(edge->is_vertical?edge->height/4:edge->height/2));}
static void divider_drag(BrowserFrame *frame,CPoint from,CPoint to)
{
    interaction_mouse(frame->view,WM_LBUTTONDOWN,from,MK_LBUTTON);
    interaction_mouse(frame->view,WM_MOUSEMOVE,to,MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,to);browser_smoke_wait(frame);
}
static int WFE_RunDividerSmoke(const char *report,const char *base)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,900,600);frame->ShowWindow(SW_SHOW);
    std::string source=std::string(base)+"dividers";frame->Navigate(source.c_str());browser_smoke_wait(frame);
    BrowserFrame *left=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("dividerLeft")));
    bool vertical=false,horizontal=false,bounds=false,cancel=false,noresize=false,history=false,form=false,navigation=false,tracking=false,cursor=false,escape=false,resize=false;
    LO_EdgeStruct *edge=divider_find(frame,true);
    if(left&&edge) {
        browser_smoke_wait(left);HWND input=WFE_FormFindControl(&left->context,"retain",0);
        if(input) SetWindowTextA(input,"retained divider edit");
        CRect before,preview,after;left->GetWindowRect(before);CPoint point=divider_point(edge),moved=point+CPoint(80,0);
        interaction_mouse(frame->view,WM_MOUSEMOVE,point);
        bool hover_cursor=::GetCursor()==::LoadCursor(nullptr,IDC_SIZEWE);
        interaction_mouse(frame->view,WM_LBUTTONDOWN,point,MK_LBUTTON);
        bool captured=::GetCapture()==frame->view.GetSafeHwnd();
        interaction_mouse(frame->view,WM_MOUSEMOVE,moved,MK_LBUTTON);left->GetWindowRect(preview);
        tracking=captured&&preview==before;cursor=hover_cursor&&::GetCursor()==::LoadCursor(nullptr,IDC_SIZEWE);
        interaction_mouse(frame->view,WM_LBUTTONUP,moved);browser_smoke_wait(frame);browser_smoke_wait(left);
        left->GetWindowRect(after);vertical=after.Width()==before.Width()+80;
        form=control_text(WFE_FormFindControl(&left->context,"retain",0))=="retained divider edit";
        int saved_x=edge->x;
        chrome_capture(frame,(std::string(report)+".frames.bmp").c_str());
        point=divider_point(edge);interaction_mouse(frame->view,WM_LBUTTONDOWN,point,MK_LBUTTON);
        interaction_mouse(frame->view,WM_MOUSEMOVE,point+CPoint(60,0),MK_LBUTTON);
        frame->SetCapture();::ReleaseCapture();left->GetWindowRect(preview);cancel=preview==after;
        point=divider_point(edge);interaction_mouse(frame->view,WM_LBUTTONDOWN,point,MK_LBUTTON);
        interaction_mouse(frame->view,WM_MOUSEMOVE,point+CPoint(30,0),MK_LBUTTON);
        MSG key={};key.hwnd=frame->view.GetSafeHwnd();key.message=WM_KEYDOWN;key.wParam=VK_ESCAPE;
        escape=frame->PreTranslateMessage(&key)!=FALSE && ::GetCapture()!=frame->view.GetSafeHwnd();
        left->GetWindowRect(preview);escape=escape&&preview==after;
        int minimum=edge->left_top_bound,maximum=edge->right_bottom_bound;
        divider_drag(frame,divider_point(edge),CPoint(-200,point.y));
        bool low=edge->x>=minimum+20;
        divider_drag(frame,divider_point(edge),CPoint(2000,point.y));
        bool high=edge->x<=maximum-20;bounds=low&&high;
        CPoint restore=divider_point(edge);divider_drag(frame,restore,restore+CPoint(saved_x-edge->x,0));
        edge=divider_find(frame,false);
        BrowserFrame *upper=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("dividerUpper")));
        if(edge&&upper) {
            CRect old_rect,new_rect;upper->GetWindowRect(old_rect);point=divider_point(edge);
            divider_drag(frame,point,point+CPoint(0,45));upper->GetWindowRect(new_rect);
            horizontal=new_rect.Height()==old_rect.Height()+45;
        }
        frame->Navigate((std::string(base)+"blank").c_str());browser_smoke_wait(frame);
        frame->SendMessage(WM_COMMAND,ID_NAV_BACK);browser_smoke_wait(frame);
        edge=divider_find(frame,true);history=edge&&edge->x==saved_x;
        left=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("dividerLeft")));
        if(left) {browser_smoke_wait(left);history=history&&control_text(WFE_FormFindControl(&left->context,"retain",0))=="retained divider edit";}
        else history=false;
        chrome_client_size(frame,980,640);browser_smoke_wait(frame);
        left=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("dividerLeft")));
        if(left) {browser_smoke_wait(left);resize=control_text(WFE_FormFindControl(&left->context,"retain",0))=="retained divider edit";}
        edge=divider_find(frame,true);
        if(edge) {
            point=divider_point(edge);interaction_mouse(frame->view,WM_LBUTTONDOWN,point,MK_LBUTTON);
            frame->Navigate((std::string(base)+"noresize").c_str());browser_smoke_wait(frame);
            navigation=::GetCapture()!=frame->view.GetSafeHwnd();
            edge=divider_find(frame,true);
            if(edge) {
                int old_x=edge->x;point=divider_point(edge);
                interaction_mouse(frame->view,WM_LBUTTONDOWN,point,MK_LBUTTON);
                bool denied=::GetCapture()!=frame->view.GetSafeHwnd();
                interaction_mouse(frame->view,WM_MOUSEMOVE,point+CPoint(70,0),MK_LBUTTON);
                interaction_mouse(frame->view,WM_LBUTTONUP,point+CPoint(70,0));
                noresize=!edge->movable&&denied&&edge->x==old_x;
                interaction_mouse(frame->view,WM_LBUTTONDBLCLK,point,MK_LBUTTON);
                noresize=noresize&&::GetCapture()!=frame->view.GetSafeHwnd();
                interaction_mouse(frame->view,WM_LBUTTONUP,point);
            }
        }
    }
    bool passed=vertical&&horizontal&&bounds&&cancel&&noresize&&history&&form&&navigation&&tracking&&cursor&&escape&&resize;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fprintf(file,"passed=%d\nvertical=%d\nhorizontal=%d\nbounds=%d\ncancel=%d\nnoresize=%d\nhistory=%d\nform=%d\nnavigation=%d\ntracking=%d\ncursor=%d\nescape=%d\nresize=%d\n",passed,vertical,horizontal,bounds,cancel,noresize,history,form,navigation,tracking,cursor,escape,resize);
    fclose(file);frame->SendMessage(WM_CLOSE);return passed?0:1;
}
