#include "queue_pthread.h"

void *count_monitor(void *arg) {
    Storage *s = arg;
    while (1) {
        Node *n = s->first;
        int total_swap = 0, total_asc = 0, total_dsc = 0, total_eq = 0;
        while (n) {
            pthread_mutex_lock(&n->sync); // чтобы другие потоки не меняли поля при принте 
            // printf("%s (swap=%d asc=%d dsc=%d eq=%d)\n", n->value, n->counter_swap, n->counter_asc, n->counter_dsc, n->counter_eq);
            total_swap += n->counter_swap;
            total_asc += n->counter_asc;
            total_dsc += n->counter_dsc;
            total_eq += n->counter_eq;
            pthread_mutex_unlock(&n->sync);
            n = n->next;
        }

        printf("TOTAL: swap=%d asc=%d dsc=%d eq=%d\n", total_swap, total_asc, total_dsc, total_eq);
        sleep(1);
    }

    return NULL;
}

void update_counter(Node *curr1, Node *curr2, int type) {
    int pair_count = strlen(curr1->value) - strlen(curr2->value);
    switch (type) {
    case EQ:
        if(pair_count == 0) ++curr1->counter_eq;
        break;
    case ASC:
        if(pair_count < 0) ++curr1->counter_asc;
        break;
    case DESC:
        if(pair_count > 0) ++curr1->counter_dsc;
        break;
    }
}

void *compare_length_thread(void *data) {
    ThreadData *thread_data = (ThreadData *)data;
    Storage *storage = thread_data->storage;
    int type = thread_data->type;

    while (1) {
        Node *curr1;
        // для получения 1 элемента
        pthread_mutex_lock(&storage->sync);

        if((curr1 = storage->first) == NULL) {
            printf("compare_length_thread(): curr1 is NULL\n");
            pthread_mutex_unlock(&storage->sync);
            break;
        }

        pthread_mutex_lock(&curr1->sync);

        pthread_mutex_unlock(&storage->sync);

        Node *curr2 = curr1->next;
        while (curr2 != NULL) {
            pthread_mutex_lock(&curr2->sync);
            // оба потока на момент сравнения имеют захваченные мьютексы
            // это логика из условия: - необходимо блокировать все записи с данными которых производится работа
            update_counter(curr1, curr2, type);

            pthread_mutex_unlock(&curr1->sync);
            curr1 = curr2;
            curr2 = curr1->next;
        }

        pthread_mutex_unlock(&curr1->sync);
        
    }
    
    return NULL;
}

/*
было: ... -> *curr1_next -> curr2 -> curr3 -> ...
стало: ... -> *curr1_next -> curr3 -> curr2 -> ...
*/
void swap_nodes(Node **curr1_next, Node *curr2, Node *curr3) {
    ++curr2->counter_swap;

    *curr1_next = curr3;
    curr2->next = curr3->next;
    curr3->next = curr2;
}

// из условия - при перестановке записей списка, необходимо блокировать три записи.
void *swap_thread(void *data) {
    Storage *storage = (Storage *)data;

    while (1) {
        Node *curr1, *curr2, *curr3;

        pthread_mutex_lock(&storage->sync);

        if((curr1 = storage->first) == NULL) {
            printf("swap_thread(): curr1 is NULL\n");
            pthread_mutex_unlock(&storage->sync);
            break;
        }

        pthread_mutex_lock(&curr1->sync);

        if((curr2 = curr1->next) == NULL) {
            printf("swap_thread(): curr2 is NULL\n");
            pthread_mutex_unlock(&curr1->sync);
            pthread_mutex_unlock(&storage->sync);
            break;
        }

        pthread_mutex_lock(&curr2->sync);

        if ((rand() % 2) == 0) {
            // ... -> *storage->first -> curr1 -> curr2 -> ...
            swap_nodes(&storage->first, curr1, curr2);
            // ... -> *storage->first -> curr2 -> curr1 -> ...
            // обновляю локальные переменные: curr1 теперь указывает на curr2, curr2 на следующий за собой
            curr1 = storage->first;
            curr2 = curr1->next;
        }

        pthread_mutex_unlock(&storage->sync);
        // до конца списка делаю 50% свап узлов 
        curr3 = curr2->next;
        while (curr3 != NULL) {
            pthread_mutex_lock(&curr3->sync);

            if ((rand() % 2) == 0) {
                swap_nodes(&curr1->next, curr2, curr3);
                // ... -> *cur1->next -> curr3 -> curr2 -> ...
            }

            // какой узел теперь идет после curr1? (curr2 или curr3)
            Node *new_second = curr1->next;
            
            // освобождаю старый curr1 (он больше не будет первым в следующей тройке)
            pthread_mutex_unlock(&curr1->sync);

            // сдвиг на узел вперед:
            curr1 = new_second; // новый первый узел следующей тройки
            curr2 = curr1->next; // новый второй узел следующей тройки

            // есть ли следующий узел после curr2?
            if (curr2 != NULL) {
                curr3 = curr2->next; // новый третий узел для следующей тройки
            } else {
                curr3 = NULL; // достиг конца списка
            }
        }

        pthread_mutex_unlock(&curr1->sync);
        pthread_mutex_unlock(&curr2->sync);
    }

    return NULL;
}

#define NUM_SWAP_THREADS 1
#define NUM_ASC_THREADS 1
#define NUM_DESC_THREADS 1
#define NUM_EQ_THREADS 1

int main() {
    srand(time(NULL)); // стартовое число

    Storage *storage = init_storage(STORAGE_CAPACITY);
    fill_storage(storage);

    pthread_t monitor;
    pthread_t swap_threads[NUM_SWAP_THREADS];
    pthread_t asc_threads[NUM_ASC_THREADS];
    pthread_t desc_threads[NUM_DESC_THREADS];
    pthread_t eq_threads[NUM_EQ_THREADS];

    ThreadData compare_asc_data = {storage, ASC};
    ThreadData compare_desc_data = {storage, DESC};
    ThreadData compare_eq_data = {storage, EQ};

    pthread_create(&monitor, NULL, count_monitor, storage);
    for (int i = 0; i < NUM_SWAP_THREADS; i++) {
        pthread_create(&swap_threads[i], NULL, swap_thread, storage);
    }
    for (int i = 0; i < NUM_ASC_THREADS; i++) {
        pthread_create(&asc_threads[i], NULL, compare_length_thread, &compare_asc_data);
    }
    for (int i = 0; i < NUM_DESC_THREADS; i++) {
        pthread_create(&desc_threads[i], NULL, compare_length_thread, &compare_desc_data);
    }
    for (int i = 0; i < NUM_EQ_THREADS; i++) {
        pthread_create(&eq_threads[i], NULL, compare_length_thread, &compare_eq_data);
    }

    pthread_join(monitor, NULL);
    for (int i = 0; i < NUM_SWAP_THREADS; i++) {
        pthread_join(swap_threads[i], NULL);
    }
    for (int i = 0; i < NUM_ASC_THREADS; i++) {
        pthread_join(asc_threads[i], NULL);
    }
    for (int i = 0; i < NUM_DESC_THREADS; i++) {
        pthread_join(desc_threads[i], NULL);
    }
    for (int i = 0; i < NUM_EQ_THREADS; i++) {
        pthread_join(eq_threads[i], NULL);
    }

    return 0;
}