#include "mythread.h"
#include <sys/syscall.h> // для SYS_futex

#define MYTHREAD_STACK_SIZE (1024 * 1024)

// поток при (*addr = ts->tid) == expected засыпает и ждет FUTEX_WAKE; 
// поток проснется при изменении ts->tid (занулится)
int futex_wait(volatile int *addr, int expected) {
/* https://man7.org/linux/man-pages/man2/futex.2.html 
    long syscall(SYS_futex, 
        uint32_t *uaddr,                    адрес futex-переменной (&ts->tid)
        int op,                             операция: FUTEX_WAIT или FUTEX_WAKE
        int val,                            ожидаемое значение
        const struct timespec *timeout,     таймаут
        int *uaddr2,                        второй адрес (для сложных операций)
        int val3);                          доп флаги
*/
    return (int)syscall(SYS_futex, (int *)addr, FUTEX_WAIT, expected, NULL, NULL, 0);
}

static int thread_startup(void *arg) {
    mythread_struct_t *ts = (mythread_struct_t *)arg;
    
    printf("[thread TID:%d] started...\n", ts->tid);

    // если поток не был отменен, то запускаю start_routine
    if (!ts->canceled) {
        printf("[thread TID:%d] calling start_routine...\n", ts->tid);
        ts->retval = ts->start_routine(ts->arg);
        printf("[thread TID:%d] start_routine completed with return value: %ld\n", ts->tid, (long)ts->retval);
    } else {
        printf("[thread TID:%d] cancelled\n", ts->tid);
        ts->retval = NULL;
    }

    // поток завершается -> ядро обнулит tid изза нового флага CLONE_CHILD_CLEARTID

    return 0;
}

// int pthread_join(pthread_t thread, void **value_ptr);
int mythread_join(mythread_t thread, void **retval) {
    // храню указатель на структуру по TID
    // thread - адрес структуры
    mythread_struct_t *ts = thread;

    // жду обнуления tid ядром (CLONE_CHILD_CLEARTID делает futex-wake)
    while (ts->tid != 0) {
        int expected = ts->tid;
        if (expected == 0) break;
        
        // futex для ожидания
        futex_wait(&ts->tid, expected);
    }

    // установил значение, которое возвращает start_routine, в retval
    if (retval) *retval = ts->retval;

    // tid всегда 0 -> нет смысла выводить
    // printf("[mythread_join] thread TID:%d joined\n", ts->tid);

    // освобождение стека и структуры ts
    if (ts->stack_base) {
        munmap(ts->stack_base, MYTHREAD_STACK_SIZE);
        ts->stack_base = NULL;
    }
    free(ts);

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

    // void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
    void *stack = mmap(NULL, MYTHREAD_STACK_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_STACK, -1, 0);
    if (stack == MAP_FAILED) {
        free(ts);
        printf("mmap() failed\n");
        return -1;
    }

    ts->stack_base = stack;
    void *stack_top = (char*)stack + MYTHREAD_STACK_SIZE;
    int flags = CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_IO | 
        CLONE_PARENT_SETTID | // ядро запишет TID в parent_ti 
        CLONE_CHILD_CLEARTID; // ядро обнулит child_tid при завершении потока

    /*
    int clone(typeof(int (void *_Nullable)) *fn, void *stack, int flags, void *_Nullable arg, 
            ... pid_t *_Nullable parent_tid, 
            void *_Nullable tls, 
            pid_t *_Nullable child_tid);
    */
    pid_t tid = clone(thread_startup, stack_top, flags, ts, &ts->tid, NULL, &ts->tid);
    if (tid == -1) {
        munmap(stack, MYTHREAD_STACK_SIZE);
        free(ts);
        printf("clone() failed\n");
        return -1;
    }

    *thread = ts;
    return 0;
}
