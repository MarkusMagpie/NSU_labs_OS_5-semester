#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* worker(void* arg) {
    int id = *(int*)arg;
    printf("[worker] поток %d запущен\n", id);
    printf("[worker] поток %d завершил работу\n", id);
    return NULL;
}

int main() {
    pthread_t tid;
    int arg = 1;
    int err;

    printf("[main] создание потока\n");
    err = pthread_create(&tid, NULL, worker, &arg);
    if (err != 0) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // ждём завершения потока
    err = pthread_join(tid, NULL);
    if (err != 0) {
        printf("main: pthread_join() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    printf("[main] главный поток вышел из ожидания потока worker и продолжил работу, так как тот завершил работу\n");
    return EXIT_SUCCESS;
}
