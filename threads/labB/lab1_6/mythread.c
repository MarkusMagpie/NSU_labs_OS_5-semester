#include "mythread.h"

#define MYTHREAD_STACK_SIZE (1024 * 1024)

static int thread_startup(void *arg) {
    mythread_struct_t *ts = (mythread_struct_t *)arg;

    ts->tid = gettid();
    
    printf("[thread TID:%d] started...\n", ts->tid);

    // если поток не был отменен, то запускаю start_routine
    if (!ts->canceled) {
        printf("[thread TID:%d] calling start_routine...\n", ts->tid);
        ts->retval = ts->start_routine(ts->arg);
        printf("[thread TID:%d] start_routine completed with return value: %ld\n", ts->tid, (long)ts->retval);
    } else {
        printf("[thread TID:%d] cancelled\n", ts->tid);
    }

    ts->finished = 1;
    printf("[thread TID:%d] finished, waiting for join...\n", ts->tid);
    
    // жиду пока кто-нибудь вызовет join
    while (!ts->joined) {
        sleep(1);
    }

    return 0;
}

// int pthread_join(pthread_t thread, void **value_ptr);
int mythread_join(mythread_t thread, void **retval) {
    // храню указатель на структуру по TID
    // thread - адрес структуры
    mythread_struct_t *ts = thread;

    // жду завершения потока от thread_startup()
    while (!ts->finished) {
        sleep(1);
    }

    // установил значение, которое возвращает start_routine, в retval
    if (retval) *retval = ts->retval;
    ts->joined = 1;

    printf("[mythread_join] thread TID:%d joined\n", ts->tid);

    // освобождение стека и структуры ts
    // if (ts->stack_base) {
    //     munmap(ts->stack_base, MYTHREAD_STACK_SIZE);
    //     ts->stack_base = NULL;
    // }
    // free(ts);

    return 0;
}

// int pthread_cancel(pthread_t thread);
int mythread_cancel(mythread_t thread) {
    mythread_struct_t *ts = thread;
    ts->canceled = 1;

    printf("[mythread_cancel] thread TID:%d cancelled\n", ts->tid);

    return 0;
}

// int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start_routine)(void*), void *arg);
int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg) {
    mythread_struct_t *ts = malloc(sizeof(*ts));
    if (!ts) { 
        printf("malloc() failed\n");
        return -1; 
    }
    memset(ts, 0, sizeof(*ts));
    ts->start_routine = start_routine;
    ts->arg = arg;

    void *stack = mmap(NULL, MYTHREAD_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        free(ts);
        printf("mmap() failed\n");
        return -1;
    }

    ts->stack_base = stack;
    void *stack_top = (char*)stack + MYTHREAD_STACK_SIZE;
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_IO;
    
    pid_t tid = clone(thread_startup, stack_top, flags, ts);
    if (tid == -1) {
        munmap(stack, MYTHREAD_STACK_SIZE);
        free(ts);
        printf("clone() failed\n");
        return -1;
    }

    *thread = ts;
    return 0;
}
