/* Focused CLI driver: all network, parser, renderer and download work is real. */
#include <bcrypt.h>
extern "C" {
#include "mkaccess.h"
}
static unsigned tls_smoke_ticks;
static int tls_smoke_security_on, tls_smoke_refresh;
static void tls_smoke_metadata(URL_Struct *url, int, MWContext *)
{
    tls_smoke_security_on=url->security_on;
    tls_smoke_refresh=url->refresh_url!=nullptr;
}
static void CALLBACK tls_smoke_timer(HWND, UINT, UINT_PTR, DWORD) { ++tls_smoke_ticks; }
static std::string tls_smoke_hash(const char *path)
{
    BCRYPT_ALG_HANDLE algorithm=nullptr;
    BCRYPT_HASH_HANDLE hash=nullptr;
    unsigned char digest[32], buffer[4096];
    std::string result;
    FILE *file=fopen(path,"rb");
    if (!file) return result;
    if (BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0 &&
        BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0) {
        size_t n;
        bool ok=true;
        while ((n=fread(buffer,1,sizeof buffer,file))!=0)
            if (BCryptHashData(hash,buffer,(ULONG)n,0)<0) { ok=false;break; }
        if (ok && !ferror(file) && BCryptFinishHash(hash,digest,sizeof digest,0)>=0) {
            char hex[3];
            for (unsigned i=0;i<sizeof digest;i++) { sprintf(hex,"%02x",digest[i]);result+=hex; }
        }
    }
    if (hash) BCryptDestroyHash(hash);
    if (algorithm) BCryptCloseAlgorithmProvider(algorithm,0);
    fclose(file);return result;
}
static int WFE_RunTLSSmoke(const char *report, const char *url, const char *bundle,
                           const char *proxy, const char *scenario)
{
    char error[256];
    WFE_TLSConfigure(bundle,0,error,sizeof error);
    if (proxy && *proxy && strcmp(proxy,"-")) NET_SetProxyServer(HTTPS_PROXY,(char *)proxy);
    tls_smoke_info=NETTLSInfo();tls_smoke_completed=tls_smoke_ticks=0;
    BrowserFrame *frame=frame_for(WFE_NewWindow(nullptr));
    if (!frame) return 2;
    tls_smoke_context=&frame->context;
    UINT_PTR timer=SetTimer(nullptr,0,10,tls_smoke_timer);
    tls_smoke_security_on=-1;tls_smoke_refresh=0;
    if (scenario && (!strcmp(scenario,"connect-isolation") ||
        !strcmp(scenario,"connect-isolation-fail") || !strcmp(scenario,"redirect-metadata"))) {
        if (strncmp(scenario,"connect-isolation",17)==0) NET_RemoveAllCookies();
        URL_Struct *request=NET_CreateURLStruct(url,NET_DONT_RELOAD);
        request->pre_exit_fn=tls_smoke_metadata;
        FE_GetURL(&frame->context,request);
    } else if (scenario && !strcmp(scenario,"headers")) {
        URL_Struct *request=NET_CreateURLStruct(url,NET_DONT_RELOAD);
        std::string headers="Host: localhost\r\nX-Large: "+std::string(40000,'a')+"\r\n";
        request->http_headers=XP_STRDUP(headers.c_str());
        FE_GetURL(&frame->context,request);
    } else if (scenario && (!strcmp(scenario,"post") || !strcmp(scenario,"post-file"))) {
        URL_Struct *request=NET_CreateURLStruct(url,NET_DONT_RELOAD);
        request->method=URL_POST_METHOD;
        const unsigned size=262177;
        char header[128];
        sprintf(header,"Content-Type: application/octet-stream\r\nContent-Length: %u\r\n%s",size,
                !strcmp(scenario,"post-file")?"\r\n":"");
        request->post_headers=XP_STRDUP(header);
        request->post_data_size=size;
        char *body=(char *)XP_ALLOC(size+1);
        for (unsigned i=0;i<size;i++) body[i]=(char)(i*19+7);
        body[size]=0;
        if (!strcmp(scenario,"post-file")) {
            std::string path=std::string(report)+".post";
            FILE *file=fopen(path.c_str(),"wb");
            if (file) { fwrite(body,1,size,file);fclose(file); }
            XP_FREE(body);request->post_data=XP_STRDUP(path.c_str());request->post_data_is_file=TRUE;
        } else request->post_data=body;
        FE_GetURL(&frame->context,request);
    } else frame->Navigate(url);
    bool scenario_ok=true;
    if (scenario && !strcmp(scenario,"cancel")) {
        ULONGLONG deadline=GetTickCount64()+100;
        while (GetTickCount64()<deadline) mail_smoke_pump();
        NET_InterruptWindow(&frame->context);
        bool canceled=tls_smoke_info.error==NET_TLS_ERROR_CANCELED;
        std::string next=url;next.resize(next.rfind('/'));next+="/index";
        frame->Navigate(next.c_str());browser_smoke_wait(frame);
        scenario_ok=canceled && frame->completed && frame->load_status>=0;
    } else if (scenario && !strncmp(scenario,"mixed:",6)) {
        BrowserFrame *other=frame_for(WFE_NewWindow(nullptr));
        BrowserFrame *plain=frame_for(WFE_NewWindow(nullptr));
        other->Navigate((std::string(url)+"?parallel").c_str());
        plain->Navigate((std::string("http://localhost:")+(scenario+6)+"/index").c_str());
        browser_smoke_wait(frame);browser_smoke_wait(other);browser_smoke_wait(plain);
        scenario_ok=frame->completed && other->completed && plain->completed &&
                    frame->load_status>=0 && other->load_status>=0 && plain->load_status>=0;
        other->SendMessage(WM_CLOSE);plain->SendMessage(WM_CLOSE);
    } else browser_smoke_wait(frame);
    if (scenario && !strcmp(scenario,"scheme")) {
        bool first=frame->completed && frame->load_status>=0;
        std::string plain=url;plain.replace(0,5,"http");
        frame->Navigate((plain+"?plain").c_str());browser_smoke_wait(frame);
        bool middle=frame->completed && frame->load_status>=0;
        frame->Navigate((std::string(url)+"?secure").c_str());browser_smoke_wait(frame);
        scenario_ok=first && middle && frame->completed && frame->load_status>=0;
    }
    if (scenario && (!strcmp(scenario,"reuse") || !strcmp(scenario,"policy"))) {
        bool first=frame->completed && frame->load_status>=0;
        if (!strcmp(scenario,"policy")) WFE_TLSSetLegacy(0);
        frame->Navigate((std::string(url)+"?second").c_str());browser_smoke_wait(frame);
        scenario_ok=first && frame->completed && frame->load_status>=0;
    }
    std::string text=frame_text(frame);
    CString title;frame->GetWindowText(title);
    unsigned images=(unsigned)frame->view.drawn_images.size();
    NETTLSInfo info=tls_smoke_info;
    int status=frame->load_status;
    bool completed=frame->completed;
    std::string hash;
    bool download_ok=true;
    if (scenario && !strcmp(scenario,"download") && status>=0) {
        std::string origin=url;
        size_t slash=origin.find('/',origin.find("://")+3);
        if (slash!=std::string::npos) origin.resize(slash);
        std::string destination=std::string(report)+".bin";
        DeleteFileA(destination.c_str());
        uint32 id=WFE_SaveLink(&frame->context,(origin+"/download").c_str(),destination.c_str());
        WFE_DownloadSnapshot snapshot={};
        ULONGLONG deadline=GetTickCount64()+15000;
        while (id && GetTickCount64()<deadline) {
            mail_smoke_pump();
            if (WFE_GetDownloadSnapshot(id,&snapshot) && snapshot.state>=WFE_DOWNLOAD_COMPLETE) break;
        }
        download_ok=id && snapshot.state==WFE_DOWNLOAD_COMPLETE;
        hash=tls_smoke_hash(destination.c_str());
    }
    bool responsive=tls_smoke_ticks>0 && IsWindow(frame->GetSafeHwnd());
    if (timer) KillTimer(nullptr,timer);
    unsigned requests=tls_smoke_completed;
    char *cookies=NET_GetCookie(&frame->context,(char *)url);
    bool proxy_cookie=cookies && strstr(cookies,"proxy_injected=evil");
    bool origin_cookie=cookies && strstr(cookies,"origin_cookie=good");
    frame->SendMessage(WM_CLOSE);
    tls_smoke_context=nullptr;
    FILE *file=fopen(report,"wb");
    if (!file) return 3;
    fprintf(file,"completed=%d\nstatus=%d\ntls_authenticated=%d\ntls_version=%u\ntls_cipher=%u\ntls_error=%d\nbody_sha256=%s\nrequests_completed=%u\nui_responsive=%d\ntitle=%s\nrendered=%d\ndownload_ok=%d\nscenario_ok=%d\nimages_rendered=%u\nsecurity_on=%d\nrefresh_present=%d\nproxy_cookie=%d\norigin_cookie=%d\n",
        completed,status,info.authenticated,info.version,info.cipher_suite,(int)info.error,
        hash.c_str(),requests,responsive,(const char *)title,!text.empty(),download_ok,scenario_ok,images,tls_smoke_security_on,tls_smoke_refresh,proxy_cookie,origin_cookie);
    fclose(file);
    return completed && responsive && download_ok ? 0 : 4;
}
