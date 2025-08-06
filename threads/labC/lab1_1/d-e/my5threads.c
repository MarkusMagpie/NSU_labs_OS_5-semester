#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdlib.h>

#define NUM_THREADS 5

int global_var = 10; // глобальная переменная

void *mythread(void *arg) {
    int id = *(int *)arg;
    pthread_t self = pthread_self();
    pid_t pid = getpid();
    pid_t ppid = getppid();
    pid_t tid = gettid();

    // локальные переменные
    int local_var = 100;

    printf("[mythread %d] before: local_var=%d, global_var=%d\n", id, local_var, global_var);

    local_var += 10;
    global_var += 10;

    printf("[mythread %d]  after: local_var=%d, global_var=%d\n\n",  id, local_var, global_var);

    return NULL;
}

pthread_t tids[NUM_THREADS]; // идентификаторы потоков

int main() {
    int thread_ids[NUM_THREADS];
    int err;

    printf("[main] pid=%d, ppid=%d, pthread_self()=%lu, gettid()=%d\n", getpid(), getppid(), (unsigned long)pthread_self(), gettid());
    printf("[main] cat /proc/%d/maps\n\n", getpid());

    sleep(10);  

    for (int i = 0; i < NUM_THREADS; i++) {
        thread_ids[i] = i + 1;

        err = pthread_create(&tids[i], NULL, mythread, &thread_ids[i]);
        if (err) {
            printf("pthread_create() failed: %s\n", strerror(err));
            return EXIT_FAILURE;
        }
    }

    sleep(10);

    // жду завершения всех пяти потоков
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(tids[i], NULL);
    }

    printf("[main] все 5 потоков завершили работу (pthread_join() пройден для каждого)\n");
    return EXIT_SUCCESS;
}
