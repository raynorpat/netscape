/* Native browser gestures: use the same window messages as desktop input. */
static CPoint interaction_point(BrowserFrame *frame,const char *query,int offset)
{
    LO_Element *start=nullptr,*end=nullptr;int32 a=0,b=0;
    if(!LO_FindText(&frame->context,const_cast<char *>(query),&start,&a,&end,&b,FALSE,TRUE) || !start || start->type!=LO_TEXT)
        return CPoint(-100,-100);
    return CPoint(start->lo_text.x+start->lo_text.x_offset+LO_TextElementWidth(&frame->context,&start->lo_text,a+offset)+1-frame->view.scroll_x,
                  start->lo_text.y+start->lo_text.y_offset+start->lo_text.height/2-frame->view.scroll_y);
}
static void interaction_mouse(BrowserView &view,UINT message,CPoint point,UINT flags=0)
{ view.SendMessage(message,flags,MAKELPARAM(point.x,point.y)); }
static std::string interaction_selection(BrowserFrame *frame)
{
    char *text=(char *)LO_GetSelectionText(&frame->context);
    std::string result=text?text:"";free(text);return result;
}
static void interaction_record(const char *report,const char *name,const std::string &value)
{
    FILE *file=fopen((std::string(report)+"."+name).c_str(),"wb");
    if(file) {fwrite(value.data(),1,value.size(),file);fclose(file);}
}
static std::string interaction_clipboard(CWnd *window)
{
    std::string result;
    /* Clipboard history can briefly acquire it after our writer closes it. */
    bool opened=false;
    for(int i=0;i<50 && !(opened=window->OpenClipboard()!=FALSE);++i) Sleep(10);
    if(opened) {
        HGLOBAL data=GetClipboardData(CF_TEXT);const char *text=data?(const char *)GlobalLock(data):nullptr;
        if(text) {result=text;GlobalUnlock(data);}CloseClipboard();
    }
    return result;
}
static int WFE_RunInteractionSmoke(const char *report,const char *base)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,800,560);frame->ShowWindow(SW_SHOW);
    std::string source=std::string(base)+"selection";frame->Navigate(source.c_str());browser_smoke_wait(frame);
    CPoint first=interaction_point(frame,"Alpha Bravo Charlie Delta Echo",6);
    CPoint last=interaction_point(frame,"Alpha Bravo Charlie Delta Echo",19);
    interaction_mouse(frame->view,WM_LBUTTONDOWN,first,MK_LBUTTON);
    interaction_mouse(frame->view,WM_MOUSEMOVE,last,MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,last);
    std::string forward=interaction_selection(frame);interaction_record(report,"forward",forward);
    chrome_capture(frame,(std::string(report)+".selection.bmp").c_str());
    frame->SendMessage(WM_COMMAND,ID_EDIT_COPY);interaction_record(report,"clipboard",interaction_clipboard(frame));
    interaction_mouse(frame->view,WM_LBUTTONDOWN,last,MK_LBUTTON);
    interaction_mouse(frame->view,WM_MOUSEMOVE,first,MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,first);
    interaction_record(report,"backward",interaction_selection(frame));
    interaction_mouse(frame->view,WM_LBUTTONDBLCLK,interaction_point(frame,"Alpha Bravo Charlie Delta Echo",8),MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,first);
    interaction_record(report,"word",interaction_selection(frame));
    CPoint shift=interaction_point(frame,"Alpha Bravo Charlie Delta Echo",25);
    interaction_mouse(frame->view,WM_LBUTTONDOWN,shift,MK_LBUTTON|MK_SHIFT);
    interaction_mouse(frame->view,WM_LBUTTONUP,shift,MK_SHIFT);
    interaction_record(report,"shift",interaction_selection(frame));
    CPoint link=interaction_point(frame,"Drag this link",0),link_end=interaction_point(frame,"Drag this link",13);
    interaction_mouse(frame->view,WM_LBUTTONDOWN,link,MK_LBUTTON);
    interaction_mouse(frame->view,WM_MOUSEMOVE,link_end,MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,link_end);browser_smoke_wait(frame);
    bool drag_link=frame->current_url==source;interaction_record(report,"linkdrag",interaction_selection(frame));
    interaction_mouse(frame->view,WM_LBUTTONDOWN,first,MK_LBUTTON);
    bool capture=::GetCapture()==frame->view.GetSafeHwnd();
    frame->SetCapture();bool cancelled=!frame->view.selecting;::ReleaseCapture();
    interaction_mouse(frame->view,WM_LBUTTONDOWN,first,MK_LBUTTON);
    frame->Navigate((std::string(base)+"selection").c_str());browser_smoke_wait(frame);
    bool navigation=!frame->view.selecting && ::GetCapture()!=frame->view.GetSafeHwnd();
    first=interaction_point(frame,"Alpha Bravo Charlie Delta Echo",6);
    CRect area;frame->view.GetClientRect(area);
    interaction_mouse(frame->view,WM_LBUTTONDOWN,first,MK_LBUTTON);
    interaction_mouse(frame->view,WM_MOUSEMOVE,CPoint(80,area.bottom+20),MK_LBUTTON);
    frame->view.SendMessage(WM_TIMER,71);frame->view.SendMessage(WM_TIMER,71);
    bool autoscroll=frame->view.scroll_y>0;
    interaction_mouse(frame->view,WM_LBUTTONUP,CPoint(80,area.bottom-5));
    bool capture_released=::GetCapture()!=frame->view.GetSafeHwnd();
    WFE_Scroll(&frame->context,0,0);
    link=interaction_point(frame,"Drag this link",3);
    interaction_mouse(frame->view,WM_LBUTTONDOWN,link,MK_LBUTTON);
    interaction_mouse(frame->view,WM_LBUTTONUP,link);browser_smoke_wait(frame);
    bool clicked=frame->current_url==std::string(base)+"clicked";
    frame->Navigate((std::string(base)+"selectionframes").c_str());browser_smoke_wait(frame);
    BrowserFrame *child=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>("selectionLeft")));
    bool child_selection=false;
    if(child) {
        browser_smoke_wait(child);WFE_Scroll(&child->context,0,300);
        CPoint a=interaction_point(child,"Scrolling selection line 12",0),b=interaction_point(child,"Scrolling selection line 12",9);
        interaction_mouse(child->view,WM_LBUTTONDOWN,a,MK_LBUTTON);
        interaction_mouse(child->view,WM_MOUSEMOVE,b,MK_LBUTTON);
        interaction_mouse(child->view,WM_LBUTTONUP,b);
        child_selection=child->view.scroll_y==300 && interaction_selection(child)=="Scrolling";
        frame->SendMessage(WM_COMMAND,ID_EDIT_COPY);interaction_record(report,"frameclipboard",interaction_clipboard(frame));
    }
    bool passed=forward=="Bravo Charlie"&&drag_link&&capture&&cancelled&&navigation&&autoscroll&&capture_released&&clicked&&child_selection;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fprintf(file,"passed=%d\ndrag_link=%d\ncapture=%d\ncapture_cancel=%d\nnavigation_cancel=%d\nautoscroll=%d\ncapture_released=%d\nclicked=%d\nframe_selection=%d\n",passed,drag_link,capture,cancelled,navigation,autoscroll,capture_released,clicked,child_selection);
    fclose(file);frame->SendMessage(WM_CLOSE);return passed?0:1;
}
