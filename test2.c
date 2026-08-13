#include "async.h"
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include <time.h>

#define TASK_COUNT 25000
#define TEST_TIMEOUT_SECONDS 30.0
#define STATE_MAGIC UINT64_C(0x7f4a7c159e3779b9)

struct Stress_State {
    uint64_t magic;
    uint64_t value;
    uint32_t step;
    uint32_t calls;
};

static _Atomic int completed_tasks;
static _Atomic int failed_tasks;
static _Atomic int callback_calls;

static double monotonic_seconds(void){
    struct timespec value;
    clock_gettime(CLOCK_MONOTONIC, &value);
    return (double)value.tv_sec + (double)value.tv_nsec / 1000000000.0;
}

static uint64_t mix_value(
    uint64_t value,
    int id,
    uint32_t step,
    uint32_t rounds,
    uint32_t salt
){
    for (uint32_t i = 0; i < rounds; i++){
        value ^= (uint64_t)(id + 1) * UINT64_C(0x9e3779b1);
        value += (uint64_t)(step + i + salt) * UINT64_C(0x100000001b3);
        value = (value << 13) | (value >> 51);
        value *= UINT64_C(0xff51afd7ed558ccd);
    }

    return value;
}

static uint64_t expected_value(
    int id,
    uint32_t steps,
    uint32_t rounds,
    uint32_t salt
){
    uint64_t value = (uint64_t)(id + 1) * UINT64_C(0x123456789abcdef);

    for (uint32_t step = 0; step < steps; step++){
        value = mix_value(value, id, step, rounds, salt);
    }

    return value;
}

static struct Stress_State *get_state(struct Job *self){
    struct Stress_State *state = (struct Stress_State *)self->stack->data;

    if (self->state == 0){
        state->magic = STATE_MAGIC ^ (uint64_t)(self->id + 1);
        state->value =
            (uint64_t)(self->id + 1) * UINT64_C(0x123456789abcdef);
        state->step = 0;
        state->calls = 0;
        self->state = 1;
    }

    if (state->magic != (STATE_MAGIC ^ (uint64_t)(self->id + 1))){
        atomic_fetch_add(&failed_tasks, 1);
    }

    atomic_fetch_add(&callback_calls, 1);
    return state;
}

static void finish_task(
    struct Job *self,
    struct Stress_State *state,
    uint32_t steps,
    uint32_t rounds,
    uint32_t salt
){
    uint64_t expected = expected_value(self->id, steps, rounds, salt);

    if (state->step != steps ||
        state->calls != steps ||
        state->value != expected){
        atomic_fetch_add(&failed_tasks, 1);
    }

    atomic_fetch_add(&completed_tasks, 1);
    self->status = Completed;
    self->state = 2;
}

static void run_step(
    struct Job *self,
    uint32_t steps,
    uint32_t rounds,
    uint32_t salt
){
    struct Stress_State *state = get_state(self);

    if (state->step >= steps){
        atomic_fetch_add(&failed_tasks, 1);
        self->status = Completed;
        return;
    }

    state->value = mix_value(
        state->value,
        self->id,
        state->step,
        rounds,
        salt
    );
    state->step++;
    state->calls++;

    if (state->step == steps){
        finish_task(self, state, steps, rounds, salt);
    } else {
        self->status = Pending;
    }
}

static void stress_task_a(struct Job *self){
    run_step(self, 8, 64, 11);
}

static void stress_task_b(struct Job *self){
    run_step(self, 5, 128, 23);
}

static void stress_task_c(struct Job *self){
    run_step(self, 12, 32, 37);
}

static void stress_task_d(struct Job *self){
    run_step(self, 7, 96, 53);
}

static void drain_job_queue(struct Queue *job_queue){
    struct Job job;

    while (queue_pop(job_queue, &job) == 0){
        free(job.stack->data);
        free(job.stack);
    }
}

static void add_test_job(
    struct Thread_Data *thread_data,
    struct Queue *job_queue,
    int id
){
    switch (id % 4){
        case 0:
            async_task_add(thread_data, job_queue, stress_task_a, id);
            break;
        case 1:
            async_task_add(thread_data, job_queue, stress_task_b, id);
            break;
        case 2:
            async_task_add(thread_data, job_queue, stress_task_c, id);
            break;
        default:
            async_task_add(thread_data, job_queue, stress_task_d, id);
            break;
    }
}

int main(void){
    struct Queue *job_queue = queue_init(sizeof(struct Job));
    if (job_queue == NULL){
        fprintf(stderr, "queue_init failed\n");
        return 1;
    }

    struct Thread_Data *thread_data = pool_init(job_queue);
    if (thread_data == NULL){
        fprintf(stderr, "pool_init failed\n");
        queue_destroy(job_queue);
        return 1;
    }

    for (int id = 0; id < TASK_COUNT; id++){
        add_test_job(thread_data, job_queue, id);
    }

    double start = monotonic_seconds();
    int timed_out = 0;

    while (!queue_is_empty(job_queue) ||
           atomic_load(thread_data->args->in_flight) != 0){
        if (!queue_is_empty(job_queue)){
            async_task_poll(thread_data, job_queue);
        } else {
            sched_yield();
        }

        if (monotonic_seconds() - start > TEST_TIMEOUT_SECONDS){
            timed_out = 1;
            break;
        }
    }

    int completed = atomic_load(&completed_tasks);
    int failures = atomic_load(&failed_tasks);
    int callbacks = atomic_load(&callback_calls);

    destroy_pool(thread_data);
    drain_job_queue(job_queue);
    queue_destroy(job_queue);

    int expected_callbacks = 0;
    for (int id = 0; id < TASK_COUNT; id++){
        switch (id % 4){
            case 0:
                expected_callbacks += 8;
                break;
            case 1:
                expected_callbacks += 5;
                break;
            case 2:
                expected_callbacks += 12;
                break;
            default:
                expected_callbacks += 7;
                break;
        }
    }

    if (timed_out){
        fprintf(stderr, "stress test timed out after %.1f seconds\n",
                TEST_TIMEOUT_SECONDS);
        return 1;
    }

    if (completed != TASK_COUNT ||
        callbacks != expected_callbacks ||
        failures != 0){
        fprintf(
            stderr,
            "stress test failed: completed=%d expected=%d callbacks=%d expected_callbacks=%d failures=%d\n",
            completed,
            TASK_COUNT,
            callbacks,
            expected_callbacks,
            failures
        );
        return 1;
    }

    printf(
        "stress test passed: tasks=%d callbacks=%d elapsed=%.3f seconds\n",
        completed,
        callbacks,
        monotonic_seconds() - start
    );
    return 0;
}
