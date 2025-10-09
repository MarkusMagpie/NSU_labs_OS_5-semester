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
    pthread_sigmask(SIG_BLOCK, &set, NULL); // заблокируй в этом потоке все сигналы из set
    
    pid_t tid = gettid();

    printf("[%d - blocker(1)] all signals blocked in this thread\n", tid);
    while (1) { 
        pause();
    }

    return NULL;
}

// второй принимает сигнал SIGINT при помощи обработчика сигнала
void sigint_handler(int signo) {
    pid_t tid = gettid(); 
    printf("[%d - sigint handler(2)] caught SIGINT (%d) in signal handler\n", tid, signo);
}

void* handler_thread(void* arg) {
    // наследует блокировку от main -> в этом потоке (на момент его создания) тоже заблокирован SIGINT и SIGQUIT
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    
    // установка обработчика сигнала SIGINT для ПРОЦЕССА, не только потока handler_thread()
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask); // маска/набор сигналов, которые должны блокироваться при обработке сигнала SIGINT 
    sigaction(SIGINT, &sa, NULL);

    pid_t tid = gettid();
    printf("[%d - sigint handler(2)] SIGINT handler installed\n", tid);
    
    // в бесконечном цикле ловлю сигналы 
    while (1) {
        pause();
    }

    return NULL;
}

// третий принимает сигнал SIGQUIT при помощи функции sigwait()
void* waiter_thread(void* arg) {
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGQUIT); // в set добавил SIGQUIT для его обработки через sigwait

    // убедился, что SIGQUIT заблокирован для всех потоков, и буду ждать его здесь
    pid_t tid = gettid();
    printf("[%d - waiter(3)] waiting for SIGQUIT via sigwait\n", tid);
    int sig;

    while (1) {
        if (sigwait(&set, &sig) == 0) {
            printf("[%d - waiter(3)] sigwait received signal %d (SIGQUIT)\n", tid, sig);
        }
    }

    return NULL;
}

int main() {
    pthread_t t1, t2, t3;
    int err1, err2, err3;
    sigset_t set; // набор сигналов
    pid_t tid = gettid();

    printf("[%d - main(0)] main thread started\n", tid);

    printf("watch -n1 cat /proc/%d/status | grep Sig\n\n", getpid());
    sleep(5);

    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGQUIT);
    pthread_sigmask(SIG_BLOCK, &set, NULL); // заблокируй в потоке main() сигналы SIGINT и SIGQUIT 

    // ! так как 3 потока ниже дочерние для main(), то они наследуют его маску/набор сигналов set

    err1 = pthread_create(&t1, NULL, blocker_thread, NULL);
    if (err1 != 0) {
        printf("[%d - main(0)] pthread_create() failed: %s\n", tid, strerror(err1));
        return EXIT_FAILURE;
    }

    err2 = pthread_create(&t2, NULL, handler_thread, NULL);
    if (err2 != 0) {
        printf("[%d - main(0)] pthread_create() failed: %s\n", tid, strerror(err2));
        return EXIT_FAILURE;
    }

    err3 = pthread_create(&t3, NULL, waiter_thread, NULL);
    if (err3 != 0) {
        printf("[%d - main(0)] pthread_create() failed: %s\n", tid, strerror(err3));
        return EXIT_FAILURE;
    }

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);
    pthread_join(t3, NULL);

    return EXIT_SUCCESS;
}
