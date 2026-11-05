#ifndef NET_TLS_SOCKET_H
#define NET_TLS_SOCKET_H
#include "net_tls.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef uintptr_t NETTLSSocket;
typedef struct {
    /* Native callbacks return bytes/EOF/-2 pending/-1 fatal. */
    int (*recv)(NETTLSSocket, void *, size_t);
    int (*send)(NETTLSSocket, const void *, size_t);
    int (*close)(NETTLSSocket);
    void (*set_error)(int);
    int (*entropy)(void *, void *, size_t);
    int (*utc_time)(void *, int64_t *);
    int would_block_error, fatal_error;
} NETTLSSocketOps;
/* All operations run on the owning event loop. Init copies callbacks.
 * Configure retains the immutable trust store and publishes one new generation.
 * Active connections retain their original trust and policy. NULL disables HTTPS.
 * Failed attachment remains secure and queryable until SocketClose, except if
 * even the marker allocation fails: then all untracked I/O fails closed until
 * shutdown and there is no per-socket diagnostic entry.
 * SocketClose releases TLS state, then invokes native close exactly once.
 * Repeated closes of an already closed handle are unspecified, as for native close.
 * Every native socket close must pass here to prevent stale descriptor state.
 */
void NETTLS_SocketsInit(const NETTLSSocketOps *);
void NETTLS_SocketsShutdown(void);
void NETTLS_Configure(NETTLSTrust *, int allow_legacy);
int NETTLS_Attach(NETTLSSocket, const char *, unsigned short);
/* Feed already-consumed CONNECT bytes before the first pump. Bounded to 32 KiB.
 * Failure leaves the socket secure and failed; no plaintext fallback. */
int NETTLS_SocketPrefix(NETTLSSocket, const void *, size_t);
int NETTLS_SocketRead(NETTLSSocket, void *, int);
int NETTLS_SocketWrite(NETTLSSocket, const void *, int);
int NETTLS_SocketClose(NETTLSSocket);
/* Cumulative encrypted transport bytes (not payload), for progress deadlines. */
uint64_t NETTLS_SocketProgress(NETTLSSocket);
int NETTLS_SocketPump(NETTLSSocket);
unsigned NETTLS_SocketWants(NETTLSSocket);
int NETTLS_SocketInfo(NETTLSSocket, NETTLSInfo *);
int NETTLS_CanReuse(NETTLSSocket, const char *, unsigned short);
#ifdef __cplusplus
}
#endif
#endif
