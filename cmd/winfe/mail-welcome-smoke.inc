/* Exercise the real first-run MSG/MIME/LO path with no public image requests. */
static void welcome_settle(BrowserFrame *frame)
{
    ULONGLONG deadline=GetTickCount64()+6000,quiet=0;
    while(GetTickCount64()<deadline) {
        mail_smoke_pump();
        if(!XP_IsContextBusy(&frame->context)&&!NET_AreThereActiveConnectionsForWindow(&frame->context)
           &&!frame->selection_update_pending) {
            if(!quiet) quiet=GetTickCount64();
            if(GetTickCount64()-quiet>=200) break;
        } else quiet=0;
    }
    frame->RefreshMailLists();
}
extern "C" char *msg_GetDummyEnvelope(void);
static int WFE_RunMailWelcomeSmoke(const char *report,const char *snapshot,const char *target)
{
    if(target&&*target) {
        char *data=nullptr,*type=nullptr;int32 length=0;
        void *owned=FE_AboutData("mailintro",&data,&length,&type);
        int created=data?WFE_CreateMailIntro(target,msg_GetDummyEnvelope(),data,length):0;
        FE_FreeAboutData(owned,"mailintro");
        FILE *file=fopen(report,"wb");if(!file) return 3;
        fprintf(file,"created=%d\nabout_outstanding=%d\n",created,WFE_AboutOutstanding());fclose(file);return 0;
    }
    /* Cache the built-in logo through IL before disabling remote images. */
    BrowserFrame *logo=frame_for(WFE_NewWindow("about:logo"));
    if(!logo) return 2;
    browser_smoke_wait(logo);logo->SendMessage(WM_CLOSE);mail_smoke_pump();
    automatic_images=false;
    BrowserFrame *frame=frame_for(WFE_NewMailWindow(0,nullptr));if(!frame) return 2;
    chrome_client_size(frame,1200,800);frame->ShowWindow(SW_SHOWNOACTIVATE);welcome_settle(frame);
    int auto_folder=frame->folders.GetNextItem(-1,LVNI_SELECTED);
    int auto_message=frame->messages.GetNextItem(-1,LVNI_SELECTED);
    std::string auto_reader=frame_text(frame);
    int inbox=find_folder(&frame->context,"Inbox");
    if(inbox>=0&&auto_folder!=inbox) {
        frame->folders.SetItemState(-1,0,LVIS_SELECTED);
        frame->folders.SetItemState(inbox,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
        welcome_settle(frame);
    }
    CString names,subject,sender,date,caption;
    for(int i=0;i<frame->folders.GetItemCount();++i) {if(i) names+='|';names+=frame->folders.GetItemText(i,0);}
    int count=frame->messages.GetItemCount();
    for(int i=0;i<count;++i) if(frame->messages.GetItemText(i,MailPaneUI::SubjectColumn)=="Welcome!") {
        subject=frame->messages.GetItemText(i,MailPaneUI::SubjectColumn);
        sender=frame->messages.GetItemText(i,MailPaneUI::SenderColumn);
        date=frame->messages.GetItemText(i,MailPaneUI::DateColumn);
        if(frame->messages.GetNextItem(-1,LVNI_SELECTED)!=i) {
            frame->messages.SetItemState(-1,0,LVIS_SELECTED);
            frame->messages.SetItemState(i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
            welcome_settle(frame);
        }
        break;
    }
    std::string reader=frame_text(frame);
    bool captured=chrome_capture(frame,snapshot);
    chrome_capture(frame,(std::string(snapshot)+".window.bmp").c_str(),true);
    CRect page;frame->view.GetClientRect(page);
    bool logo_drawn=false;
    auto record_logo=[&]() {
        for(auto image:frame->view.drawn_images)
            if(image->image_url&&!_stricmp((const char*)image->image_url,"about:logo")) logo_drawn=true;
    };
    record_logo();
    frame->view.scroll_y=(std::min)(230,(std::max)(0,frame->view.document_height-page.Height()));
    chrome_capture(frame,(std::string(snapshot)+".body.bmp").c_str(),true);record_logo();
    for(int y=page.Height();y<frame->view.document_height;y+=page.Height()) {
        frame->view.scroll_y=y;reader+=' ';reader+=frame_text(frame);record_logo();
    }
    std::replace(reader.begin(),reader.end(),'\n',' ');std::replace(reader.begin(),reader.end(),'\r',' ');
    frame->GetWindowText(caption);frame->SendMessage(WM_CLOSE);mail_smoke_pump();
    FILE *file=fopen(report,"wb");if(!file) return 3;
    fprintf(file,"count=%d\nfolders=%s\nsubject=%s\nsender=%s\ndate=%s\ncaption=%s\nauto_folder=%d\nauto_message=%d\nauto_reader=%d\nreader=%s\nabout_outstanding=%d\ncaptured=%d\nlogo_drawn=%d\n",
        count,(const char*)names,(const char*)subject,(const char*)sender,(const char*)date,(const char*)caption,
        auto_folder==inbox,auto_message>=0,auto_reader.find("Features in Brief")!=std::string::npos,
        reader.c_str(),WFE_AboutOutstanding(),captured,logo_drawn);
    fclose(file);return captured?0:4;
}
