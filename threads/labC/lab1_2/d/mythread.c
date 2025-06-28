#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* func(void* arg) {
    pthread_t tid = pthread_self();
    printf("[func] pthread_self() = %lu\n", (unsigned long)tid);
    return NULL;
}

int main() {
    printf("[main] создание потоков\n");

    while (1) {
        pthread_t tid;
        int err = pthread_create(&tid, NULL, func, NULL);
        if (err) {
            printf("[main] pthread_create() failed: %s\n", strerror(err));
            break;
        }
        // usleep(100000);
    }

    printf("[main] все потоки завершили работу\n");
    return EXIT_FAILURE;
}
