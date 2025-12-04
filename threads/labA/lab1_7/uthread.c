#include "uthread.h"

static ucontext_t ucontexts[UTHREAD_MAX]; // массив контекстов юзер потоков
static char ustacks[UTHREAD_MAX][UTHREAD_STACK_SIZE];
static void *(*ufuncs[UTHREAD_MAX])(void *);
static void *uargs[UTHREAD_MAX];
static void *uretv[UTHREAD_MAX]; // retvals потоков
static int finished[UTHREAD_MAX];

static int ucount = 0;
static int current = -1;
static int alive = 0;
static ucontext_t main_context;

// точка входа в каждый юзер поток после переключения контекста
static void uthread_wrapper(int id) {
    current = id;

    void *ret = ufuncs[id](uargs[id]); // вызываю юзер функцию
    uretv[id] = ret;
    
    finished[id] = 1;
    alive--;
    printf("[uthread_wrapper] thread %d finished\n", id);

    if (alive <= 0) {
        // все потоки завершились -> main
        setcontext(&main_context);

    } else {
        // передача управления следующему alive потоку
        uthread_yield();
    }

    // uthread_yield();
    
    // попал сюда после передачи управления следующему потоку -> этот поток завершается
    // exit(0);
}

// создание юзер потока: контекст, "стек", инкременируем счетчик юзер потоков ucount
int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (ucount >= UTHREAD_MAX) return -1;
    
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
    finished[id] = 0; // поток ещё не завершен
    uretv[id] = NULL; // результат работы потока ещё не определен

    makecontext(&ucontexts[id], (void (*)())uthread_wrapper, 1, id);

    alive++;

    *thread = id;
    return 0;
}

// передача управления от текущего юзер потока следующему 
void uthread_yield(void) {
    if (ucount == 0 || alive == 0) {
        // потоков нема || все завершены -> возвращаюсь в main
        if (current != -1) {
            current = -1;
            setcontext(&main_context);
        }
        return;
    }

    int prev = current;
    int next;

    if (prev == -1) {
        // первый вызов - из main -> поиск 1 незавершенный поток (у него finished == 0)
        next = 0;
        while (next < ucount && finished[next]) {
            next++;
        }
        if (next == ucount) return; // все завершены
    } else {
        // вызов не из main -> ищу следующий незавершенный поток
        next = (prev + 1) % ucount;
        int started = next;
        while (finished[next] && next != started) {
            next = (next + 1) % ucount;
        }

        // все остальные потоки завершены -> возвращаюсь в main
        if (finished[next]) {
            current = -1; 
            setcontext(&main_context);
            return;
        }
    }

    current = next;
    
    if (prev == -1) {
        swapcontext(&main_context, &ucontexts[current]);
    } else {
        swapcontext(&ucontexts[prev], &ucontexts[current]);
    }
}

// запуск планировщика: сохраняю контекст main и передаю управление потокам
// void uthread_run(void) {
//     if (ucount == 0) return;

//     if (getcontext(&main_context) == -1) {
//         printf("getcontext() failed\n");   
//         return;
//     }

//     current = -1;

//     uthread_yield();
// }

// ожидание завершения tid + возвращениеего  retval
int uthread_join(uthread_t tid, void **retval) {
    if (tid < 0 || tid >= ucount) return -1;

    if (finished[tid]) {
        if (retval) *retval = uretv[tid];
        return 0;
    }

    while (!finished[tid]) {
        // в контексте потока (current != -1) передаю управление следующему потоку
        // в main (current == -1) uthread_yield() запустит планировщик
        uthread_yield();
    }

    if (retval) *retval = uretv[tid];
    return 0;
}