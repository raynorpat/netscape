#ifndef NET_TLS_H
#define NET_TLS_H

#include <stddef.h>
#include <stdint.h>
#include "net_tls_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NETTLSTrust NETTLSTrust;
typedef struct NETTLS NETTLS;

typedef struct {
    void *arg;
    int (*recv)(void *, void *, size_t);
    int (*send)(void *, const void *, size_t);
    int (*entropy)(void *, void *, size_t);
    int (*utc_time)(void *, int64_t *);
} NETTLSIO;

#define NET_TLS_WANT_READ  1
#define NET_TLS_WANT_WRITE 2
#define NET_TLS_WANT_LOCAL 4

/* The caller supplies nonblocking I/O (-2 would-block, -1 failure, 0 EOF),
 * secure entropy and trustworthy UTC seconds. Callbacks never retain buffers.
 * Callback arg remains caller-owned through Destroy.
 * Create retains trust. All calls belong to one owning event loop.
 * Read/Write require a nonzero length and return bytes, -2 pending, -1 fatal; only Read returns clean EOF.
 * Pump performs bounded work; LOCAL requests another turn without waiting for socket readiness.
 * Failed Create clears *out; CreateWithError also returns a typed reason.
 * Close is idempotent and attempts one bounded pump; Pump may drain later wants.
 * Destroy releases state and trust, never a native socket.
 */
int NETTLS_Create(const char *hostname, NETTLSTrust *trust, int allow_legacy,
                  const NETTLSIO *io, NETTLS **out);
int NETTLS_CreateWithError(const char *hostname, NETTLSTrust *trust, int allow_legacy,
                           const NETTLSIO *io, NETTLS **out, NETTLSError *error);
int NETTLS_Pump(NETTLS *tls);
int NETTLS_Read(NETTLS *tls, void *buf, size_t len);
int NETTLS_Write(NETTLS *tls, const void *buf, size_t len);
int NETTLS_Wants(const NETTLS *tls);
void NETTLS_GetInfo(const NETTLS *tls, NETTLSInfo *info);
void NETTLS_Close(NETTLS *tls);
void NETTLS_Destroy(NETTLS *tls);

/* A successful load owns one reference. Failure always clears *out.
 * The immutable store owns all its data; path and parser buffers may go away.
 * Diagnostics are optional and NUL-terminated when error_size is nonzero.
 * Reference operations are serialized by the adapter's owning event loop.
 */
int NETTLS_TrustLoad(const char *path, NETTLSTrust **out,
                    char *error, size_t error_size);
void NETTLS_TrustRetain(NETTLSTrust *trust);
void NETTLS_TrustRelease(NETTLSTrust *trust);

#ifdef __cplusplus
}
#endif

#endif /* NET_TLS_H */
