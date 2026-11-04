/* Exercise the real EDT engine through the native frontend. */
static BrowserFrame *editor_close_frame;
static bool editor_nested_close_blocked;
static void CALLBACK editor_cancel_close(HWND,UINT,UINT_PTR timer,DWORD)
{
    HWND dialog=FindWindowA("#32770","Netscape Gold");
    if(!dialog || GetWindowThreadProcessId(dialog,nullptr)!=GetCurrentThreadId()) return;
    KillTimer(nullptr,timer);
    HWND frame=editor_close_frame->GetSafeHwnd();
    editor_close_frame->SendMessage(WM_CLOSE);
    editor_nested_close_blocked=::IsWindow(frame)!=FALSE;
    PostMessage(dialog,WM_COMMAND,IDCANCEL,0);
}
static bool editor_bold(MWContext *context)
{
    EDT_CharacterData *data=EDT_GetCharacterData(context);
    bool result=data && (data->mask&TF_BOLD) && (data->values&TF_BOLD);
    if(data) EDT_FreeCharacterData(data);
    return result;
}
static bool editor_save_wait(MWContext *context,const char *path)
{
    if(WFE_EditorSave(context,path,TRUE)!=1) return false;
    ULONGLONG deadline=GetTickCount64()+15000;
    while(WFE_EditorSaving(context) && GetTickCount64()<deadline) mail_smoke_pump();
    return !WFE_EditorSaving(context) && WFE_EditorSaveStatus(context)==ED_ERROR_NONE;
}
static int WFE_RunEditorSmoke(const char *report,const char *input)
{
    WFE_EditorTestMode(TRUE);
    if(!dump_native_menus(report)) return 2;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fclose(file);
    BrowserFrame *frame=frame_for(WFE_NewEditorWindow(input));
    if(!frame) return 2;
    frame->ShowWindow(SW_SHOWNOACTIVATE);browser_smoke_wait(frame);
    MWContext *context=&frame->context;
    bool loaded=context->is_editor && EDT_HaveEditBuffer(context) && !EDT_IsBlocked(context)
        && frame_text(frame).find("Original Gold heading")!=std::string::npos;
    bool fresh=false;
    if(loaded) {
        WFE_EditorCommand(context,ID_EDITOR_NEW);
        BrowserFrame *blank=browser_windows.empty()?nullptr:browser_windows.back();
        if(blank && blank!=frame && blank->editing) {
            browser_smoke_wait(blank);
            MWContext *other=&blank->context;
            if(EDT_HaveEditBuffer(other) && !EDT_IsBlocked(other)) {
                EDT_KeyDown(other,'G',1,0);
                fresh=other->is_new_document && EDT_DirtyFlag(other) &&
                    editor_save_wait(other,(std::string(report)+".new.html").c_str()) && !EDT_DirtyFlag(other);
                EDT_SetDirtyFlag(other,FALSE);
            }
            blank->SendMessage(WM_CLOSE);
        }
    }
    bool typed=false,format=false,undo=false,redo=false,clipboard=false,selected=false,mouse=false;
    bool ui=loaded && WFE_EditorUISmoke(context,report);
    if(loaded) {
        auto link=frame->view.links.find("Relative link");
        if(link!=frame->view.links.end()) {
            CPoint point=link->second;point.Offset(-frame->view.scroll_x,-frame->view.scroll_y);
            std::string before=frame->current_url;
            frame->view.SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,MAKELPARAM(point.x,point.y));
            frame->view.SendMessage(WM_LBUTTONUP,0,MAKELPARAM(point.x,point.y));
            frame->view.SendMessage(WM_LBUTTONDBLCLK,MK_LBUTTON,MAKELPARAM(point.x,point.y));
            LO_Element *first=nullptr,*last=nullptr;int32 a=0,b=0;
            LO_GetSelectionEndpoints(context,&first,&last,&a,&b);
            mouse=first && last && frame->current_url==before;
        }
        EDT_EndOfDocument(context,FALSE);EDT_ReturnKey(context);
        MSG key={};key.hwnd=frame->view.GetSafeHwnd();key.message=WM_CHAR;
        const char *text="Native Gold editing";
        for(const char *p=text;*p;++p) {key.wParam=(unsigned char)*p;WFE_EditorKey(context,&key);}
        typed=frame_text(frame).find(text)!=std::string::npos && EDT_DirtyFlag(context);
        EDT_BeginOfLine(context,TRUE);
        LO_Element *first=nullptr,*last=nullptr;int32 a=0,b=0;
        LO_GetSelectionEndpoints(context,&first,&last,&a,&b);selected=first && last;
        WFE_EditorCommand(context,ID_EDITOR_BOLD);format=editor_bold(context);
        WFE_EditorCommand(context,ID_EDIT_UNDO);undo=!editor_bold(context);
        WFE_EditorCommand(context,ID_EDIT_REDO);redo=editor_bold(context);
        WFE_EditorCommand(context,ID_EDIT_COPY);
        clipboard=IsClipboardFormatAvailable(RegisterClipboardFormatA("NETSCAPE_EDITOR"))!=0;
        EDT_EndOfDocument(context,FALSE);EDT_ReturnKey(context);
        WFE_EditorCommand(context,ID_EDIT_PASTE);
        std::string rendered=frame_text(frame);size_t at=rendered.find(text);
        clipboard=clipboard && at!=std::string::npos && rendered.find(text,at+strlen(text))!=std::string::npos;
    }
    std::string destination=std::string(report)+".saved.html";
    bool saved=loaded && editor_save_wait(context,destination.c_str()) && !EDT_DirtyFlag(context);
    CString caption;frame->GetWindowText(caption);
    bool title=caption=="Netscape Editor - [Gold round trip]";
    bool capture=chrome_capture(frame,(std::string(report)+".bmp").c_str(),true);
    CRect bounds;frame->GetWindowRect(bounds);
    bool size=bounds.Width()==1105 && bounds.Height()==765;
    bool reopened=false,browsed=false;
    if(saved) {
        /* Use a second path so opening cannot simply activate the live editor. */
        std::string second=std::string(report)+".reopen.html";
        CopyFileA(destination.c_str(),second.c_str(),FALSE);
        BrowserFrame *other=frame_for(WFE_NewEditorWindow(second.c_str()));
        if(other) {
            browser_smoke_wait(other);
            reopened=EDT_HaveEditBuffer(&other->context) && frame_text(other).find("Native Gold editing")!=std::string::npos;
            EDT_SetDirtyFlag(&other->context,FALSE);other->SendMessage(WM_CLOSE);
        }
        BrowserFrame *browser=frame_for(WFE_NewWindow(destination.c_str()));
        if(browser) {browser_smoke_wait(browser);browsed=frame_text(browser).find("Native Gold editing")!=std::string::npos;browser->SendMessage(WM_CLOSE);}
    }
    bool failure=false;
    bool close_cancel=false;
    bool navigation_cancel=false;
#ifdef WFE_EDITOR_STREAM_TEST
    extern bool WFE_EditorStreamSmoke(const char *);
    bool flush=WFE_EditorStreamSmoke((std::string(report)+".flush").c_str());
#else
    bool flush=false;
#endif
    if(loaded) {
        EDT_EndOfDocument(context,FALSE);EDT_KeyDown(context,'!',1,0);
        bool result=editor_save_wait(context,(std::string(report)+".missing\\failed.html").c_str());
        failure=!result && EDT_DirtyFlag(context) && WFE_EditorSaveStatus(context)!=ED_ERROR_NONE;
        editor_close_frame=frame;editor_nested_close_blocked=false;
        UINT_PTR timer=::SetTimer(nullptr,0,50,editor_cancel_close);
        if(timer) {
            close_cancel=!WFE_EditorCheckSave(context) && EDT_DirtyFlag(context) && editor_nested_close_blocked;
            ::KillTimer(nullptr,timer);
        }
        editor_nested_close_blocked=false;
        timer=::SetTimer(nullptr,0,50,editor_cancel_close);
        if(timer) {
            std::string before=frame->current_url;
            WFE_NavigateTarget(context,"about:blank",nullptr);
            navigation_cancel=frame->current_url==before && EDT_DirtyFlag(context) && editor_nested_close_blocked;
            ::KillTimer(nullptr,timer);
        }
        editor_close_frame=nullptr;
    }
    bool passed=loaded&&typed&&selected&&format&&undo&&redo&&clipboard&&ui&&saved&&reopened&&browsed&&failure&&capture&&size&&title&&flush&&mouse&&close_cancel&&fresh&&navigation_cancel;
    file=fopen(report,"ab");
    if(file) {
        fprintf(file,"final_flush_rejected=%d\n",flush);
        fprintf(file,"mouse_selection=%d\nclose_cancel=%d\n",mouse,close_cancel);
        fprintf(file,"new_document=%d\n",fresh);
        fprintf(file,"navigation_cancel=%d\n",navigation_cancel);
        fprintf(file,"passed=%d\nloaded=%d\ntyped=%d\nselection=%d\nformat=%d\nundo=%d\nredo=%d\nclipboard=%d\nsaved=%d\nreopened=%d\nbrowsed=%d\nfailed_save_retained=%d\ncapture=%d\nreference_size=%d\ntitle=%d\ncaption=%s\nerror=%s\n",passed,loaded,typed,selected,format,undo,redo,clipboard,saved,reopened,browsed,failure,capture,size,title,(const char *)caption,smoke_error.c_str());
        fclose(file);
    } else passed=false;
    if(EDT_HaveEditBuffer(context)) EDT_SetDirtyFlag(context,FALSE);
    frame->SendMessage(WM_CLOSE);WFE_EditorTestMode(FALSE);
    return passed?0:1;
}
