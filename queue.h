#ifndef QUEUE_H
#define QUEUE_H

#include <pthread.h>
#include <stddef.h>

struct Queue {
    size_t size;
    size_t capacity;
    char *queue;
    size_t data_size;
    size_t head;
    size_t tail;
    pthread_mutex_t lock;
};

struct Queue *queue_init(size_t data_size);
void queue_destroy(struct Queue *queue);
int queue_push(struct Queue *queue, const void *data);
int queue_pop(struct Queue *queue, void *out);
int queue_is_empty(struct Queue *queue);

#endif
