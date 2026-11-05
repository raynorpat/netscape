/* HTTPS purpose policy layered over the unmodified BearSSL validator. */
#include <string.h>
#include "net_tls_x509.h"

typedef struct {
    const unsigned char *data;
    size_t len;
} DER;

/* Definite, minimally encoded DER lengths only. All arithmetic is bounded by
 * remaining input before a pointer advances; values never exceed cert limit.
 */
static int take(DER *input, unsigned tag, DER *value)
{
    size_t len, count, i;
    if (input->len < 2 || input->data[0] != tag) return 0;
    len = input->data[1];
    input->data += 2; input->len -= 2;
    if (len & 128) {
        count = len & 127;
        if (!count || count > sizeof(size_t) || count > input->len ||
            input->data[0] == 0) return 0;
        len = 0;
        for (i = 0; i < count; i++) {
            if (len > NET_TLS_CERT_LIMIT / 256) return 0;
            len = len * 256 + input->data[i];
            if (len > NET_TLS_CERT_LIMIT) return 0;
        }
        if (len < 128) return 0;
        input->data += count; input->len -= count;
    }
    if (len > input->len) return 0;
    value->data = input->data; value->len = len;
    input->data += len; input->len -= len;
    return 1;
}

static int oid_valid(DER oid)
{
    size_t i;
    int first = 1;
    if (!oid.len) return 0;
    for (i = 0; i < oid.len; i++) {
        if (first && oid.data[i] == 128) return 0;
        first = !(oid.data[i] & 128);
    }
    return first;
}

static int equal(DER value, const unsigned char *bytes, size_t len)
{
    return value.len == len && !memcmp(value.data, bytes, len);
}

static NETTLSError purposes(DER value)
{
    static const unsigned char server[] = { 43, 6, 1, 5, 5, 7, 3, 1 };
    static const unsigned char any[] = { 85, 29, 37, 0 };
    DER list, oid;
    int allowed = 0;
    if (!take(&value, 0x30, &list) || value.len || !list.len)
        return NET_TLS_ERROR_CERT_MALFORMED;
    while (list.len) {
        if (!take(&list, 6, &oid) || !oid_valid(oid))
            return NET_TLS_ERROR_CERT_MALFORMED;
        if (equal(oid, server, sizeof server) || equal(oid, any, sizeof any))
            allowed = 1;
    }
    return allowed ? NET_TLS_ERROR_NONE : NET_TLS_ERROR_CERT_PURPOSE;
}

NETTLSError NETTLS_X509Purpose(const unsigned char *cert, size_t len)
{
    static const unsigned char eku[] = { 85, 29, 37 };
    DER input, outer, tbs, ignored, explicit_ext, extensions, ext, oid, value;
    NETTLSError result = NET_TLS_ERROR_NONE;
    int i, seen = 0;
    if (!cert || !len || len > NET_TLS_CERT_LIMIT)
        return NET_TLS_ERROR_CERT_MALFORMED;
    input.data = cert; input.len = len;
    if (!take(&input, 0x30, &outer) || input.len ||
        !take(&outer, 0x30, &tbs) || !take(&outer, 0x30, &ignored) ||
        !take(&outer, 3, &ignored) || outer.len)
        return NET_TLS_ERROR_CERT_MALFORMED;
    if (tbs.len && tbs.data[0] == 0xa0 && !take(&tbs, 0xa0, &ignored))
        return NET_TLS_ERROR_CERT_MALFORMED;
    /* serialNumber, signature, issuer, validity, subject, subjectPublicKeyInfo.
     * Their contents and all cryptographic semantics belong to the delegate.
     */
    for (i = 0; i < 6; i++)
        if (!take(&tbs, i ? 0x30 : 2, &ignored))
            return NET_TLS_ERROR_CERT_MALFORMED;
    if (tbs.len && tbs.data[0] == 0x81 && !take(&tbs, 0x81, &ignored))
        return NET_TLS_ERROR_CERT_MALFORMED;
    if (tbs.len && tbs.data[0] == 0x82 && !take(&tbs, 0x82, &ignored))
        return NET_TLS_ERROR_CERT_MALFORMED;
    if (!tbs.len) return NET_TLS_ERROR_NONE;
    if (!take(&tbs, 0xa3, &explicit_ext) || tbs.len ||
        !take(&explicit_ext, 0x30, &extensions) || explicit_ext.len || !extensions.len)
        return NET_TLS_ERROR_CERT_MALFORMED;
    while (extensions.len) {
        if (!take(&extensions, 0x30, &ext) || !take(&ext, 6, &oid) || !oid_valid(oid))
            return NET_TLS_ERROR_CERT_MALFORMED;
        if (ext.len && ext.data[0] == 1) {
            /* DER omits the DEFAULT FALSE value. Criticality remains enforced
             * by BearSSL, including its rejection of unsupported critical EKU.
             */
            if (!take(&ext, 1, &ignored) || ignored.len != 1 || ignored.data[0] != 255)
                return NET_TLS_ERROR_CERT_MALFORMED;
        }
        if (!take(&ext, 4, &value) || ext.len)
            return NET_TLS_ERROR_CERT_MALFORMED;
        if (equal(oid, eku, sizeof eku)) {
            if (seen++) return NET_TLS_ERROR_CERT_MALFORMED;
            result = purposes(value);
            if (result == NET_TLS_ERROR_CERT_MALFORMED) return result;
        }
    }
    return result;
}

static void start_chain(const br_x509_class **vctx, const char *name)
{
    NETTLSX509 *ctx = (NETTLSX509 *)vctx;
    ctx->error = ctx->result = NET_TLS_ERROR_NONE;
    ctx->used = ctx->expected = 0;
    ctx->delegate->vtable->start_chain(&ctx->delegate->vtable, name);
}

static void start_cert(const br_x509_class **vctx, uint32_t len)
{
    NETTLSX509 *ctx = (NETTLSX509 *)vctx;
    ctx->used = 0; ctx->expected = len;
    if (!len || len > sizeof ctx->certificate) ctx->error = NET_TLS_ERROR_CERT_MALFORMED;
    ctx->delegate->vtable->start_cert(&ctx->delegate->vtable, len);
}

static void append(const br_x509_class **vctx, const unsigned char *buf, size_t len)
{
    NETTLSX509 *ctx = (NETTLSX509 *)vctx;
    if (ctx->error == NET_TLS_ERROR_NONE) {
        if (len > ctx->expected - ctx->used) ctx->error = NET_TLS_ERROR_CERT_MALFORMED;
        else {
            memcpy(ctx->certificate + ctx->used, buf, len);
            ctx->used += len;
        }
    }
    ctx->delegate->vtable->append(&ctx->delegate->vtable, buf, len);
}

static void end_cert(const br_x509_class **vctx)
{
    NETTLSX509 *ctx = (NETTLSX509 *)vctx;
    if (ctx->error == NET_TLS_ERROR_NONE) {
        ctx->error = ctx->used != ctx->expected ? NET_TLS_ERROR_CERT_MALFORMED :
            NETTLS_X509Purpose(ctx->certificate, ctx->used);
    }
    ctx->delegate->vtable->end_cert(&ctx->delegate->vtable);
}

static unsigned end_chain(const br_x509_class **vctx)
{
    NETTLSX509 *ctx = (NETTLSX509 *)vctx;
    unsigned err = ctx->delegate->vtable->end_chain(&ctx->delegate->vtable);
    /* A policy success must never suppress an underlying validation failure. */
    if (err) return err;
    ctx->result = ctx->error;
    return ctx->result == NET_TLS_ERROR_NONE ? 0 : BR_ERR_X509_INVALID_VALUE;
}

static const br_x509_pkey *get_pkey(const br_x509_class *const *vctx, unsigned *usages)
{
    const NETTLSX509 *ctx = (const NETTLSX509 *)vctx;
    if (ctx->result != NET_TLS_ERROR_NONE) return NULL;
    return ctx->delegate->vtable->get_pkey(&ctx->delegate->vtable, usages);
}

static const br_x509_class policy = {
    sizeof(NETTLSX509), start_chain, start_cert, append, end_cert, end_chain, get_pkey
};

void NETTLS_X509Init(NETTLSX509 *ctx, br_x509_minimal_context *delegate)
{
    memset(ctx, 0, sizeof *ctx);
    ctx->vtable = &policy;
    ctx->delegate = delegate;
}
