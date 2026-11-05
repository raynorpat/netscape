/* Native services are kept out of the portable TLS engine and registry. */
#include <winsock2.h>
#include <windows.h>
#include <bcrypt.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "net_tls_socket.h"
static NETTLSTrust *wfe_tls_trust;
static int wfe_tls_initialized;
static char wfe_tls_error[256];
static int wfe_tls_receive(NETTLSSocket socket, void *buf, size_t n)
{
    int r=recv((SOCKET)socket,(char *)buf,n>INT_MAX?INT_MAX:(int)n,0);
    return r==SOCKET_ERROR ? (WSAGetLastError()==WSAEWOULDBLOCK ? -2 : -1) : r;
}
static int wfe_tls_transmit(NETTLSSocket socket, const void *buf, size_t n)
{
    int r=send((SOCKET)socket,(const char *)buf,n>INT_MAX?INT_MAX:(int)n,0);
    return r==SOCKET_ERROR ? (WSAGetLastError()==WSAEWOULDBLOCK ? -2 : -1) : r;
}
static int wfe_tls_close(NETTLSSocket socket) { return closesocket((SOCKET)socket); }
static void wfe_tls_set_error(int error) { WSASetLastError(error); }
static int wfe_tls_entropy(void *arg, void *buf, size_t n)
{
    (void)arg;
    if (n>ULONG_MAX) return 0;
    return BCryptGenRandom(NULL,(PUCHAR)buf,(ULONG)n,BCRYPT_USE_SYSTEM_PREFERRED_RNG)>=0;
}
static int wfe_tls_utc(void *arg, int64_t *seconds)
{
    SYSTEMTIME utc;
    FILETIME file;
    ULARGE_INTEGER ticks;
    const uint64_t epoch=UINT64_C(116444736000000000);
    (void)arg;
    GetSystemTime(&utc);
    if (!seconds || !SystemTimeToFileTime(&utc,&file)) return 0;
    ticks.LowPart=file.dwLowDateTime;ticks.HighPart=file.dwHighDateTime;
    if (ticks.QuadPart<epoch) return 0;
    *seconds=(int64_t)((ticks.QuadPart-epoch)/UINT64_C(10000000));
    return 1;
}
static int wfe_tls_default_path(char *path, size_t size)
{
    DWORD n=GetModuleFileNameA(NULL,path,(DWORD)size);
    char *end;
    static const char tail[]="\\..\\share\\netscape\\ca\\cacert.pem";
    if (!n || n>=size) return 0;
    end=strrchr(path,'\\');
    if (!end || (size_t)(end-path)+sizeof tail>size) return 0;
    strcpy(end,tail);
    return 1;
}
const char *WFE_TLSError(void) { return wfe_tls_error; }
/* An explicit override replaces the default store; failure disables new HTTPS.
 * The caller decides when to apply it. Existing connections retain their store.
 */
int WFE_TLSConfigure(const char *bundle, int allow_legacy, char *error, size_t size)
{
    NETTLSTrust *trust=NULL, *old=wfe_tls_trust;
    char path[MAX_PATH];
    int ok;
    wfe_tls_error[0]=0;
    if (!bundle || !*bundle) {
        if (!wfe_tls_default_path(path,sizeof path)) {
            strcpy(wfe_tls_error,"Cannot resolve the executable-relative CA bundle.");
            bundle=NULL;
        } else bundle=path;
    }
    ok=bundle && NETTLS_TrustLoad(bundle,&trust,wfe_tls_error,sizeof wfe_tls_error);
    wfe_tls_trust=trust;
    NETTLS_Configure(trust,allow_legacy);
    NETTLS_TrustRelease(old);
    if (error && size) {
        size_t n=strlen(wfe_tls_error);
        if (n>=size) n=size-1;
        memcpy(error,wfe_tls_error,n);error[n]=0;
    }
    return ok;
}
void WFE_TLSSetLegacy(int allow_legacy)
{ NETTLS_Configure(wfe_tls_trust,allow_legacy); }
int WFE_TLSInit(void)
{
    NETTLSSocketOps ops={wfe_tls_receive,wfe_tls_transmit,wfe_tls_close,
        wfe_tls_set_error,wfe_tls_entropy,wfe_tls_utc,WSAEWOULDBLOCK,WSAECONNABORTED};
    if (wfe_tls_initialized) return wfe_tls_trust!=NULL;
    NETTLS_SocketsInit(&ops);
    wfe_tls_initialized=1;
    return WFE_TLSConfigure(NULL,0,NULL,0);
}
void WFE_TLSShutdown(void)
{
    NETTLS_SocketsShutdown();
    NETTLS_TrustRelease(wfe_tls_trust);wfe_tls_trust=NULL;
    wfe_tls_initialized=0;wfe_tls_error[0]=0;
}
