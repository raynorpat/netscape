extern int WFE_BackgroundUISmoke(CWnd *,const char *,const char *,bool);
static int WFE_RunBackgroundSmoke(const char *report,const char *base,const char *image,bool reopen)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,800,560);frame->ShowWindow(SW_SHOW);
    int ui=WFE_BackgroundUISmoke(frame,report,image,reopen);if(ui) return ui;
    frame->Navigate((std::string(base)+"plain").c_str());browser_smoke_wait(frame);frame_text(frame);
    if(reopen && GetFileAttributesA(image)==INVALID_FILE_ATTRIBUTES) {
        bool fallback=!WFE_GeneralPreferences().background_image && !frame->view.backdrop;
        chrome_capture(frame,(std::string(report)+".clear.bmp").c_str());
        FILE *file=fopen(report,"ab");if(!file) return 2;
        fprintf(file,"fallback=%d\npassed=%d\n",fallback,fallback);fclose(file);
        frame->SendMessage(WM_CLOSE);return fallback?0:1;
    }
    bool default_image=frame->view.backdrop!=nullptr;
    chrome_capture(frame,(std::string(report)+".default.bmp").c_str());
    frame->Navigate((std::string(base)+"pagebg").c_str());browser_smoke_wait(frame);frame_text(frame);
    bool page_image=frame->view.backdrop!=nullptr;
    chrome_capture(frame,(std::string(report)+".page.bmp").c_str());
    GeneralPreferences p=WFE_GeneralPreferences();std::string error;p.override_colors=true;
    bool saved=WFE_SaveGeneralPreferences(p,&error);
    frame->Navigate((std::string(base)+"override").c_str());browser_smoke_wait(frame);frame_text(frame);
    bool override_image=frame->view.backdrop!=nullptr;
    chrome_capture(frame,(std::string(report)+".override.bmp").c_str());
    bool editor_preserved=true;
#ifdef EDITOR
    BrowserFrame *editor=frame_for(WFE_NewEditorWindow(nullptr));
    if(editor) {
        browser_smoke_wait(editor);std::string destination=std::string(report)+".editor.html";
        editor_preserved=WFE_EditorSave(&editor->context,destination.c_str(),1)>0;browser_smoke_wait(editor);
        EDT_PageData *data=EDT_GetPageData(&editor->context);
        editor_preserved=editor_preserved && data && (!data->pBackgroundImage || !*data->pBackgroundImage);
        if(data) EDT_FreePageData(data);editor->SendMessage(WM_CLOSE);
    } else editor_preserved=false;
#endif
    p.override_colors=false;p.background_image=false;saved=WFE_SaveGeneralPreferences(p,&error)&&saved;
    frame->Navigate((std::string(base)+"clear").c_str());browser_smoke_wait(frame);frame_text(frame);
    bool cleared=!frame->view.backdrop;
    chrome_capture(frame,(std::string(report)+".clear.bmp").c_str());
    /* Return to the saved image mode for the next process. */
    p.background_image=true;saved=WFE_SaveGeneralPreferences(p,&error)&&saved;
    bool passed=saved&&default_image&&page_image&&override_image&&cleared&&editor_preserved;
    FILE *file=fopen(report,"ab");if(!file) return 2;
    fprintf(file,"default_image=%d\npage_image=%d\noverride_image=%d\npassed=%d\n",default_image,page_image,override_image,passed);fclose(file);
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}
