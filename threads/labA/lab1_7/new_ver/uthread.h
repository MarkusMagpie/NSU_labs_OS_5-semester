#ifndef UTHREAD_H
#define UTHREAD_H

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

#define UTHREAD_STACK_SIZE (1024 * 1024)

typedef struct {
    void *(*start_routine)(void *);
    void *arg;
    void *retval;
    
    int canceled;
    int finished;
    int joined;
    
    // контекст и стек для пользовательских потоков
    ucontext_t context;
    void *stack_base;
    size_t stack_size;
    
    int id;
} uthread_struct_t;

typedef uthread_struct_t* uthread_t;

int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg);
int uthread_join(uthread_t thread, void **retval);
int uthread_cancel(uthread_t thread);
void uthread_yield(void);

#endif