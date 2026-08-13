#include "async.h"
#include <string.h>
#include <stdio.h>

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
			printf("TASK ID : %d DATA : Not init\n",self->id);
			self->state=1;
			self->status=Pending;
			data=1;
			memcpy(self->stack->data,&data,sizeof(int));
			break;
		case 1:
			printf("TASK ID : %d DATA : %d\n",self->id,data);
			self->state=2;
			self->status=Pending;
			data=2;
			memcpy(self->stack->data,&data,sizeof(int));
			break;
		case 2:
			printf("TASK ID : %d DATA : TASK END\n",self->id);
			self->status=Completed;
			break;
	}
}

void test_func3(struct Job *self){
	int iter=0;
	if (self->state!=0){
		memcpy(&iter,self->stack->data,sizeof(int));
	}
	self->state=1;
	for (int i=iter;i<7;i++){
		printf("ITERATION : %d ,TASK ID : %d\n",i,self->id);
		if (i%3==0){
			iter=i+1;
			memcpy(self->stack->data,&iter,sizeof(int));
			self->status=Pending;
			return;
		}
	}
	self->status=Completed;
}

int main(){
	struct Queue *job_queue=queue_init(sizeof(struct Job));
	if (job_queue==NULL) return 1;

	struct Thread_Data *thread_data=pool_init(job_queue);
	if (thread_data==NULL){
		queue_destroy(job_queue);
		return 1;
	}

	for (int i=0;i<6;i++){
		int choice=i%3;
		if (choice==1){
			async_task_add(thread_data,job_queue,test_func1,i);
		}else if (choice==2){
			async_task_add(thread_data,job_queue,test_func2,i);
		}else{
			async_task_add(thread_data,job_queue,test_func3,i);
		}
	}
	while (!queue_is_empty(job_queue) || atomic_load(thread_data->args->in_flight) != 0){
		if (!queue_is_empty(job_queue)){
			async_task_poll(thread_data,job_queue);
		}
	}
	destroy_pool(thread_data);
	queue_destroy(job_queue);
	return 0;
}
