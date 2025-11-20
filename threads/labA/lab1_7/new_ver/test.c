#include "uthread.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define N 10

void *func(void *arg) {
    long thread_id = (long)arg;

    for (int i = 0; i < 3; ++i) {
        printf("[thread ID:%ld] - iteration: %d\n", thread_id, i);
        uthread_yield();
    }

    return (void *)(123 + thread_id);
}

int main(void) {
    uthread_t tid[N];
    void *retval; // для join

    printf("[main] starting, creating %d uthreads\n", N);

    // Создаем обычные потоки
    for (int i = 0; i < N; ++i) {
        if (uthread_create(&tid[i], func, (void *)(long)i) != 0) {
            printf("uthread_create() failed\n");
            return EXIT_FAILURE;
        }
        printf("[main] created thread with ID:%d\n", tid[i]->id);
    }

    printf("\n[main] joining all threads\n");

    // Присоединяемся ко всем потокам
    for (int i = 0; i < N; ++i) {
        if (tid[i] != NULL) { // поток еще существует
            printf("[main] joining thread ID:%d\n", tid[i]->id);
            if (uthread_join(tid[i], &retval) != 0) {
                printf("uthread_join() failed for ID:%d\n", tid[i]->id);
            } else {
                if (retval == NULL) {
                    printf("[main] thread ID:%d was cancelled\n", tid[i]->id);
                } else {
                    printf("[main] thread ID:%d returned: %ld\n", tid[i]->id, (long)retval);
                }
            }
        }
    }

    printf("\n[main] all threads completed successfully!!!\n");
    
    return EXIT_SUCCESS;
}