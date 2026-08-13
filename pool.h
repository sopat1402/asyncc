#ifndef POOL_H
#define POOL_H

#include <pthread.h>
#include <stdatomic.h>
#include "queue.h"

#define THREAD_COUNT 4

struct Node {
    struct Job *job;
    struct Node *next;
};

struct Ring_Queue {
    struct Node *head;
    struct Node *tail;
    pthread_mutex_t lock;
};

struct Thread {
    atomic_int active;
};

struct Args {
    struct Ring_Queue *queue;
    int id;
    struct Thread **lookup;
    struct Args *args;
    atomic_int *done;
    struct Queue *job_queue;
    atomic_int *in_flight;
};

struct Thread_Data {
    pthread_t *threads;
    struct Args *args;
    struct Thread **lookup;
    atomic_int *done;
    int last_index;
    struct Queue *job_queue;
};

int ring_queue_init(struct Ring_Queue *queue);
int push(struct Ring_Queue *queue, struct Node *node);
struct Node *pop(struct Args *a);
int submit_job(struct Thread_Data *thread_data, struct Node *node);
void *run(void *args);
struct Thread_Data *pool_init(struct Queue *job_queue);
void destroy_pool(struct Thread_Data *thread_data);

#endif
