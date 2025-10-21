#include <stdio.h>
#include <stdlib.h>
#include "lwp.h"

typedef struct rr_node {
thread thrd;
struct rr_node *next;
struct rr_node *prev;
} *rr_node;

static rr_node rr_head = NULL;
static rr_node rr_current = NULL;
static int rr_count = 0;

static void rr_admit(thread new);
static void rr_remove(thread victim);
static thread rr_next(void);
static int rr_qlen(void);

static void rr_admit(thread new) {
rr_node node;

if (new == NULL) {
return;
}

node = (rr_node)malloc(sizeof(struct rr_node));
if (node == NULL) {
return;
}

node->thrd = new;

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

new->sched_one = (thread)node;
rr_count++;
}

static void rr_remove(thread victim) {
rr_node node;

if (victim == NULL || victim->sched_one == NULL) {
return;
}

node = (rr_node)victim->sched_one;

if (node->next == node) {
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

free(node);
victim->sched_one = NULL;
rr_count--;
}

static thread rr_next(void) {
thread t;

if (rr_current == NULL) {
return NULL;
}

t = rr_current->thrd;
rr_current = rr_current->next;

return t;
}

static int rr_qlen(void) {
return rr_count;
}

static struct scheduler rr_publish = {
NULL,
NULL,
rr_admit,
rr_remove,
rr_next,
rr_qlen
};

scheduler RoundRobin = &rr_publish;