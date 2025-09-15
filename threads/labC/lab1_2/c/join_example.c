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
    char *msg = "hello world";
    // Segmentation fault (core dumped) подтверждает что строка находится в памяти только для чтения
    // msg[0] = 'H';
    printf("[worker] msg string adress: %p\n", msg);
    sleep(10);
    printf("[worker] поток %d завершил работу\n", id);
    return (void*)msg;
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

    char *result = (char*)retval;
    printf("[main] поток worker вернул строку: '%s'\n", result);
    return EXIT_SUCCESS;
}
