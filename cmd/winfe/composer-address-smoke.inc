static ComposerFrame *recipient_smoke_frame;
static UINT_PTR recipient_smoke_timer;
static int recipient_smoke_mode;
static bool recipient_smoke_seen,recipient_smoke_capture;
static std::string recipient_smoke_snapshot;
static bool pick_named_recipient(HWND dialog,const char *name,int command)
{
    HWND list=::GetDlgItem(dialog,IDC_AB_PICK_LIST);if(!list) return false;
    ::SendMessage(list,LB_SETSEL,FALSE,-1);
    int count=(int)::SendMessage(list,LB_GETCOUNT,0,0),found=-1;
    for(int i=1;i<count;++i) {
        int length=(int)::SendMessage(list,LB_GETTEXTLEN,i,0);if(length<0||length>32767) continue;
        std::vector<char> text(length+1);::SendMessage(list,LB_GETTEXT,i,(LPARAM)text.data());
        if(strstr(text.data(),name)) {found=i;break;}
    }
    if(found<0) return false;
    ::SendMessage(list,LB_SETSEL,TRUE,found);
    ::SendMessage(::GetDlgItem(dialog,command),BM_CLICK,0,0);return true;
}
static void CALLBACK drive_recipient_picker(HWND,UINT,UINT_PTR,DWORD)
{
    if(!recipient_smoke_frame) return;
    HWND owner=recipient_smoke_frame->GetSafeHwnd(),dialog=GetLastActivePopup(owner);
    if(dialog==owner||!::IsWindowVisible(dialog)||!::GetDlgItem(dialog,IDC_AB_PICK_LIST)
       ||::SendMessage(::GetDlgItem(dialog,IDC_AB_PICK_LIST),LB_GETCOUNT,0,0)<1) return;
    KillTimer(nullptr,recipient_smoke_timer);recipient_smoke_seen=true;
    if(recipient_smoke_mode==3) pick_named_recipient(dialog,"Native Team",IDC_AB_TO);
    else {
        pick_named_recipient(dialog,"alpha@example.invalid",IDC_AB_TO);
        pick_named_recipient(dialog,"alpha@example.invalid",IDC_AB_TO);
        pick_named_recipient(dialog,"beta@example.invalid",IDC_AB_CC);
        pick_named_recipient(dialog,"seed@example.invalid",IDC_AB_BCC);
        pick_named_recipient(dialog,"alpha@example.invalid",IDC_AB_BCC);
    }
    if(recipient_smoke_mode==1&&!recipient_smoke_snapshot.empty())
        recipient_smoke_capture=compose_capture(CWnd::FromHandle(dialog),recipient_smoke_snapshot.c_str(),false);
    if(recipient_smoke_mode==2) {
        recipient_smoke_frame->backend_closed=true;
        ::SendMessage(owner,WM_CLOSE,0,0);
    } else ::PostMessage(dialog,WM_COMMAND,recipient_smoke_mode==0?IDCANCEL:IDOK,0);
}
static bool run_recipient_picker(ComposerFrame *frame,int mode)
{
    frame->UpdateButtons();
    recipient_smoke_frame=frame;recipient_smoke_mode=mode;recipient_smoke_seen=false;
    recipient_smoke_timer=::SetTimer(nullptr,0,30,drive_recipient_picker);
    frame->address.SendMessage(BM_CLICK);
    KillTimer(nullptr,recipient_smoke_timer);recipient_smoke_frame=nullptr;
    return recipient_smoke_seen;
}
extern "C" int WFE_ComposerAddressSmoke(MWContext *source,const char *report,const char *snapshot)
{
    MWContext *context=MSG_ComposeMessage(source,nullptr,nullptr,"existing@example.invalid","copy@example.invalid","blind@example.invalid",
        nullptr,nullptr,nullptr,nullptr,"Address picker",nullptr,nullptr,nullptr,nullptr,nullptr,FALSE,FALSE);
    auto frame=composer_for(context);if(!frame) return 2;frame->UpdateButtons();
    bool enabled=frame->address.IsWindowEnabled()!=FALSE;
    if(!enabled) {FILE *file=fopen(report,"wb");if(file) {fprintf(file,"address_enabled=0\n");fclose(file);}frame->backend_closed=true;frame->DestroyWindow();return 1;}
    frame->changing=true;frame->body.SetWindowText("Recipient picker body\r\n");frame->changing=false;MSG_MessageBodyEdited(context);
    std::string attachment=std::string(WFE_ProfileDirectory())+"\\picker-attachment.txt";
    FILE *local=fopen(attachment.c_str(),"wb");if(!local) return 3;fputs("Recipient attachment\n",local);fclose(local);
    bool attached=frame->AttachPath(attachment.c_str());smoke_wait_ready(context);
    frame->displayed_headers&=~MSG_BCC_HEADER_MASK;frame->Arrange();frame->dirty=false;
    CString initial_to=frame->Header(2),initial_cc=frame->Header(3),initial_bcc=frame->Header(4);
    bool cancel=run_recipient_picker(frame,0)&&frame->Header(2)==initial_to&&frame->Header(3)==initial_cc&&frame->Header(4)==initial_bcc&&!frame->dirty;
    recipient_smoke_snapshot=snapshot?snapshot:"";recipient_smoke_capture=false;
    bool accepted=run_recipient_picker(frame,1);auto addresses=WFE_AddressBookContext(FALSE);
    bool empty=addresses&&BM_GetChildren(BM_GetRoot(addresses))==nullptr;
    CString to=frame->Header(2),cc=frame->Header(3),bcc=frame->Header(4);
    bool values=empty?(to==initial_to&&cc==initial_cc&&bcc==initial_bcc):
        (to.Find("alpha@example.invalid")>=0&&cc.Find("beta@example.invalid")>=0&&bcc.Find("seed@example.invalid")>=0&&bcc.Find("alpha@example.invalid")<0);
    bool bcc_visible=empty||frame->fields[4].IsWindowVisible();
    bool dirty=empty?!frame->dirty:frame->dirty;
    frame->dirty=false;
    bool repeated=run_recipient_picker(frame,3)&&frame->Header(2)==to&&frame->Header(3)==cc&&frame->Header(4)==bcc&&!frame->dirty;
    CString body;frame->body.GetWindowText(body);
    auto list=MSG_GetAttachmentList(context);bool retained=attached&&list&&list[0].url&&!list[1].url&&body=="Recipient picker body\r\n";
    bool draft=smoke_save_draft(context);
    bool captured=compose_capture(frame,(recipient_smoke_snapshot+".composition.bmp").c_str(),false)&&recipient_smoke_capture;
    frame->backend_closed=true;frame->DestroyWindow();
    MWContext *closing=MSG_ComposeMessage(source,nullptr,nullptr,"existing@example.invalid",nullptr,nullptr,
        nullptr,nullptr,nullptr,nullptr,"Picker owner close",nullptr,nullptr,nullptr,nullptr,nullptr,FALSE,FALSE);
    auto closing_frame=composer_for(closing);bool owner_close=false;
    if(closing_frame) {
        closing_frame->UpdateButtons();bool observed=run_recipient_picker(closing_frame,2);
        DWORD started=GetTickCount();while(composer_for(closing)&&GetTickCount()-started<3000) smoke_pump();
        owner_close=observed&&!composer_for(closing)&&WFE_ComposerWindowCount()==0;
        if(auto remaining=composer_for(closing)) {remaining->backend_closed=true;remaining->DestroyWindow();}
    }
    FILE *file=fopen(report,"wb");if(!file) return 3;
    fprintf(file,"address_enabled=%d\npicker_cancel=%d\npicker_accepted=%d\nrecipients=%d\nbcc_visible=%d\ndirty_state=%d\nrepeat_deduplicated=%d\nbody_attachments=%d\ndraft_saved=%d\nowner_closed=%d\ncaptured=%d\nempty_book=%d\nto=%s\ncc=%s\nbcc=%s\n",
        enabled,cancel,accepted,values,bcc_visible,dirty,repeated,retained,draft,owner_close,captured,empty,(const char*)to,(const char*)cc,(const char*)bcc);
    fclose(file);return enabled&&cancel&&accepted&&values&&bcc_visible&&dirty&&repeated&&retained&&draft&&owner_close&&captured?0:1;
}
