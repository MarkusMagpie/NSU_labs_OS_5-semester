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
    thread_args *targ = (thread_args*)arg;
    printf("[func] number = %d, message = %s\n", targ->number, targ->message);
    return NULL;
}

void* jober(void* arg) {
    pthread_attr_t attr;
    pthread_t func_tid;
    int err;

    // инициализируем атрибуты, задаём detached-состояние
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    
    // на стеке jober
    thread_args targ;
    targ.number = 123;
    targ.message = "hello everynyan";

    printf("[main] создание потоков в detached состоянии\n");

    err = pthread_create(&func_tid, &attr, func, (void*)&targ);
    
    // атрибуты больше не нужны
    pthread_attr_destroy(&attr);
    
    if (err) {
        printf("[jober] pthread_create() failed: %s\n", strerror(err));
        return NULL;
    }

    // ВАЖНО: продемонстрируй что убрав комментарий значения number выведутся нормальные
    // sleep(1);

    // func продолжает работать с указателем на уничтоженный стек
    printf("[jober] завершение (его стек будет уничтожен)\n");
    
    return NULL;
}

int main() {
    pthread_t jober_tid;
    
    printf("[main] создание промежуточного потока jober\n");
    int err = pthread_create(&jober_tid, NULL, jober, NULL);
    if (err) {
        printf("[main] pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    
    pthread_join(jober_tid, NULL);
    
    // время func-у попытаться обратиться к освобожденной памяти
    sleep(2);
    
    printf("[main] все потоки завершили работу\n");
    return EXIT_SUCCESS;
}