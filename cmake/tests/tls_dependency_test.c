/* Ensure the selected dependency initializes a real TLS client in Release too. */
#include <stdio.h>
#include "bearssl.h"

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #test); return 1; \
} } while (0)

int main(void)
{
    br_ssl_client_context client;
    br_x509_minimal_context x509;

    CHECK(BR_TLS12 == 0x0303);
    br_ssl_client_init_full(&client, &x509, NULL, 0);
    CHECK(client.eng.version_min == 0x0301);
    CHECK(client.eng.version_max == 0x0303);
    CHECK(client.eng.x509ctx == &x509.vtable);
    puts("BearSSL client initialization and TLS version state passed");
    return 0;
}
