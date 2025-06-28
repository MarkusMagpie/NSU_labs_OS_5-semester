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
    return (void*)(long)42;
}

int main() {
    pthread_t tid;
    int arg = 1;
    int err;
    void* retval;

    printf("[main] создание потока\n");
    err = pthread_create(&tid, NULL, worker, &arg);
    if (err != 0) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // ждём завершения потока и получаем возвращённое значение
    err = pthread_join(tid, &retval); // 2 параметр - указатель на переменную в которую записывается возвращённое значение worker потока
    if (err != 0) {
        printf("main: pthread_join() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    int result = (int)(long)retval;
    printf("[main] поток worker вернул значение %d\n", result);
    return EXIT_SUCCESS;
}
