#ifndef NET_TLS_X509_H
#define NET_TLS_X509_H

#include "net_tls_internal.h"

#define NET_TLS_CERT_LIMIT 65536

/* Private policy context, embedded in NETTLS. delegate and certificate bytes
 * remain owned by that same connection until NETTLS_Destroy. No allocation,
 * recursion, I/O or retained pointers into caller-supplied certificate chunks.
 */
typedef struct {
    const br_x509_class *vtable;
    br_x509_minimal_context *delegate;
    unsigned char certificate[NET_TLS_CERT_LIMIT];
    size_t used, expected;
    NETTLSError error, result;
} NETTLSX509;

void NETTLS_X509Init(NETTLSX509 *ctx, br_x509_minimal_context *delegate);
/* Only checks DER framing and EKU policy; never authenticates a certificate. */
NETTLSError NETTLS_X509Purpose(const unsigned char *cert, size_t len);

#endif /* NET_TLS_X509_H */
