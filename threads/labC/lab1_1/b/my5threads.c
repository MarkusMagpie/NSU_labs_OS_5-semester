#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>

#define NUM_THREADS 5

void *mythread(void *arg) {
    int id = *(int *)arg;
    printf("mythread %d [%d %d %d]: Hello from mythread!\n", id, getpid(), getppid(), gettid());
    return NULL;
}

int main() {
    pthread_t tids[NUM_THREADS]; // идентификаторы потоков 
    int thread_ids[NUM_THREADS];
    int err;

    printf("main [%d %d %d]: Hello from main!\n", getpid(), getppid(), gettid());

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;

        err = pthread_create(&tids[i], NULL, mythread, &thread_ids[i]);
        if (err) {
            printf("main: pthread_create() failed: %s\n", strerror(err));
            return EXIT_FAILURE;
        }
    }

    // ожидаем завершения потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    printf("все %d потоков завершили работу\n", NUM_THREADS);
    return EXIT_SUCCESS;
}
