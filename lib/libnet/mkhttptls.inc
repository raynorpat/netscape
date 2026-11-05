/* HTTPS bodies must yield just like headers. The legacy shared producer uses
 * blocking loops for memory bodies and file headers, so keep these offsets in
 * the request and leave the other transports' historical behavior untouched. */
PRIVATE int net_http_tls_post(ActiveEntry *ce)
{
    HTTPConData *cd=(HTTPConData *)ce->con_data;
    const char *data;
    int32 size;
    int n;
    if (cd->tls_post_stage==0) {
        data=ce->URL_s->post_headers;
        size=data ? XP_STRLEN(data) : 0;
    } else if (cd->tls_post_stage==1) {
        data=CRLF;size=ce->URL_s->post_data_is_file ? 0 : 2;
    } else if (ce->URL_s->post_data_is_file) {
        if (!cd->tls_post_file) {
            cd->tls_post_file=XP_FileOpen(ce->URL_s->post_data,xpFileToPost,XP_FILE_READ_BIN);
            if (!cd->tls_post_file) return MK_UNABLE_TO_LOCATE_FILE;
        }
        if (cd->tls_post_offset==cd->tls_post_size) {
            cd->tls_post_size=XP_FileRead(cd->tls_post_buffer,sizeof cd->tls_post_buffer,cd->tls_post_file);
            /* fread may return zero or a short count on a local read error.
             * Reject that entire read; HTTP_FREE still owns the file. */
            if (ferror(cd->tls_post_file)) {
                ce->URL_s->error_msg=NET_ExplainErrorDetails(MK_UNABLE_TO_OPEN_FILE,
                                                           ce->URL_s->post_data);
                return MK_UNABLE_TO_OPEN_FILE;
            }
            cd->tls_post_offset=0;
            if (!cd->tls_post_size) {
                XP_FileClose(cd->tls_post_file);cd->tls_post_file=NULL;
                cd->tls_post_stage=0;return 0;
            }
        }
        data=cd->tls_post_buffer;size=cd->tls_post_size;
    } else {
        data=ce->URL_s->post_data;size=ce->URL_s->post_data_size;
        if (!data || size<0) return MK_TCP_WRITE_ERROR;
    }
    if (cd->tls_post_offset<size) {
        n=NETWRITE(cd->connection->sock,data+cd->tls_post_offset,size-cd->tls_post_offset);
        if (n<0) return SOCKET_ERRNO==XP_ERRNO_EWOULDBLOCK ? 1 : MK_TCP_WRITE_ERROR;
        cd->tls_post_offset+=n;
        return n>0 ? n : 1;
    }
    cd->tls_post_offset=0;
    if (cd->tls_post_stage<2) { ++cd->tls_post_stage;return 1; }
    cd->tls_post_stage=0;
    return 0;
}
