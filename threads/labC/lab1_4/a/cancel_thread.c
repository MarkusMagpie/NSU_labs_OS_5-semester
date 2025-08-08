#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* func(void* arg) {
    // разрешаю отмену и устанавливаем отложенный тип (дефолтные значения)
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    while (1) {
        printf("[func] thread printing...\n");
        // printf, sleep это точки отмены - здесь поток может быть отменён!
        sleep(1);
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

    // func работает несколько секунд
    sleep(3);

    // запрашиваю отмену потока
    printf("[main] canceling func thread...\n");
    if (pthread_cancel(tid) != 0) {
        printf("[main] pthread_cancel() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // жду завершения потока и проверяю причину 
    void* retval;
    pthread_join(tid, &retval);
    if (retval == PTHREAD_CANCELED) {
        printf("[main] func thread canceled\n");
    } else {
        printf("[main] func thread terminated normally\n");
    }

    return EXIT_SUCCESS;
}
