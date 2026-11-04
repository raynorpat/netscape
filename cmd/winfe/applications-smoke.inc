extern int WFE_AppsUISmoke(CWnd *,const char *,const char *,const char *,bool);
#include "applications.h"
static bool apps_wait(const std::string &path)
{
    ULONGLONG deadline=GetTickCount64()+10000;
    while(GetTickCount64()<deadline) {
        FILE *file=fopen(path.c_str(),"rb");
        if(file) {
            bool ready=true;
            if(path.size()>=5 && path.compare(path.size()-5,5,".argv")==0) {
                char buffer[4096]={};size_t size=fread(buffer,1,sizeof(buffer)-1,file);buffer[size]=0;
                ready=strstr(buffer,"\ndone=1\n")!=nullptr;
            }
            fclose(file);if(ready) return true;
        }
        mail_smoke_pump();Sleep(5);
    }
    return false;
}
static WFE_DownloadSnapshot apps_wait_download(uint32 id)
{
    WFE_DownloadSnapshot snapshot={};ULONGLONG deadline=GetTickCount64()+10000;
    while(id && GetTickCount64()<deadline) {
        if(WFE_GetDownloadSnapshot(id,&snapshot) && snapshot.state>=WFE_DOWNLOAD_COMPLETE) return snapshot;
        mail_smoke_pump();Sleep(5);
    }
    return snapshot;
}
static bool apps_argument(const std::string &path,int index,const char *expected)
{
    FILE *file=fopen(path.c_str(),"rb");if(!file) return false;
    char buffer[4096]={};size_t size=fread(buffer,1,sizeof(buffer)-1,file);fclose(file);
    char key[32];sprintf(key,"\narg%d=",index);
    return std::string(buffer,size).find(std::string(key)+expected+"\n")!=std::string::npos;
}
static int WFE_RunApplicationsSmoke(const char *report,const char *base,const char *program,const char *temporary,bool reopen)
{
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));if(!frame) return 2;
    chrome_client_size(frame,800,560);frame->ShowWindow(SW_SHOW);
    int ui=WFE_AppsUISmoke(frame,report,program,temporary,reopen);if(ui) return ui;
    if(reopen && GetFileAttributesA(temporary)==INVALID_FILE_ATTRIBUTES) {
        char *path=WH_TempName(xpTemporary,"app");
        bool fallback=path&&std::string(path).find(WFE_DefaultTemporaryDirectory())==0;
        if(path) {DeleteFileA(path);free(path);}
        FILE *file=fopen(report,"ab");if(!file) return 2;
        fprintf(file,"startup_fallback=%d\npassed=%d\n",fallback,fallback);fclose(file);
        frame->SendMessage(WM_CLOSE);return fallback?0:1;
    }
    std::string probe=std::string(report)+".source.argv",copy=std::string(report)+".source.bytes";
    SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());SetEnvironmentVariableA("NETSCAPE_APP_PROBE_COPY",copy.c_str());
    frame->Navigate((std::string(base)+"source").c_str());browser_smoke_wait(frame);
    frame->SendMessage(WM_COMMAND,ID_MENU_DOCUMENT_SOURCE);
    bool source=apps_wait(copy)&&apps_wait(probe);
    probe=std::string(report)+".telnet.argv";SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());
    frame->Navigate("telnet://john.doe@terminal.test:2323");browser_smoke_wait(frame);
    bool telnet=apps_wait(probe)&&apps_argument(probe,2,"john.doe");
    probe=std::string(report)+".tn3270.argv";SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());
    frame->Navigate("tn3270://jane_doe-user@terminal.test:3270");browser_smoke_wait(frame);
    bool tn3270=apps_wait(probe)&&apps_argument(probe,2,"jane_doe-user");
    char *name=WH_TempName(xpTemporary,"app");
    bool temp=name&&std::string(name).find(temporary)==0;
    if(name) {DeleteFileA(name);free(name);}
    probe=std::string(report)+".quote.argv";SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());
    std::string error;bool quoting=WFE_LaunchApplication(program,{"quoted \"word\"\\","%PATH%","semi;&",""},&error)&&apps_wait(probe);
    probe=std::string(report)+".failure.argv";SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());
    WFE_DownloadSnapshot failed=apps_wait_download(WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"truncated").c_str(),program));
    bool truncated=failed.state==WFE_DOWNLOAD_FAILED && GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES && failed.destination[0] && GetFileAttributesA(failed.destination)==INVALID_FILE_ATTRIBUTES;
    uint32 cancel_job=WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"slow").c_str(),program);
    WFE_DownloadCancel(cancel_job);failed=apps_wait_download(cancel_job);
    bool cancelled=failed.state==WFE_DOWNLOAD_CANCELLED && GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES;
    failed=apps_wait_download(WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"source").c_str(),(std::string(program)+".missing").c_str()));
    bool launch_failed=failed.state==WFE_DOWNLOAD_FAILED && failed.destination[0] && GetFileAttributesA(failed.destination)==INVALID_FILE_ATTRIBUTES;
    smoke_error.clear();
    bool terminal_invalid=WFE_TerminalApplication(&frame->context,FE_TELNET_URL_TYPE,"-option","23",nullptr)<0 && !smoke_error.empty() && GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES;
    frame->Navigate("telnet://john%20bad@terminal.test:2323");browser_smoke_wait(frame);
    bool invalid_username=frame->load_status<0&&GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES;
    URL_Struct *post=NET_CreateURLStruct((std::string(base)+"post").c_str(),NET_DONT_RELOAD);
    if(post) {post->method=URL_POST_METHOD;post->post_data=_strdup("original=message");post->post_data_size=16;FE_GetURL(&frame->context,post);browser_smoke_wait(frame);}
    smoke_error.clear();frame->SendMessage(WM_COMMAND,ID_MENU_DOCUMENT_SOURCE);
    bool post_guard=post && smoke_error.find("POST")!=std::string::npos && GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES &&
        !WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"post").c_str(),program);
    GeneralPreferences preferences=WFE_GeneralPreferences();preferences.source_application.clear();
    bool fallback=WFE_SaveGeneralPreferences(preferences,&error);
    frame->Navigate((std::string(base)+"source").c_str());browser_smoke_wait(frame);
    frame->SendMessage(WM_COMMAND,ID_MENU_DOCUMENT_SOURCE);browser_smoke_wait(frame);
    std::string source_name="%ViewSourceWindow"+std::string(base)+"source";
    BrowserFrame *source_view=frame_for(XP_FindNamedContextInList(&frame->context,const_cast<char *>(source_name.c_str())));
    std::string fallback_text;
    if(source_view) {browser_smoke_wait(source_view);fallback_text=frame_text(source_view);source_view->SendMessage(WM_CLOSE);}
    interaction_record(report,"fallback.text",fallback_text);
    std::string compact;for(unsigned char c:fallback_text) if(!isspace(c)) compact+=(char)c;
    fallback=fallback&&source_view&&GetFileAttributesA(probe.c_str())==INVALID_FILE_ATTRIBUTES && compact.find("<html>")!=std::string::npos;
    preferences.source_application=program;WFE_SaveGeneralPreferences(preferences,&error);
    const char *long_directory=getenv("NETSCAPE_APP_LONG_TEMP");
    preferences.temporary_directory=long_directory?long_directory:"";
    bool long_path=long_directory&&WFE_SaveGeneralPreferences(preferences,&error);
    probe=std::string(report)+".long.argv";copy=std::string(report)+".long.bytes";
    SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());SetEnvironmentVariableA("NETSCAPE_APP_PROBE_COPY",copy.c_str());
    if(long_path) {
        WFE_DownloadSnapshot boundary=apps_wait_download(WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"source").c_str(),program));
        long_path=boundary.state==WFE_DOWNLOAD_COMPLETE&&apps_wait(probe)&&apps_wait(copy)&&std::string(boundary.destination).find(long_directory)==0;
    }
    preferences.temporary_directory=temporary;WFE_SaveGeneralPreferences(preferences,&error);
    BrowserFrame *survivor=frame_for(WFE_NewWindow(nullptr));
    probe=std::string(report)+".closed.argv";copy=std::string(report)+".closed.bytes";
    SetEnvironmentVariableA("NETSCAPE_APP_PROBE_REPORT",probe.c_str());SetEnvironmentVariableA("NETSCAPE_APP_PROBE_COPY",copy.c_str());
    uint32 closed_job=WFE_SaveSourceForApplication(&frame->context,(std::string(base)+"slowclose").c_str(),program);
    frame->SendMessage(WM_CLOSE);frame=survivor;
    preferences.temporary_directory.clear();bool switched=WFE_SaveGeneralPreferences(preferences,&error);
    WFE_DownloadSnapshot completed=apps_wait_download(closed_job);
    bool close_safe=switched&&completed.state==WFE_DOWNLOAD_COMPLETE&&apps_wait(copy)&&apps_wait(probe)&&std::string(completed.destination).find(temporary)==0&&GetFileAttributesA(completed.destination)!=INVALID_FILE_ATTRIBUTES;
    preferences.temporary_directory=temporary;bool restored=WFE_SaveGeneralPreferences(preferences,&error);
    bool passed=source&&telnet&&tn3270&&temp&&quoting&&truncated&&cancelled&&launch_failed&&terminal_invalid&&invalid_username&&post_guard&&fallback&&close_safe&&restored&&long_path;
    FILE *file=fopen(report,"ab");if(!file) return 2;
    fprintf(file,"source=%d\ntelnet=%d\ntn3270=%d\ntemp=%d\npassed=%d\n",source,telnet,tn3270,temp,passed);fclose(file);
    file=fopen(report,"ab");if(file) {fprintf(file,"quoting=%d\ntruncated=%d\ncancelled_transfer=%d\nlaunch_failed=%d\nterminal_invalid=%d\npost_guard=%d\nfallback=%d\nclose_safe=%d\nrestored=%d\n",quoting,truncated,cancelled,launch_failed,terminal_invalid,post_guard,fallback,close_safe,restored);fclose(file);}
    file=fopen(report,"ab");if(file) {fprintf(file,"long_path=%d\ninvalid_username=%d\n",long_path,invalid_username);fclose(file);}
    frame->SendMessage(WM_CLOSE);return passed?0:1;
}
