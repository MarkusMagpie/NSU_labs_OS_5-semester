#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

// Создайте структуру с полями типа int и char*.
typedef struct {
    int number;
    char *message;
} thread_args;

// В поточной функции распечатайте содержимое структуры.
void* func(void* arg) {
    thread_args *targ = (thread_args*)arg; // привел void* к thread_args* для доступа к полям структуры
    printf("[func] number = %d, message = %s\n", targ->number, targ->message);
    return NULL;
}

int main() {
    pthread_t tid;
    int err;

    // Создайте экземпляр этой структуры и проинициализируйте.
    thread_args targ;
    targ.number = 123;
    targ.message = strdup("hello everynyan");
    if (!targ.message) {
        printf("[main] strdup() failed");
        return EXIT_FAILURE;
    }

    // Создайте поток и передайте указатель на эту структуру в качестве параметра.
    err = pthread_create(&tid, NULL, func, &targ);
    if (err) {
        printf("[main] pthread_create() failed: %s\n", strerror(err));
        free(targ.message);
        return EXIT_FAILURE;
    }

    pthread_join(tid, NULL);
    free(targ.message);

    printf("[main] все потоки завершили работу\n");
    return EXIT_SUCCESS;
}