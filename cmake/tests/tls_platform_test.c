#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "net_tls_socket.h"
extern int WFE_TLSInit(void);
extern void WFE_TLSShutdown(void);
extern int WFE_TLSConfigure(const char *, int, char *, size_t);
extern const char *WFE_TLSError(void);
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while(0)
extern void WFE_TLSSetLegacy(int);
static int peer_test(char **argv)
{
    WSADATA wsa; SOCKET socket; struct sockaddr_in addr; u_long nonblock=1;
    NETTLSInfo info; int i,r,written=0,readn=0;
    unsigned short port=(unsigned short)atoi(argv[1]);
    const char *host=!strcmp(argv[3],"fail_name")?"wrong.test":"fixture.test";
    char error[128],tx[32791],rx[32791],byte;
    CHECK(WSAStartup(MAKEWORD(2,2),&wsa)==0 && WFE_TLSInit());
    CHECK(WFE_TLSConfigure(argv[2],0,error,sizeof error));
    socket=WSASocketA(AF_INET,SOCK_STREAM,0,NULL,0,0);CHECK(socket!=INVALID_SOCKET);
    memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;
    addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);addr.sin_port=htons(port);
    CHECK(connect(socket,(struct sockaddr *)&addr,sizeof addr)==0);
    CHECK(ioctlsocket(socket,FIONBIO,&nonblock)==0 && NETTLS_Attach(socket,host,port));
    for(i=0;i<100000;i++) {
        CHECK(NETTLS_SocketWrite(socket,"GET",3)==-1);
        r=NETTLS_SocketPump(socket);if(r) break;
        CHECK(WSAGetLastError()==WSAEWOULDBLOCK);
        if(i%100==0) Sleep(1);
    }
    CHECK(i<100000 && NETTLS_SocketInfo(socket,&info));
    if(!strncmp(argv[3],"fail_",5)) {
        CHECK(r==-1 && !info.authenticated);
        CHECK(info.error==(!strcmp(argv[3],"fail_name")?NET_TLS_ERROR_HOSTNAME:NET_TLS_ERROR_CERT_UNTRUSTED));
        CHECK(NETTLS_SocketWrite(socket,"GET",3)==-1 && WSAGetLastError()==WSAECONNABORTED);
        CHECK(NETTLS_SocketRead(socket,&byte,1)==-1);
        NETTLS_SocketClose(socket);WFE_TLSShutdown();WSACleanup();return 0;
    }
    CHECK(r==1 && info.authenticated && info.version==0x0303);
    CHECK(NETTLS_CanReuse(socket,"FiXtUrE.TeSt",port));
    CHECK(!NETTLS_CanReuse(socket,"other.test",port) && !NETTLS_CanReuse(socket,host,port+1));
    if(atoi(argv[4])!=1) {
        WFE_TLSSetLegacy(1);CHECK(!NETTLS_CanReuse(socket,host,port));
        /* Replacing trust/policy retires reuse while preserving this active session. */
        CHECK(!WFE_TLSConfigure("Z:\\missing-ca-bundle.pem",0,error,sizeof error));
        CHECK(NETTLS_SocketInfo(socket,&info) && info.authenticated && info.version==0x0303);
    }
    for(i=0;i<sizeof tx;i++) tx[i]=(char)(i*13+5);
    for(i=0;i<200000 && readn<sizeof rx;i++) {
        if(written<sizeof tx) {
            r=NETTLS_SocketWrite(socket,tx+written,sizeof tx-written);
            CHECK(r>0 || (r==-1 && WSAGetLastError()==WSAEWOULDBLOCK));
            if(r>0) written+=r;
        }
        CHECK(NETTLS_SocketPump(socket)==1);
        r=NETTLS_SocketRead(socket,rx+readn,sizeof rx-readn);
        CHECK(r>0 || (r==-1 && WSAGetLastError()==WSAEWOULDBLOCK));
        if(r>0) readn+=r;
        if(i%100==0) Sleep(1);
    }
    CHECK(readn==sizeof rx && written==sizeof tx && !memcmp(tx,rx,sizeof tx));
    for(i=0;i<100000;i++) {
        r=NETTLS_SocketRead(socket,&byte,1);if(r==0) break;
        CHECK(r==-1 && WSAGetLastError()==WSAEWOULDBLOCK);
        if(i%100==0) Sleep(1);
    }
    CHECK(i<100000);
    CHECK(!NETTLS_CanReuse(socket,host,port)); /* clean EOF, including current generation */
    NETTLS_SocketClose(socket);
    CHECK(!NETTLS_SocketInfo(socket,&info));WFE_TLSShutdown();WSACleanup();return 0;
}
int main(int argc,char **argv)
{
    WSADATA wsa; SOCKET listener,client,peer; struct sockaddr_in addr; int n=sizeof addr;
    NETTLSInfo info; char buf[64],error[128];u_long nonblock=1;
    int missing=argc==2 && !strcmp(argv[1],"missing");
    if(argc==6) return peer_test(argv);
    CHECK(WSAStartup(MAKEWORD(2,2),&wsa)==0);
    CHECK(WFE_TLSInit()==!missing); CHECK(!!*WFE_TLSError()==missing);
    listener=socket(AF_INET,SOCK_STREAM,0); CHECK(listener!=INVALID_SOCKET);
    memset(&addr,0,sizeof addr);addr.sin_family=AF_INET;addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    CHECK(bind(listener,(struct sockaddr *)&addr,sizeof addr)==0 && listen(listener,1)==0);
    CHECK(getsockname(listener,(struct sockaddr *)&addr,&n)==0);
    client=socket(AF_INET,SOCK_STREAM,0); CHECK(connect(client,(struct sockaddr *)&addr,n)==0);
    peer=accept(listener,NULL,NULL);CHECK(peer!=INVALID_SOCKET);
    CHECK(NETTLS_SocketWrite(client,"http",4)==4 && recv(peer,buf,4,0)==4 && !memcmp(buf,"http",4));
    CHECK(send(peer,"plain",5,0)==5 && NETTLS_SocketRead(client,buf,5)==5 && !memcmp(buf,"plain",5));
    CHECK(ioctlsocket(client,FIONBIO,&nonblock)==0);
    CHECK(NETTLS_SocketRead(client,buf,1)==-1 && WSAGetLastError()==WSAEWOULDBLOCK);
    CHECK(NETTLS_Attach(client,"fixture.test",443)==!missing); /* BCrypt and checked real UTC */
    CHECK(NETTLS_SocketInfo(client,&info) && !info.authenticated);
    CHECK(info.error==(missing ? NET_TLS_ERROR_CA_BUNDLE : NET_TLS_ERROR_NONE));
    CHECK(NETTLS_SocketClose(client)==0);
    CHECK(!NETTLS_SocketInfo(client,&info));closesocket(peer);closesocket(listener);
    CHECK(!WFE_TLSConfigure("Z:\\missing-ca-bundle.pem",0,error,sizeof error) && *error);
    client=socket(AF_INET,SOCK_STREAM,0); CHECK(!NETTLS_Attach(client,"fixture.test",443));
    CHECK(NETTLS_SocketInfo(client,&info) && info.error==NET_TLS_ERROR_CA_BUNDLE);
    CHECK(NETTLS_SocketWrite(client,"GET",3)==-1 && WSAGetLastError()==WSAECONNABORTED);
    CHECK(WFE_TLSConfigure(NULL,0,error,sizeof error)==!missing);
    WFE_TLSShutdown();CHECK(!NETTLS_SocketInfo(client,&info));
    CHECK(closesocket(client)==SOCKET_ERROR && WSAGetLastError()==WSAENOTSOCK);
    WFE_TLSShutdown();WSACleanup();(void)argc;(void)argv;return 0;
}
