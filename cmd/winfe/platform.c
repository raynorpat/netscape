/* Native Win32 file and network services for the 3.02 front end. */
#include "frontend.h"
#include "mailsettings.h"
#include "xp_file.h"
#include "msgcom.h"
#include "mkgeturl.h"
#include <direct.h>
#include <io.h>
#include <fcntl.h>
#include <errno.h>
#include <process.h>
#include <ctype.h>

static char wfe_profile[MAX_PATH];
static char wfe_cache[MAX_PATH];
static char wfe_selected_temp[MAX_PATH];
static char wfe_temp[MAX_PATH];
static char wfe_history[MAX_PATH];
static int wfe_initialized;
static int wfe_polling;

static void wfe_file_error(DWORD error)
{
    switch (error) {
    case ERROR_FILE_NOT_FOUND: case ERROR_PATH_NOT_FOUND: errno = ENOENT; break;
    case ERROR_ACCESS_DENIED: case ERROR_SHARING_VIOLATION: errno = EACCES; break;
    case ERROR_ALREADY_EXISTS: case ERROR_FILE_EXISTS: errno = EEXIST; break;
    case ERROR_NOT_SAME_DEVICE: errno = EXDEV; break;
    case ERROR_DISK_FULL: errno = ENOSPC; break;
    case ERROR_DIRECTORY: errno = ENOTDIR; break;
    case ERROR_FILENAME_EXCED_RANGE: errno = ENAMETOOLONG; break;
    default: errno = EIO; break;
    }
}

char *FE_UserNewsHost;
char *FE_UserNewsRC;
char *FE_TempDir;
char *FE_CacheDir;
char *FE_DownloadDir;
char *FE_GlobalHist;

static char *wfe_join(const char *directory, const char *name)
{
    size_t length = strlen(directory), tail = strlen(name);
    char *result = (char *)malloc(length + tail + 2);
    if (!result) return NULL;
    memcpy(result, directory, length);
    if (length && directory[length-1] != '\\' && directory[length-1] != '/')
        result[length++] = '\\';
    memcpy(result + length, name, tail + 1);
    return result;
}

static int wfe_create_directories(char *path)
{
    char *p = path;
    DWORD attributes;
    if (path[0] == '\\' && path[1] == '\\') {
        p = strchr(path + 2, '\\');
        if (!p) return 0;
        p = strchr(p + 1, '\\');
        if (!p) p = path + strlen(path);
        else ++p;
    }
    for (; *p; ++p) {
        if ((*p == '\\' || *p == '/') && p != path && p[-1] != ':') {
            char separator = *p;
            *p = 0;
            if (!CreateDirectoryA(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
                *p = separator;
                return 0;
            }
            *p = separator;
        }
    }
    if (!CreateDirectoryA(path, NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
        return 0;
    attributes = GetFileAttributesA(path);
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY);
}

int WFE_PlatformInit(void)
{
    WSADATA data;
    char base[MAX_PATH];
    char *path;
    DWORD length;
    if (wfe_initialized) return 1;
    length = GetEnvironmentVariableA("NETSCAPE_PROFILE_DIR", base, sizeof(base));
    if (!length) {
        length = GetEnvironmentVariableA("LOCALAPPDATA", base, sizeof(base));
        if (!length || length >= sizeof(base)) return 0;
        path = wfe_join(base, "NetscapeLegacy");
        if (!path) return 0;
        length = GetFullPathNameA(path, sizeof(wfe_profile), wfe_profile, NULL);
        free(path);
    } else {
        if (length >= sizeof(base)) return 0;
        length = GetFullPathNameA(base, sizeof(wfe_profile), wfe_profile, NULL);
    }
    if (!length || length >= sizeof(wfe_profile) || !wfe_create_directories(wfe_profile))
        return 0;
    if (strlen(wfe_profile) + 14 >= MAX_PATH) return 0;
    sprintf(wfe_cache, "%s\\cache", wfe_profile);
    sprintf(wfe_temp, "%s\\temp", wfe_profile);
    sprintf(wfe_history, "%s\\history.db", wfe_profile);
    if (!wfe_create_directories(wfe_cache) || !wfe_create_directories(wfe_temp)) return 0;
    if (WSAStartup(MAKEWORD(1, 1), &data)) return 0;
    FE_CacheDir = wfe_cache;
    FE_TempDir = wfe_temp;
    FE_GlobalHist = wfe_history;
    FE_UserNewsRC = wfe_profile;
    FE_DownloadDir = wfe_profile;
    wfe_initialized = 1;
#ifdef NETSCAPE_USE_BEARSSL
    /* CA failure disables HTTPS only. Preferences initialize after platform. */
    (void)WFE_TLSInit();
#endif
    return 1;
}

const char *WFE_ProfileDirectory(void) { return wfe_profile; }
const char *WFE_DefaultTemporaryDirectory(void) { return wfe_temp; }
int WFE_TestTemporaryDirectory(const char *directory)
{
    char probe[MAX_PATH];DWORD attributes=directory?GetFileAttributesA(directory):INVALID_FILE_ATTRIBUTES;
    if(attributes==INVALID_FILE_ATTRIBUTES || !(attributes&FILE_ATTRIBUTE_DIRECTORY) || strlen(directory)+14>=MAX_PATH) return 0;
    if(!GetTempFileNameA(directory,"chk",0,probe)) return 0;
    return DeleteFileA(probe)!=FALSE;
}
void WFE_ApplyTemporaryDirectory(const char *directory)
{
    DWORD attributes=directory&&*directory?GetFileAttributesA(directory):INVALID_FILE_ATTRIBUTES;
    if(attributes!=INVALID_FILE_ATTRIBUTES && (attributes&FILE_ATTRIBUTE_DIRECTORY) && strlen(directory)+14<MAX_PATH) {
        strcpy(wfe_selected_temp,directory);FE_TempDir=wfe_selected_temp;
    } else FE_TempDir=wfe_temp;
}

static char *wfe_full_path(const char *name)
{
    DWORD length = GetFullPathNameA(name, 0, NULL, NULL), written;
    char *result;
    if (!length) { errno = EINVAL; return NULL; }
    result = (char *)malloc(length);
    if (!result) return NULL;
    written = GetFullPathNameA(name, length, result, NULL);
    if (!written || written >= length) { free(result); errno = ENAMETOOLONG; return NULL; }
    return result;
}

/* NET_ParseFileURL has already unescaped xpURL path arguments. Only a full
 * file: URL needs decoding here; a literal '%' is legal in a local filename. */
static char *wfe_url_path(const char *name)
{
    const char *source = name;
    char *result, *p;
    int full_url = !_strnicmp(source, "file:", 5);
    if (full_url) {
        source += 5;
        if (!_strnicmp(source, "//localhost/", 12)) source += 11;
        else if (source[0] == '/' && source[1] == '/' && source[2] == '/') source += 2;
    }
    result = _strdup(source);
    if (!result) return NULL;
    if (full_url) NET_UnEscape(result);
    if (result[0] == '/' && isalpha((unsigned char)result[1]) &&
        (result[2] == ':' || result[2] == '|'))
        memmove(result, result + 1, strlen(result));
    if (isalpha((unsigned char)result[0]) && result[1] == '|') result[1] = ':';
    for (p = result; *p; ++p) if (*p == '/') *p = '\\';
    { char *full = wfe_full_path(result); free(result); return full; }
}

char *WH_FileName(const char *name, XP_FileType type)
{
    const char *fixed = NULL;
    const char *directory = wfe_profile;
    char decorated[MAX_PATH];
    char *result;
    if (!wfe_initialized && !WFE_PlatformInit()) { errno = EIO; return NULL; }
    if (!name) name = "";
    switch (type) {
    case xpURL: return wfe_url_path(name);
    case xpFileToPost: case xpExtCache: case xpPKCS12File:
        return wfe_full_path(name);
    case xpTemporary:
        directory = FE_TempDir;
        break;
    case xpCache:
        directory = FE_CacheDir;
        break;
    case xpCacheFAT: directory = FE_CacheDir; fixed = "index.db"; break;
    case xpUserPrefs: fixed = "prefs.js"; break;
    case xpKeyChain: fixed = "keychain.db"; break;
    case xpGlobalHistory: return _strdup(FE_GlobalHist);
    case xpHotlist: case xpBookmarks: if (!*name) fixed = "bookmark.htm"; break;
    case xpHTTPCookie: fixed = "cookies.txt"; break;
    case xpMimeTypes: if (!*name) fixed = "mime.types"; break;
    case xpProxyConfig: fixed = "proxyconf"; break;
    case xpSocksConfig: if (!*name) fixed = "socks.conf"; break;
    case xpSignature: if (!*name) fixed = "signature.txt"; break;
    case xpNewsrcFileMap: fixed = "newsrc.map"; break;
    case xpExtCacheIndex: fixed = "cachelist"; break;
    case xpMailSort: fixed = "mailsort"; break;
    case xpMailPopState: fixed = "popstate.dat"; break;
    case xpCertDB: case xpKeyDB:
        if (strlen(name) + 8 >= sizeof(decorated)) { errno = ENAMETOOLONG; return NULL; }
        sprintf(decorated, "%s%s.db", type == xpCertDB ? "cert" : "key", name);
        fixed = decorated;
        break;
    case xpCertDBNameIDX: fixed = "cert-nameidx.db"; break;
    case xpSecModuleDB: fixed = "secmodule.db"; break;
    case xpCryptoPolicy: fixed = "policyMoz40P2"; break;
    case xpEditColorScheme: if (!*name) fixed = "colors"; break;
    case xpNewsRC: case xpSNewsRC: case xpTemporaryNewsRC:
    case xpNewsgroups: case xpSNewsgroups:
        {
        int secure = type == xpSNewsRC || type == xpSNewsgroups;
        int groups = type == xpNewsgroups || type == xpSNewsgroups;
        char *mapped = NET_MapNewsrcHostToFilename((char *)name, secure, groups);
        if (mapped) {
            result = (mapped[0] == '\\' || mapped[0] == '/' ||
                      (mapped[0] && mapped[1] == ':')) ? _strdup(mapped) : wfe_join(directory, mapped);
            if (result && type == xpTemporaryNewsRC) {
                char *temporary = (char *)malloc(strlen(result) + 5);
                if (temporary) { strcpy(temporary, result); strcat(temporary, ".tmp"); }
                free(result);
                result = temporary;
            }
            return result;
        }
        if (strlen(name) + 20 >= sizeof(decorated)) { errno = ENAMETOOLONG; return NULL; }
        sprintf(decorated, "%s%s%s%s",
                (type == xpSNewsRC || type == xpSNewsgroups) ? "s" : "",
                (type == xpNewsgroups || type == xpSNewsgroups) ? "newsgroups" : "newsrc",
                *name ? "-" : "", name);
        { char *p; for (p = decorated; *p; ++p) {
            if (*p == ':') *p = '#';
            else if (*p == '/' || *p == '\\') *p = '_';
        } }
        if (*name && type != xpTemporaryNewsRC) {
            result = wfe_join(directory, decorated);
            if (!result) return NULL;
            if (!NET_RegisterNewsrcFile(result, (char *)name, secure, groups)) {
                free(result); errno = ENOMEM; return NULL;
            }
            NET_SaveNewsrcFileMappings();
            return result;
        }
        if (type == xpTemporaryNewsRC) strcat(decorated, ".tmp");
        fixed = decorated;
        break;
        }
    case xpMailFolderSummary:
        result = WH_FileName(name, xpMailFolder);
        if (result) {
            char *summary = (char *)malloc(strlen(result) + 5);
            if (summary) { strcpy(summary, result); strcat(summary, ".snm"); }
            free(result);
            return summary;
        }
        return NULL;
    case xpXoverCache:
        if (strlen(name) + 14 >= sizeof(decorated)) { errno = ENAMETOOLONG; return NULL; }
        sprintf(decorated, "xover-cache\\%s", name);
        fixed = decorated;
        break;
    case xpMailFolder:
        /* Libmsg keeps raw paths in /D|/dir form, not escaped URLs. */
        if (name[0] == '\\' || name[0] == '/' || (name[0] && name[1] == ':'))
            return wfe_url_path(name);
        directory = WFE_MailDirectory();
        if (!directory) { errno = EIO; return NULL; }
        break;
    default: errno = EINVAL; return NULL;
    }
    if (!fixed && (name[0] == '\\' || name[0] == '/' || (name[0] && name[1] == ':')))
        return wfe_full_path(name);
    return wfe_join(directory, fixed ? fixed : name);
}

char *WH_FilePlatformName(const char *name) { return name ? _strdup(name) : NULL; }
char *XP_TempDirName(void) { return FE_TempDir; }

/* GetTempFileName reserves the name atomically; callers own the empty file. */
char *WH_TempName(XP_FileType type, const char *prefix)
{
    char path[MAX_PATH];
    const char *directory;
    const char *basename;
    if (!wfe_initialized && !WFE_PlatformInit()) return NULL;
    directory = type == xpCache ? FE_CacheDir : FE_TempDir;
    if (!GetTempFileNameA(directory, prefix && *prefix ? prefix : "nsc", 0, path))
        return NULL;
    basename = strrchr(path, '\\');
    { char *result = _strdup(type == xpCache && basename ? basename + 1 : path);
      if (!result) DeleteFileA(path);
      return result;
    }
}

XP_File XP_FileOpen(const char *name, XP_FileType type, const XP_FilePerm permissions)
{
    char *path = WH_FileName(name, type);
    XP_File file;
    if (!path) return NULL;
    file = fopen(path, permissions);
    free(path);
    return file;
}

int XP_Stat(const char *name, XP_StatStruct *information, XP_FileType type)
{
    char *path = WH_FileName(name, type);
    int result;
    if (!path) return -1;
    result = _stat(path, information);
    free(path);
    return result;
}

typedef struct WFE_Directory {
    DIR directory;
    XP_DirEntryStruct entry;
    int first;
} WFE_Directory;

XP_Dir XP_OpenDir(const char *name, XP_FileType type)
{
    WFE_Directory *directory;
    char *path = WH_FileName(name, type), *pattern;
    if (!path) return NULL;
    pattern = wfe_join(path, "*");
    free(path);
    if (!pattern) return NULL;
    directory = (WFE_Directory *)calloc(1, sizeof(*directory));
    if (!directory) { free(pattern); return NULL; }
    directory->directory.directoryPtr = FindFirstFileA(pattern, &directory->directory.data);
    free(pattern);
    if (directory->directory.directoryPtr == INVALID_HANDLE_VALUE) {
        free(directory);
        wfe_file_error(GetLastError());
        return NULL;
    }
    directory->first = 1;
    return &directory->directory;
}

XP_DirEntryStruct *XP_ReadDir(XP_Dir handle)
{
    WFE_Directory *directory = (WFE_Directory *)handle;
    if (!directory) return NULL;
    if (!directory->first && !FindNextFileA(handle->directoryPtr, &handle->data)) return NULL;
    directory->first = 0;
    strncpy(directory->entry.d_name, handle->data.cFileName, sizeof(directory->entry.d_name)-1);
    directory->entry.d_name[sizeof(directory->entry.d_name)-1] = 0;
    return &directory->entry;
}

void XP_CloseDir(XP_Dir directory)
{
    if (directory) { FindClose(directory->directoryPtr); free(directory); }
}

int XP_FileRename(const char *from, XP_FileType fromtype, const char *to, XP_FileType totype)
{
    char *source = WH_FileName(from, fromtype), *target = WH_FileName(to, totype);
    int result = -1;
    if (source && target) {
        if (MoveFileExA(source, target, MOVEFILE_REPLACE_EXISTING)) result = 0;
        else wfe_file_error(GetLastError());
    }
    free(source); free(target);
    return result;
}

int XP_FileRemove(const char *name, XP_FileType type)
{
    char *path = WH_FileName(name, type);
    int result;
    if (!path) return -1;
    result = remove(path); free(path); return result;
}

int XP_MakeDirectory(const char *name, XP_FileType type)
{
    char *path = WH_FileName(name, type);
    int result;
    if (!path) return -1;
    result = _mkdir(path); free(path); return result;
}

int XP_RemoveDirectory(const char *name, XP_FileType type)
{
    char *path = WH_FileName(name, type);
    int result;
    if (!path) return -1;
    result = _rmdir(path); free(path); return result;
}

int XP_FileTruncate(const char *name, XP_FileType type, int32 length)
{
    char *path;
    int descriptor, result, saved;
    if (length < 0) { errno = EINVAL; return -1; }
    path = WH_FileName(name, type);
    if (!path) return -1;
    descriptor = _open(path, _O_RDWR | _O_BINARY);
    free(path);
    if (descriptor == -1) return -1;
    result = _chsize(descriptor, length);
    saved = errno;
    if (_close(descriptor) && !result) return -1;
    errno = saved;
    return result;
}

uint32 FE_DiskSpaceAvailable(MWContext *context)
{
    ULARGE_INTEGER available;
    (void)context;
    if (!GetDiskFreeSpaceExA(wfe_profile, &available, NULL, NULL)) return 0;
    return available.QuadPart > 0xffffffffUL ? 0xffffffffUL : (uint32)available.QuadPart;
}

char *FE_GetTempFileFor(MWContext *context, const char *name, XP_FileType type, XP_FileType *returntype)
{
    char *path = WH_FileName(name, type), *slash;
    char temporary[MAX_PATH];
    (void)context;
    if (!path || !returntype) { free(path); return NULL; }
    slash = strrchr(path, '\\');
    { char *other = strrchr(path, '/'); if (other && (!slash || other > slash)) slash = other; }
    if (!slash) { free(path); return NULL; }
    slash[1] = 0;
    if (!GetTempFileNameA(path, "nsm", 0, temporary)) { free(path); return NULL; }
    free(path);
    *returntype = xpFileToPost;
    { char *result = _strdup(temporary);
      if (!result) DeleteFileA(temporary);
      return result;
    }
}

#define WFE_READ 1
#define WFE_CONNECT 2
#define WFE_FILE 4
#define WFE_ALWAYS 8
typedef struct WFE_Watch {
    struct WFE_Watch *next;
    MWContext *context;
    int descriptor;
    unsigned int flags;
    unsigned long serial;
    DWORD last_poll;
} WFE_Watch;
static WFE_Watch *wfe_watches;
static unsigned long wfe_serial;
static unsigned int wfe_rotation;

static void wfe_watch(MWContext *context, int descriptor, unsigned int flag, int enabled)
{
    WFE_Watch **link = &wfe_watches, *watch;
    while (*link && !((*link)->context == context && (*link)->descriptor == descriptor &&
           (((*link)->flags & (WFE_FILE | WFE_ALWAYS)) == (flag & (WFE_FILE | WFE_ALWAYS)))))
        link = &(*link)->next;
    watch = *link;
    if (enabled) {
        if (!watch) {
            watch = (WFE_Watch *)calloc(1, sizeof(*watch));
            if (!watch) { WFE_Status(context, "Not enough memory to monitor network connection."); return; }
            watch->context = context;
            watch->descriptor = descriptor;
            watch->serial = ++wfe_serial;
            *link = watch;
        }
        watch->flags |= flag;
    } else if (watch) {
        watch->flags &= ~flag;
        if (!watch->flags) { *link = watch->next; free(watch); }
    }
}

void FE_SetReadSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_READ, 1); }
void FE_ClearReadSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_READ, 0); }
void FE_SetConnectSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_CONNECT, 1); }
void FE_ClearConnectSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_CONNECT, 0); }
void FE_SetFileReadSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_FILE, 1); }
void FE_ClearFileReadSelect(MWContext *c, int fd) { wfe_watch(c, fd, WFE_FILE, 0); }
void WFE_SetCallNetlibAllTheTime(MWContext *c) { wfe_watch(c, -1, WFE_ALWAYS, 1); }
void WFE_ClearCallNetlibAllTheTime(MWContext *c) { wfe_watch(c, -1, WFE_ALWAYS, 0); }

int FE_SetupAsyncSelect(unsigned int socket, MWContext *context)
{
    unsigned long nonblocking = 1;
    (void)context;
    return ioctlsocket((SOCKET)socket, FIONBIO, &nonblocking);
}

/* Workers never call netlib or touch a window. Refcounts allow cancellation
 * and shutdown to release a pending lookup without waiting on the resolver. */
typedef struct WFE_DNS {
    struct WFE_DNS *next;
    MWContext *context;
    int socket;
    char *name;
    struct hostent host;
    HANDLE thread;
    volatile LONG references;
    volatile LONG done;
    int error;
    int delivered;
} WFE_DNS;
static WFE_DNS *wfe_dns;

static void wfe_release_dns(WFE_DNS *job)
{
    if (!InterlockedDecrement(&job->references)) {
        char **p;
        if (job->host.h_aliases) {
            for (p = job->host.h_aliases; *p; ++p) free(*p);
            free(job->host.h_aliases);
        }
        if (job->host.h_addr_list) {
            for (p = job->host.h_addr_list; *p; ++p) free(*p);
            free(job->host.h_addr_list);
        }
        free(job->host.h_name);
        free(job->name);
        free(job);
    }
}

static unsigned __stdcall wfe_lookup(void *argument)
{
    WFE_DNS *job = (WFE_DNS *)argument;
    WSADATA data;
    struct hostent *host;
    int count, i;
    job->error = WSAStartup(MAKEWORD(1, 1), &data);
    if (!job->error) {
        host = gethostbyname(job->name);
        if (!host) job->error = WSAGetLastError();
        else {
            job->host.h_addrtype = host->h_addrtype;
            job->host.h_length = host->h_length;
            job->host.h_name = _strdup(host->h_name ? host->h_name : job->name);
            for (count = 0; host->h_addr_list[count]; ++count) ;
            job->host.h_addr_list = (char **)calloc(count + 1, sizeof(char *));
            if (!job->host.h_name || !job->host.h_addr_list) job->error = WSAENOBUFS;
            for (i = 0; !job->error && i < count; ++i) {
                job->host.h_addr_list[i] = (char *)malloc(host->h_length);
                if (!job->host.h_addr_list[i]) job->error = WSAENOBUFS;
                else memcpy(job->host.h_addr_list[i], host->h_addr_list[i], host->h_length);
            }
            job->host.h_aliases = (char **)calloc(1, sizeof(char *));
            if (!job->host.h_aliases) job->error = WSAENOBUFS;
        }
        WSACleanup();
    }
    InterlockedExchange(&job->done, 1);
    wfe_release_dns(job);
    return 0;
}

int FE_StartAsyncDNSLookup(MWContext *context, char *name, void **result, int socket)
{
    WFE_DNS *job;
    *result = NULL;
    for (job = wfe_dns; job; job = job->next)
        if (job->socket == socket && job->context == context && !_stricmp(job->name, name)) break;
    if (job) {
        if (!InterlockedCompareExchange(&job->done, 0, 0)) return MK_WAITING_FOR_LOOKUP;
        job->delivered = 1;
        if (job->error) { WSASetLastError(job->error); return -1; }
        *result = &job->host;
        return 0;
    }
    job = (WFE_DNS *)calloc(1, sizeof(*job));
    if (!job) { WSASetLastError(WSAENOBUFS); return -1; }
    job->name = _strdup(name);
    if (!job->name) { free(job); WSASetLastError(WSAENOBUFS); return -1; }
    job->socket = socket;
    job->context = context;
    job->references = 2;
    job->thread = (HANDLE)_beginthreadex(NULL, 0, wfe_lookup, job, 0, NULL);
    if (!job->thread) {
        job->references = 1;
        wfe_release_dns(job);
        WSASetLastError(WSAENOBUFS);
        return -1;
    }
    job->next = wfe_dns;
    wfe_dns = job;
    return MK_WAITING_FOR_LOOKUP;
}

void FE_SetDNSSelect(MWContext *context, int socket)
{
    /* DNS completion is registered by FE_StartAsyncDNSLookup itself. */
    (void)context; (void)socket;
}

void FE_ClearDNSSelect(MWContext *context, int socket)
{
    WFE_DNS **link = &wfe_dns;
    while (*link) {
        WFE_DNS *job = *link;
        if (job->context == context && job->socket == socket) {
            *link = job->next;
            CloseHandle(job->thread);
            wfe_release_dns(job);
        } else link = &job->next;
    }
}

void WFE_TransferURL(URL_Struct *url, MWContext *old_context, MWContext *new_context)
{
    NETSOCK socket, connecting;
    Bool local_file, memory_file;
    WFE_Watch *watch;
    WFE_DNS *job;
    if (NET_GetURLDescriptors(url, &socket, &connecting, &local_file, &memory_file) < 0)
        return;
    for (watch = wfe_watches; watch; watch = watch->next) {
        if (watch->context != old_context || (watch->flags & WFE_ALWAYS)) continue;
        if ((socket != SOCKET_INVALID && watch->descriptor == (int)socket &&
             !!(watch->flags & WFE_FILE) == !!local_file) ||
            (connecting != SOCKET_INVALID && watch->descriptor == (int)connecting &&
             (watch->flags & WFE_CONNECT)))
            watch->context = new_context;
    }
    for (job = wfe_dns; job; job = job->next)
        if (job->context == old_context &&
            (job->socket == (int)socket || job->socket == (int)connecting))
            job->context = new_context;
    if (memory_file) {
        WFE_SetCallNetlibAllTheTime(new_context);
        if (!NET_AreThereActiveConnectionsForWindow(old_context))
            WFE_ClearCallNetlibAllTheTime(old_context);
    }
}

void WFE_PollNetworking(void)
{
    WFE_Watch *watch;
    WFE_DNS *job;
    struct { unsigned long serial; int fd; unsigned int flags; } pending[32];
    unsigned int total = 0, start, count = 0, index, budget = 8;
    int always = 0;
    if (!wfe_initialized || wfe_polling) return;
    wfe_polling = 1;
    /* Restart after every callback: netlib can cancel any other DNS job. */
    while (budget--) {
        for (job = wfe_dns; job; job = job->next)
            if (!job->delivered && InterlockedCompareExchange(&job->done, 0, 0)) break;
        if (!job) break;
        NET_ProcessNet((NETSOCK)job->socket, NET_SOCKET_FD);
    }
    for (watch = wfe_watches; watch; watch = watch->next) ++total;
    start = total ? wfe_rotation % total : 0;
    for (index = 0, watch = wfe_watches; watch; watch = watch->next, ++index) {
        if (watch->flags & WFE_ALWAYS) always = 1;
        if (index < start || count == 32) continue;
        pending[count].serial = watch->serial;
        pending[count].fd = watch->descriptor;
        pending[count++].flags = watch->flags;
    }
    wfe_rotation = total ? (start + count) % total : 0;
    for (index = 0; index < count; ++index) {
        fd_set reads, writes, errors;
        struct timeval timeout = { 0, 0 };
        SOCKET socket = (SOCKET)pending[index].fd;
        int ready;
        for (watch = wfe_watches; watch && watch->serial != pending[index].serial; watch = watch->next) ;
        if (!watch || (watch->flags & WFE_ALWAYS)) continue;
        if (watch->flags & WFE_FILE) {
            NET_ProcessNet((NETSOCK)watch->descriptor, NET_LOCAL_FILE_FD);
            continue;
        }
        for (job = wfe_dns; job; job = job->next)
            if (job->socket == watch->descriptor && job->context == watch->context && !job->delivered) break;
        if (job || socket == INVALID_SOCKET) continue;
        FD_ZERO(&reads); FD_ZERO(&writes); FD_ZERO(&errors);
        if (watch->flags & WFE_READ) FD_SET(socket, &reads);
        if (watch->flags & WFE_CONNECT) FD_SET(socket, &writes);
        FD_SET(socket, &errors);
        ready = select(0, &reads, &writes, &errors, &timeout);
        if (ready > 0 || ((watch->flags & WFE_CONNECT) && GetTickCount() - watch->last_poll >= 1000)) {
            watch->last_poll = GetTickCount();
            NET_ProcessNet((NETSOCK)socket, NET_SOCKET_FD);
        }
    }
    /* Memory-cache streams have no socket to put in a Winsock fd_set. */
    if (always) {
        NET_ProcessNet((NETSOCK)0, NET_UNKNOWN_FD);
        /* The mailbox protocol registers a synthetic local-file descriptor. */
        NET_ProcessNet((NETSOCK)1, NET_LOCAL_FILE_FD);
    }
    wfe_polling = 0;
}

void WFE_PlatformShutdown(void)
{
    while (wfe_dns) FE_ClearDNSSelect(wfe_dns->context, wfe_dns->socket);
    while (wfe_watches) {
        WFE_Watch *next = wfe_watches->next;
        free(wfe_watches);
        wfe_watches = next;
    }
#ifdef NETSCAPE_USE_BEARSSL
    WFE_TLSShutdown();
#endif
    if (wfe_initialized) WSACleanup();
    wfe_initialized = 0;
}

void WFE_PlatformForgetContext(MWContext *context)
{
    WFE_DNS *job;
    WFE_Watch **link = &wfe_watches;
    while (*link) {
        WFE_Watch *watch = *link;
        if (watch->context == context) { *link = watch->next; free(watch); }
        else link = &watch->next;
    }
    for (;;) {
        for (job = wfe_dns; job && job->context != context; job = job->next) ;
        if (!job) break;
        FE_ClearDNSSelect(context, job->socket);
    }
}
