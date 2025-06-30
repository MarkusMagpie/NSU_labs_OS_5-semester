#define _GNU_SOURCE
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdlib.h>
#include <signal.h>

// первый поток блокирует получения всех сигналов
void* blocker_thread(void* arg) {
    sigset_t set;
    sigfillset(&set);
    pthread_sigmask(SIG_BLOCK, &set, NULL); // заблокируй в этом потоке все сигналы, перечисленные в set
    printf("[1 - blocker] all signals blocked in this thread\n");
    pause(); // ждем бесконечно 

    return NULL;
}

void sigint_handler(int signo) {
    printf("[2 - sigint handler] caught SIGINT (%d) in signal handler\n", signo);
}

// второй принимает сигнал SIGINT при помощи обработчика сигнала
void* handler_thread(void* arg) {
    // наследует блокировку от main, сигналы должны быть разблокированы
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    printf("[2 - sigint handler] SIGINT handler installed\n");
    // в бесконечном цикле ловим в обработчике сигнал SIGINT 
    while (1) {
        pause();
    }

    return NULL;
}

// третий принимает сигнал SIGQUIT при помощи функции sigwait()
void* waiter_thread(void* arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT); // в set добавляем SIGQUIT для его обработки через sigwait

    // убедимся, что SIGQUIT заблокирован для всех потоков, и будем ждать его здесь
    printf("[3 - waiter] waiting for SIGQUIT via sigwait\n");
    int sig;
    while (1) {
        if (sigwait(&set, &sig) == 0) {
            printf("[3 - waiter] sigwait received signal %d (SIGQUIT)\n", sig);
        }
    }

    return NULL;
}

int main() {
    pthread_t t1, t2, t3;
    int err1, err2, err3;
    sigset_t set; // набор сигналов

    // блокируем SIGQUIT для всех потоков по умолчанию
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &set, NULL); // заблокируй в этом потоке все сигналы, перечисленные в set (SIGQUIT)

    err1 = pthread_create(&t1, NULL, blocker_thread, NULL);
    if (err1) {
        printf("[main] pthread_create() failed: %s\n", strerror(err1));
        return EXIT_FAILURE;
    }

    err2 = pthread_create(&t2, NULL, handler_thread, NULL);
    if (err2) {
        printf("[main] pthread_create() failed: %s\n", strerror(err2));
        return EXIT_FAILURE;
    }

    err3 = pthread_create(&t3, NULL, waiter_thread, NULL);
    if (err3) {
        printf("[main] pthread_create() failed: %s\n", strerror(err3));
        return EXIT_FAILURE;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return EXIT_SUCCESS;
}
