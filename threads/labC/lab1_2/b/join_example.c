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
    printf("[worker] поток %d запущен (10 сек sleep())\n", id);
    sleep(10);
    void *ret = (void*)(long)42;
    // в proc/PID/maps не будет такой области в которой может лежать этот адрес. 
    // как понимаю значение будет просто встраиваться в .text секцию
    printf("[worker] ret address: %p\n", ret);
    sleep(10);
    printf("[worker] поток %d завершил работу\n", id);
    return ret;
}

int main() {
    pthread_t tid;
    int arg = 1;
    int err;
    void* retval;

    printf("[main] создание потока\n");
    printf("[main] cat /proc/%d/maps\n\n", getpid());
    err = pthread_create(&tid, NULL, worker, &arg);
    if (err != 0) {
        printf("main: pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    // жду завершения потока и получаем возвращённое значение
    err = pthread_join(tid, &retval); // 2 параметр - указатель на переменную в которую записывается возвращённое значение worker потока
    if (err != 0) {
        printf("main: pthread_join() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }

    int result = (int)(long)retval;
    printf("[main] поток worker вернул значение %d\n", result);
    return EXIT_SUCCESS;
}
