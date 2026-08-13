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

void test_func1(struct Job *self){
	switch (self->state){
		case 0:
			printf("%d : We're at 0\n",self->id);
			self->state=1;
			self->status=Pending;
			break;
		case 1:
			printf("%d : We're at 1\n",self->id);
			self->state=2;
			self->status=Pending;
			break;
		case 2:
			printf("%d : We're done!!\n",self->id);
			self->status=Completed;
			break;
	}
}

void test_func2(struct Job *self){
	int data;
	if (self->state!=0){
		memcpy(&data,self->stack->data,sizeof(int));
	}
	switch (self->state){
		case 0:
			printf("%d : You're at 0\n",self->id);
			self->state=1;
			self->status=Pending;
			data=1;
			memcpy(self->stack->data,&data,sizeof(int));
			break;
		case 1:
			printf("%d : You're at 1, Data : %d\n",self->id,data);
			self->state=2;
			self->status=Pending;
			data=2;
			memcpy(self->stack->data,&data,sizeof(int));
			break;
		case 2:
			printf("%d : You're done!!\n",self->id);
			self->status=Completed;
			break;
	}
}

void async_task_add(struct Queue *job_queue,void (*function)(struct Job *self),int id){
	struct Job *new_job=malloc(sizeof(struct Job));
	if (new_job==NULL){
		queue_destroy(job_queue);
		exit(EXIT_FAILURE);
	}
	new_job->id=id;
	struct Stack *stk=malloc(sizeof(struct Stack));
	if (stk==NULL){
		free(new_job);
		queue_destroy(job_queue);
		exit(EXIT_FAILURE);
	}
	stk->data=malloc(STACK_SIZE*sizeof(unsigned char));
	if (stk->data==NULL){
		free(stk);
		free(new_job);
		exit(EXIT_FAILURE);
	}
	new_job->function=function;
	new_job->stack=stk;
	new_job->state=0;
	new_job->status=Pending;
	queue_push(job_queue,new_job);
	free(new_job);
}

void async_task_poll(struct Queue *job_queue){
	struct Job *job=malloc(sizeof(struct Job));
	queue_pop(job_queue,job);
	job->function(job);
	if (job->status!=Completed){
		queue_push(job_queue,job);
	}else{
		free(job->stack->data);
		free(job->stack);
	}
	free(job);
}
