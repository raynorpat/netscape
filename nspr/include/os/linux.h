#ifndef nspr_linux_defs_h___
#define nspr_linux_defs_h___

#define PR_LINKER_ARCH	"linux"

#define _MD_GC_VMBASE		0x40000000
#define _MD_STACK_VMBASE	0x50000000
#define _MD_DEFAULT_STACK_SIZE	65536L

#ifdef SW_THREADS
#include <ucontext.h>

#define PR_CONTEXT_TYPE	ucontext_t

#define CONTEXT(_th) (&(_th)->context)

/* glibc hides the REG_* enum names when this old tree enables strict
 * POSIX feature flags.  The Linux ucontext register slots are ABI-stable. */
#if defined(__i386__)
# define PR_LINUX_REG_SP 7
#elif defined(__x86_64__)
# define PR_LINUX_REG_SP 15
#elif defined(__aarch64__)
#else
# error "Unsupported Linux software-thread architecture"
#endif

#if defined(__aarch64__)
# define PR_GetSP(_t) ((_t)->context.uc_mcontext.sp)
#else
# define PR_GetSP(_t) ((_t)->context.uc_mcontext.gregs[PR_LINUX_REG_SP])
#endif

/*
** Initialize a thread context to run "e(o,a)" when started
*/
#define _MD_INIT_CONTEXT(_thread, e, o, a)	  \
{						  \
	    ucontext_t *uc = CONTEXT(_thread);		  \
	    getcontext(uc);					  \
	    uc->uc_stack.ss_sp = (_thread)->stack->stackBottom; \
	    uc->uc_stack.ss_size = (_thread)->stack->stackSize; \
	    uc->uc_stack.ss_flags = 0;			  \
	    uc->uc_link = 0;					  \
    (_thread)->asyncCall = e;			  \
    (_thread)->asyncArg0 = o;			  \
    (_thread)->asyncArg1 = a;			  \
    makecontext(uc, (void (*)(void)) HopToadNoArgs, 0); \
}

#define _MD_SWITCH_CONTEXT(_thread)  \
    if (!getcontext(CONTEXT(_thread))) { \
	if ((_thread)->contextRestored) { \
	    (_thread)->contextRestored = 0; \
	} else { \
	    (_thread)->errcode = errno; \
	    _PR_Schedule(); \
	} \
    }

/*
** Restore a thread context, saved by _MD_SWITCH_CONTEXT
*/
#define _MD_RESTORE_CONTEXT(_thread) \
{				     \
	    ucontext_t *uc = CONTEXT(_thread);	 \
	    (_thread)->contextRestored = 1;	 \
    _pr_current_thread = _thread;    \
    PR_LOG(SCHED, warn, ("Scheduled")); \
    errno = (_thread)->errcode;	     \
    setcontext(uc);			     \
}

#endif /* SW_THREADS */

#undef	HAVE_LONG_LONG
#undef	HAVE_ALIGNED_DOUBLES
#undef	HAVE_ALIGNED_LONGLONGS

/*
 * Elf linux supports dl* functions
 */
#ifdef LINUX1_2
#define	HAVE_DLL
#define	USE_DLFCN
#else
#undef	HAVE_DLL
#undef	USE_DLFCN
#endif

#define NEED_TIME_R

#endif /* nspr_linux_defs_h___ */
