# C Async Runtime

A small asynchronous task runtime for C built around cooperative state-machine scheduling and a multithreaded worker pool.

## Overview

The runtime executes user-defined tasks as resumable jobs.

A task is represented by a `Job` containing:

- a task identifier
- execution status
- resumable state
- task-local storage
- a function pointer representing the task

Tasks are not required to complete in a single invocation. A task can update its state and return with `Pending`, allowing the runtime to schedule it again later.

This provides a C-level stackless coroutine model without requiring compiler-generated coroutines or a full context switch.

## Architecture

```text
                    ┌─────────────────────┐
                    │     User Tasks      │
                    │  function(Job *)    │
                    └──────────┬──────────┘
                               │
                               ▼
                    ┌─────────────────────┐
                    │     Job Queue       │
                    │  pending tasks      │
                    └──────────┬──────────┘
                               │
                     async_task_poll()
                               │
                               ▼
                    ┌─────────────────────┐
                    │    Worker Pool      │
                    │                     │
                    │  Worker 0           │
                    │  Worker 1           │
                    │  Worker 2           │
                    │  ...                │
                    └──────────┬──────────┘
                               │
                         execute one step
                               │
                    ┌──────────┴──────────┐
                    │                     │
                 Pending               Completed
                    │                     │
                    ▼                     ▼
              resubmit task          release task
```

Each worker owns a worker queue. When a worker has no local work, it can attempt to steal work from another worker.

## Task Execution Model

Tasks are explicitly resumable state machines.

A typical task looks like:

```c
void task(struct Job *self) {
    switch (self->state) {
        case 0:
            /* initialization */
            self->state = 1;
            self->status = Pending;
            break;

        case 1:
            /* next stage */
            self->state = 2;
            self->status = Pending;
            break;

        case 2:
            /* completion */
            self->status = Completed;
            break;
    }
}
```

The runtime invokes the function repeatedly until the task reports `Completed`.

Task-local data that must survive between invocations is stored separately from the transient C stack. The current implementation provides a heap-backed byte buffer for this purpose.

This allows constructs such as loops to be transformed into resumable tasks by explicitly storing the iterator:

```c
int iter = 0;

if (self->state != 0) {
    memcpy(&iter, self->stack->data, sizeof(int));
}

for (int i = iter; i < 7; i++) {
    /* work */

    if (should_yield(i)) {
        iter = i + 1;
        memcpy(self->stack->data, &iter, sizeof(int));
        self->status = Pending;
        return;
    }
}

self->status = Completed;
```

The important property is that task state survives independently of the invocation's normal stack frame.

## Worker Pool

The runtime separates task submission from task execution.

`async_task_add()` creates a job and places it in the runtime's pending-job queue.

`async_task_poll()` removes pending work and submits it to the worker pool.

Workers then:

1. obtain a job from their local queue
2. execute one task step
3. resubmit the job if it remains pending
4. release its task storage when the job completes

Workers can also steal work from other worker queues when they become idle.

## Synchronization

The runtime contains several distinct synchronization domains.

### Worker queues

Worker queues are protected with mutexes. This keeps queue operations and queue state updates atomic as a single operation and avoids coupling the queue implementation to lock-free memory reclamation.

### Runtime state

Atomic counters are used where state is shared between the main thread and workers, including runtime shutdown and in-flight task tracking.

The distinction is intentional:

- mutexes protect compound data structures
- atomics protect small pieces of shared state

## Memory Ownership

A `Job` owns its task-local `Stack`, and the stack owns its backing byte buffer.

The lifetime is therefore:

```text
Job
 └── Stack
      └── data[4096]
```

When a task completes, the runtime releases:

1. the task-local data buffer
2. the `Stack`
3. the `Job`
4. the worker-queue node

The runtime also handles partially failed allocations during task and pool initialization.

## Work Stealing

Jobs are distributed across worker-local queues rather than maintaining a single execution queue for all workers.

When a worker has no local work, it selects another worker and attempts to steal a job.

This allows work to continue when task distribution is uneven while avoiding unnecessary central scheduling contention.

## Shutdown

The worker pool maintains explicit runtime state rather than treating an empty submission queue as proof that execution is finished.

A runtime can have:

```text
pending jobs = 0
in-flight jobs > 0
```

because a worker may currently be executing a task.

Shutdown therefore waits for the relevant in-flight work before destroying the worker pool and its associated resources.

## API

The runtime is split into separate components.

### `async.h`

Public task/runtime interface and task-related structures.

### `async.c`

Task creation, submission, polling, and runtime-facing scheduling logic.

### `pool.h`

Worker-pool interface and worker-facing structures.

### `pool.c`

Worker management, local queues, work stealing, task execution, and pool shutdown.

### `queue.h`

Generic pending-job queue interface.

### `queue.c`

Generic queue implementation, including dynamic growth and FIFO operations.

## Stress Testing

The runtime includes a stress test which creates **25,000 tasks** with multiple task shapes.

Tasks perform multiple resumable steps and CPU work per step while validating their persistent state.

The stress test checks:

- task completion count
- callback invocation count
- task-local state integrity
- per-task final values
- state progression across yields
- corruption detection using a per-task magic value
- timeout/failure detection

The test therefore exercises both scheduler correctness and preservation of task-local state across repeated scheduling.

## Design Goals

The runtime is intentionally small and focused on the mechanics required for asynchronous execution in C:

- explicit resumable task state
- heap-backed task-local storage
- multithreaded execution
- worker-local queues
- work stealing
- explicit task ownership
- bounded task-step execution
- synchronization around shared runtime state
- deterministic shutdown
- minimal public API surface

It is intended as a runtime component that can be embedded into larger C programs rather than as a complete application framework.

## Building

A typical build requires:

- a C compiler with C11 atomics support
- POSIX threads
- pthreads

For example:

```sh
cc -std=c11 -O2 -Wall -Wextra -pthread \
    async.c pool.c queue.c test.c \
    -o test
```

Run:

```sh
./test
```

For debugging:

```sh
cc -std=c11 -g -O0 -Wall -Wextra -pthread \
    async.c pool.c queue.c test.c \
    -o test
```

## Status

The runtime currently implements cooperative, stackless task execution using explicit state machines and persistent task-local storage, backed by a multithreaded worker pool with work stealing.
