#include "mycache.h"

void cache_init() {
    pthread_mutex_init(&cache.mutex, NULL);
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        cache.entries[i] = NULL;
    }
}

cache_entry_t *cache_find(const char *key) {
    pthread_mutex_lock(&cache.mutex);
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache.entries[i] && strcmp(cache.entries[i]->key, key) == 0) {
            cache.entries[i]->last_access = time(NULL);
            pthread_mutex_unlock(&cache.mutex);
            return cache.entries[i];
        }
    }
    pthread_mutex_unlock(&cache.mutex);
    return NULL;
}

cache_entry_t *cache_add(const char *key) {
    cache_entry_t *entry = malloc(sizeof(cache_entry_t));
    entry->key = strdup(key);
    entry->data = NULL;
    entry->size = 0;
    entry->capacity = 0;
    entry->is_complete = 0;
    entry->is_error = 0;
    entry->last_access = time(NULL);
    pthread_mutex_init(&entry->mutex, NULL);
    pthread_cond_init(&entry->cond, NULL);
    
    pthread_mutex_lock(&cache.mutex);
    
    // поиск свободного слота
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (!cache.entries[i]) {
            cache.entries[i] = entry;
            pthread_mutex_unlock(&cache.mutex);
            return entry;
        }
    }
    
    // поиск самой старой завершенной записи
    int oldest_index = -1;
    time_t oldest_time = 0;
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache.entries[i] && cache.entries[i]->is_complete) {
            if (oldest_index == -1 || cache.entries[i]->last_access < oldest_time) {
                oldest_index = i;
                oldest_time = cache.entries[i]->last_access;
            }
        }
    }
    
    if (oldest_index != -1) {
        free(cache.entries[oldest_index]->key);
        free(cache.entries[oldest_index]->data);
        free(cache.entries[oldest_index]);
        cache.entries[oldest_index] = entry;
    }
    
    pthread_mutex_unlock(&cache.mutex);
    return entry;
}