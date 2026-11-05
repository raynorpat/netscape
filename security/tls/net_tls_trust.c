/* Immutable, bounded PEM trust stores for the BearSSL transport adapter. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tls_internal.h"

#define TLS_BUNDLE_LIMIT (4 * 1024 * 1024)
#define TLS_CERT_LIMIT 65536
#define TLS_ANCHOR_LIMIT 512

struct NETTLSTrust {
    size_t references;
    size_t count;
    br_x509_trust_anchor anchors[TLS_ANCHOR_LIMIT];
};

struct TLSBuffer {
    unsigned char *data;
    size_t len;
    int overflow;
};

static void set_error(char *error, size_t size, const char *text)
{
    size_t n;
    if (error == NULL || size == 0) return;
    n = strlen(text);
    if (n >= size) n = size - 1;
    memcpy(error, text, n);
    error[n] = 0;
}

static void append_bytes(void *ctx, const void *data, size_t len)
{
    struct TLSBuffer *buffer = (struct TLSBuffer *)ctx;
    if (len > TLS_CERT_LIMIT - buffer->len) {
        buffer->overflow = 1;
        return;
    }
    memcpy(buffer->data + buffer->len, data, len);
    buffer->len += len;
}

void NETTLS_TrustRetain(NETTLSTrust *trust)
{
    if (trust != NULL) trust->references++;
}

void NETTLS_TrustRelease(NETTLSTrust *trust)
{
    size_t i;
    if (trust == NULL || --trust->references != 0) return;
    for (i = 0; i < trust->count; i++) {
        /* DN and public-key bytes share this single owned allocation. */
        free(trust->anchors[i].dn.data);
    }
    free(trust);
}

const br_x509_trust_anchor *NETTLS_TrustAnchors(const NETTLSTrust *trust,
                                             size_t *count)
{
    if (count != NULL) *count = trust == NULL ? 0 : trust->count;
    return trust == NULL ? NULL : trust->anchors;
}

/* Require exactly one canonical, definite-length DER SEQUENCE per PEM
 * block in addition to the upstream certificate decoder checks.
 */
static int exact_der(const unsigned char *data, size_t len)
{
    size_t content, header, n, i;
    if (len < 2 || data[0] != 0x30) return 0;
    content = data[1];
    header = 2;
    if (content >= 0x80) {
        n = content & 0x7f;
        if (n == 0 || n > 3 || len < 2 + n || data[2] == 0) return 0;
        header += n;
        content = 0;
        for (i = 0; i < n; i++) content = (content << 8) | data[2 + i];
        if (content < 0x80) return 0;
    }
    return content == len - header;
}

static const char *add_anchor(NETTLSTrust *trust, struct TLSBuffer *der,
                              struct TLSBuffer *dn)
{
    br_x509_decoder_context decoder;
    br_x509_pkey *key;
    br_x509_trust_anchor *anchor;
    unsigned char *owned;
    size_t key_len;

    if (der->overflow) return "CA certificate exceeds 64 KiB";
    if (!exact_der(der->data, der->len)) return "Malformed CA certificate DER";
    dn->len = 0;
    dn->overflow = 0;
    br_x509_decoder_init(&decoder, append_bytes, dn);
    br_x509_decoder_push(&decoder, der->data, der->len);
    key = br_x509_decoder_get_pkey(&decoder);
    if (br_x509_decoder_last_error(&decoder) != 0 || key == NULL ||
        dn->overflow || dn->len == 0) return "Malformed or unsupported CA certificate";
    if (!br_x509_decoder_isCA(&decoder)) return "Certificate is not a CA anchor";
    if (key->key_type == BR_KEYTYPE_RSA) {
        if (key->key.rsa.nlen == 0 || key->key.rsa.elen == 0)
            return "Unsupported CA RSA key";
        key_len = key->key.rsa.nlen + key->key.rsa.elen;
    } else if (key->key_type == BR_KEYTYPE_EC) {
        int curve = key->key.ec.curve;
        size_t expected;
        if (curve == BR_EC_secp256r1) expected = 65;
        else if (curve == BR_EC_secp384r1) expected = 97;
        else if (curve == BR_EC_secp521r1) expected = 133;
        else return "Unsupported CA elliptic curve";
        if (key->key.ec.qlen != expected || key->key.ec.q[0] != 4)
            return "Unsupported CA EC point encoding";
        key_len = key->key.ec.qlen;
    } else {
        return "Unsupported CA public key";
    }
    if (trust->count == TLS_ANCHOR_LIMIT) return "CA bundle exceeds 512 certificates";
    owned = (unsigned char *)malloc(dn->len + key_len);
    if (owned == NULL) return "Out of memory loading CA anchor";
    anchor = &trust->anchors[trust->count];
    anchor->dn.data = owned;
    anchor->dn.len = dn->len;
    memcpy(owned, dn->data, dn->len);
    owned += dn->len;
    anchor->flags = BR_X509_TA_CA;
    anchor->pkey = *key;
    if (key->key_type == BR_KEYTYPE_RSA) {
        anchor->pkey.key.rsa.n = owned;
        memcpy(owned, key->key.rsa.n, key->key.rsa.nlen);
        owned += key->key.rsa.nlen;
        anchor->pkey.key.rsa.e = owned;
        memcpy(owned, key->key.rsa.e, key->key.rsa.elen);
    } else {
        anchor->pkey.key.ec.q = owned;
        memcpy(owned, key->key.ec.q, key->key.ec.qlen);
    }
    trust->count++;
    return NULL;
}

static const char *decode_certificate(NETTLSTrust *trust,
                                      const unsigned char *data, size_t len,
                                      struct TLSBuffer *der, struct TLSBuffer *dn)
{
    br_pem_decoder_context pem;
    size_t pos = 0;
    int state = 0, event;
    const unsigned char *input = data;
    size_t input_len = len;
    int final_newline = 0;

    der->len = 0;
    der->overflow = 0;
    br_pem_decoder_init(&pem);
    for (;;) {
        size_t consumed = br_pem_decoder_push(&pem, input + pos, input_len - pos);
        pos += consumed;
        event = br_pem_decoder_event(&pem);
        if (event == BR_PEM_BEGIN_OBJ && state == 0 &&
            strcmp(br_pem_decoder_name(&pem), "CERTIFICATE") == 0) {
            state = 1;
            br_pem_decoder_setdest(&pem, append_bytes, der);
        } else if (event == BR_PEM_END_OBJ && state == 1) {
            state = 2;
        } else if (event != 0) {
            return "Malformed CA certificate PEM";
        }
        if (der->overflow) return "CA certificate exceeds 64 KiB";
        if (pos == input_len) {
            if (!final_newline) {
                /* The decoder completes a footer only after a newline. */
                input = (const unsigned char *)"\n";
                input_len = 1;
                pos = 0;
                final_newline = 1;
            } else break;
        } else if (consumed == 0 && event == 0) {
            return "Incomplete CA certificate PEM";
        }
    }
    if (state != 2) return "Incomplete CA certificate PEM";
    return add_anchor(trust, der, dn);
}

static int line_is(const unsigned char *line, size_t len, const char *text)
{
    return len == strlen(text) && memcmp(line, text, len) == 0;
}

/* Outside PEM: blank/# comment lines, or a non-control certificate label
 * followed by an '=' separator, then a certificate (comments/blanks allowed).
 * curl's NetLock label is UTF-8; labels are informational, never identities.
 * Pending labels/separators at EOF and all misplaced PEM banners fail closed.
 */
static const char *parse_bundle(NETTLSTrust *trust,
                                const unsigned char *file, size_t len,
                                struct TLSBuffer *der, struct TLSBuffer *dn)
{
    size_t pos = 0, start = 0;
    int in_certificate = 0, label = 0;
    while (pos < len) {
        size_t line_start = pos, line_len, i;
        const unsigned char *line;
        int blank = 1, separator = 1;
        while (pos < len && file[pos] != '\n') pos++;
        line_len = pos - line_start;
        if (pos < len) pos++;
        if (line_len != 0 && file[line_start + line_len - 1] == '\r') line_len--;
        line = file + line_start;
        if (line_is(line, line_len, "-----BEGIN CERTIFICATE-----")) {
            if (in_certificate || label == 1) return "Malformed CA bundle framing";
            in_certificate = 1;
            label = 0;
            start = line_start;
            continue;
        }
        if (line_is(line, line_len, "-----END CERTIFICATE-----")) {
            const char *why;
            if (!in_certificate) return "Unexpected CA certificate footer";
            why = decode_certificate(trust, file + start, pos - start, der, dn);
            if (why != NULL) return why;
            in_certificate = 0;
            continue;
        }
        for (i = 0; i < line_len; i++) {
            unsigned char c = line[i];
            if ((c < 32 && c != '\t') || c == 127)
                return "Unexpected control byte in CA bundle";
            if (c != ' ' && c != '\t') blank = 0;
            if (c != '=') separator = 0;
            if (in_certificate && !((c >= 'A' && c <= 'Z') ||
                (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                c == '+' || c == '/' || c == '=' || c == ' ' || c == '\t'))
                return "Malformed CA certificate PEM body";
        }
        if (in_certificate) continue;
        if (blank || (line_len != 0 && line[0] == '#')) continue;
        /* All banners other than the exact CERTIFICATE pair are errors. */
        for (i = 0; i + 5 <= line_len; i++) {
            if (memcmp(line + i, "-----", 5) == 0)
                return "Unexpected PEM object or malformed CA banner";
        }
        if (separator) {
            if (label != 1 || line_len < 3) return "Unexpected CA label separator";
            label = 2;
        } else {
            if (label != 0) return "Unexpected text in CA bundle";
            label = 1;
        }
    }
    if (in_certificate || label != 0) return "Incomplete or trailing CA bundle data";
    if (trust->count == 0) return "CA bundle contains no certificates";
    return NULL;
}

int NETTLS_TrustLoad(const char *path, NETTLSTrust **out,
                    char *error, size_t error_size)
{
    FILE *file = NULL;
    unsigned char *input = NULL;
    NETTLSTrust *trust = NULL;
    struct TLSBuffer der = { NULL, 0, 0 }, dn = { NULL, 0, 0 };
    const char *why = NULL;
    size_t len;
    int ok = 0;

    if (out != NULL) *out = NULL;
    set_error(error, error_size, "");
    if (out == NULL || path == NULL || path[0] == 0) {
        set_error(error, error_size, "Invalid CA bundle argument");
        return 0;
    }
    file = fopen(path, "rb");
    if (file == NULL) {
        why = "Cannot open CA bundle";
        goto done;
    }
    input = (unsigned char *)malloc(TLS_BUNDLE_LIMIT + 1);
    trust = (NETTLSTrust *)calloc(1, sizeof *trust);
    der.data = (unsigned char *)malloc(TLS_CERT_LIMIT);
    dn.data = (unsigned char *)malloc(TLS_CERT_LIMIT);
    if (trust != NULL) trust->references = 1;
    if (input == NULL || trust == NULL || der.data == NULL || dn.data == NULL) {
        why = "Out of memory loading CA bundle";
        goto done;
    }
    len = fread(input, 1, TLS_BUNDLE_LIMIT + 1, file);
    if (ferror(file)) why = "Cannot read CA bundle";
    else if (len > TLS_BUNDLE_LIMIT) why = "CA bundle exceeds 4 MiB";
    if (fclose(file) != 0 && why == NULL) why = "Cannot close CA bundle";
    file = NULL;
    if (why != NULL) goto done;
    why = parse_bundle(trust, input, len, &der, &dn);
    if (why == NULL) {
        *out = trust;
        trust = NULL;
        ok = 1;
    }
done:
    if (file != NULL) fclose(file);
    free(input);
    free(der.data);
    free(dn.data);
    NETTLS_TrustRelease(trust);
    if (!ok) set_error(error, error_size, why);
    return ok;
}
