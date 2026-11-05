/* Execute the same private producer included by mkhttp.c. Only its transport
 * boundary is replaced; file reads and error indicators use the native UCRT.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <io.h>
#include <windows.h>

#define CHECK(test) do { if (!(test)) { \
    fprintf(stderr, "Failed at line %d: %s\n", __LINE__, #test); exit(1); \
} } while (0)

/* The producer consumes only these request fields. No browser/event-loop or
 * TLS-engine substitute is exercised by this focused file-input test. */
typedef int32_t int32;
typedef FILE *XP_File;
typedef struct { int sock; } HTTPConnection;
typedef struct {
    int tls_post_stage;
    int32 tls_post_offset, tls_post_size;
    XP_File tls_post_file;
    char tls_post_buffer[2048];
    HTTPConnection *connection;
} HTTPConData;
typedef struct {
    char *post_headers, *post_data;
    int post_data_is_file;
    int32 post_data_size;
    char *error_msg;
} URL_Struct;
typedef struct { void *con_data; URL_Struct *URL_s; } ActiveEntry;

#define PRIVATE static
#define CRLF "\r\n"
#define XP_STRLEN strlen
#define XP_FILE_READ_BIN "rb"
#define xpFileToPost 0
#define XP_FileRead(dest,count,file) fread((dest),1,(count),(file))
#define XP_FileOpen(path,type,mode) fopen((path),(mode))
#define MK_UNABLE_TO_LOCATE_FILE (-215)
#define MK_UNABLE_TO_OPEN_FILE (-223)
#define MK_TCP_WRITE_ERROR (-236)
#define SOCKET_ERRNO WSAGetLastError()
#define XP_ERRNO_EWOULDBLOCK WSAEWOULDBLOCK

static int closes, writes, closed_error, closed_eof;
static size_t sent;
static char output[32];

static int test_close(FILE *file)
{
    closes++;
    closed_error=!!ferror(file);
    closed_eof=!!feof(file);
    return fclose(file);
}
#define XP_FileClose test_close

static int test_write(int sock, const char *data, int32 size)
{
    CHECK(sock==17);
    CHECK(size>0 && sent+size<=sizeof output);
    memcpy(output+sent,data,size);
    sent+=size;
    writes++;
    return size;
}
#define NETWRITE test_write

/* The ordinary netlib formatter allocates the URL-owned error string. */
static char *NET_ExplainErrorDetails(int code, ...)
{
    va_list args;
    const char *path;
    CHECK(code==MK_UNABLE_TO_OPEN_FILE);
    va_start(args,code);
    path=va_arg(args,const char *);
    va_end(args);
    return _strdup(path);
}

#include "../../lib/libnet/mkhttptls.inc"

int main(int argc, char **argv)
{
    HTTPConnection connection={17};
    HTTPConData cd;
    URL_Struct url;
    ActiveEntry ce;
    FILE *file;
    char file_buffer[4096];
    int fail, short_read, with_data, fd=-1, backup=-1, n, turns;

    CHECK(argc==2);
    fail=!strcmp(argv[1],"zero_error") || !strcmp(argv[1],"short_error");
    short_read=!strcmp(argv[1],"short_error");
    with_data=short_read || !strcmp(argv[1],"data_eof");
    CHECK(fail || !strcmp(argv[1],"eof") || !strcmp(argv[1],"data_eof"));
    memset(&cd,0,sizeof cd);
    memset(&url,0,sizeof url);
    cd.connection=&connection;
    cd.tls_post_stage=2;
    url.post_data="upload-fixture.dat";
    url.post_data_is_file=1;
    ce.con_data=&cd;
    ce.URL_s=&url;
    file=tmpfile();
    CHECK(file!=NULL);
    CHECK(setvbuf(file,file_buffer,_IOFBF,sizeof file_buffer)==0);
    if (with_data) CHECK(fwrite("abc",1,3,file)==3);
    CHECK(fseek(file,0,SEEK_SET)==0);
    if (short_read) CHECK(fgetc(file)=='a'); /* Leaves "bc" buffered. */
    if (fail) {
        fd=_fileno(file);
        backup=_dup(fd);
        CHECK(backup>=0);
        /* Invalidate only the OS handle, retaining a valid CRT descriptor.
         * fread must set ferror (not invoke invalid-parameter handling). */
        CHECK(CloseHandle((HANDLE)_get_osfhandle(fd)));
    }
    cd.tls_post_file=file;
    n=net_http_tls_post(&ce);
    printf("%s: result=%d count=%ld error=%d eof=%d writes=%d closes=%d\n",
           argv[1],n,(long)cd.tls_post_size,
           cd.tls_post_file ? !!ferror(file) : closed_error,
           cd.tls_post_file ? !!feof(file) : closed_eof,writes,closes);
    if (fail) {
        CHECK(n==MK_UNABLE_TO_OPEN_FILE);
        CHECK(ferror(file) && !feof(file));
        CHECK(cd.tls_post_size==(short_read ? 2 : 0));
        CHECK(writes==0 && sent==0); /* Reject even the buffered short tail. */
        CHECK(cd.tls_post_file==file && cd.tls_post_stage==2);
        CHECK(closes==0); /* HTTP_FREE retains ownership after the error. */
        CHECK(url.error_msg && !strcmp(url.error_msg,"upload-fixture.dat"));
        CHECK(_dup2(backup,fd)==0);
        CHECK(_close(backup)==0);
        /* Like HTTP_FREE, attempt close even with the stream error set.
         * UCRT may return EOF here because the stream retains that error. */
        XP_FileClose(cd.tls_post_file);
        cd.tls_post_file=NULL;
        free(url.error_msg);
    } else {
        turns=0;
        while (n>0 && turns++<8) n=net_http_tls_post(&ce);
        CHECK(n==0 && turns<8);
        CHECK(cd.tls_post_file==NULL && cd.tls_post_stage==0);
        CHECK(url.error_msg==NULL);
        CHECK(sent==(with_data ? 3 : 0));
        CHECK(writes==(with_data ? 1 : 0));
        CHECK(!with_data || !memcmp(output,"abc",3));
    }
    CHECK(closes==1);
    printf("PASS %s\n",argv[1]);
    return 0;
}
