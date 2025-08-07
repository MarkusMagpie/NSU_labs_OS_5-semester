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
    pthread_attr_t attr;
    int err;

    // инициализирую атрибуты, задаю detached-состояние
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    printf("[main] создание потоков в detached состоянии\n");

    while (1) {
        pthread_t tid;
        int err = pthread_create(&tid, &attr, func, NULL);
        if (err) {
            printf("[main] pthread_create() failed: %s\n", strerror(err));
            break;
        }
        // usleep(100000);
    }

    // атрбуты больше не нужны
    pthread_attr_destroy(&attr);

    printf("[main] все потоки завершили работу\n");
    return EXIT_FAILURE;
}
