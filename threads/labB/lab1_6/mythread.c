#include "mythread.h"

#define MYTHREAD_STACK_SIZE (1024 * 1024)

static int thread_startup(void *arg) {
    mythread_struct_t *ts = (mythread_struct_t *)arg;
    // сохраняем контекст перед запуском start_routine в поле before_start_routine
    if (getcontext(&ts->before_start_routine) == -1) {
        _exit(1);
    }
    // если поток не был отменен, то запускаем start_routine
    if (!ts->canceled) {
        ts->retval = ts->start_routine(ts->arg);
    }
    ts->finished = 1;
    // жидем, пока кто-нибудь вызовет join
    while (!ts->joined) {
        sleep(1);
    }

    return 0;
}

// int pthread_join(pthread_t thread, void **value_ptr);
int mythread_join(mythread_t thread, void **retval) {
    // храним указатель на структуру по TID
    // thread - адрес структуры
    mythread_struct_t *ts = thread;
    // жду завершения потока от thread_startup()
    while (!ts->finished) {
        sleep(1);
    }
    // установил значение, которое возвращает start_routine, в retval
    if (retval) *retval = ts->retval;
    ts->joined = 1;

    return 0;
}

// int pthread_cancel(pthread_t thread);
int mythread_cancel(mythread_t thread) {
    mythread_struct_t *ts = thread;
    ts->canceled = 1;

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

    void *stack = malloc(MYTHREAD_STACK_SIZE);
    if (!stack) {
        free(ts);
        printf("malloc() failed\n");
        return -1;
    }

    void *stack_top = (char*)stack + MYTHREAD_STACK_SIZE;
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_IO;
    pid_t tid = clone(thread_startup, stack_top, flags, ts);
    if (tid == -1) {
        free(stack);
        free(ts);
        printf("clone() failed\n");
        return -1;
    }

    *thread = ts;
    return 0;
}
