#ifndef ASYNC

#define ASYNC 1

#include <stdlib.h>
#include <stdio.h>
#include "queue.h"
#include <string.h>

#define STACK_SIZE 4096

enum Status{
	Pending,
	Completed,
};

struct Stack{
	unsigned char *data;
};

struct Job{
	int status;
	int state;
	unsigned int id;
	struct Stack *stack;
	void (*function)(struct Job*);
};

void test_func1(struct Job *self);

void test_func2(struct Job *self);

void async_task_add(struct Queue *job_queue,void (*function)(struct Job *self),int id);

void async_task_poll(struct Queue *job_queue);

#endif
