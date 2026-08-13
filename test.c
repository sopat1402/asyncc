#include <stdlib.h>
#include "queue.h"
#include "async.h"

int main(){
	struct Queue *job_queue=queue_init(sizeof(struct Job));
	for (int i=0;i<10;i++){
		int choice=i%2;
		if (choice){
			async_task_add(job_queue,test_func1,i);
		}else{
			async_task_add(job_queue,test_func2,i);
		}
	}
	while (job_queue->size!=0){
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
	queue_destroy(job_queue);
	return 0;
}
