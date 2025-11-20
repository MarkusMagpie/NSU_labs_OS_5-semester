#include "uthread.h"

static uthread_struct_t *threads[128]; // массив указателей на структуры потоков
static int thread_count = 0;
static int current_thread_id = -1; // текущий юзер поток - изначально из main
static ucontext_t main_context;

// точка входа в каждый юзер поток после переключения контекста
static void uthread_wrapper(uthread_struct_t *thread) {
    printf("[uthread_wrapper] поток ID:%d запущен...\n", thread->id);

    if (!thread->canceled) {
        printf("[uthread_wrapper] поток ID:%d вызывает start_routine...\n", thread->id);
        thread->retval = thread->start_routine(thread->arg);
        printf("[uthread_wrapper] поток ID:%d завершил start_routine с результатом: %ld\n", thread->id, (long)thread->retval);
    }
    
    thread->finished = 1;
    printf("[uthread_wrapper] поток ID:%d завершил работу, ожидает join...\n", thread->id);
    
    // передача управления другому потоку
    uthread_yield();
    
    // завершенные потоки бесконечно передают управление
    while (1) {
        uthread_yield();
    }
}

int uthread_create(uthread_t *thread, void *(*start_routine)(void *), void *arg) {
    if (thread_count >= 128) return -1;

    uthread_struct_t *new_thread = malloc(sizeof(uthread_struct_t));
    if (!new_thread) {
        printf("[uthread_create] malloc() failed\n");
        return -1;
    }

    // структуру потока забиваю   нулями
    memset(new_thread, 0, sizeof(uthread_struct_t));
    new_thread->start_routine = start_routine;
    new_thread->arg = arg;
    new_thread->id = thread_count;
    new_thread->stack_size = UTHREAD_STACK_SIZE;
    
    new_thread->stack_base = malloc(UTHREAD_STACK_SIZE);
    if (!new_thread->stack_base) {
        printf("[uthread_create] malloc() failed\n");
        free(new_thread);
        return -1;
    }
    
    // инициализирую контекст текущего потока
    if (getcontext(&new_thread->context) == -1) {
        printf("[uthread_create] getcontext() failed\n");
        free(new_thread->stack_base);
        free(new_thread);
        return -1;
    }
    
    new_thread->context.uc_link = NULL; // когда контекст завершится, никакой другой не запустится 
    new_thread->context.uc_stack.ss_sp = new_thread->stack_base; // адрес начала стека - сам стек заранее выделен в массиве ustacks
    new_thread->context.uc_stack.ss_size = UTHREAD_STACK_SIZE; // размер стека
    
    // контекст на точку входа в юзер поток
    makecontext(&new_thread->context, (void (*)())uthread_wrapper, 1, new_thread);
    
    threads[thread_count++] = new_thread;
    *thread = new_thread;
    
    printf("[uthread_create] создан поток ID:%d\n", new_thread->id);

    return 0;
}

int uthread_join(uthread_t thread, void **retval) {
    // жду завершения потока
    while (!thread->finished) {
        uthread_yield();
    }
    
    if (retval) *retval = thread->retval;
    
    thread->joined = 1;
    printf("[uthread_join] поток ID:%d joined\n", thread->id);
    
    if (thread->stack_base) {
        free(thread->stack_base);
        thread->stack_base = NULL;
        printf("[uthread_join] стек потока ID:%d освобожден\n", thread->id);
    }
    
    // выношу поток из глобального массива и освобождаю структуру
    for (int i = 0; i < thread_count; i++) {
        if (threads[i] == thread) {
            threads[i] = NULL;
            free(thread);
            printf("[uthread_join] структура потока ID:%d освобождена\n", i);
            break;
        }
    }
    
    return 0;
}

int uthread_cancel(uthread_t thread) {
    thread->canceled = 1;
    return 0;
}

// передача управления от текущего юзер потока следующему 
void uthread_yield(void) {
    if (thread_count == 0) return;
    
    int prev_id = current_thread_id;
    int next_id;
    
    if (prev_id == -1) {
        // первый вызов - из main -> поиск 1 незавершенный поток (у него finished == 0)
        next_id = 0;
        while (next_id < thread_count && (threads[next_id] == NULL || threads[next_id]->finished)) {
            next_id++;
        }
        if (next_id == thread_count) return; // все завершены
    } else {
        // вызов не из main -> ищу следующий незавершенный поток
        next_id = (prev_id + 1) % thread_count;
        int started = next_id;
        
        while (threads[next_id] == NULL || threads[next_id]->finished) {
            next_id = (next_id + 1) % thread_count;
            
            // все остальные потоки завершены -> возвращаюсь в main
            if (next_id == started) {
                current_thread_id = -1;
                setcontext(&main_context);
                return;
            }
        }
    }
    
    current_thread_id = next_id;
    uthread_struct_t *next_thread = threads[next_id];
    
    if (prev_id == -1) {
        // сохраняю контекст main, переключаю на контекст потока
        swapcontext(&main_context, &next_thread->context);
    } else {
        uthread_struct_t *prev_thread = threads[prev_id];
        swapcontext(&prev_thread->context, &next_thread->context);
    }
}