#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include "lwp.h"

#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#ifndef MAP_STACK
#define MAP_STACK 0x20000
#endif

static thread current_thread = NULL;
static thread thread_list = NULL;
static thread terminated_queue = NULL;
static thread waiting_queue = NULL;
static scheduler current_scheduler = NULL;
static tid_t next_tid = 1;

static void lwp_wrap(lwpfun function, void *argument);
static unsigned long *setup_stack(size_t stack_size);
static size_t get_stack_size(void);

static void lwp_wrap(lwpfun function, void *argument) {
    function(argument);
    lwp_exit(0);
}

static unsigned long *setup_stack(size_t stack_size) {
    void *stack = mmap(NULL, stack_size, 
                       PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, 
                       -1, 0);
    if (stack == MAP_FAILED) {
        return NULL;
    }
    return (unsigned long *)stack;
}

static size_t get_stack_size(void) {
    struct rlimit limit;
    long page_size = sysconf(_SC_PAGE_SIZE);
    size_t stack_size;
    
    if (getrlimit(RLIMIT_STACK, &limit) != 0) {
        return 8 * 1024 * 1024;
    }
    
    if (limit.rlim_cur == RLIM_INFINITY) {
        stack_size = 8 * 1024 * 1024;
    } else {
        stack_size = limit.rlim_cur;
    }
    
    if (stack_size % page_size != 0) {
        stack_size = ((stack_size / page_size) + 1) * page_size;
    }
    
    return stack_size;
}

tid_t lwp_create(lwpfun function, void *argument) {
    thread new_thread;
    size_t stack_size;
    unsigned long *sp;
    
    if (current_scheduler == NULL) {
        lwp_set_scheduler(RoundRobin);
    }
    
    new_thread = (thread)malloc(sizeof(struct threadinfo_st));
    if (new_thread == NULL) {
        return NO_THREAD;
    }
    
    stack_size = get_stack_size();
    new_thread->stack = setup_stack(stack_size);
    if (new_thread->stack == NULL) {
        free(new_thread);
        return NO_THREAD;
    }
    
    new_thread->tid = next_tid++;
    new_thread->status = MKTERMSTAT(LWP_LIVE, 0);
    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL;
    new_thread->sched_one = NULL;
    new_thread->sched_two = NULL;
    new_thread->exited = NULL;
    new_thread->state.fxsave = FPU_INIT;
    
    sp = new_thread->stack + (stack_size / sizeof(unsigned long));
    
    sp = (unsigned long *)((unsigned long)sp & ~0xFUL);
    
    sp -= 2;
    sp[0] = 0;
    sp[1] = (unsigned long)lwp_wrap;
    
    new_thread->state.rax = 0;
    new_thread->state.rbx = 0;
    new_thread->state.rcx = 0;
    new_thread->state.rdx = 0;
    new_thread->state.rsi = (unsigned long)argument;
    new_thread->state.rdi = (unsigned long)function;
    new_thread->state.rbp = (unsigned long)sp;
    new_thread->state.rsp = (unsigned long)sp;
    new_thread->state.r8 = 0;
    new_thread->state.r9 = 0;
    new_thread->state.r10 = 0;
    new_thread->state.r11 = 0;
    new_thread->state.r12 = 0;
    new_thread->state.r13 = 0;
    new_thread->state.r14 = 0;
    new_thread->state.r15 = 0;
    
    new_thread->lib_one = thread_list;
    thread_list = new_thread;
    
    current_scheduler->admit(new_thread);
    
    return new_thread->tid;
}

void lwp_start(void) {
    thread original;
    
    if (current_scheduler == NULL) {
        lwp_set_scheduler(RoundRobin);
    }
    
    original = (thread)malloc(sizeof(struct threadinfo_st));
    if (original == NULL) {
        return;
    }
    
    original->tid = next_tid++;
    original->stack = NULL;
    original->status = MKTERMSTAT(LWP_LIVE, 0);
    original->lib_one = thread_list;
    original->lib_two = NULL;
    original->sched_one = NULL;
    original->sched_two = NULL;
    original->exited = NULL;
    original->state.fxsave = FPU_INIT;
    
    thread_list = original;
    current_thread = original;
    
    current_scheduler->admit(original);
    lwp_yield();
}

void lwp_yield(void) {
    thread next_thread;
    thread old_thread = current_thread;
    
    if (current_scheduler == NULL) {
        return;
    }
    
    next_thread = current_scheduler->next();
    
    if (next_thread == NULL) {
        if (old_thread != NULL) {
            exit(LWPTERMSTAT(old_thread->status));
        } else {
            exit(0);
        }
    }
    
    current_thread = next_thread;
    swap_rfiles(&old_thread->state, &next_thread->state);
}

void lwp_exit(int status) {
    thread exiting = current_thread;
    
    if (exiting == NULL || current_scheduler == NULL) {
        return;
    }
    
    exiting->status = MKTERMSTAT(LWP_TERM, status);
    current_scheduler->remove(exiting);
    
    exiting->lib_two = terminated_queue;
    terminated_queue = exiting;
    
    if (exiting->exited != NULL) {
        thread waiting = exiting->exited;
        exiting->exited = NULL;
        
        thread *ptr = &waiting_queue;
        while (*ptr != NULL) {
            if (*ptr == waiting) {
                *ptr = waiting->lib_two;
                break;
            }
            ptr = &((*ptr)->lib_two);
        }
        
        current_scheduler->admit(waiting);
    }
    
    lwp_yield();
}

tid_t lwp_wait(int *status) {
    thread terminated;
    tid_t tid;
    
    if (current_scheduler == NULL) {
        return NO_THREAD;
    }
    
    if (terminated_queue != NULL) {
        thread *ptr = &terminated_queue;
        thread prev = NULL;
        
        while ((*ptr)->lib_two != NULL) {
            prev = *ptr;
            ptr = &((*ptr)->lib_two);
        }
        
        terminated = *ptr;
        
        if (prev == NULL) {
            terminated_queue = NULL;
        } else {
            prev->lib_two = NULL;
        }
        
        tid = terminated->tid;
        if (status != NULL) {
            *status = terminated->status;
        }
        
        if (terminated->stack != NULL) {
            size_t stack_size = get_stack_size();
            munmap(terminated->stack, stack_size);
        }
        
        thread *list_ptr = &thread_list;
        while (*list_ptr != NULL) {
            if (*list_ptr == terminated) {
                *list_ptr = terminated->lib_one;
                break;
            }
            list_ptr = &((*list_ptr)->lib_one);
        }
        
        free(terminated);
        return tid;
    }
    
    if (current_scheduler->qlen() <= 1) {
        return NO_THREAD;
    }
    
    current_scheduler->remove(current_thread);
    current_thread->lib_two = waiting_queue;
    waiting_queue = current_thread;
    
    lwp_yield();
    
    terminated = current_thread->exited;
    current_thread->exited = NULL;
    
    if (terminated == NULL) {
        return NO_THREAD;
    }
    
    tid = terminated->tid;
    if (status != NULL) {
        *status = terminated->status;
    }
    
    if (terminated->stack != NULL) {
        size_t stack_size = get_stack_size();
        munmap(terminated->stack, stack_size);
    }
    
    thread *list_ptr = &thread_list;
    while (*list_ptr != NULL) {
        if (*list_ptr == terminated) {
            *list_ptr = terminated->lib_one;
            break;
        }
        list_ptr = &((*list_ptr)->lib_one);
    }
    
    free(terminated);
    return tid;
}

tid_t lwp_gettid(void) {
    if (current_thread == NULL) {
        return NO_THREAD;
    }
    return current_thread->tid;
}

thread tid2thread(tid_t tid) {
    thread t = thread_list;
    
    while (t != NULL) {
        if (t->tid == tid) {
            return t;
        }
        t = t->lib_one;
    }
    
    return NULL;
}

void lwp_set_scheduler(scheduler sched) {
    thread t;
    scheduler old = current_scheduler;
    
    if (sched == NULL) {
        sched = RoundRobin;
    }
    
    if (sched->init != NULL) {
        sched->init();
    }
    
    if (old != NULL) {
        while ((t = old->next()) != NULL) {
            old->remove(t);
            sched->admit(t);
        }
        
        if (old->shutdown != NULL) {
            old->shutdown();
        }
    }
    
    current_scheduler = sched;
}

scheduler lwp_get_scheduler(void) {
    return current_scheduler;
}