#ifndef NETSCAPE_NATIVE_DOWNLOADS_H
#define NETSCAPE_NATIVE_DOWNLOADS_H
#include "frontend.h"
#ifdef __cplusplus
extern "C" {
#endif
enum {
    WFE_DOWNLOAD_PENDING, WFE_DOWNLOAD_ACTIVE, WFE_DOWNLOAD_COMPLETE,
    WFE_DOWNLOAD_FAILED, WFE_DOWNLOAD_CANCELLED
};
typedef struct WFE_DownloadSnapshot {
    uint32 id;
    int state;
    unsigned __int64 bytes;
    char destination[MAX_PATH];
    char error[256];
} WFE_DownloadSnapshot;
void WFE_DownloadInit(void);
void WFE_DownloadShutdown(void);
/* Zero means declined/start failure. NULL destination opens a native picker;
   an existing directory uses the sanitized server-suggested basename. */
uint32 WFE_SavePage(MWContext *source, const char *destination);
uint32 WFE_SaveLink(MWContext *source, const char *address, const char *destination);
int WFE_DownloadCancel(uint32 id);
int WFE_GetDownloadSnapshot(uint32 id, WFE_DownloadSnapshot *snapshot);
void WFE_DownloadProgress(MWContext *, URL_Struct *, int32 received, int32 length);
NET_StreamClass *fe_MakeSaveAsStream(FO_Present_Types, void *, URL_Struct *, MWContext *);
int WFE_DownloadPromptForFileName(MWContext *, const char *, const char *,
    XP_Bool must_exist, XP_Bool directories, ReadFileNameCallbackFunction, void *);
/* Uses the same transfer APIs with explicit destinations and loopback fixtures. */
int WFE_DownloadSmoke(MWContext *, const char *report, const char *base_url, const char *output_directory);
#ifdef __cplusplus
}
#endif
#endif
