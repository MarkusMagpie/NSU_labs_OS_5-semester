#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#define PORT 80
#define BUFFER_SIZE 4096
#define MAX_CACHE_ENTRIES 100

// структура элемента кэша
typedef struct {
    char *key; // ключ кэша <-> URL
    char *data; // данные 
    int size; // текущий размер данных
    int capacity; // максимальный размер данных/выделенная память
    int is_complete; // флаг завершения загрузки http-ответа в кэш 
    int is_error; // флаг ошибки
    time_t last_access; // время последнего обращения
    // для синхронизации: mutex+cd (lab2_2 (f))
    pthread_mutex_t mutex; 
    pthread_cond_t cond;
} cache_entry_t;

// структура глобального кэша
typedef struct {
    cache_entry_t *entries[MAX_CACHE_ENTRIES];
    pthread_mutex_t mutex;
} cache_t;

// структура потока загрузки
typedef struct {
    char *key; // имя_сервера:тип(GET,POST,...) URL-путь 
    char *request; // http-запрос клиента
    size_t request_len;
    char *host; // имя_сервера
    int port;
} loader_data_t;

// структура клиентского потока
typedef struct {
    int client_fd;
    char *key;
} client_data_t;

cache_t cache;

void cache_init();
cache_entry_t *cache_find(const char *key);
cache_entry_t *cache_add(const char *key);
void *loader_thread(void *arg);
// для обработки соединения в отдельном потоке
void *handle_connection(void *arg);
