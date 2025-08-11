#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

#include <pthread.h>
#include <sched.h>

#include <semaphore.h>

#include "queue.h"

#define RED "\033[41m"
#define NOCOLOR "\033[0m"

static sem_t started;

void set_cpu(int n) {
	int err;
	cpu_set_t cpuset;
	pthread_t tid = pthread_self();

	// все биты установлены в 0
	CPU_ZERO(&cpuset);
	// устанавливаем бит под номером n
	CPU_SET(n, &cpuset);

	err = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
	if (err != 0) {
		printf("set_cpu: pthread_setaffinity failed for cpu %d\n", n);
		return;
	}

	printf("set_cpu: set cpu %d\n", n);
}

void *reader(void *arg) {
	int expected = 0;
	queue_t *q = (queue_t *)arg;

    sem_wait(&started); // НОВОЕ - если счетчик окажется <= 0, то reader() заблокируется и будет ждать sem_post() от writer()

	printf("reader [%d %d %d]\n", getpid(), getppid(), gettid());

    set_cpu(1);

	while (1) {
		int val = -1;
		int ok = queue_get(q, &val); // val - значение извлеченного элемента
		if (!ok)
			continue;

		if (expected != val)
			printf(RED"ERROR: get value is %d but expected - %d" NOCOLOR "\n", val, expected);

		expected = val + 1;
	}

	return NULL;
}

void *writer(void *arg) {
	int i = 0;
	queue_t *q = (queue_t *)arg;

    sem_post(&started);
    
    printf("writer [%d %d %d]\n", getpid(), getppid(), gettid());

    set_cpu(1);

	while (1) {
		int ok = queue_add(q, i);
		if (!ok)
			continue;
		i++;
	}

	return NULL;
}

int main() {
	pthread_t tid1, tid2;
	queue_t *q;
	int err;

    // НОВОЕ - создал семафор для синхронизации
    sem_init(&started, 0, 0);

	printf("main [%d %d %d]\n", getpid(), getppid(), gettid());

	q = queue_init(10000);

    err = pthread_create(&tid1, NULL, writer, q);
	if (err != 0) {
		printf("main: pthread_create() failed: %s\n", strerror(err));
		return -1;
	}

	err = pthread_create(&tid2, NULL, reader, q);
	if (err != 0) {
		printf("main: pthread_create() failed: %s\n", strerror(err));
		return -1;
	}

    pthread_join(tid1, NULL);
    pthread_join(tid2, NULL);

    sem_destroy(&started);

	pthread_exit(NULL);

	return 0;
}
