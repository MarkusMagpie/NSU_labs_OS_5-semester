#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>

void* func(void* arg) {
    /* так как func() - detached поток, то переданный ему аргумент arg, лежащий на стеке потока main(), 
    может быть стерт вместе со стеком потока main(), а значит буду выводить мусор*/ 
    printf("[func] number = %d, message = %s\n", ((int*)arg)[0], ((char**)arg)[1]);
    return NULL;
}

int main() {
    pthread_t tid;

    int number = 1;
    char message[] = "hello everynyan";
    void* arg[2] = { &number, message }; // локальная переменная arg, лежит на стеке потока main()

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    printf("[main] создание потоков в detached состоянии\n");

    // передаю адрес arg detached потоку func()
    int err = pthread_create(&tid, &attr, func, arg);
    if (err) {
        printf("[main] pthread_create() failed: %s\n", strerror(err));
        return EXIT_FAILURE;
    }
    
    pthread_attr_destroy(&attr);

    sleep(2);

    printf("[main] все потоки завершили работу\n");
    return EXIT_SUCCESS;
}