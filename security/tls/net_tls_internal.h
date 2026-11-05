#ifndef NET_TLS_INTERNAL_H
#define NET_TLS_INTERNAL_H

#include "net_tls.h"
#include "bearssl.h"

/* Adapter-private, borrowed read-only view. The caller must hold a trust
 * reference throughout engine use. Never free or change these anchors.
 */
const br_x509_trust_anchor *NETTLS_TrustAnchors(const NETTLSTrust *trust,
                                             size_t *count);

#endif /* NET_TLS_INTERNAL_H */
