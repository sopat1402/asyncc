#ifndef ASYNC_H
#define ASYNC_H

#include <stddef.h>
#include "pool.h"

enum Job_Status {
    Pending,
    Completed
};

struct Stack {
    unsigned char *data;
};

struct Job {
    int id;
    void (*function)(struct Job *self);
    struct Stack *stack;
    int state;
    enum Job_Status status;
};

void async_task_add(
    struct Thread_Data *thread_data,
    struct Queue *job_queue,
    void (*function)(struct Job *self),
    int id
);

void async_task_poll(
    struct Thread_Data *thread_data,
    struct Queue *job_queue
);

#endif
