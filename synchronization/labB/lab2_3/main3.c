#include "queue3.h"

void *count_monitor(void *arg) {
    Storage *s = arg;
    while (1) {
        Node *n = s->first;
        int total_swap = 0, total_asc = 0, total_dsc = 0, total_eq = 0;
        while (n) {
            // pthread_mutex_lock(&n->sync); // чтобы другие потоки не меняли поля при принте 
            pthread_rwlock_rdlock(&n->sync);
            printf("%s (swap=%d asc=%d dsc=%d eq=%d)\n", n->value, n->counter_swap, n->counter_asc, n->counter_dsc, n->counter_eq);
            total_swap += n->counter_swap;
            total_asc += n->counter_asc;
            total_dsc += n->counter_dsc;
            total_eq += n->counter_eq;
            // pthread_mutex_unlock(&n->sync);
            pthread_rwlock_unlock(&n->sync);
            n = n->next;
        }

        printf("TOTAL: swap=%d asc=%d dsc=%d eq=%d\n\n", total_swap, total_asc, total_dsc, total_eq);
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
        // 1 - блокировка чтения-записи для чтения хранилища для получения указателя 1 узла (тк только читаю storage->first, не меняю)
        pthread_rwlock_rdlock(&storage->sync);

        if((curr1 = storage->first) == NULL) {
            printf("compare_length_thread(): curr1 is NULL\n");
            pthread_rwlock_unlock(&storage->sync);
            break;
        }

        // 2 - блокировка чтения-записи для записи 1 узла, так как буду менять в update_counter()
        pthread_rwlock_wrlock(&curr1->sync);

        // 3 - освобождение блокировки чтоения-записи хранилища
        pthread_rwlock_unlock(&storage->sync);

        // 4 - прозод по парам узлов
        Node *curr2 = curr1->next;
        while (curr2 != NULL) {
            pthread_rwlock_rdlock(&curr2->sync);
            // оба потока на момент сравнения имеют захваченные rwlock: curr1 заблокирован на ЗАПИСЬ, curr2 на ЧТЕНИЕ
            // это логика из условия: - необходимо блокировать все записи с данными которых производится работа
            update_counter(curr1, curr2, type);

            pthread_rwlock_unlock(&curr1->sync);
            curr1 = curr2;
            curr2 = curr1->next;
        }

        // 5 - освобождение блокировки чтения-записи последнего узла
        pthread_rwlock_unlock(&curr1->sync);
        
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

        // блокировка хранилища на запись тк изменяю storage->first
        pthread_rwlock_wrlock(&storage->sync);

        if((curr1 = storage->first) == NULL) {
            printf("swap_thread(): curr1 is NULL\n");
            pthread_rwlock_unlock(&storage->sync);
            break;
        }

        // блокировка первого узла на запись тк изменяю его указатель next и counter_swap
        pthread_rwlock_wrlock(&curr1->sync);

        if((curr2 = curr1->next) == NULL) {
            printf("swap_thread(): curr2 is NULL\n");
            pthread_rwlock_unlock(&curr1->sync);
            pthread_rwlock_unlock(&storage->sync);
            break;
        }

        // блокировка второго узла на запись тк изменяю его указатель next и counter_swap
        pthread_rwlock_wrlock(&curr2->sync);

        if ((rand() % 2) == 0) {
            // ... -> *storage->first -> curr1 -> curr2 -> ...
            swap_nodes(&storage->first, curr1, curr2);
            // ... -> *storage->first -> curr2 -> curr1 -> ...
            // обновляю локальные переменные: curr1 теперь указывает на curr2, curr2 на следующий за собой
            curr1 = storage->first;
            curr2 = curr1->next;
        }

        pthread_rwlock_unlock(&storage->sync);
        // до конца списка делаю 50% свап узлов 
        curr3 = curr2->next;
        while (curr3 != NULL) {
            // блокировка третьего узла на запись тк может измениться его указатель next
            pthread_rwlock_wrlock(&curr3->sync);

            if ((rand() % 2) == 0) {
                swap_nodes(&curr1->next, curr2, curr3);
                // ... -> *cur1->next -> curr3 -> curr2 -> ...
            }

            // какой узел теперь идет после curr1? (curr2 или curr3)
            Node *new_second = curr1->next;
            
            // освобождаю старый curr1 (он больше не будет первым в следующей тройке)
            pthread_rwlock_unlock(&curr1->sync);
            
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

        pthread_rwlock_unlock(&curr1->sync);
        pthread_rwlock_unlock(&curr2->sync);
    }

    return NULL;
}

int main() {
    srand(time(NULL)); // стартовое число

    Storage *storage = init_storage(STORAGE_CAPACITY);
    fill_storage(storage);

    pthread_t monitor, compare_asc_tid, compare_desc_tid, compare_eq_tid, swap_tid1, swap_tid2, swap_tid3;

    ThreadData compare_asc_data = {storage, ASC};
    ThreadData compare_desc_data = {storage, DESC};
    ThreadData compare_eq_data = {storage, EQ};

    pthread_create(&monitor, NULL, count_monitor, storage);
    pthread_create(&compare_asc_tid, NULL, compare_length_thread, &compare_asc_data);
    pthread_create(&compare_desc_tid, NULL, compare_length_thread, &compare_desc_data);
    pthread_create(&compare_eq_tid, NULL, compare_length_thread, &compare_eq_data);
    pthread_create(&swap_tid1, NULL, swap_thread, storage);
    pthread_create(&swap_tid2, NULL, swap_thread, storage);
    pthread_create(&swap_tid3, NULL, swap_thread, storage);

    pthread_join(compare_asc_tid, NULL);
    pthread_join(compare_desc_tid, NULL);
    pthread_join(compare_eq_tid, NULL);
    pthread_join(swap_tid1, NULL);
    pthread_join(swap_tid2, NULL);
    pthread_join(swap_tid3, NULL);
    pthread_join(monitor, NULL);

    return 0;
}