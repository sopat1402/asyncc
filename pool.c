#include "async.h"
#include <stdlib.h>

int ring_queue_init(struct Ring_Queue *queue){
    if (pthread_mutex_init(&queue->lock, NULL) != 0) return 1;

    queue->head = NULL;
    queue->tail = NULL;
    return 0;
}

int push(struct Ring_Queue *queue, struct Node *node){
    node->next = NULL;

    pthread_mutex_lock(&queue->lock);

    if (queue->tail == NULL){
        queue->head = node;
        queue->tail = node;
    } else {
        queue->tail->next = node;
        queue->tail = node;
    }

    pthread_mutex_unlock(&queue->lock);
    return 0;
}

struct Node *pop(struct Args *a){
    struct Ring_Queue *queue = a->queue;

    pthread_mutex_lock(&queue->lock);

    struct Node *node = queue->head;
    if (node != NULL){
        queue->head = node->next;
        if (queue->head == NULL) queue->tail = NULL;
    }

    pthread_mutex_unlock(&queue->lock);
    return node;
}

static int pick_victim(int self_id, unsigned *seed){
    int victim;

    do {
        *seed = *seed * 1103515245u + 12345u;
        victim = (int)((*seed >> 16) % THREAD_COUNT);
    } while (victim == self_id);

    return victim;
}

static void process_node(struct Args *a, struct Node *node){
    node->job->function(node->job);

    if (node->job->status != Completed){
        queue_push(a->job_queue, node->job);
    } else {
        free(node->job->stack->data);
        free(node->job->stack);
        atomic_fetch_sub(a->in_flight, 1);
    }

    free(node->job);
    free(node);
}

void *run(void *args){
    struct Args *a = args;
    unsigned seed = (unsigned)a->id + 1u;

    for (;;){
        atomic_store(&a->lookup[a->id]->active, 1);
        struct Node *node = pop(a);
        atomic_store(&a->lookup[a->id]->active, 0);

        if (node != NULL){
            process_node(a, node);
            continue;
        }

        if (atomic_load(a->done)) break;

        int victim = pick_victim(a->id, &seed);
        atomic_store(&a->lookup[a->id]->active, 1);
        node = pop(&a->args[victim]);
        atomic_store(&a->lookup[a->id]->active, 0);

        if (node != NULL){
            process_node(a, node);
            continue;
        }
    }

    return NULL;
}

int submit_job(struct Thread_Data *thread_data, struct Node *node){
    int index = (thread_data->last_index + 1) % THREAD_COUNT;

    if (push(thread_data->args[index].queue, node) != 0){
        free(node->job->stack->data);
        free(node->job->stack);
        free(node->job);
        free(node);
        atomic_fetch_sub(thread_data->args->in_flight, 1);
    }

    return index;
}
