#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void cleanup_handler(void *arg) {
    char *str = (char *)arg;
    printf("[func] очистка памяти по адресу: %p\n", str);
    free(str);
}

void *func(void *arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    // НОВОЕ: выделяем память для hello world
    char *msg = malloc(12);
    if (!msg) {
        printf("[func] malloc() failed");
        return NULL;
    }
    strcpy(msg, "hello world");

    // регистрируем обработчик очистки
    pthread_cleanup_push(cleanup_handler, msg);

    // распечатываю в бесконечном цикле полученную строку.
    while (1) {
        printf("[func] %s\n", msg);
        sleep(1); // точка отмены!
    }

    // снимаем обработчик очистки - вызывается при завершении потока
    pthread_cleanup_pop(1);

    return NULL;
}

int main() {
    pthread_t tid;
    int err;

    err = pthread_create(&tid, NULL, func, NULL);
    if (err) {
        printf("[main] pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    sleep(3);

    // запрашиваем отмену потока
    printf("[main] canceling func thread...\n");
    if (pthread_cancel(tid) != 0) {
        printf("[main] pthread_cancel() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // ждём завершения потока и проверяем причину 
    void* retval;
    pthread_join(tid, &retval);
    if (retval == PTHREAD_CANCELED) {
        printf("[main] func thread canceled\n");
    } else {
        printf("[main] func thread terminated normally\n");
    }

    return EXIT_SUCCESS;
}
