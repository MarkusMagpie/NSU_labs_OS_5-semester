#define _GNU_SOURCE
#include <pthread.h>
#include <assert.h>

#include "queue.h"

// фоновый периодический вывод статистики очереди queue_t q
void *qmonitor(void *arg) {
	queue_t *q = (queue_t *)arg;

	printf("qmonitor: [%d %d %d]\n", getpid(), getppid(), gettid());

	while (1) {
		queue_print_stats(q);
		sleep(1);
	}

	return NULL;
}

// инициализация очереди q
queue_t* queue_init(int max_count) {
	int err;

	queue_t *q = malloc(sizeof(queue_t));
	if (!q) {
		printf("Cannot allocate memory for a queue\n");
		abort();
	}

	q->first = NULL;
	q->last = NULL;
	q->max_count = max_count;
	q->count = 0;

	q->add_attempts = q->get_attempts = 0;
	q->add_count = q->get_count = 0;

	err = pthread_create(&q->qmonitor_tid, NULL, qmonitor, q);
	if (err) {
		printf("queue_init: pthread_create() failed: %s\n", strerror(err));
		abort();
	}

	return q;
}

// освобождение ресурсов очереди q
void queue_destroy(queue_t *q) {
    int err = pthread_cancel(q->qmonitor_tid);
	if (err != 0) {
		printf("queue_destroy: pthread_cancel() failed: %s\n", strerror(err));
	}
	err = pthread_join(q->qmonitor_tid, NULL);
	if (err != 0) {
		printf("queue_destroy: pthread_join() failed: %s\n", strerror(err));
	}

    // вычищаю все узлы в списке
    qnode_t *cur = q->first;
    while (cur) {
        qnode_t *next = cur->next;
        free(cur);
        cur = next;
    }

    // освободил структуру самой очереди
    free(q);

	printf("queue_destroy: [%d %d %d]\n", getpid(), getppid(), gettid());
}

// попытка поместить новый элемент в конец очереди q и обновить статистику 
int queue_add(queue_t *q, int val) {
	q->add_attempts++;

	assert(q->count <= q->max_count);

	if (q->count == q->max_count)
		return 0;

	qnode_t *new = malloc(sizeof(qnode_t));
	if (!new) {
		printf("Cannot allocate memory for new node\n");
		abort();
	}

	new->val = val;
	new->next = NULL;

	if (!q->first)
		q->first = q->last = new;
	else {
		q->last->next = new;
		q->last = q->last->next;
	}

	q->count++;
	q->add_count++;

	return 1;
}

// попытка извлечь элемент из головы очереди и вернуть его значение, обновляя при этом статистику q
int queue_get(queue_t *q, int *val) {
	q->get_attempts++;

	assert(q->count >= 0);

	if (q->count == 0)
		return 0;

	/* ДОП 2 - так не ловим сигсегв 
	if (q->count == 0 || !q->first)
		return 0;
	*/

	// сохраняю указатель на головной узел
	qnode_t *tmp = q->first; 

	*val = tmp->val; // записал значение головного узла в *val
	q->first = q->first->next;

	// if (q->first == NULL) q->last = NULL; 

	free(tmp);

	q->count--;
	q->get_count++;

	return 1;
}

void queue_print_stats(queue_t *q) {
	printf("queue stats: current size %d; attempts: (%ld %ld %ld); counts (%ld %ld %ld)\n",
		q->count,
		q->add_attempts, q->get_attempts, q->add_attempts - q->get_attempts,
		q->add_count, q->get_count, q->add_count -q->get_count);
}

