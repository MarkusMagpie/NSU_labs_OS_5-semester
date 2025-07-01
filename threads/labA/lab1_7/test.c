#include "uthread.h"
#include <stdio.h>

void *fn(void *arg) {
    for(int i = 0; i < 5; i++){
        printf("[thread %ld] - iteration: %d\n", (long)arg, i);
        uthread_yield();
    }
    return NULL;
}

int main(){
    uthread_t t1, t2;

    uthread_create(&t1, fn, (void*)1);
    uthread_create(&t2, fn, (void*)2);

    uthread_run();
    
    return 0;
}
