extern "C" char *XP_TempDirName(void);
int WFE_AppsUISmoke(CWnd *parent,const char *report,const char *program,const char *temporary,bool reopen)
{
    const GeneralPreferences before=WFE_GeneralPreferences();std::string error;
    std::string profile=std::string(WFE_ProfileDirectory())+"\\browser.ini",old_bytes,new_bytes;
    std::string old_temp=XP_TempDirName();
    bool enabled=true,cancelled=true,invalid=true,failed=true,saved=true;
    if(!reopen) {
        WFE_SaveGeneralPreferences(before,&error);preferences_file(profile,old_bytes);
        GeneralPreferencesDialog cancel(parent,true);if(!cancel.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
        cancel.SelectPage(4);cancel.ShowWindow(SW_SHOW);
        for(int i=0;i<4;++i) {enabled=enabled&&cancel.Control(4,7500+i)->IsWindowEnabled();cancel.SetText(4,7500+i,i==3?temporary:program);}
        cancel.SendMessage(WM_COMMAND,IDCANCEL);cancel.DestroyWindow();
        cancelled=preferences_file(profile,new_bytes)&&old_bytes==new_bytes&&same_preferences(before,WFE_GeneralPreferences())&&old_temp==XP_TempDirName();
        GeneralPreferencesDialog save(parent,true);if(!save.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
        save.SelectPage(4);save.ShowWindow(SW_SHOW);
        for(int i=0;i<3;++i) save.SetText(4,7500+i,program);save.SetText(4,7503,temporary);
        save.SetText(4,7502,(std::string(program)+".missing").c_str());save.SendMessage(WM_COMMAND,IDOK);
        invalid=!save.accepted&&!save.error.empty()&&same_preferences(before,WFE_GeneralPreferences());
        save.accepted=false;save.ShowWindow(SW_SHOW);save.SetText(4,7502,program);
        save.SetText(4,7503,(std::string(temporary)+".missing").c_str());save.SendMessage(WM_COMMAND,IDOK);
        invalid=invalid&&!save.accepted&&!save.error.empty()&&same_preferences(before,WFE_GeneralPreferences());
        save.accepted=false;save.ShowWindow(SW_SHOW);save.SetText(4,7503,temporary);
        HANDLE lock=CreateFileA(profile.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        failed=false;
        if(lock!=INVALID_HANDLE_VALUE) {
            save.SendMessage(WM_COMMAND,IDOK);failed=!save.accepted&&same_preferences(before,WFE_GeneralPreferences())&&old_temp==XP_TempDirName();CloseHandle(lock);
            failed=failed&&preferences_file(profile,new_bytes)&&old_bytes==new_bytes;
        }
        save.SendMessage(WM_COMMAND,IDOK);saved=save.accepted;save.DestroyWindow();
    }
    GeneralPreferencesDialog inspect(parent,true);if(!inspect.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
    inspect.SelectPage(4);inspect.ShowWindow(SW_SHOW);
    const char *effective=GetFileAttributesA(temporary)==INVALID_FILE_ATTRIBUTES?WFE_DefaultTemporaryDirectory():temporary;
    bool persisted=inspect.Text(4,7503)==effective&&std::string(XP_TempDirName())==effective;
    for(int i=0;i<3;++i) persisted=persisted&&inspect.Text(4,7500+i)==program;
    bool capture=preferences_capture(&inspect,(std::string(report)+".apps.bmp").c_str());inspect.DestroyWindow();
    bool passed=enabled&&cancelled&&invalid&&failed&&saved&&persisted&&capture;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fprintf(file,"ui=%d\nenabled=%d\ncancel=%d\ninvalid=%d\nsave_failure=%d\nsaved=%d\npersisted=%d\ncapture=%d\n",passed,enabled,cancelled,invalid,failed,saved,persisted,capture);fclose(file);
    return passed?0:1;
}
