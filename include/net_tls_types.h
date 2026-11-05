#ifndef NET_TLS_TYPES_H
#define NET_TLS_TYPES_H

/* Portable TLS status: do not expose a backend's private structures here. */
typedef enum {
    NET_TLS_ERROR_NONE = 0,
    NET_TLS_ERROR_ARGUMENT = 1,
    NET_TLS_ERROR_IDENTITY = 2,
    NET_TLS_ERROR_MEMORY = 3,
    NET_TLS_ERROR_IO = 4,
    NET_TLS_ERROR_ENTROPY = 5,
    NET_TLS_ERROR_TIME = 6,
    NET_TLS_ERROR_PROTOCOL = 7,
    NET_TLS_ERROR_HANDSHAKE = 8,
    NET_TLS_ERROR_HOSTNAME = 9,
    NET_TLS_ERROR_CERT_DATE = 10,
    NET_TLS_ERROR_CERT_UNTRUSTED = 11,
    NET_TLS_ERROR_CERT_MALFORMED = 12,
    NET_TLS_ERROR_CA_BUNDLE = 13,
    NET_TLS_ERROR_TRUNCATED = 14,
    NET_TLS_ERROR_CANCELED = 15,
    NET_TLS_ERROR_CERT_PURPOSE = 16
} NETTLSError;

typedef struct {
    unsigned short version, cipher_suite;
    int authenticated;
    NETTLSError error;
} NETTLSInfo;

#endif /* NET_TLS_TYPES_H */
