/* Real peer driver: platform I/O lives only in this test, never in the engine. */
#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "net_tls.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); exit(1); } } while (0)
#define PAYLOAD 32791
static SOCKET sock;
static int limit, calls, entropy_ok = 1, time_ok = 1, send_block;
static int http_bytes_sent_before_auth, io_calls, recv_calls;
static int64_t now = INT64_C(1793750400); /* 2026-11-04 UTC */
static int receive(void *arg, void *buf, size_t n)
{
    int r;
    (void)arg;
    io_calls++; recv_calls++;
    if (++calls % 3 == 0) return -2;
    if (n > (size_t)limit) n = limit;
    r = recv(sock, buf, (int)n, 0);
    return r == SOCKET_ERROR ? (WSAGetLastError() == WSAEWOULDBLOCK ? -2 : -1) : r;
}
static int transmit(void *arg, const void *buf, size_t n)
{
    int r;
    (void)arg;
    io_calls++;
    if (send_block || ++calls % 3 == 0) return -2;
    if (n > (size_t)limit) n = limit;
    r = send(sock, buf, (int)n, 0);
    return r == SOCKET_ERROR ? (WSAGetLastError() == WSAEWOULDBLOCK ? -2 : -1) : r;
}
static int entropy(void *arg, void *buf, size_t n)
{
    size_t i;
    (void)arg;
    /* Deterministic entropy is deliberately confined to the fixture driver. */
    for (i = 0; i < n; i++) ((unsigned char *)buf)[i] = (unsigned char)(i * 71 + 19);
    return entropy_ok;
}
static int utc(void *arg, int64_t *value)
{
    (void)arg;
    *value = now;
    return time_ok;
}
static int pump(NETTLS *tls)
{
    int before = io_calls, result = NETTLS_Pump(tls);
    CHECK(io_calls - before <= 32);
    return result;
}

int main(int argc, char **argv)
{
    WSADATA wsa;
    struct sockaddr_in addr;
    u_long nonblock = 1;
    NETTLSTrust *trust;
    NETTLS *tls = NULL;
    NETTLSIO io = { NULL, receive, transmit, entropy, utc };
    NETTLSInfo info;
    char error[128], tx[PAYLOAD], rx[PAYLOAD], byte;
    const char *name = "fixture.test", *mode;
    int i, r = 0, written = 0, readn = 0, legacy, expected_failure;
    int buffered_read_checked = 0;
    CHECK(argc == 6);
    mode = argv[3]; limit = atoi(argv[4]); legacy = atoi(argv[5]);
    expected_failure = !strncmp(mode, "fail_", 5);
    if (!strcmp(mode, "fail_name")) name = "wrong.test";
    if (!strcmp(mode, "fail_identity")) name = "127.0.0.1";
    if (!strcmp(mode, "fail_ipv4hex")) name = "0x7f000001";
    if (!strcmp(mode, "mixed_case")) name = "FiXtUrE.TeSt";
    if (!strcmp(mode, "fail_ipv6")) name = "[::1]";
    if (!strcmp(mode, "fail_nonascii")) name = "caf\xe9.test";
    if (!strcmp(mode, "fail_entropy")) entropy_ok = 0;
    if (!strcmp(mode, "fail_time")) time_ok = 0;
    if (!strcmp(mode, "fail_time_range")) now = INT64_MAX;
    if (!strcmp(mode, "fail_time_negative_range")) now = INT64_MIN;
    if (!strcmp(mode, "fail_expired")) now = INT64_C(2524608000); /* 2050 */
    if (!strcmp(mode, "fail_future")) now = 0;
    if (!strcmp(mode, "future")) now = INT64_C(2208988800); /* 2040 */
    CHECK(NETTLS_TrustLoad(argv[2], &trust, error, sizeof error));
    if (!strcmp(mode, "fail_identity") || !strcmp(mode, "fail_ipv6") ||
        !strcmp(mode, "fail_ipv4hex") || !strcmp(mode, "fail_nonascii") || !entropy_ok || !time_ok ||
        !strcmp(mode, "fail_time_range") || !strcmp(mode, "fail_time_negative_range")) {
        CHECK(!NETTLS_Create(name, trust, legacy, &io, &tls));
        CHECK(tls == NULL);
        NETTLS_GetInfo(tls, &info);
        assert(info.authenticated == 0); CHECK(!info.authenticated);
        NETTLS_TrustRelease(trust);
        return 0;
    }
    CHECK(WSAStartup(MAKEWORD(2,2), &wsa) == 0);
    sock = socket(AF_INET, SOCK_STREAM, 0); CHECK(sock != INVALID_SOCKET);
    memset(&addr, 0, sizeof addr); addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK); addr.sin_port = htons((unsigned short)atoi(argv[1]));
    CHECK(connect(sock, (struct sockaddr *)&addr, sizeof addr) == 0);
    CHECK(ioctlsocket(sock, FIONBIO, &nonblock) == 0);
    CHECK(NETTLS_Create(name, trust, legacy, &io, &tls));
    NETTLS_TrustRelease(trust); /* engine must own its reference */
    if (!strcmp(mode, "cancel")) {
        NETTLS_Close(tls); NETTLS_GetInfo(tls, &info);
        CHECK(info.error == NET_TLS_ERROR_CANCELED && !info.authenticated);
        NETTLS_Destroy(tls); closesocket(sock); WSACleanup(); return 0;
    }
    for (i = 0; i < 1000000; i++) {
        NETTLS_GetInfo(tls, &info);
        if (!info.authenticated) {
            r = NETTLS_Write(tls, "GET / HTTP/1.0\r\n\r\n", 18);
            if (r > 0) http_bytes_sent_before_auth += r;
            CHECK(r == -2 || r == -1);
        }
        r = pump(tls);
        if (r != 0) break;
        CHECK(NETTLS_Wants(tls) != 0);
        if (i % 100 == 0) Sleep(1);
    }
    CHECK(i < 1000000);
    NETTLS_GetInfo(tls, &info);
    assert(http_bytes_sent_before_auth == 0);
    CHECK(http_bytes_sent_before_auth == 0);
    if (expected_failure) {
        CHECK(r == -1); assert(info.authenticated == 0); CHECK(!info.authenticated);
        if (!strcmp(mode, "fail_name")) CHECK(info.error == NET_TLS_ERROR_HOSTNAME);
        if (!strcmp(mode, "fail_root")) CHECK(info.error == NET_TLS_ERROR_CERT_UNTRUSTED);
        if (!strcmp(mode, "fail_expired") || !strcmp(mode, "fail_future")) CHECK(info.error == NET_TLS_ERROR_CERT_DATE);
        i = io_calls;
        CHECK(pump(tls) == -1 && NETTLS_Read(tls, &byte, 1) == -1);
        CHECK(NETTLS_Write(tls, "x", 1) == -1 && io_calls == i);
        NETTLS_Destroy(tls); closesocket(sock); WSACleanup(); return 0;
    }
    CHECK(r == 1 && info.authenticated);
    if (!legacy) { unsigned default_negotiated_version = info.version; assert(default_negotiated_version == 0x0303); CHECK(default_negotiated_version == 0x0303); }
    else CHECK(info.version == (unsigned)atoi(argv[5]));
    CHECK(info.cipher_suite == 0xc02f || info.cipher_suite == 0xc030 || info.cipher_suite == 0xcca8 || info.cipher_suite == 0xc02b || info.cipher_suite == 0xc02c || info.cipher_suite == 0xcca9 || (legacy && (info.cipher_suite == 0xc013 || info.cipher_suite == 0xc009)));
    if (!strcmp(mode, "close_pending")) {
        int before = io_calls;
        send_block = 1;
        NETTLS_Close(tls);
        CHECK(io_calls - before <= 32);
        CHECK(NETTLS_Wants(tls) & NET_TLS_WANT_WRITE);
        CHECK(NETTLS_Write(tls, "x", 1) == -1);
        send_block = 0;
        for (i = 0; i < 10; i++) pump(tls);
        NETTLS_Destroy(tls); closesocket(sock); WSACleanup(); return 0;
    }
    if (!strcmp(mode, "renegotiate")) {
        const char request[] = "GET /reneg HTTP/1.0\r\n\r\n";
        int offset = 0;
        for (i = 0; i < 100000 && offset < sizeof request - 1; i++) {
            r = NETTLS_Write(tls, request + offset, sizeof request - 1 - offset);
            CHECK(r > 0 || r == -2);
            if (r > 0) offset += r;
            pump(tls);
            if (i % 100 == 0) Sleep(1);
        }
        CHECK(offset == sizeof request - 1);
        for (i = 0; i < 100000; i++) {
            r = NETTLS_Read(tls, rx, sizeof rx);
            if (r == -1 || r == 0) break;
            if (i % 100 == 0) Sleep(1);
        }
        CHECK(i < 100000);
        NETTLS_Destroy(tls); closesocket(sock); WSACleanup(); return 0;
    }
    for (i = 0; i < PAYLOAD; i++) tx[i] = (char)(i * 13 + 5);
    send_block = 1;
    r = NETTLS_Write(tls, tx, PAYLOAD); CHECK(r > 0); written = r;
    for (i = 0; i < 5; i++) CHECK(pump(tls) == 1);
    CHECK(NETTLS_Wants(tls) & NET_TLS_WANT_WRITE);
    send_block = 0;
    for (i = 0; i < 2000000 && readn < PAYLOAD; i++) {
        if (written < PAYLOAD) { r = NETTLS_Write(tls, tx + written, PAYLOAD - written); CHECK(r > 0 || r == -2); if (r > 0) written += r; }
        CHECK(pump(tls) == 1);
        r = NETTLS_Read(tls, rx + readn, buffered_read_checked ? PAYLOAD - readn : 1);
        CHECK(r > 0 || r == -2);
        if (r > 0) {
            readn += r;
            if (!buffered_read_checked && (NETTLS_Wants(tls) & NET_TLS_WANT_LOCAL)) {
                int before = recv_calls;
                /* CBC may split a single byte first; check a later buffered
                 * record too, without assuming the peer record boundaries. */
                r = NETTLS_Read(tls, rx + readn, PAYLOAD - readn);
                CHECK(r > 0 && recv_calls == before);
                readn += r;
                buffered_read_checked = 1;
            }
        }
        if (i % 100 == 0) Sleep(1);
    }
    CHECK(buffered_read_checked);
    CHECK(readn == PAYLOAD && written == PAYLOAD && !memcmp(tx, rx, PAYLOAD));
    for (i = 0; i < 1000000; i++) {
        r = NETTLS_Read(tls, &byte, 1);
        if (r != -2) break;
        pump(tls);
        if (i % 100 == 0) Sleep(1);
    }
    CHECK(i < 1000000);
    NETTLS_GetInfo(tls, &info);
    if (!strcmp(mode, "truncate")) CHECK(r == -1 && info.error == NET_TLS_ERROR_TRUNCATED && !info.authenticated);
    else if (!strcmp(mode, "alert") || !strcmp(mode, "malformed_alert")) CHECK(r == -1 && !info.authenticated);
    else CHECK(r == 0 && info.error == NET_TLS_ERROR_NONE);
    NETTLS_Close(tls); NETTLS_Destroy(tls); closesocket(sock); WSACleanup();
    puts("real TLS peer and payload assertions passed");
    return 0;
}
