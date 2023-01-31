#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <stdbool.h>
#include <stddef.h>

#if !defined(__STDC_NO_THREADS__) && !defined(__STDC_NO_ATOMICS__)
#define HAS_C11_CONCURRENCY
#include <stdatomic.h>
#include <threads.h>
#endif

#define THREADPOOL_QUEUE_SIZE 256

#ifndef HAS_C11_CONCURRENCY
typedef struct {
    char _;
} mtx_t;
typedef struct {
    char _;
} cnd_t;
typedef size_t atomic_size_t;
typedef unsigned short atomic_ushort;
#endif

typedef bool (*TaskFunc)(void *);

struct Task {
    TaskFunc func;
    void *arg;
};

// Do not use these members directly. Use the threadpool_ functions.
struct ThreadPool {
    mtx_t mutex;  // Guards queue, queue_front, queue_back, task_available
    cnd_t task_available;
    struct Task queue[THREADPOOL_QUEUE_SIZE];
    size_t queue_front;  // Index of the next item to be dequeued, or THREADPOOL_QUEUE_SIZE
    size_t queue_back;   // Index where the next enqueued item should be placed
    atomic_size_t thread_count;
};

// Atomic ushort counter.
// Used for atomic reference counting.
struct AtomicCounter {
    atomic_ushort count;
};

struct ThreadPool *threadpool_init();
void threadpool_enqueue(struct ThreadPool *pool, TaskFunc func, void *arg);
void threadpool_deinit(struct ThreadPool *pool);
struct AtomicCounter *acnt_init(unsigned short val);
bool acnt_dec(struct AtomicCounter *counter);

#endif /* THREADPOOL_H */