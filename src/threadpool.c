#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <threadpool.h>
#include <util.h>

#if defined(__unix__) || (defined(__APPLE__) && defined(__MACH__))
#include <unistd.h>
#endif

// Returns the number of logical cores on systems supporting sysconf(_SC_NPROCESSORS_ONLN), or 1 otherwise.
static unsigned long nproc() {
    char *count = getenv("CHESS_NPROC");
    if (count != NULL) {
        return MAX(atol(count), 1);
    } else {
#ifdef _SC_NPROCESSORS_ONLN
        long onl = sysconf(_SC_NPROCESSORS_ONLN);
        return MAX(1, onl);
#endif
        return 1;
    }
}

#ifdef HAS_C11_CONCURRENCY
static void check_err(int ret) {
    if (ret != thrd_success) {
        puts("threadpool c11threads error");
        exit(EXIT_FAILURE);
    }
}
#endif

// Enqueues a task. Returns false if the queue is full.
// Thread safe
#ifdef HAS_C11_CONCURRENCY
static bool enqueue(struct ThreadPool *pool, struct Task task) {
    check_err(mtx_lock(&pool->mutex));

    if (pool->queue_back == pool->queue_front) {
        check_err(mtx_unlock(&pool->mutex));
        return false;
    }

    size_t inserted_at = pool->queue_back;
    pool->queue[pool->queue_back] = task;
    pool->queue_back = (pool->queue_back + 1) % THREADPOOL_QUEUE_SIZE;

    if (pool->queue_front == THREADPOOL_QUEUE_SIZE) {
        pool->queue_front = inserted_at;
    }

    check_err(mtx_unlock(&pool->mutex));
    return true;
}
#endif

// Dequeues a task. Returns false if the queue is empty.
// Caller must lock the mutex.
#ifdef HAS_C11_CONCURRENCY
static bool dequeue_already_locked(struct ThreadPool *pool, struct Task *task) {
    if (pool->queue_back == pool->queue_front || pool->queue_front == THREADPOOL_QUEUE_SIZE) {
        return false;
    }

    *task = pool->queue[pool->queue_front];
    pool->queue_front = (pool->queue_front + 1) % THREADPOOL_QUEUE_SIZE;

    if (pool->queue_back == pool->queue_front) {
        pool->queue_front = THREADPOOL_QUEUE_SIZE;
    }

    return true;
}
#endif

#ifdef HAS_C11_CONCURRENCY
static int thread_start_func(struct ThreadPool *pool) {
    for (;;) {
        check_err(mtx_lock(&pool->mutex));

        struct Task task;
        if (dequeue_already_locked(pool, &task)) {
            goto got_task;
        } else {
            check_err(cnd_wait(&pool->task_available, &pool->mutex));
        }

        if (!dequeue_already_locked(pool, &task)) {
            // Should never happen
            check_err(mtx_unlock(&pool->mutex));
            break;
        }

    got_task:
        check_err(mtx_unlock(&pool->mutex));

        // Return value of false is a stop request.
        if (!task.func(task.arg)) {
            break;
        }
    }

    atomic_fetch_sub(&pool->thread_count, 1);

    return 0;
}
#endif

static bool task_stop(void *_arg) {
    (void)_arg;
    return false;
}

// Terminates all threads in the pool
// Must be called on the thread which created the pool
#ifdef HAS_C11_CONCURRENCY
static void threadpool_stop(struct ThreadPool *pool) {
    struct Task stop = {.func = task_stop, .arg = NULL};

    while (atomic_load(&pool->thread_count) > 0) {
        while (!enqueue(pool, stop)) {
        }
        check_err(cnd_signal(&pool->task_available));
    }
}
#endif

// Creates a new ThreadPool.
// Must be deallocated with threadpool_deinit
struct ThreadPool *threadpool_init() {
    struct ThreadPool *pool = malloc(sizeof(*pool));

    pool->queue_front = THREADPOOL_QUEUE_SIZE;
    pool->queue_back = 0;

#ifdef HAS_C11_CONCURRENCY
    check_err(mtx_init(&pool->mutex, mtx_plain));
    check_err(cnd_init(&pool->task_available));

    size_t thread_count = nproc();
    atomic_init(&pool->thread_count, thread_count);

    printf("[threadpool] Starting %zu threads. To change set CHESS_NPROC environment variable.\n", thread_count);

    for (size_t i = 0; i < thread_count; i++) {
        thrd_t thread;
        check_err(thrd_create(&thread, (thrd_start_t)thread_start_func, pool));
        check_err(thrd_detach(thread));
    }
#else
    printf("[threadpool] Compiled without C11 threads support. Thread pool will not be used.\n");
    pool->thread_count = 0;
#endif

    return pool;
}

// Enqueues a task on the thread pool.
// If threading is unsupported or the queue is full
// the task is immediately executed.
// Thread safe.
void threadpool_enqueue(struct ThreadPool *pool, TaskFunc func, void *arg) {
#ifdef HAS_C11_CONCURRENCY
    struct Task task = {.func = func, .arg = arg};

    if (atomic_load(&pool->thread_count) == 0 || !enqueue(pool, task)) {
        func(arg);
    } else {
        check_err(cnd_signal(&pool->task_available));
    }
#else
    func(arg);
#endif
}

// Deallocates the pool and terminates all threads.
// Must be called on the thread which created the pool
void threadpool_deinit(struct ThreadPool *pool) {
#ifdef HAS_C11_CONCURRENCY
    threadpool_stop(pool);
    mtx_destroy(&pool->mutex);
    cnd_destroy(&pool->task_available);
#endif

    free(pool);
}

// Returns a new atomic counter with value `val`.
// The counter must be freed.
struct AtomicCounter *acnt_init(unsigned short val) {
    struct AtomicCounter *c = malloc(sizeof(*c));

#ifdef HAS_C11_CONCURRENCY
    atomic_init(&c->count, val);
#else
    c->count = val;
#endif

    return c;
}

// Decrements an atomic counter.
// Returns true if the value is now zero.
bool acnt_dec(struct AtomicCounter *counter) {
#ifdef HAS_C11_CONCURRENCY
    return atomic_fetch_sub(&counter->count, 1) == 1;
#else
    return counter->count-- == 1;
#endif
}