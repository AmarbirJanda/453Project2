#ifndef LWPH
#define LWPH

#include <sys/types.h>
#include "fp.h"

/* Define rfile since fp.h doesn't have it */
typedef struct registers {
    unsigned long rax;
    unsigned long rbx;
    unsigned long rcx;
    unsigned long rdx;
    unsigned long rsi;
    unsigned long rdi;
    unsigned long rbp;
    unsigned long rsp;
    unsigned long r8;
    unsigned long r9;
    unsigned long r10;
    unsigned long r11;
    unsigned long r12;
    unsigned long r13;
    unsigned long r14;
    unsigned long r15;
    struct fxsave fxsave;
} rfile;

typedef unsigned long tid_t;
typedef void (*lwpfun)(void *);

#define NO_THREAD 0

/* Status flags */
#define LWP_LIVE	(1)
#define LWP_TERM	(2)

/* Macros for termination status */
#define MKTERMSTAT(exitcode)	(LWP_TERM | ((exitcode) << 8))
#define LWPTERMINATED(s)	(((s) & 0xFF) == LWP_TERM)
#define LWPTERMSTAT(s)		((s) >> 8)

/* Thread context structure */
typedef struct threadinfo_st {
    tid_t tid;
    unsigned long *stack;

    size_t stack_size;

    lwpfun initial_function;
    void *initial_argument;

    rfile state;
    unsigned int status;
    struct threadinfo_st *lib_one;
    struct threadinfo_st *lib_two;
    struct threadinfo_st *sched_one;
    struct threadinfo_st *sched_two;
    struct threadinfo_st *exited;
} context;

typedef context *thread;

/* Scheduler structure */
typedef struct scheduler {
    void (*init)(void);
    void (*shutdown)(void);
    void (*admit)(thread new);
    void (*remove)(thread victim);
    thread (*next)(void);
    int (*qlen)(void);
} *scheduler;

/* LWP function prototypes */
extern tid_t lwp_create(lwpfun function, void *argument);
extern void lwp_start(void);
extern void lwp_yield(void);
extern void lwp_exit(int status);
extern tid_t lwp_wait(int *status);
extern tid_t lwp_gettid(void);
extern thread tid2thread(tid_t tid);
extern void lwp_set_scheduler(scheduler sched);
extern scheduler lwp_get_scheduler(void);

/* Defined in magic64.S */
extern void swap_rfiles(rfile *old, rfile *new);

/* External scheduler */
extern scheduler RoundRobin;

#endif
