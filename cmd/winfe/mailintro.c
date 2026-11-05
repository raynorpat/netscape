/* Checked, exclusive first-run mailbox creation for the Windows MSG adapter. */
#include "frontend.h"
#include "xp_file.h"
#include "mailintro.h"

static int write_all(HANDLE file,const char *data,DWORD length)
{
    while(length) {
        DWORD written=0;
        if(!WriteFile(file,data,length,&written,NULL)||!written) return 0;
        data+=written;length-=written;
    }
    return 1;
}

int WFE_CreateMailIntro(const char *name,const char *envelope,const char *message,int32 length)
{
    char *target,*slash,*backslash;
    char staging[MAX_PATH];
    unsigned int attempt,seed;
    size_t prefix;
    HANDLE file=INVALID_HANDLE_VALUE;
    int written,published=0,moved=0;
    DWORD attributes;
    if(!name||!envelope||!message||length<0) return 0;
    target=WH_FileName(name,xpMailFolder);
    if(!target) return 0;
    slash=strrchr(target,'/');backslash=strrchr(target,'\\');
    if(backslash&&(!slash||backslash>slash)) slash=backslash;
    prefix=slash?(size_t)(slash-target+1):0;
    if(strlen(target)>=MAX_PATH||prefix+5>sizeof(staging)) { XP_FREE(target);return 0; }
    memcpy(staging,target,prefix);
    /* Short sibling names keep valid near-MAX_PATH Inbox paths usable. The
     * leading dot also keeps unfinished files out of the legacy folder walk. */
    seed=(unsigned int)(GetCurrentProcessId()^GetTickCount());
    for(attempt=0;attempt<256;++attempt) {
        sprintf(staging+prefix,".w%02x",(seed+attempt)&255);
        file=CreateFileA(staging,GENERIC_WRITE,0,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
        if(file!=INVALID_HANDLE_VALUE) break;
        if(GetLastError()!=ERROR_FILE_EXISTS&&GetLastError()!=ERROR_ALREADY_EXISTS) break;
    }
    if(file==INVALID_HANDLE_VALUE) { XP_FREE(target);return 0; }
    written=write_all(file,envelope,(DWORD)strlen(envelope))&&write_all(file,message,(DWORD)length);
    if(written) written=FlushFileBuffers(file)!=0;
    if(!CloseHandle(file)) written=0;
    if(written) {
        moved=MoveFileExA(staging,target,MOVEFILE_WRITE_THROUGH)!=0;
        published=moved;
        if(!published) {
            /* Another first-run writer may have won. Never truncate its Inbox. */
            attributes=GetFileAttributesA(target);
            published=attributes!=INVALID_FILE_ATTRIBUTES&&!(attributes&FILE_ATTRIBUTE_DIRECTORY);
        }
    }
    if(!moved) DeleteFileA(staging);
    XP_FREE(target);
    return published;
}
