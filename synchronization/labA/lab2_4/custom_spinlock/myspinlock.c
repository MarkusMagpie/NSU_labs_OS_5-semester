#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>

#include "myspinlock.h"

void spinlock_init(custom_spinlock_t *s) {
    atomic_init(&s->lock, 0);
}

void spinlock_lock(custom_spinlock_t *s) {
	while (1) {
		int expected = 0;
		if (atomic_compare_exchange_strong(&s->lock, &expected, 1)) {
            return;
		}
        // продолжаю проверку в цикле пока лок не станет доступен
	}
}

void spinlock_unlock(custom_spinlock_t *s) {
    // старый вариант
    /*
    int expected = 0;
	atomic_compare_exchange_strong(&s->lock, &expected, 1);
    */
    
    // сбрасываю лок атомарно
    atomic_store(&s->lock, 0);
}
