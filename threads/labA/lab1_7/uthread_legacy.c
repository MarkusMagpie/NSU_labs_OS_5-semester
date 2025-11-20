#include "uthread.h"

static ucontext_t ucontexts[UTHREAD_MAX]; // массив контекстов юзер потоков
static char ustacks[UTHREAD_MAX][UTHREAD_STACK_SIZE];
static void *(*ufuncs[UTHREAD_MAX])(void *);
static void *uargs[UTHREAD_MAX];

static int ucount = 0;
static int current = -1;

// точка входа в каждый юзер поток после переключения контекста
static void uthread_wrapper(void) {
    int id = current;
    void *ret = ufuncs[id](uargs[id]);
    printf("[uthread_wrapper] поток %d завершил работу.\n", id);

    uthread_yield();
    
    // попал сюда после передачи управления следующему потоку -> этот поток завершается
    exit(0);
}

// создание юзер потока: контекст, "стек", инкременируем счетчик юзер потоков ucount
int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (ucount >= UTHREAD_MAX) {
        return -1;
    }
    
    // инициализирую контекст текущего потока
    int id = ucount++;
    if (getcontext(&ucontexts[id]) == -1) {
        return -1;
    }

    ucontexts[id].uc_link = NULL; // когда контекст завершится, никакой другой не запустится 
    ucontexts[id].uc_stack.ss_sp = ustacks[id]; // адрес начала стека - сам стек заранее выделен в массиве ustacks
    ucontexts[id].uc_stack.ss_size = UTHREAD_STACK_SIZE; // размер стека
    
    ufuncs[id] = start_routine;
    uargs[id] = arg;

    makecontext(&ucontexts[id], uthread_wrapper, 0); 

    *thread = id;
    return 0;
}

// передача управления от текущего юзер потока следующему 
void uthread_yield(void) {
    if (ucount == 0) {
        return;
    }

    int prev = current;
    current = (current + 1) % ucount;

    swapcontext(&ucontexts[prev], &ucontexts[current]);
}

// запуск первого юзер потока
void uthread_run(void) {
    current = 0;
    setcontext(&ucontexts[current]);
}