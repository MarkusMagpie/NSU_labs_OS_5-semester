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

typedef struct {
    void *(*start_routine)(void *);
    void *arg;

    void *retval;
    int canceled;
    int finished;
    int joined;
    pid_t tid;
    void *stack_base; // для освобождение стека в mythread_join()
} mythread_struct_t;

typedef mythread_struct_t* mythread_t; // указатель на mythread_struct - дескриптор потока

int mythread_create(mythread_t *thread, void *(*start_routine)(void *), void *arg);
int mythread_join(mythread_t thread, void **retval);
int mythread_cancel(mythread_t thread);

#endif /*MYTHREAD_H*/ 