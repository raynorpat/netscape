/* Portable, bounded BearSSL client driver. The owning adapter supplies all I/O. */
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "net_tls_x509.h"

typedef struct {
    const br_ssl_client_certificate_class *vtable;
    NETTLS *owner;
} NETTLSAuth;

struct NETTLS {
    br_ssl_client_context client;
    br_x509_minimal_context x509;
    NETTLSX509 purpose;
    unsigned char buffer[BR_SSL_BUFSIZE_BIDI];
    NETTLSAuth auth;
    NETTLSIO io;
    NETTLSTrust *trust;
    NETTLSInfo info;
    int client_auth, closing, clean_eof, local;
};

static void wipe(void *data, size_t len)
{
    volatile unsigned char *p = (volatile unsigned char *)data;
    while (len--) *p++ = 0;
}

static int fail(NETTLS *tls, NETTLSError error)
{
    if (tls->info.error == NET_TLS_ERROR_NONE) tls->info.error = error;
    tls->info.authenticated = 0;
    tls->local = 0;
    return -1;
}

static NETTLSError translate(int err)
{
    if (err == BR_ERR_X509_BAD_SERVER_NAME) return NET_TLS_ERROR_HOSTNAME;
    if (err == BR_ERR_X509_EXPIRED) return NET_TLS_ERROR_CERT_DATE;
    if (err == BR_ERR_X509_NOT_TRUSTED) return NET_TLS_ERROR_CERT_UNTRUSTED;
    if (err >= BR_ERR_X509_INVALID_VALUE && err <= BR_ERR_X509_NOT_TRUSTED)
        return NET_TLS_ERROR_CERT_MALFORMED;
    if (err == BR_ERR_BAD_VERSION || err == BR_ERR_UNSUPPORTED_VERSION ||
        err == BR_ERR_BAD_CIPHER_SUITE) return NET_TLS_ERROR_PROTOCOL;
    return NET_TLS_ERROR_HANDSHAKE;
}

/* Reject historical numeric IPv4 forms too (one to four components,
 * including hexadecimal). Conservative rejection avoids platform parsing
 * differences; the DNS path never authenticates an address literal.
 */
static int numeric_address(const char *name)
{
    const unsigned char *p = (const unsigned char *)name;
    int parts = 0, digits, hex;
    for (;;) {
        if (++parts > 4) return 0;
        hex = p[0] == '0' && (p[1] == 'x' || p[1] == 'X');
        if (hex) p += 2;
        digits = 0;
        while ((*p >= '0' && *p <= '9') || (hex &&
               ((*p >= 'a' && *p <= 'f') || (*p >= 'A' && *p <= 'F')))) {
            digits = 1;
            p++;
        }
        if (!digits) return 0;
        if (!*p) return 1;
        if (*p++ != '.') return 0;
    }
}

static int identity(const char *name)
{
    size_t n = 0, label = 0;
    int alphabetic = 0;
    unsigned char c, prev = 0;
    if (!name) return 0;
    while ((c = (unsigned char)name[n]) != 0) {
        if (++n > 253) return 0;
        if (c == '.') {
            if (!label || prev == '-') return 0;
            label = 0;
        } else {
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || c == '-')) return 0;
            if ((!label && c == '-') || ++label > 63) return 0;
            if (c != '-' && !(c >= '0' && c <= '9')) alphabetic = 1;
        }
        prev = c;
    }
    /* Reject numeric address forms as well as bracketed/colon IPv6 literals. */
    return n && label && prev != '-' && alphabetic && !numeric_address(name);
}

static void auth_start(const br_ssl_client_certificate_class **ctx) { (void)ctx; }
static void auth_name(const br_ssl_client_certificate_class **ctx, size_t len)
{ (void)ctx; (void)len; }
static void auth_append(const br_ssl_client_certificate_class **ctx,
                        const unsigned char *data, size_t len)
{ (void)ctx; (void)data; (void)len; }
static void auth_choose(const br_ssl_client_certificate_class **ctx,
                        const br_ssl_client_context *client, uint32_t types,
                        br_ssl_client_certificate *choices)
{
    /* vtable is the first member of this stable, engine-owned context. */
    NETTLS *tls;
    (void)client; (void)types;
    tls = ((const NETTLSAuth *)ctx)->owner;
    tls->client_auth = 1;
    memset(choices, 0, sizeof *choices);
}
static const br_ssl_client_certificate_class reject_auth = {
    sizeof(NETTLSAuth), auth_start, auth_name, auth_append, auth_start,
    auth_start, auth_choose, NULL, NULL
};

int NETTLS_CreateWithError(const char *hostname, NETTLSTrust *trust, int allow_legacy,
                           const NETTLSIO *io, NETTLS **out, NETTLSError *error)
{
    /* These exact IANA suites exist in the pinned upstream bearssl_ssl.h. */
    static const uint16_t suites[] = {
        BR_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
        BR_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
        BR_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
        BR_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA
    };
    NETTLS *tls;
    const br_x509_trust_anchor *anchors;
    size_t count;
    unsigned char seed[48];
    int64_t seconds, days, rem;
    if (error) *error = NET_TLS_ERROR_ARGUMENT;
    if (!out) return 0;
    *out = NULL;
    if (!identity(hostname)) { if (error) *error = NET_TLS_ERROR_IDENTITY; return 0; }
    if (!trust || !io || !io->recv || !io->send ||
        !io->entropy || !io->utc_time) return 0;
    if (error) *error = NET_TLS_ERROR_TIME;
    if (!io->utc_time(io->arg, &seconds)) return 0;
    days = seconds / 86400;
    rem = seconds % 86400;
    if (rem < 0) { rem += 86400; days--; }
    /* Check before adding the epoch offset, and never use BearSSL's unset 0. */
    if (days <= -INT64_C(719528) || days > INT64_C(4294967295) - 719528)
        return 0;
    days += 719528;
    if (error) *error = NET_TLS_ERROR_ENTROPY;
    memset(seed, 0, sizeof seed);
    if (!io->entropy(io->arg, seed, sizeof seed)) {
        wipe(seed, sizeof seed);
        return 0;
    }
    if (error) *error = NET_TLS_ERROR_CA_BUNDLE;
    anchors = NETTLS_TrustAnchors(trust, &count);
    if (!anchors || !count) { wipe(seed, sizeof seed); return 0; }
    if (error) *error = NET_TLS_ERROR_MEMORY;
    tls = (NETTLS *)calloc(1, sizeof *tls);
    if (!tls) { wipe(seed, sizeof seed); return 0; }
    tls->io = *io;
    tls->trust = trust;
    NETTLS_TrustRetain(trust);
    br_ssl_client_init_full(&tls->client, &tls->x509, anchors, count);
    NETTLS_X509Init(&tls->purpose, &tls->x509);
    br_ssl_engine_set_x509(&tls->client.eng, &tls->purpose.vtable);
    br_ssl_engine_set_versions(&tls->client.eng, allow_legacy ? BR_TLS10 : BR_TLS12, BR_TLS12);
    br_ssl_engine_set_suites(&tls->client.eng, suites, allow_legacy ? 8 : 6);
    br_ssl_engine_set_all_flags(&tls->client.eng, BR_OPT_NO_RENEGOTIATION);
    br_x509_minimal_set_time(&tls->x509, (uint32_t)days, (uint32_t)rem);
    tls->auth.vtable = &reject_auth;
    tls->auth.owner = tls;
    br_ssl_client_set_client_certificate(&tls->client, &tls->auth.vtable);
    br_ssl_engine_set_buffer(&tls->client.eng, tls->buffer, sizeof tls->buffer, 1);
    br_ssl_engine_inject_entropy(&tls->client.eng, seed, sizeof seed);
    wipe(seed, sizeof seed);
    /* A fresh context and resume_session=0: no imported session or retry path. */
    if (error) *error = NET_TLS_ERROR_HANDSHAKE;
    if (!br_ssl_client_reset(&tls->client, hostname, 0)) {
        NETTLS_Destroy(tls);
        return 0;
    }
    *out = tls;
    if (error) *error = NET_TLS_ERROR_NONE;
    return 1;
}

int NETTLS_Create(const char *hostname, NETTLSTrust *trust, int allow_legacy,
                  const NETTLSIO *io, NETTLS **out)
{
    return NETTLS_CreateWithError(hostname, trust, allow_legacy, io, out, NULL);
}

static int state(NETTLS *tls, unsigned *flags)
{
    *flags = br_ssl_engine_current_state(&tls->client.eng);
    if (tls->info.error != NET_TLS_ERROR_NONE) return -1;
    if (tls->client_auth) return fail(tls, NET_TLS_ERROR_HANDSHAKE);
    if (*flags & BR_SSL_CLOSED) {
        int err = br_ssl_engine_last_error(&tls->client.eng);
        if (err) return fail(tls, tls->purpose.result != NET_TLS_ERROR_NONE ?
                             tls->purpose.result : translate(err));
        if (!tls->info.authenticated && !tls->closing)
            return fail(tls, NET_TLS_ERROR_HANDSHAKE);
        tls->clean_eof = 1;
    } else if (!tls->closing && (*flags & (BR_SSL_SENDAPP | BR_SSL_RECVAPP))) {
        br_ssl_session_parameters session;
        br_ssl_engine_get_session_parameters(&tls->client.eng, &session);
        tls->info.version = session.version;
        tls->info.cipher_suite = session.cipher_suite;
        tls->info.authenticated = 1;
    }
    return 0;
}

int NETTLS_Pump(NETTLS *tls)
{
    unsigned flags;
    int i, progress, blocked_read = 0, blocked_write = 0, r;
    size_t n;
    unsigned char *buf;
    if (!tls) return -1;
    tls->local = 0;
    /* A callback is nonblocking; at most 32 callbacks/acknowledgments per turn. */
    for (i = 0; i < 32; i++) {
        if (state(tls, &flags) < 0) return -1;
        if (tls->clean_eof) break;
        progress = 0;
        if ((flags & BR_SSL_SENDREC) && !blocked_write) {
            buf = br_ssl_engine_sendrec_buf(&tls->client.eng, &n);
            r = tls->io.send(tls->io.arg, buf, n);
            if (r == -2) blocked_write = 1;
            else if (r <= 0 || (size_t)r > n) return fail(tls, NET_TLS_ERROR_IO);
            else { br_ssl_engine_sendrec_ack(&tls->client.eng, r); progress = 1; }
        } else if ((flags & BR_SSL_RECVREC) && !blocked_read) {
            buf = br_ssl_engine_recvrec_buf(&tls->client.eng, &n);
            r = tls->io.recv(tls->io.arg, buf, n);
            if (r == -2) blocked_read = 1;
            else if (r == 0) return fail(tls, NET_TLS_ERROR_TRUNCATED);
            else if (r < 0 || (size_t)r > n) return fail(tls, NET_TLS_ERROR_IO);
            else { br_ssl_engine_recvrec_ack(&tls->client.eng, r); progress = 1; }
        }
        if (!progress && (blocked_write || !(flags & BR_SSL_SENDREC)) &&
            (blocked_read || !(flags & BR_SSL_RECVREC))) break;
    }
    if (state(tls, &flags) < 0) return -1;
    if (i == 32) tls->local = 1;
    return tls->info.authenticated ? 1 : 0;
}

int NETTLS_Read(NETTLS *tls, void *buf, size_t len)
{
    unsigned char *src;
    size_t n;
    if (!tls || !buf || !len) return -1;
    if (NETTLS_Pump(tls) < 0) return -1;
    if (tls->clean_eof) return 0;
    if (!tls->info.authenticated) return -2;
    src = br_ssl_engine_recvapp_buf(&tls->client.eng, &n);
    if (!n) return -2;
    if (n > len) n = len;
    if (n > INT_MAX) n = INT_MAX;
    memcpy(buf, src, n);
    br_ssl_engine_recvapp_ack(&tls->client.eng, n);
    return (int)n;
}

int NETTLS_Write(NETTLS *tls, const void *buf, size_t len)
{
    unsigned char *dest;
    unsigned flags;
    size_t n;
    if (!tls || !buf || !len) return -1;
    if (state(tls, &flags) < 0) return -1;
    if (tls->closing || tls->clean_eof) return -1;
    if (!tls->info.authenticated) return -2;
    dest = br_ssl_engine_sendapp_buf(&tls->client.eng, &n);
    if (!n) return -2;
    if (n > len) n = len;
    if (n > INT_MAX) n = INT_MAX;
    memcpy(dest, buf, n);
    br_ssl_engine_sendapp_ack(&tls->client.eng, n);
    br_ssl_engine_flush(&tls->client.eng, 0);
    return (int)n;
}

int NETTLS_Wants(const NETTLS *tls)
{
    unsigned flags;
    int wants = 0;
    if (!tls || tls->info.error || tls->clean_eof) return 0;
    flags = br_ssl_engine_current_state(&tls->client.eng);
    if (flags & BR_SSL_RECVREC) wants |= NET_TLS_WANT_READ;
    if (flags & BR_SSL_SENDREC) wants |= NET_TLS_WANT_WRITE;
    if (tls->local || (flags & BR_SSL_RECVAPP)) wants |= NET_TLS_WANT_LOCAL;
    return wants;
}

void NETTLS_GetInfo(const NETTLS *tls, NETTLSInfo *info)
{
    if (!info) return;
    if (tls) *info = tls->info;
    else { memset(info, 0, sizeof *info); info->error = NET_TLS_ERROR_ARGUMENT; }
}

void NETTLS_Close(NETTLS *tls)
{
    if (!tls || tls->closing) return;
    tls->closing = 1;
    if (!tls->info.authenticated) { fail(tls, NET_TLS_ERROR_CANCELED); return; }
    br_ssl_engine_close(&tls->client.eng);
    (void)NETTLS_Pump(tls);
}

void NETTLS_Destroy(NETTLS *tls)
{
    if (!tls) return;
    NETTLS_TrustRelease(tls->trust);
    wipe(tls, sizeof *tls);
    free(tls);
}
