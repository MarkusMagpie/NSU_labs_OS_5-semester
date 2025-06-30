#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* func(void* arg) {
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    // 1 способ завершить поток
    // pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);

    int counter = 0;
    while (1) {
        ++counter;
        // 2 способ завершить поток
        // pthread_testcancel();
    }

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
