#include "mythread.h"

void *func(void *arg) {
    printf("[func] [PID: %d, PPID: %d, TID: %d] - hello everynan, I'm from custom thread!\n", getpid(), getppid(), gettid());
    return (void *)123;
}

int main() {
    mythread_t tid;
    void *retval;

    printf("[main] [PID: %d, PPID: %d, TID: %d] - hello from main!\n", getpid(), getppid(), gettid());

    if (mythread_create(&tid, func, NULL) != 0) {
        printf("mythread_create() failed\n");
        return EXIT_FAILURE;
    }

    sleep(2);

    printf("[main] cancelling func thread\n");
    mythread_cancel(tid);
    
    mythread_join(tid, &retval);
    
    // printf("[main] func's return value: %ld\n", (long)retval);
    if (retval == NULL) {
        printf("[main] func thread was cancelled before completion\n");
    } else {
        printf("[main] func thread returned: %ld\n", (long)retval);
    }

    return EXIT_SUCCESS;
}
