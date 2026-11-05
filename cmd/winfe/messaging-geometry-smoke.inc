/* Capture native controls before the interaction fixtures resize their panes. */
static void ui_message_geometry(BrowserFrame *frame,const char *report)
{
    FILE *file=fopen((std::string(report)+".geometry").c_str(),"wb");if(!file) return;
    auto bounds=[&](const char *name,CWnd *window,CWnd *parent) {
        CRect rect;window->GetWindowRect(rect);parent->ScreenToClient(rect);
        fprintf(file,"%s=%d,%d,%d,%d\n",name,rect.left,rect.top,rect.right,rect.bottom);
    };
    bounds("first",frame->chrome.Button(frame->mode==2?ID_CLIENT_POST:ID_MESSAGE_GET),&frame->chrome);
    bounds("second",frame->chrome.Button(frame->mode==2?ID_NEWS_MAIL_NEW:ID_MESSAGE_DELETE),&frame->chrome);
    bounds("last",frame->chrome.Button(ID_NAV_STOP),&frame->chrome);
    bounds("logo",frame->chrome.GetDlgItem(ID_CHROME_LOGO),&frame->chrome);
    CRect area;frame->chrome.GetClientRect(area);fprintf(file,"chrome_height=%d\n",area.Height());
    frame->messages.GetClientRect(area);fprintf(file,"message_client_width=%d\n",area.Width());
    for(int i=0;i<3;++i) fprintf(file,"folder_column_%d=%d\n",i,frame->folders.GetColumnWidth(i));
    for(int i=0;i<5;++i) fprintf(file,"message_column_%d=%d\n",i,frame->messages.GetColumnWidth(i));
    HDITEM item={};item.mask=HDI_FORMAT;frame->messages.GetHeaderCtrl()->GetItem(MailPaneUI::DateColumn,&item);
    fprintf(file,"date_marker=%d\n",(item.fmt&HDF_SORTDOWN)!=0);
    fclose(file);
}
