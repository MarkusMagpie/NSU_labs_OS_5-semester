#include "mythread.h"

int N = 10;

void *func(void *arg) {
    long thread_id = (long)arg;
    // printf("[func] [PID: %d, PPID: %d, TID: %d] - hello everynan, I'm from custom thread!\n", getpid(), getppid(), gettid());
    return (void *)(123 + thread_id);
}

int main() {
    mythread_t tid[N];
    int thread_tids[N]; // НОВОЕ: массив TID 
    void *retval;

    printf("[main] [PID: %d, PPID: %d, TID: %d] - hello from main!\n", getpid(), getppid(), gettid());

    for (int i = 0; i < N; i++) {
        // printf("[main] creating thread %d/%d\n", i, N-1);
        if (mythread_create(&tid[i], func, (void*)(long)i) != 0) {
            printf("mythread_create() failed\n");
            return EXIT_FAILURE;
        }
        thread_tids[i] = tid[i]->tid;
        // usleep(1000);
    }

    sleep(3);

    // printf("[main] cancelling func thread\n");
    // mythread_cancel(tid);
    
    // mythread_join(tid, &retval);

    for (int i = 0; i < N; i++) {
        printf("[main] joining thread (TID:%d)\n", thread_tids[i]);
        if (mythread_join(tid[i], &retval) != 0) {
            printf("mythread_join() failed\n");
            return EXIT_FAILURE;
        }
        
        if (retval == NULL) {
            printf("[main] func thread (TID:%d) was cancelled before completion\n", thread_tids[i]);
        } else {
            printf("[main] func thread (TID:%d) returned: %ld\n", thread_tids[i], (long)retval);
        }
    }

    printf("[main] all threads completed successfully!!!\n");

    return EXIT_SUCCESS;
}
