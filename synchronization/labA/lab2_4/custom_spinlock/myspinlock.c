#define _GNU_SOURCE
#include <linux/futex.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>

#include "myspinlock.h"

void custom_spinlock_init(custom_spinlock_t *s) {
    atomic_init(&s->lock, 0);
    s->owner = 0;
    s->cnt = 0; 
}

// 1 вариант
// void custom_spinlock_init(custom_spinlock_t *s) {
//     atomic_init(&s->lock, 0);
//     atomic_init(&s->owner, 0);
//     atomic_init(&s->cnt, 0);
// }

// 2 вариант
// void custom_spinlock_init(custom_spinlock_t *s) {
//     atomic_init(&s->lock, 0);
// }

void custom_spinlock_lock(custom_spinlock_t *s) {
	while (1) {
		int expected = 0;
		if (atomic_compare_exchange_strong(&s->lock, &expected, 1)) {
            s->owner = gettid();
            s->cnt++;
            return;
		}
        // продолжаю проверку в цикле пока лок не станет доступен
	}
}

// 2 вариант
// void custom_spinlock_lock(custom_spinlock_t *s) {
//     while (1) {
//         int expected = 0;
//         if (atomic_compare_exchange_strong(&s->lock, &expected, 1)) {
//             return;
//         }
//     }
// }

void custom_spinlock_unlock(custom_spinlock_t *s) {
    // старый вариант
    /*
    int expected = 0;
	atomic_compare_exchange_strong(&s->lock, &expected, 1);
    */

    // текущий поток является владельцем?
    if (s->owner == gettid()) {
        s->cnt--;
        if (s->cnt == 0) {
            s->owner = 0;
            // сбрасываю лок атомарно
            atomic_store(&s->lock, 0);
        }
    } else {
        // попытка разблокировки спинлока, которым не владеешь
        printf("custom_spinlock_unlock(): attempt to unlock spinlock not owned by current thread\n");
    }
}

// 2 вариант
// void custom_spinlock_unlock(custom_spinlock_t *s) {
//     atomic_store(&s->lock, 0);
// }