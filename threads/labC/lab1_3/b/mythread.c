#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

typedef struct {
    int number;
    char *message;
} thread_args;

void* func(void* arg) {
    thread_args *targ = (thread_args*)arg; // привел void* к thread_args* для доступа к полям структуры
    printf("[func] number = %d, message = %s\n", targ->number, targ->message);
    free(targ->message);
    free(targ);
    return NULL;
}

int main() {
    pthread_attr_t attr;
    pthread_t tid;
    int err;

    // инициализируем атрибуты, задаём detached-состояние
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    printf("[main] создание потоков в detached состоянии\n");

    // thread_args targ;
    // targ.number = 123;
    // targ.message = strdup("hello everynyan");

    thread_args *targ = malloc(sizeof(thread_args));
    if (!targ) {
        printf("[main] malloc failed");
        return EXIT_FAILURE;
    }

    targ->number = 456;
    targ->message = strdup("hello detached everynyan");
    if (!targ->message) {
        printf("[main] strdup() failed");
        free(targ);
        return EXIT_FAILURE;
    }

    err = pthread_create(&tid, &attr, func, targ);
    if (err) {
        printf("[main] pthread_create() failed: %s\n", strerror(err));
        free(targ->message);
        free(targ);
        return EXIT_FAILURE;
    }

    // атрибуты больше не нужны
    pthread_attr_destroy(&attr);

    sleep(1); // НОВОЕ - надо немного подождать чтобы detached поток выполнился перед завершением основного

    printf("[main] все потоки завершили работу\n");
    return EXIT_SUCCESS;
}