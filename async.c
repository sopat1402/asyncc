#include <stdlib.h>
#include "async.h"

#define STACK_SIZE 4096

void async_task_add(
    struct Thread_Data *thread_data,
    struct Queue *job_queue,
    void (*function)(struct Job *self),
    int id
){
    struct Job *new_job = malloc(sizeof(struct Job));
    if (new_job == NULL) return;

    struct Stack *stk = malloc(sizeof(struct Stack));
    if (stk == NULL){
        free(new_job);
        return;
    }

    stk->data = malloc(STACK_SIZE * sizeof(unsigned char));
    if (stk->data == NULL){
        free(stk);
        free(new_job);
        return;
    }

    new_job->id = id;
    new_job->function = function;
    new_job->stack = stk;
    new_job->state = 0;
    new_job->status = Pending;

    atomic_fetch_add(thread_data->args->in_flight, 1);

    if (queue_push(job_queue, new_job) != 0){
        atomic_fetch_sub(thread_data->args->in_flight, 1);
        free(stk->data);
        free(stk);
        free(new_job);
        return;
    }

    free(new_job);
}

void async_task_poll(
    struct Thread_Data *thread_data,
    struct Queue *job_queue
){
    struct Job *job = malloc(sizeof(struct Job));
    if (job == NULL) return;

    struct Node *node = malloc(sizeof(struct Node));
    if (node == NULL){
        free(job);
        return;
    }

    if (queue_pop(job_queue, job) != 0){
        free(node);
        free(job);
        return;
    }

    node->job = job;
    node->next = NULL;
    thread_data->last_index = submit_job(thread_data, node);
}

struct Thread_Data *pool_init(struct Queue *job_queue){
    pthread_t *threads = malloc(THREAD_COUNT * sizeof(pthread_t));
    atomic_int *done = malloc(sizeof(atomic_int));
    atomic_int *in_flight = malloc(sizeof(atomic_int));
    struct Args *args = malloc(THREAD_COUNT * sizeof(struct Args));
    struct Thread **lookup = malloc(THREAD_COUNT * sizeof(struct Thread *));
    struct Thread_Data *thread_data = malloc(sizeof(struct Thread_Data));

    if (threads == NULL || done == NULL || in_flight == NULL ||
        args == NULL || lookup == NULL || thread_data == NULL){
        free(threads);
        free(done);
        free(in_flight);
        free(args);
        free(lookup);
        free(thread_data);
        return NULL;
    }

    atomic_init(done, 0);
    atomic_init(in_flight, 0);

    for (int i = 0; i < THREAD_COUNT; i++){
        lookup[i] = malloc(sizeof(struct Thread));
        if (lookup[i] == NULL){
            for (int j = 0; j < i; j++) free(lookup[j]);
            free(threads);
            free(done);
            free(in_flight);
            free(args);
            free(lookup);
            free(thread_data);
            return NULL;
        }

        atomic_init(&lookup[i]->active, 0);
    }

    for (int i = 0; i < THREAD_COUNT; i++){
        struct Ring_Queue *queue = malloc(sizeof(struct Ring_Queue));
        if (queue == NULL){
            for (int j = 0; j < THREAD_COUNT; j++) free(lookup[j]);
            free(threads);
            free(done);
            free(in_flight);
            free(args);
            free(lookup);
            free(thread_data);
            return NULL;
        }

        if (ring_queue_init(queue) != 0){
            free(queue);
            for (int j = 0; j < THREAD_COUNT; j++) free(lookup[j]);
            free(threads);
            free(done);
            free(in_flight);
            free(args);
            free(lookup);
            free(thread_data);
            return NULL;
        }

        args[i] = (struct Args){
            .id = i,
            .queue = queue,
            .lookup = lookup,
            .args = args,
            .done = done,
            .job_queue = job_queue,
            .in_flight = in_flight
        };
    }

    for (int i = 0; i < THREAD_COUNT; i++){
        if (pthread_create(&threads[i], NULL, run, &args[i]) != 0){
            atomic_store(done, 1);
            for (int j = 0; j < i; j++) pthread_join(threads[j], NULL);
            for (int j = 0; j < THREAD_COUNT; j++){
                pthread_mutex_destroy(&args[j].queue->lock);
                free(args[j].queue);
                free(lookup[j]);
            }
            free(threads);
            free(done);
            free(in_flight);
            free(args);
            free(lookup);
            free(thread_data);
            return NULL;
        }
    }

    thread_data->threads = threads;
    thread_data->args = args;
    thread_data->lookup = lookup;
    thread_data->done = done;
    thread_data->last_index = 0;
    thread_data->job_queue = job_queue;

    return thread_data;
}

void destroy_pool(struct Thread_Data *thread_data){
    if (thread_data == NULL) return;

    atomic_store(thread_data->done, 1);

    for (int i = 0; i < THREAD_COUNT; i++){
        pthread_join(thread_data->threads[i], NULL);
    }

    for (int i = 0; i < THREAD_COUNT; i++){
        struct Node *node;

        while ((node = pop(&thread_data->args[i])) != NULL){
            free(node->job->stack->data);
            free(node->job->stack);
            free(node->job);
            free(node);
        }

        pthread_mutex_destroy(&thread_data->args[i].queue->lock);
        free(thread_data->args[i].queue);
    }

    for (int i = 0; i < THREAD_COUNT; i++) free(thread_data->lookup[i]);

    free(thread_data->lookup);
    free(thread_data->done);
    free(thread_data->args->in_flight);
    free(thread_data->args);
    free(thread_data->threads);
    free(thread_data);
}
