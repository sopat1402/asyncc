#ifndef QUEUE

#define QUEUE 1

#include <stdlib.h>

struct Queue{
	size_t size;
	size_t capacity;
	char *queue;
	size_t data_size;
	size_t head;
	size_t tail;
};

struct Queue* queue_init(size_t data_size);

void queue_destroy(struct Queue *queue);

int queue_push(struct Queue *queue,const void *data);

int queue_pop(struct Queue *queue,void *out);

#endif
