#ifndef LWPH
#define LWPH
#include <sys/types.h>
#include "fp.h"

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

#define LWP_LIVE 1
#define LWP_TERM 0

#define TERMOFFSET 8
#define MKTERMSTAT(status, exitcode) (((status) << TERMOFFSET) | ((exitcode) & ((1 << TERMOFFSET) - 1)))
#define LWPTERMINATED(status) (((status) >> TERMOFFSET) & ((1 << (sizeof(int) * 8 - TERMOFFSET)) - 1))
#define LWPTERMSTAT(status) ((status) & ((1 << TERMOFFSET) - 1))

typedef struct threadinfo_st {
tid_t tid;
unsigned long *stack;
rfile state;
unsigned int status;
struct threadinfo_st *lib_one;
struct threadinfo_st *lib_two;
struct threadinfo_st *sched_one;
struct threadinfo_st *sched_two;
struct threadinfo_st *exited;
} context;

typedef context *thread;

typedef struct scheduler {
void (*init)(void);
void (*shutdown)(void);
void (*admit)(thread new);
void (*remove)(thread victim);
thread (*next)(void);
int (*qlen)(void);
} *scheduler;

extern tid_t lwp_create(lwpfun function, void *argument);
extern void lwp_start(void);
extern void lwp_yield(void);
extern void lwp_exit(int status);
extern tid_t lwp_wait(int *status);
extern tid_t lwp_gettid(void);
extern thread tid2thread(tid_t tid);
extern void lwp_set_scheduler(scheduler sched);
extern scheduler lwp_get_scheduler(void);

extern void swap_rfiles(rfile *old, rfile *new);

extern scheduler RoundRobin;

#endif