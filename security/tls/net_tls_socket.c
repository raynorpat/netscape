/* Portable registry: only the frontend callbacks know about native sockets. */
#include <stdlib.h>
#include <string.h>
#include "net_tls_socket.h"
typedef struct NETTLSEntry {
    struct NETTLSEntry *next;
    NETTLSSocket socket;
    NETTLS *tls;
    NETTLSInfo failure;
    uint64_t generation;
    unsigned short port;
    char host[254];
} NETTLSEntry;
static NETTLSEntry *entries;
static NETTLSSocketOps socket_ops;
static NETTLSTrust *current_trust;
static uint64_t generation;
static int legacy_policy, memory_failed;
static NETTLSEntry *find_socket(NETTLSSocket socket)
{
    NETTLSEntry *e;
    for (e=entries; e && e->socket!=socket; e=e->next) ;
    return e;
}
static int native_error(int pending)
{
    if (socket_ops.set_error)
        socket_ops.set_error(pending ? socket_ops.would_block_error : socket_ops.fatal_error);
    return -1;
}
void NETTLS_Configure(NETTLSTrust *trust, int allow_legacy)
{
    NETTLSTrust *old=current_trust;
    NETTLS_TrustRetain(trust);
    current_trust=trust; legacy_policy=!!allow_legacy; generation++;
    NETTLS_TrustRelease(old);
}
int NETTLS_SocketClose(NETTLSSocket socket)
{
    NETTLSEntry **p=&entries, *e;
    while (*p && (*p)->socket!=socket) p=&(*p)->next;
    if ((e=*p)!=NULL) {
        *p=e->next;
        NETTLS_Close(e->tls); NETTLS_Destroy(e->tls); free(e);
    }
    return socket_ops.close ? socket_ops.close(socket) : native_error(0);
}
void NETTLS_SocketsShutdown(void)
{
    while (entries) NETTLS_SocketClose(entries->socket);
    NETTLS_Configure(NULL,0);
    memory_failed=0;
}
void NETTLS_SocketsInit(const NETTLSSocketOps *ops)
{
    NETTLS_SocketsShutdown();
    memset(&socket_ops,0,sizeof socket_ops);
    if (ops) socket_ops=*ops;
}
static int socket_receive(void *arg, void *buf, size_t n)
{
    NETTLSEntry *e=(NETTLSEntry *)arg;
    return socket_ops.recv(e->socket,buf,n);
}
static int socket_transmit(void *arg, const void *buf, size_t n)
{
    NETTLSEntry *e=(NETTLSEntry *)arg;
    return socket_ops.send(e->socket,buf,n);
}
int NETTLS_Attach(NETTLSSocket socket, const char *host, unsigned short port)
{
    NETTLSEntry *e;
    NETTLSIO io;
    if (find_socket(socket)) { native_error(0); return 0; }
    e=(NETTLSEntry *)calloc(1,sizeof *e);
    /* If even a failure marker cannot be allocated, all untracked I/O must
     * fail closed until shutdown. Never turn an OOM attachment into plaintext.
     */
    if (!e) { memory_failed=1; native_error(0); return 0; }
    e->socket=socket;e->port=port;e->generation=generation;
    e->next=entries;entries=e;
    if (host && strlen(host)<sizeof e->host) strcpy(e->host,host);
    if (!current_trust) e->failure.error=NET_TLS_ERROR_CA_BUNDLE;
    else if (!socket_ops.recv || !socket_ops.send || !socket_ops.close || !port)
        e->failure.error=NET_TLS_ERROR_ARGUMENT;
    else {
        io.arg=e;io.recv=socket_receive;io.send=socket_transmit;
        io.entropy=socket_ops.entropy;io.utc_time=socket_ops.utc_time;
        if (NETTLS_CreateWithError(host,current_trust,legacy_policy,&io,&e->tls,&e->failure.error))
            return 1;
    }
    native_error(0);return 0;
}
int NETTLS_SocketRead(NETTLSSocket socket, void *buf, int len)
{
    NETTLSEntry *e=find_socket(socket);
    int r;
    if (!buf || len<0) return native_error(0);
    if (e) {
        if (!e->tls) return native_error(0);
        if (!len) return 0;
        r=NETTLS_Read(e->tls,buf,(size_t)len);
    } else {
        if (memory_failed || !socket_ops.recv) return native_error(0);
        r=socket_ops.recv(socket,buf,(size_t)len);
    }
    return r<0 ? native_error(r==-2) : r;
}
int NETTLS_SocketWrite(NETTLSSocket socket, const void *buf, int len)
{
    NETTLSEntry *e=find_socket(socket);
    int r;
    if (!buf || len<0) return native_error(0);
    if (e) {
        if (!e->tls) return native_error(0);
        if (!len) return 0;
        r=NETTLS_Write(e->tls,buf,(size_t)len);
    } else {
        if (memory_failed || !socket_ops.send) return native_error(0);
        r=socket_ops.send(socket,buf,(size_t)len);
    }
    return r<0 ? native_error(r==-2) : r;
}
int NETTLS_SocketPump(NETTLSSocket socket)
{
    NETTLSEntry *e=find_socket(socket);
    int r=e && e->tls ? NETTLS_Pump(e->tls) : -1;
    if (r<0) native_error(0);
    return r;
}
unsigned NETTLS_SocketWants(NETTLSSocket socket)
{
    NETTLSEntry *e=find_socket(socket);
    return e && e->tls ? (unsigned)NETTLS_Wants(e->tls) : 0;
}
int NETTLS_SocketInfo(NETTLSSocket socket, NETTLSInfo *info)
{
    NETTLSEntry *e=find_socket(socket);
    if (!e) {
        if (info) memset(info,0,sizeof *info);
        return 0;
    }
    if (info) {
        if (e->tls) NETTLS_GetInfo(e->tls,info);
        else *info=e->failure;
    }
    return 1;
}
static unsigned char fold(unsigned char c)
{ return c>='A' && c<='Z' ? c+('a'-'A') : c; }
int NETTLS_CanReuse(NETTLSSocket socket, const char *host, unsigned short port)
{
    NETTLSEntry *e=find_socket(socket);
    NETTLSInfo info;
    size_t i;
    if (!e || !e->tls || !host || e->generation!=generation || e->port!=port) return 0;
    for (i=0;e->host[i] && host[i];i++)
        if (fold((unsigned char)e->host[i])!=fold((unsigned char)host[i])) return 0;
    if (e->host[i] || host[i]) return 0;
    NETTLS_GetInfo(e->tls,&info);
    return info.authenticated && !info.error && NETTLS_Wants(e->tls)!=0;
}
