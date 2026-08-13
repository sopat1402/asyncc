#include <stdlib.h>
#include <string.h>

#define MIN_QUEUE_SIZE 8

struct Queue{
	size_t size;
	size_t capacity;
	char *queue;
	size_t data_size;
	size_t head;
	size_t tail;
};

struct Queue* queue_init(size_t data_size){
	struct Queue *queue=malloc(sizeof(struct Queue));
	if (queue==NULL) return NULL;
	queue->queue=malloc(MIN_QUEUE_SIZE*data_size);
	if (queue->queue==NULL){
		free(queue);
		return NULL;
	}
	queue->capacity=MIN_QUEUE_SIZE;
	queue->size=0;
	queue->data_size=data_size;
	queue->head=0;
	queue->tail=0;
	return queue;
}

void queue_destroy(struct Queue *queue){
	free(queue->queue);
	free(queue);
}

int queue_push(struct Queue *queue,const void *data){
	if (queue->size==queue->capacity){
		char *new_queue=malloc(queue->capacity*2*queue->data_size);
		if (new_queue==NULL){
			return 1;
		}
		size_t new_head=0;
		size_t new_tail=0;
		if (queue->head<queue->tail){
			memcpy(new_queue,queue->queue+queue->head,(queue->tail-queue->head));
			new_tail=queue->tail-queue->head;
		}else{
			memcpy(new_queue,queue->queue+queue->head,queue->capacity*queue->data_size-queue->head);
			new_tail=queue->capacity*queue->data_size-queue->head;
			memcpy(new_queue+new_tail,queue->queue,queue->tail);
			new_tail+=queue->tail;
		}

		queue->capacity*=2;
		queue->tail=new_tail;
		queue->head=new_head;
		free(queue->queue);
		queue->queue=new_queue;
	}
	memcpy(queue->queue+queue->tail,data,queue->data_size);
	queue->tail=(queue->tail+queue->data_size)%(queue->capacity*queue->data_size);
	queue->size++;
	return 0;
}

int queue_pop(struct Queue *queue,void *out){
	if (queue->size==0){
		return 1;
	}else{
		memcpy(out,queue->queue+queue->head,queue->data_size);
		queue->head=(queue->head+queue->data_size)%(queue->capacity*queue->data_size);
		queue->size--;
		return 0;
	}
}
