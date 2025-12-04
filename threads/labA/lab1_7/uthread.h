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

// определение макс. количества юзер потоков + размер юзер стека
#define UTHREAD_MAX 128 
#define UTHREAD_STACK_SIZE (1024 * 1024)

typedef int uthread_t;

// создание пользовательского потока, 0 - успех, -1 - ошибка 
int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg);
// передает управления следующему потоку
void uthread_yield(void);
// запуск планировщика 
void uthread_run(void);

int uthread_join(uthread_t tid, void **retval);

// void uthread_run(void);

#endif 