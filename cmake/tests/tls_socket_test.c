/* Lifecycle and allocation failures run against the real engine and registry. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tls_socket.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int allocations, fail_allocation = -1;
static void *test_calloc(size_t n, size_t size)
{ return allocations++ == fail_allocation ? NULL : calloc(n, size); }
#define calloc test_calloc
#include "../../security/tls/net_tls_engine.c"
#include "../../security/tls/net_tls_socket.c"
#undef calloc
static int received, sent, closed, last_error, entropy_ok = 1, time_ok = 1;
static int receive(NETTLSSocket s, void *b, size_t n)
{ (void)s; received++; if(n) ((char *)b)[0]='p'; return n ? 1 : 0; }
static int transmit(NETTLSSocket s, const void *b, size_t n)
{ (void)s;(void)b;sent++;return n ? -2 : 0; }
static int close_socket(NETTLSSocket s) { (void)s;closed++;return 0; }
static void set_error(int e) { last_error=e; }
static int entropy(void *arg, void *b, size_t n)
{ (void)arg;memset(b,42,n);return entropy_ok; }
static int utc(void *arg, int64_t *t)
{ (void)arg;*t=INT64_C(1793750400);return time_ok; }
static int io_receive(void *arg, void *b, size_t n)
{ (void)arg;return receive(0,b,n); }
static int io_transmit(void *arg, const void *b, size_t n)
{ (void)arg;return transmit(0,b,n); }
int main(int argc, char **argv)
{
    NETTLSSocketOps ops = {receive,transmit,close_socket,set_error,entropy,utc,11,22};
    NETTLSIO io = {NULL,NULL,NULL,entropy,utc};
    NETTLSTrust *trust; NETTLSInfo info; NETTLS *engine; NETTLSError error;
    char b, diagnostic[128]; int before;
    CHECK(argc==2); CHECK(NETTLS_TrustLoad(argv[1],&trust,diagnostic,sizeof diagnostic));
    NETTLS_SocketsInit(&ops); memset(&ops,0,sizeof ops); /* callbacks are copied */
    CHECK(NETTLS_SocketRead(7,&b,1)==1 && b=='p');
    CHECK(NETTLS_SocketWrite(7,"x",1)==-1 && last_error==11);
    CHECK(!NETTLS_SocketInfo(7,&info));
    NETTLS_Configure(trust,0);
    CHECK(NETTLS_Attach(7,"fixture.test",443));
    CHECK(!NETTLS_Attach(7,"other.test",443));
    CHECK(NETTLS_SocketInfo(7,&info) && !info.authenticated && !info.error);
    before=received; CHECK(NETTLS_SocketWrite(7,"GET",3)==-1 && last_error==11);
    CHECK(received==before); CHECK(NETTLS_SocketWants(7)&NET_TLS_WANT_WRITE);
    CHECK(!NETTLS_CanReuse(7,"fixture.test",443)); /* no handshake */
    CHECK(NETTLS_SocketProgress(7)==0);
    CHECK(NETTLS_SocketProgress(777)==0);
    /* A buffered CONNECT tail is owned by the registry, and bounded failures
     * retain a secure marker: no native application I/O can follow. */
    CHECK(NETTLS_SocketPrefix(7,"tail",4));
    CHECK(!NETTLS_SocketPrefix(7,"x",32769));
    CHECK(NETTLS_SocketInfo(7,&info) && info.error==NET_TLS_ERROR_ARGUMENT);
    before=received;CHECK(NETTLS_SocketRead(7,&b,1)==-1 && received==before);
    CHECK(NETTLS_SocketClose(7)==0 && closed==1);
    CHECK(!NETTLS_SocketInfo(7,&info));
    CHECK(NETTLS_Attach(7,"fixture.test",443)); /* numeric descriptor reused */
    CHECK(NETTLS_SocketInfo(7,&info) && !info.authenticated && !info.error);
    NETTLS_SocketClose(7); CHECK(closed==2);
    entropy_ok=0; CHECK(!NETTLS_Attach(9,"fixture.test",443));
    CHECK(NETTLS_SocketInfo(9,&info) && info.error==NET_TLS_ERROR_ENTROPY);
    before=sent; CHECK(NETTLS_SocketWrite(9,"GET",3)==-1 && last_error==22 && sent==before);
    CHECK(NETTLS_SocketRead(9,&b,1)==-1 && last_error==22);
    CHECK(!NETTLS_Attach(9,"fixture.test",443)); entropy_ok=1;
    NETTLS_SocketClose(9); CHECK(!NETTLS_SocketInfo(9,&info));
    CHECK(NETTLS_SocketRead(9,&b,1)==1); /* fresh plaintext number has no TLS state */
    time_ok=0; CHECK(!NETTLS_Attach(10,"fixture.test",443));
    CHECK(NETTLS_SocketInfo(10,&info) && info.error==NET_TLS_ERROR_TIME);time_ok=1;
    CHECK(!NETTLS_Attach(11,"127.0.0.1",443));
    CHECK(NETTLS_SocketInfo(11,&info) && info.error==NET_TLS_ERROR_IDENTITY);
    NETTLS_Configure(NULL,0); CHECK(!NETTLS_Attach(12,"fixture.test",443));
    CHECK(NETTLS_SocketInfo(12,&info) && info.error==NET_TLS_ERROR_CA_BUNDLE);
    CHECK(NETTLS_SocketRead(77,&b,1)==1); /* missing CA leaves HTTP usable */
    NETTLS_Configure(trust,1);
    io.recv=io_receive;
    io.send=io_transmit;
    engine=(NETTLS *)1;fail_allocation=allocations;
    CHECK(!NETTLS_CreateWithError("fixture.test",trust,0,&io,&engine,&error));
    CHECK(!engine && error==NET_TLS_ERROR_MEMORY);fail_allocation=-1;
    /* Registry allocation succeeds, actual engine allocation fails. */
    fail_allocation=allocations+1; CHECK(!NETTLS_Attach(13,"fixture.test",443));fail_allocation=-1;
    CHECK(NETTLS_SocketInfo(13,&info) && info.error==NET_TLS_ERROR_MEMORY);
    before=sent; CHECK(NETTLS_SocketWrite(13,"GET",3)==-1 && sent==before);
    fail_allocation=allocations;CHECK(!NETTLS_Attach(99,"fixture.test",443));fail_allocation=-1;
    before=received;CHECK(NETTLS_SocketRead(88,&b,1)==-1 && received==before);
    CHECK(NETTLS_SocketInfo(13,&info) && info.error==NET_TLS_ERROR_MEMORY);
    NETTLS_TrustRelease(trust);
    before=closed; NETTLS_SocketsShutdown(); CHECK(closed==before+4);
    NETTLS_SocketsShutdown(); CHECK(closed==before+4);
    NETTLS_SocketsInit(&ops); /* empty callbacks deliberately deny I/O */
    CHECK(NETTLS_SocketRead(8,&b,1)==-1);
    NETTLS_SocketsShutdown();
    return 0;
}
