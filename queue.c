#include "queue.h"
#include <stdlib.h>
#include <string.h>

#define MIN_QUEUE_SIZE 8

struct Queue* queue_init(size_t data_size){
    if (data_size == 0) return NULL;

    struct Queue *queue = malloc(sizeof(struct Queue));
    if (queue == NULL) return NULL;

    queue->queue = malloc(MIN_QUEUE_SIZE * data_size);
    if (queue->queue == NULL){
        free(queue);
        return NULL;
    }

    queue->capacity = MIN_QUEUE_SIZE;
    queue->size = 0;
    queue->data_size = data_size;
    queue->head = 0;
    queue->tail = 0;

    if (pthread_mutex_init(&queue->lock, NULL) != 0){
        free(queue->queue);
        free(queue);
        return NULL;
    }

    return queue;
}

void queue_destroy(struct Queue *queue){
    if (queue == NULL) return;
    pthread_mutex_destroy(&queue->lock);
    free(queue->queue);
    free(queue);
}

int queue_push(struct Queue *queue,const void *data){
    if (queue == NULL || data == NULL) return 1;

    pthread_mutex_lock(&queue->lock);

    if (queue->size == queue->capacity){
        size_t old_capacity = queue->capacity;
        size_t new_capacity = old_capacity * 2;
        size_t old_bytes = old_capacity * queue->data_size;
        char *new_queue = malloc(new_capacity * queue->data_size);

        if (new_queue == NULL){
            pthread_mutex_unlock(&queue->lock);
            return 1;
        }

        for (size_t i = 0; i < queue->size; i++){
            size_t offset = (queue->head + i * queue->data_size) % old_bytes;
            memcpy(
                new_queue + i * queue->data_size,
                queue->queue + offset,
                queue->data_size
            );
        }

        free(queue->queue);
        queue->queue = new_queue;
        queue->capacity = new_capacity;
        queue->head = 0;
        queue->tail = queue->size * queue->data_size;
    }

    memcpy(queue->queue + queue->tail, data, queue->data_size);
    queue->tail =
        (queue->tail + queue->data_size) %
        (queue->capacity * queue->data_size);
    queue->size++;

    pthread_mutex_unlock(&queue->lock);
    return 0;
}

int queue_pop(struct Queue *queue,void *out){
    if (queue == NULL || out == NULL) return 1;

    pthread_mutex_lock(&queue->lock);

    if (queue->size == 0){
        pthread_mutex_unlock(&queue->lock);
        return 1;
    }

    memcpy(out, queue->queue + queue->head, queue->data_size);
    queue->head =
        (queue->head + queue->data_size) %
        (queue->capacity * queue->data_size);
    queue->size--;

    pthread_mutex_unlock(&queue->lock);
    return 0;
}

int queue_is_empty(struct Queue *queue){
    if (queue == NULL) return 1;

    pthread_mutex_lock(&queue->lock);
    int empty = queue->size == 0;
    pthread_mutex_unlock(&queue->lock);
    return empty;
}
