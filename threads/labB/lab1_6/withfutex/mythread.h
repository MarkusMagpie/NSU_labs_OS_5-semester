#ifndef MYTHREAD_H
#define MYTHREAD_H

#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <ucontext.h>
#include <sys/mman.h>

// НОВОЕ
#ifndef FUTEX_WAIT
#define FUTEX_WAIT 0
#endif
#ifndef FUTEX_WAKE
#define FUTEX_WAKE 1
#endif

typedef struct {
    void *(*start_routine)(void *);
    void *arg;

    ucontext_t before_start_routine;
    void *retval;
    int canceled;

    // ЛИКВИДИРУЮ флаги finished и joined

    // НОВОЕ
    int tid;
    void *stack_base;
} mythread_struct_t;

typedef mythread_struct_t* mythread_t; // указатель на mythread_struct - дескриптор потока

int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg);
int mythread_join(mythread_t thread, void **retval);
int mythread_cancel(mythread_t thread);

int futex_wait(volatile int *addr, int expected);

#endif /*MYTHREAD_H*/ 