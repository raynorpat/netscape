/* Allocator interposition is confined to this test translation unit.
 * Production uses the ordinary CRT allocator and has no test hooks.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

static void *allocations[1024];
static size_t calls, live;
static size_t fail_at = (size_t)-1;

static void *track(void *p)
{
    size_t i;
    if (p == NULL) return NULL;
    for (i = 0; i < sizeof allocations / sizeof allocations[0]; i++) {
        if (allocations[i] == NULL) {
            allocations[i] = p;
            live++;
            return p;
        }
    }
    CHECK(0);
    return NULL;
}

static void *test_malloc(size_t n)
{
    if (calls++ == fail_at) return NULL;
    return track(malloc(n));
}

static void *test_calloc(size_t n, size_t size)
{
    if (calls++ == fail_at) return NULL;
    return track(calloc(n, size));
}

static void test_free(void *p)
{
    size_t i;
    if (p == NULL) return;
    for (i = 0; i < sizeof allocations / sizeof allocations[0]; i++) {
        if (allocations[i] == p) {
            allocations[i] = NULL;
            CHECK(live > 0);
            live--;
            free(p);
            return;
        }
    }
    CHECK(0); /* Double-free or an allocation owned by somebody else. */
}

#define malloc test_malloc
#define calloc test_calloc
#define free test_free
#include "../../security/tls/net_tls_trust.c"
#undef malloc
#undef calloc
#undef free

int main(int argc, char **argv)
{
    char path[1024], pem[4096], error[128];
    FILE *f;
    size_t len, total, i;
    NETTLSTrust *out;
    CHECK(argc == 2);
    CHECK(strlen(argv[1]) + 32 < sizeof path);
    sprintf(path, "%s/test-ca.pem", argv[1]);
    f = fopen(path, "rb");
    CHECK(f != NULL);
    len = fread(pem, 1, sizeof pem, f);
    CHECK(!ferror(f) && feof(f) && fclose(f) == 0);
    f = fopen("alloc-input.pem", "wb");
    CHECK(f != NULL);
    CHECK(fwrite(pem, 1, len, f) == len);
    CHECK(fwrite(pem, 1, len, f) == len);
    CHECK(fclose(f) == 0);
    CHECK(NETTLS_TrustLoad("alloc-input.pem", &out, error, sizeof error));
    total = calls;
    CHECK(live == 3); /* Store and two independently owned anchor blocks. */
    NETTLS_TrustRetain(out);
    NETTLS_TrustRelease(out);
    CHECK(live == 3);
    NETTLS_TrustRelease(out);
    CHECK(live == 0);
    CHECK(total >= 6);
    for (i = 0; i < total; i++) {
        calls = 0;
        fail_at = i;
        out = (NETTLSTrust *)1;
        CHECK(!NETTLS_TrustLoad("alloc-input.pem", &out, error, sizeof error));
        CHECK(out == NULL && error[0] != 0);
        CHECK(live == 0);
    }
    fail_at = (size_t)-1;
    f = fopen("alloc-input.pem", "ab");
    CHECK(f != NULL && fputs("-----BEGIN CERTIFICATE-----\nAAAA\n", f) >= 0);
    CHECK(fclose(f) == 0);
    CHECK(!NETTLS_TrustLoad("alloc-input.pem", &out, error, sizeof error));
    CHECK(out == NULL && live == 0);
    printf("Allocation failures at all %u sites and partial-parse cleanup passed\n", (unsigned)total);
    return 0;
}
