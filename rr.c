#include <stdio.h>
#include <stdlib.h>
#include "lwp.h"

/* Round-robin scheduler structure */
/*typedef struct rr_node {
    thread thrd;
    struct rr_node *next;
    struct rr_node *prev;
} *rr_node;*/

static thread rr_head = NULL;
static thread rr_current = NULL;
static int rr_count = 0;

/* Round-robin functions */
static void rr_admit(thread new_thread);
static void rr_remove(thread victim);
static thread rr_next(void);
static int rr_qlen(void);

/* Admit thread to scheduler */
static void rr_admit(thread new_thread) {
    // rr_node node;
    
    if (new_thread == NULL) {
        return;
    }
    
    /* Allocate new node */
    // node = (rr_node)malloc(sizeof(struct rr_node));
    if (rr_head == NULL) {
        // making first thread point to itself
        new_thread->sched_one = new_thread; // next
	new_thread->sched_two = new_thread; // prev
	rr_head = new_thread;
	rr_current = new_thread;
    } else {
	thread tail = rr_head->sched_two; // get the last node

	// link 'new_thread' between 'tail' and 'rr_head'
	new_thread->sched_one = rr_head; // new->next = head
	new_thread->sched_two = tail;    // new->prev = tail

	tail->sched_one = new_thread;	 // tail->next = new
	rr_head->sched_two = new_thread; // head->prev = new
    }
    
    /*
    node->thrd = new;
    
    // Add to circular list 
    if (rr_head == NULL) {
        node->next = node;
        node->prev = node;
        rr_head = node;
        rr_current = node;
    } else {
        node->next = rr_head;
        node->prev = rr_head->prev;
        rr_head->prev->next = node;
        rr_head->prev = node;
    }
    
    // Use sched_one to store node pointer
    new->sched_one = (thread)node;
    */    

    rr_count++;
}

/* Remove thread from scheduler */
static void rr_remove(thread victim) {
    // rr_node node;
    
    if (victim == NULL || victim->sched_one == NULL) {
        return;
    }

    if (victim->sched_one == victim) {
	// last thread in queue
	rr_head = NULL;
	rr_current = NULL;
    } else {
	thread next = victim->sched_one;
	thread prev = victim->sched_two;

	prev->sched_one = next;
	next->sched_two = prev;

	if (rr_head == victim) {
	    rr_head = next;
	}
	if (rr_current == victim) {
	    rr_current = next;
	}
    }
    
    /* node = (rr_node)victim->sched_one;
    
    if (node->next == node) {
        // Last node
        rr_head = NULL;
        rr_current = NULL;
    } else {
        node->prev->next = node->next;
        node->next->prev = node->prev;
        
        if (rr_head == node) {
            rr_head = node->next;
        }
        if (rr_current == node) {
            rr_current = node->next;
        }
    }
    
    free(node); */

    victim->sched_one = NULL;
    victim->sched_two = NULL;
    rr_count--;
}

/* Get next thread */
static thread rr_next(void) {
    if (rr_current == NULL) {
        return NULL;
    }

    // thread returned is the one that runs
    thread next_thread = rr_current;

    // advance pointer to rr_next
    rr_current = rr_current->sched_one;
    
    return next_thread;
}

/* Get queue length */
static int rr_qlen(void) {
    return rr_count;
}

/* Scheduler structure */
static struct scheduler rr_publish = {
    NULL,        /* init */
    NULL,        /* shutdown */
    rr_admit,    /* admit */
    rr_remove,   /* remove */
    rr_next,     /* next */
    rr_qlen      /* qlen */
};

scheduler RoundRobin = &rr_publish; 
