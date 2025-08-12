#ifndef __FITOS_QUEUE_H__
#define __FITOS_QUEUE_H__

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>

# include <semaphore.h>

typedef struct _QueueNode {
	int val;
	struct _QueueNode *next;
} qnode_t;

typedef struct _Queue {
	// голова и хвост очереди
	qnode_t *first;
	qnode_t *last;

	pthread_t qmonitor_tid; // идентификатор потока-монитора

	// текущий и максимальный размер очереди
	int count; 
	int max_count; 

	long add_attempts; // сколько раз пытались добавлять элемент в очередь
	long get_attempts; // сколько раз пытались извлекать элемент из очереди
	long add_count; // сколько раз успешно добавлен элемент
	long get_count; // сколько раз успешно извлечен элемент

	// мьютекс вместо спинлока
    pthread_mutex_t lock;
	//  НОВОЕ - семафоры
	sem_t slots; // сколько ещё узлов еще можно добавить до max_count
    sem_t items; // сколько узлов сейчас в очереди 
} queue_t;

queue_t* queue_init(int max_count);
void queue_destroy(queue_t *q);
int queue_add(queue_t *q, int val);
int queue_get(queue_t *q, int *val);
void queue_print_stats(queue_t *q);

#endif		// __FITOS_QUEUE_H__
