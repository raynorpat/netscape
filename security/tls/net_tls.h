#ifndef NET_TLS_H
#define NET_TLS_H

#include <stddef.h>
#include "net_tls_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NETTLSTrust NETTLSTrust;

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
