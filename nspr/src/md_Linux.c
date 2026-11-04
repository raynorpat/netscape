#include "prtypes.h"
#include "prlog.h"
#include "prosdep.h"
#include "prthread.h"
#include <sys/time.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <ucontext.h>
#include <errno.h>
#include <linux/net.h>
#include "mdint.h"
#include <signal.h>
#include <time.h>

/* The original Linux port supplied these through 32-bit socketcall
 * assembly.  Keep the same negative-errno convention with portable libc
 * calls so the port also links on amd64 and arm64. */
int _OS_SOCKET(int domain, int type, int protocol)
{
    int rv = socket(domain, type, protocol);
    return rv < 0 ? -errno : rv;
}

int _OS_CONNECT(int fd, void *addr, int addrlen)
{
    int rv = connect(fd, (const struct sockaddr *) addr,
                     (socklen_t) addrlen);
    return rv < 0 ? -errno : rv;
}

int _OS_RECV(int fd, void *buf, int len, int flags)
{
    int rv = recv(fd, buf, (size_t) len, flags);
    return rv < 0 ? -errno : rv;
}

int _OS_SEND(int fd, const void *buf, int len, int flags)
{
    int rv = send(fd, buf, (size_t) len, flags);
    return rv < 0 ? -errno : rv;
}

int _OS_SELECT(int nfds, fd_set *readfds, fd_set *writefds,
               fd_set *exceptfds, struct timeval *timeout)
{
    struct timespec ts;
    struct timespec *tsp = 0;
    int rv;

    if (timeout) {
        ts.tv_sec = timeout->tv_sec;
        ts.tv_nsec = timeout->tv_usec * 1000;
        tsp = &ts;
    }
    rv = pselect(nfds, readfds, writefds, exceptfds, tsp, 0);
    return rv < 0 ? -errno : rv;
}

static void CatchSegv(void)
{
    PR_LOG_FLUSH();
    kill(getpid(), SIGBUS);
}

void _MD_InitOS(int when)
{
    struct sigaction vtact;

    _MD_InitUnixOS(when);

    if (when == _MD_INIT_READY) {
	vtact.sa_handler = (void (*)()) CatchSegv;
	vtact.sa_flags = 0;
	sigemptyset(&vtact.sa_mask);
	sigaddset(&vtact.sa_mask, SIGALRM);
	sigaction(SIGSEGV, &vtact, 0);

	/* and ignore FPE */
	sigemptyset(&vtact.sa_mask);
	vtact.sa_handler = SIG_IGN;
	sigaction(SIGFPE, &vtact, 0);
    }
} 

prword_t *_MD_HomeGCRegisters(PRThread *t, int isCurrent, int *np)
{
    if (isCurrent) {
        (void) getcontext(CONTEXT(t));
    }
    *np = sizeof(t->context) / sizeof(prword_t);
    return (prword_t*) CONTEXT(t);
}

/************************************************************************/

/*
** XXX these don't really work right because they use errno which can be
** XXX raced against
*/

int _OS_READ(int fd, void *buf, size_t len)
{
    int rv;
    rv = read(fd, buf, len);
    if (rv < 0) {
	return -errno;
    }
    return rv;
}

int _OS_WRITE(int fd, const void *buf, size_t len)
{
    int rv;
    rv = write(fd, buf, len);
    if (rv < 0) {
	return -errno;
    }
    return rv;
}

int _OS_OPEN(const char *path, int oflag, int mode)
{
    int rv;
    /* AArch64 has openat(2) but no SYS_open macro.  libc selects the
     * correct syscall on every Linux architecture. */
    rv = open(path, oflag, mode);
    if(rv < 0)
        return -errno;
    return rv;
}

int _OS_CLOSE(int fd)
{
    int rv;
    rv = close(fd);
    if(rv < 0)
        return -errno;
    return rv;
}

int _OS_FCNTL(int fd, int flag, int value)
{
    int rv;
    rv = fcntl(fd, flag, value);
    if (rv < 0) {
	return -errno;
    }
    return rv;
}

int _OS_IOCTL(int fd, int tag, int arg)
{
    int rv;
    rv = ioctl(fd, tag, arg);
    if (rv < 0) {
	return -errno;
    }
    return rv;
}

/* XXX Stubs until we get to linux 1.3 */

#ifndef LINUX1_2
void * dlopen(const char *filename, int flag)
{
    return 0;
}

const char *dlerror(void)
{
    return "dlerror doesn't work";
}

void *dlsym(void *h, char *sym)
{
    return 0;
}
#endif /* LINUX1_2 */
