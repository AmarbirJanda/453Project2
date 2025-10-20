#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>
#include "lwp.h"

/* Ensure MAP_ANONYMOUS and MAP_STACK are defined */
#ifndef MAP_ANONYMOUS
#define MAP_ANONYMOUS 0x20
#endif

#ifndef MAP_STACK
#define MAP_STACK 0x20000
#endif

/* Global variables */
static thread current_thread = NULL;
static thread thread_list = NULL;
static thread terminated_queue = NULL;
static thread waiting_queue = NULL;
static scheduler current_scheduler = NULL;
static tid_t next_tid = 1;

/* Forward declarations */
static void lwp_wrap(void);
static unsigned long *setup_stack(size_t stack_size);
static size_t get_stack_size(void);

/* Wrapper function to handle thread termination */
static void lwp_wrap() {
    lwpfun function;
    void *argument;

    function = current_thread->initial_function;
    argument = current_thread->initial_argument;

    function(argument);

    lwp_exit(0);
}

/* Setup stack using mmap */
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

/* Get current stack size */
static size_t get_stack_size(void) {
    struct rlimit limit;
    long page_size = sysconf(_SC_PAGE_SIZE);
    size_t stack_size;
    const size_t MIN_STACK_SIZE = 16 * 1024 * 1024;
    
    if (getrlimit(RLIMIT_STACK, &limit) != 0) {
        return MIN_STACK_SIZE;
    }
    
    if (limit.rlim_cur == RLIM_INFINITY) {
        stack_size = MIN_STACK_SIZE;
    } else {
        stack_size = limit.rlim_cur;
    }
    
    // Ensure stack_size is at least the min 16MB
    if (stack_size < MIN_STACK_SIZE) {
        stack_size = MIN_STACK_SIZE;
    }

    /* Round up to page size */
    if (stack_size % page_size != 0) {
	stack_size = ((stack_size / page_size) + 1) * page_size;
    }
    
    return stack_size;
}

/* Create a new thread */
tid_t lwp_create(lwpfun function, void *argument) {
    thread new_thread;
    size_t stack_size;
    unsigned long *stack_base, *stack_top;
    
    if (current_scheduler == NULL) {
        return NO_THREAD;
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
    
    new_thread->stack_size = stack_size;

    new_thread->tid = next_tid++;
    new_thread->status = LWP_LIVE;
    new_thread->lib_one = NULL;
    new_thread->lib_two = NULL;
    new_thread->sched_one = NULL;
    new_thread->sched_two = NULL;
    new_thread->exited = NULL;
    
    //new_thread->state.fxsave = FPU_INIT;
    
    memset(&new_thread->state.fxsave, 0, sizeof(new_thread->state.fxsave));

    // calculate the address just past the end of the mmap'd stack
    stack_base = (unsigned long *)((char *)new_thread->stack + stack_size);

    // align the highest usable address down to a 16-byte boundary
    stack_top = (unsigned long *)((unsigned long)stack_base & ~0xFUL);
    
    stack_top--;
    *stack_top = (unsigned long)lwp_wrap;
    // stack_top--;
    // *stack_top = 0;
    
    new_thread->state.rsp = (unsigned long)stack_top;
    new_thread->state.rbp = (unsigned long)stack_top;
    // new_thread->state.rdi = (unsigned long)function;
    // new_thread->state.rsi = (unsigned long)argument;
    
    new_thread->initial_function = function;
    new_thread->initial_argument = argument;
    
    // stack_top--;
    // *stack_top = (unsigned long)lwp_wrap;
    // new_thread->state.rsp = (unsigned long)stack_top;
    
    new_thread->lib_one = thread_list;
    thread_list = new_thread;
    
    current_scheduler->admit(new_thread);
    
    return new_thread->tid;
}

void lwp_start(void) {
    thread original;
    
    if (current_scheduler == NULL) {
        return;
    }
    
    original = (thread)malloc(sizeof(struct threadinfo_st));
    if (original == NULL) {
        return;
    }
    
    original->tid = next_tid++;
    original->stack = NULL;
    original->status = LWP_LIVE;
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
    thread next;
    thread old = current_thread;
    
    if (current_scheduler == NULL) {
        return;
    }
    
    next = current_scheduler->next();
    
    if (next == NULL) {
        if (old != NULL) {
            exit(LWPTERMSTAT(old->status));
        } else {
            exit(0);
        }
    }
    
    current_thread = next;
    swap_rfiles(&old->state, &next->state);
}

void lwp_exit(int status) {
    thread exiting = current_thread;
    thread waiting;
    
    if (exiting == NULL || current_scheduler == NULL) {
        return;
    }
    
    exiting->status = MKTERMSTAT(status);
    current_scheduler->remove(exiting);
    
    // check for thread waiting to reap this one
    if (waiting_queue != NULL) {
	// unblock the first thread in the waiting_queue
	waiting = waiting_queue;
	waiting_queue = waiting_queue->lib_two; // remove from waiting list

	// transfer TCB of thread currently exiting to the waiting thread's exited field
	// this is how lwp_wait() wakes up and knows which TCB to reap
	waiting->exited = exiting;

	// put waiting threaf back into the scheduler so it can run immediately
	current_scheduler->admit(waiting);

    } else {
	// if no thread is waitinf, add exiting thread to terminated_queue
	// so lwp_wait() can reap later
	exiting->lib_two = terminated_queue; // lib_two used for terminated_queue
	terminated_queue = exiting;
    }

    /*
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
    }*/
    
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
            munmap(terminated->stack, terminated->stack_size);
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
        munmap(terminated->stack, terminated->stack_size);
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
