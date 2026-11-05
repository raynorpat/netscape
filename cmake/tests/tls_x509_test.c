/* Focused parser and streaming-boundary tests. Authentication remains the
 * responsibility of the independent-peer engine tests, never this parser.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tls_x509.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
static NETTLSX509 policy;
static br_ssl_client_context client;
static br_x509_minimal_context minimal;
static unsigned char data[NET_TLS_CERT_LIMIT + 1];

static void reset(void)
{
    br_ssl_client_init_full(&client, &minimal, NULL, 0);
    NETTLS_X509Init(&policy, &minimal);
    policy.vtable->start_chain(&policy.vtable, "fixture.test");
    CHECK(policy.error == NET_TLS_ERROR_NONE && policy.result == NET_TLS_ERROR_NONE);
}

int main(int argc, char **argv)
{
    FILE *f;
    size_t len, i, chunk, offset, n;
    NETTLSError expected;
    CHECK(argc == 3);
    expected = (NETTLSError)atoi(argv[2]);
    f = fopen(argv[1], "rb"); CHECK(f != NULL);
    len = fread(data, 1, sizeof data, f); CHECK(!ferror(f));
    CHECK(fgetc(f) == EOF); fclose(f);
    CHECK(NETTLS_X509Purpose(data, len) == expected);
    if (expected == NET_TLS_ERROR_NONE) {
        /* Every truncated prefix must fail, including inside long lengths. */
        for (i = 0; i < len; i++)
            CHECK(NETTLS_X509Purpose(data, i) == NET_TLS_ERROR_CERT_MALFORMED);
        if (len < sizeof data) {
            data[len] = 0;
            CHECK(NETTLS_X509Purpose(data, len + 1) == NET_TLS_ERROR_CERT_MALFORMED);
        }
    }
    for (chunk = 1; chunk <= 4096; chunk *= 8) {
        reset();
        policy.vtable->start_cert(&policy.vtable, (uint32_t)len);
        for (offset = 0; offset < len; offset += n) {
            n = len - offset; if (n > chunk) n = chunk;
            policy.vtable->append(&policy.vtable, data + offset, n);
        }
        policy.vtable->end_cert(&policy.vtable);
        CHECK(policy.error == expected);
        /* Without trust anchors the real delegate must still reject even a
         * policy-valid certificate; result must not replace its error.
         */
        CHECK(policy.vtable->end_chain(&policy.vtable) != 0);
        CHECK(policy.result == NET_TLS_ERROR_NONE);
    }
    reset();
    policy.vtable->start_cert(&policy.vtable, 65537);
    CHECK(policy.error == NET_TLS_ERROR_CERT_MALFORMED);
    policy.vtable->append(&policy.vtable, data, 1);
    CHECK(policy.used == 0);
    reset();
    policy.vtable->start_cert(&policy.vtable, UINT32_MAX);
    CHECK(policy.error == NET_TLS_ERROR_CERT_MALFORMED);
    reset();
    policy.vtable->start_cert(&policy.vtable, 0);
    CHECK(policy.error == NET_TLS_ERROR_CERT_MALFORMED);
    reset();
    policy.vtable->start_cert(&policy.vtable, 1);
    policy.vtable->append(&policy.vtable, data, 2);
    CHECK(policy.error == NET_TLS_ERROR_CERT_MALFORMED && policy.used == 0);
    reset();
    policy.vtable->start_cert(&policy.vtable, 2);
    policy.vtable->append(&policy.vtable, data, 1);
    policy.vtable->end_cert(&policy.vtable);
    CHECK(policy.error == NET_TLS_ERROR_CERT_MALFORMED);
    puts("purpose DER and streaming bounds passed");
    return 0;
}
