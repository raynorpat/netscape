/* Test real bundle parsing, fail-closed boundaries, and owned anchor data. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tls_internal.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

static const char *fixtures;
static const char *bundle;
static NETTLSTrust *out;
static char error[128];

static char *read_file(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    long n;
    char *s;
    CHECK(f != NULL);
    CHECK(fseek(f, 0, SEEK_END) == 0);
    n = ftell(f);
    CHECK(n >= 0 && fseek(f, 0, SEEK_SET) == 0);
    s = (char *)malloc((size_t)n + 1);
    CHECK(s != NULL);
    CHECK(fread(s, 1, (size_t)n, f) == (size_t)n);
    CHECK(fclose(f) == 0);
    s[n] = 0;
    *len = (size_t)n;
    return s;
}

static char *fixture(const char *name, size_t *len)
{
    char path[1024];
    CHECK(strlen(fixtures) + strlen(name) + 2 < sizeof path);
    sprintf(path, "%s/%s", fixtures, name);
    return read_file(path, len);
}

static void save(const char *name, const void *data, size_t len)
{
    FILE *f = fopen(name, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(data, 1, len, f) == len);
    CHECK(fclose(f) == 0);
}

static int load(const char *name)
{
    int ok;
    out = (NETTLSTrust *)1;
    memset(error, 'x', sizeof error);
    ok = NETTLS_TrustLoad(name, &out, error, sizeof error);
    CHECK(memchr(error, 0, sizeof error) != NULL);
    if (!ok) {
        CHECK(out == NULL);
        CHECK(error[0] != 0);
    }
    return ok;
}

static void join(const char *name, const char *a, size_t alen,
                 const char *b, size_t blen)
{
    FILE *f = fopen(name, "wb");
    CHECK(f != NULL);
    CHECK(fwrite(a, 1, alen, f) == alen);
    CHECK(fwrite(b, 1, blen, f) == blen);
    CHECK(fclose(f) == 0);
}

static void valid_bundle(void)
{
    size_t n, count;
    char *pem = fixture("test-ca.pem", &n);
    const br_x509_trust_anchor *anchors;
    save("test-ca.pem", pem, n);
    CHECK(load("test-ca.pem"));
    anchors = NETTLS_TrustAnchors(out, &count);
    CHECK(count == 1 && anchors[0].pkey.key_type == BR_KEYTYPE_RSA);
    CHECK(anchors[0].flags == BR_X509_TA_CA);
    NETTLS_TrustRelease(out);
    {
        const char text[] = "# comment\r\n\r\nTest CA\r\n=======\r\n# Fingerprint\r\n";
        join("text.pem", text, sizeof text - 1, pem, n);
    }
    CHECK(load("text.pem"));
    NETTLS_TrustRelease(out);
    {
        /* curl's actual NetLock label contains UTF-8 text. */
        const char text[] = "NetLock Arany (Class Gold) F\xc5\x91tan\xc3\xbas\xc3\xadtv\xc3\xa1ny\n==================================\n";
        join("utf8-label.pem", text, sizeof text - 1, pem, n);
        CHECK(load("utf8-label.pem"));
        NETTLS_TrustRelease(out);
    }
    free(pem);
    pem = fixture("test-ec-ca.pem", &n);
    save("ec.pem", pem, n);
    CHECK(load("ec.pem"));
    anchors = NETTLS_TrustAnchors(out, &count);
    CHECK(count == 1 && anchors[0].pkey.key_type == BR_KEYTYPE_EC);
    NETTLS_TrustRelease(out);
    /* A final footer need not have a newline. */
    CHECK(n > 0 && pem[n - 1] == '\n');
    save("no-newline.pem", pem, n - 1);
    CHECK(load("no-newline.pem"));
    NETTLS_TrustRelease(out);
    free(pem);
}

static void unknown_pem_object(void)
{
    size_t n;
    char *pem = fixture("test-ca.pem", &n);
    const char tail[] = "-----BEGIN PRIVATE KEY-----\nAAAA\n-----END PRIVATE KEY-----\n";
    join("unknown.pem", pem, n, tail, sizeof tail - 1);
    CHECK(!load("unknown.pem"));
    CHECK(out == NULL);
    free(pem);
}

static void empty_bundle(void)
{
    const char text[] = "# No roots\n \t\r\n";
    char tiny[1] = { 'x' };
    save("empty.pem", "", 0);
    CHECK(!load("empty.pem"));
    save("comments.pem", text, sizeof text - 1);
    CHECK(!load("comments.pem"));
    CHECK(!load("missing.pem"));
    CHECK(!NETTLS_TrustLoad(NULL, &out, tiny, sizeof tiny));
    CHECK(out == NULL && tiny[0] == 0);
    CHECK(!NETTLS_TrustLoad("empty.pem", NULL, NULL, 0));
    CHECK(!NETTLS_TrustLoad("empty.pem", &out, NULL, 0));
    CHECK(out == NULL);
}

struct decoded { unsigned char data[8192]; size_t len; };
static void append_der(void *ctx, const void *data, size_t len)
{
    struct decoded *d = (struct decoded *)ctx;
    CHECK(len <= sizeof d->data - d->len);
    memcpy(d->data + d->len, data, len);
    d->len += len;
}

static void decode_pem(const char *pem, size_t len, struct decoded *d)
{
    br_pem_decoder_context pc;
    size_t pos = 0;
    int event;
    d->len = 0;
    br_pem_decoder_init(&pc);
    while (pos < len) {
        pos += br_pem_decoder_push(&pc, pem + pos, len - pos);
        event = br_pem_decoder_event(&pc);
        if (event == BR_PEM_BEGIN_OBJ) br_pem_decoder_setdest(&pc, append_der, d);
        CHECK(event != BR_PEM_ERROR);
    }
}

static void malformed_tail(void)
{
    const char *tails[] = {
        "-----BEGIN CERTIFICATE-----\nAAAA\n",
        "-----BEGIN CERTIFICATE----\nAAAA\n-----END CERTIFICATE-----\n",
        "-----BEGIN CERTIFICATE-----\n!@#$\n-----END CERTIFICATE-----\n",
        "-----BEGIN CERTIFICATE-----\nAAAA\n-----END PRIVATE KEY-----\n",
        "-----END CERTIFICATE-----\n",
        "trailing garbage\n", "Dangling label\n=============\n", "====\n",
        "-----BEGIN CERTIFICATE-----\nAAAA=\n-----END CERTIFICATE-----\n"
    };
    size_t n, i;
    char *pem = fixture("test-ca.pem", &n);
    for (i = 0; i < sizeof tails / sizeof tails[0]; i++) {
        join("valid-then-malformed.pem", pem, n, tails[i], strlen(tails[i]));
        CHECK(!load("valid-then-malformed.pem"));
        CHECK(out == NULL);
    }
    join("nul.pem", pem, n, "\0garbage", 8);
    CHECK(!load("nul.pem"));
    /* Real certificate DER with an extra byte or a truncated signature. */
    {
        struct decoded d = { {0}, 0 };
        char encoded[16384];
        size_t len;
        decode_pem(pem, n, &d);
        CHECK(d.len > 0 && d.len + 1 <= sizeof d.data);
        d.data[d.len] = 0;
        len = br_pem_encode(encoded, d.data, d.len + 1, "CERTIFICATE", 0);
        join("der-junk.pem", pem, n, encoded, len);
        CHECK(!load("der-junk.pem"));
        len = br_pem_encode(encoded, d.data, d.len - 1, "CERTIFICATE", 0);
        join("der-truncated.pem", pem, n, encoded, len);
        CHECK(!load("der-truncated.pem"));
    }
    free(pem);
}

static void unsupported_anchor(void)
{
    size_t n, u;
    char *pem = fixture("test-ca.pem", &n);
    char *unsupported = fixture("unsupported-ca.pem", &u);
    join("unsupported.pem", pem, n, unsupported, u);
    CHECK(!load("unsupported.pem"));
    CHECK(out == NULL);
    free(unsupported);
    free(pem);
}

static void oversized_bundle(void)
{
    size_t n, i;
    char *pem = fixture("test-ca.pem", &n);
    FILE *f = fopen("large.pem", "wb");
    CHECK(f != NULL);
    for (i = 0; i < 4 * 1024 * 1024 + 1; i++) CHECK(fputc(' ', f) != EOF);
    CHECK(fclose(f) == 0);
    CHECK(!load("large.pem"));
    CHECK(strstr(error, "4 MiB") != NULL);
    /* An otherwise valid file exactly at the file limit must be accepted. */
    f = fopen("file-limit.pem", "wb");
    CHECK(f != NULL && fwrite(pem, 1, n, f) == n && fputc('#', f) != EOF);
    for (i = n + 1; i < 4 * 1024 * 1024; i++) CHECK(fputc(' ', f) != EOF);
    CHECK(fclose(f) == 0);
    CHECK(load("file-limit.pem"));
    NETTLS_TrustRelease(out);
    f = fopen("file-limit.pem", "ab");
    CHECK(f != NULL && fputc(' ', f) != EOF && fclose(f) == 0);
    CHECK(!load("file-limit.pem"));
    CHECK(strstr(error, "4 MiB") != NULL);
    f = fopen("many.pem", "wb");
    CHECK(f != NULL);
    for (i = 0; i < 512; i++) CHECK(fwrite(pem, 1, n, f) == n);
    CHECK(fclose(f) == 0);
    CHECK(load("many.pem"));
    NETTLS_TrustRelease(out);
    f = fopen("many.pem", "ab");
    CHECK(f != NULL && fwrite(pem, 1, n, f) == n && fclose(f) == 0);
    CHECK(!load("many.pem"));
    {
        size_t len, der_len;
        char *der = fixture("padded-ca.der", &der_len);
        char *encoded;
        CHECK(der_len == 65536);
        len = br_pem_encode(NULL, der, der_len, "CERTIFICATE", 0);
        encoded = (char *)malloc(len + 128);
        CHECK(encoded != NULL);
        br_pem_encode(encoded, der, der_len, "CERTIFICATE", 0);
        save("der-limit.pem", encoded, len);
        CHECK(load("der-limit.pem"));
        NETTLS_TrustRelease(out);
        /* read_file allocated an extra byte; one more DER byte exceeds the cap. */
        len = br_pem_encode(encoded, der, der_len + 1, "CERTIFICATE", 0);
        save("large-der.pem", encoded, len);
        CHECK(!load("large-der.pem"));
        CHECK(strstr(error, "64 KiB") != NULL);
        free(encoded);
        free(der);
    }
    free(pem);
}

static void ownership(void)
{
    size_t n, e, count, i;
    char *pem = fixture("test-ca.pem", &n);
    char *ec = fixture("test-ec-ca.pem", &e);
    NETTLSTrust *trust;
    const br_x509_trust_anchor *a;
    unsigned char dn[256], modulus[256], exponent[3], point[65];
    size_t dnlen;
    join("owned.pem", pem, n, ec, e);
    CHECK(load("owned.pem"));
    trust = out;
    NETTLS_TrustRetain(trust);
    NETTLS_TrustRelease(trust);
    a = NETTLS_TrustAnchors(trust, &count);
    CHECK(count == 2 && a[0].dn.len <= sizeof dn);
    dnlen = a[0].dn.len;
    memcpy(dn, a[0].dn.data, dnlen);
    CHECK(a[0].pkey.key.rsa.nlen == sizeof modulus);
    CHECK(a[0].pkey.key.rsa.elen == sizeof exponent);
    CHECK(a[1].pkey.key.ec.qlen == sizeof point);
    memcpy(modulus, a[0].pkey.key.rsa.n, sizeof modulus);
    memcpy(exponent, a[0].pkey.key.rsa.e, sizeof exponent);
    memcpy(point, a[1].pkey.key.ec.q, sizeof point);
    CHECK(remove("owned.pem") == 0);
    /* Reuse the decoders and allocator after their original buffers are gone. */
    save("ec.pem", ec, e);
    for (i = 0; i < 32; i++) {
        CHECK(load("ec.pem"));
        NETTLS_TrustRelease(out);
    }
    CHECK(memcmp(a[0].dn.data, dn, dnlen) == 0);
    CHECK(memcmp(a[0].pkey.key.rsa.n, modulus, sizeof modulus) == 0);
    CHECK(memcmp(a[0].pkey.key.rsa.e, exponent, sizeof exponent) == 0);
    CHECK(memcmp(a[1].pkey.key.ec.q, point, sizeof point) == 0);
    {
        br_ssl_client_context client;
        br_x509_minimal_context x509;
        struct decoded d = { {0}, 0 };
        free(pem);
        pem = fixture("test-leaf.pem", &n);
        decode_pem(pem, n, &d);
        br_ssl_client_init_full(&client, &x509, a, count);
        br_x509_minimal_set_time(&x509, 740289, 0); /* 2026-11-04 */
        x509.vtable->start_chain(&x509.vtable, "test.example");
        x509.vtable->start_cert(&x509.vtable, (uint32_t)d.len);
        x509.vtable->append(&x509.vtable, d.data, d.len);
        x509.vtable->end_cert(&x509.vtable);
        CHECK(x509.vtable->end_chain(&x509.vtable) == 0);
    }
    NETTLS_TrustRelease(trust);
    NETTLS_TrustRetain(NULL);
    NETTLS_TrustRelease(NULL);
    free(ec);
    free(pem);
}

static void shipped_bundle(void)
{
    size_t count;
    CHECK(load(bundle));
    CHECK(NETTLS_TrustAnchors(out, &count) != NULL);
    CHECK(count == 118); /* Independent curl CA Extract release manifest. */
    NETTLS_TrustRelease(out);
}

int main(int argc, char **argv)
{
    CHECK(argc == 4);
    fixtures = argv[2]; bundle = argv[3];
#define RUN(name) if (strcmp(argv[1], #name) == 0) { name(); puts(#name " passed"); return 0; }
    RUN(valid_bundle)
    RUN(unknown_pem_object)
    RUN(empty_bundle)
    RUN(malformed_tail)
    RUN(unsupported_anchor)
    RUN(oversized_bundle)
    RUN(ownership)
    RUN(shipped_bundle)
    CHECK(0);
    return 1;
}
