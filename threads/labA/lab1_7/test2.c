#include "uthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N 100
#define MAX_ITERATIONS 5



// void *func(void *arg) {
//     long thread_id = (long)arg;

//     for (int i = 0; i < 3; ++i) {
//         printf("[thread %ld] - iteration: %d\n", thread_id, i);
//         uthread_yield();
//     }

//     return (void *)(100 + thread_id);
// }

void *func2(void *arg) {
    long thread_id = (long)arg;
    
    // случайное количество итераций для каждого потока
    int iterations = (rand() % MAX_ITERATIONS) + 1;
    
    printf("[thread %ld] started, will do %d iterations\n", thread_id, iterations);
    
    for (int i = 0; i < iterations; i++) {
        printf("[thread %ld] iteration %d/%d\n", thread_id, i + 1, iterations);
        uthread_yield();
        usleep(100000 * (thread_id % 3 + 1)); // 100-300 ms
    }

    printf("[thread %ld] completed\n", thread_id);

    return (void *)(100 + thread_id);
}

int main(void) {
    uthread_t tid[N];
    void *retval; // для join

    printf("[main] starting, creating %d uthreads\n", N);

    for (int i = 0; i < N; ++i) {
        if (uthread_create(&tid[i], func2, (void *)(long)i) != 0) {
            printf("uthread_create() failed\n");
            return EXIT_FAILURE;
        }
    }

    // когда все потоки завершатся то управление вернётся в main и uthread_run() завершится
    // uthread_run();

    // просто читаю результат из глобального массива
    // for (int i = 0; i < N; ++i) {
    //     printf("[main] joining thread (UTID:%d)\n", thread_ids[i]);
    //     if (results[i] == NULL) {
    //         printf("[main] func thread (UTID:%d) was cancelled or returned NULL\n", thread_ids[i]);
    //     } else {
    //         printf("[main] func thread (UTID:%d) returned: %ld\n", thread_ids[i], (long)results[i]);
    //     }
    // }

    // с join:
    for (int i = 0; i < N; ++i) {
        printf("[main] joining thread (UTID:%d)\n", tid[i]);
        if (uthread_join(tid[i], &retval) != 0) {
            printf("uthread_join() failed for %d\n", tid[i]);
            return EXIT_FAILURE;
        }
        if (retval == NULL) {
            printf("[main] func thread (UTID:%d) returned NULL\n", tid[i]);
        } else {
            printf("[main] func thread (UTID:%d) returned: %ld\n", tid[i], (long)retval);
        }
    }

    printf("[main] all threads completed successfully!!!\n");
    return EXIT_SUCCESS;
}
