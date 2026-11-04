/* Real Colors-page controls and profile replacement, under an isolated profile. */
int WFE_BackgroundUISmoke(CWnd *parent,const char *report,const char *image,bool reopened)
{
    GeneralPreferences before=WFE_GeneralPreferences();std::string error;
    std::string profile=std::string(WFE_ProfileDirectory())+"\\browser.ini",old_bytes,new_bytes;
    bool controls=false,cancelled=true,invalid=true,failed=true,saved=true;
    if(!reopened) {
        WFE_SaveGeneralPreferences(before,&error);preferences_file(profile,old_bytes);
        GeneralPreferencesDialog cancel(parent,true);if(!cancel.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
        cancel.SelectPage(2);cancel.ShowWindow(SW_SHOW);
        controls=cancel.Control(2,IDC_PREF_BG_IMAGE)->IsWindowEnabled()!=FALSE;
        cancel.Control(2,IDC_PREF_BG_IMAGE)->SendMessage(BM_CLICK);cancel.SetText(2,7340,image);
        controls=controls&&cancel.Control(2,7340)->IsWindowEnabled()&&cancel.Control(2,7341)->IsWindowEnabled();
        cancel.SendMessage(WM_COMMAND,IDCANCEL);cancel.DestroyWindow();
        cancelled=preferences_file(profile,new_bytes)&&old_bytes==new_bytes&&same_preferences(before,WFE_GeneralPreferences());
        GeneralPreferencesDialog save(parent,true);if(!save.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
        save.SelectPage(2);save.ShowWindow(SW_SHOW);save.Control(2,IDC_PREF_BG_IMAGE)->SendMessage(BM_CLICK);
        save.SetText(2,7340,(std::string(image)+".missing").c_str());save.SendMessage(WM_COMMAND,IDOK);
        invalid=!save.accepted&&!save.error.empty()&&same_preferences(before,WFE_GeneralPreferences());
        save.accepted=false;save.ShowWindow(SW_SHOW);save.SetText(2,7340,image);
        HANDLE lock=CreateFileA(profile.c_str(),GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,0,nullptr);
        failed=false;
        if(lock!=INVALID_HANDLE_VALUE) {
            save.SendMessage(WM_COMMAND,IDOK);
            failed=!save.accepted&&same_preferences(before,WFE_GeneralPreferences());CloseHandle(lock);
            failed=failed&&preferences_file(profile,new_bytes)&&old_bytes==new_bytes;
        }
        save.SendMessage(WM_COMMAND,IDOK);saved=save.accepted;save.DestroyWindow();
    }
    GeneralPreferencesDialog inspect(parent,true);if(!inspect.Create(IDD_GENERAL_PREFERENCES,parent)) return 2;
    inspect.SelectPage(2);inspect.ShowWindow(SW_SHOW);
    bool available=GetFileAttributesA(image)!=INVALID_FILE_ATTRIBUTES;
    bool persisted=(available?inspect.Checked(2,IDC_PREF_BG_IMAGE):inspect.Checked(2,IDC_PREF_BG_DEFAULT))&&inspect.Text(2,7340)==image;
    bool capture=preferences_capture(&inspect,(std::string(report)+".colors.bmp").c_str());inspect.DestroyWindow();
    bool passed=controls&&cancelled&&invalid&&failed&&saved&&persisted&&capture;
    if(reopened) passed=persisted&&capture;
    FILE *file=fopen(report,"wb");if(!file) return 2;
    fprintf(file,"controls=%d\ncancel=%d\ninvalid=%d\nsave_failure=%d\nsaved=%d\npersisted=%d\ncapture=%d\nui=%d\n",controls,cancelled,invalid,failed,saved,persisted,capture,passed);fclose(file);
    return passed?0:1;
}
