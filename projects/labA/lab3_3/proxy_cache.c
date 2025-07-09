#include "mycache.h"

void cache_init() {
    pthread_mutex_init(&cache.mutex, NULL);

    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        cache.entries[i] = NULL;
    }
}

// поиск в кэше ключа "имя_сервера:тип(GET,POST,...) URL-путь"
cache_entry_t *cache_find(const char *key) {
    pthread_mutex_lock(&cache.mutex);
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        if (cache.entries[i] && strcmp(cache.entries[i]->key, key) == 0) {
            pthread_mutex_unlock(&cache.mutex);
            return cache.entries[i];
        }
    }
    pthread_mutex_unlock(&cache.mutex);
    return NULL;
}

// добавление в кэш нового ключа
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
    for (int i = 0; i < MAX_CACHE_ENTRIES; i++) {
        // нашел свободный элемент, заполняю его и возвращаю
        if (!cache.entries[i]) {
            cache.entries[i] = entry;
            pthread_mutex_unlock(&cache.mutex);
            return entry;
        }
    }

    // если кэш заполнен полностью - ищу индекс самой старой записи и заменяю ее на новую
    time_t oldest_time = cache.entries[0]->last_access;
    int oldest_index = 0;
    for (int i = 1; i < MAX_CACHE_ENTRIES; i++) {
        if (cache.entries[i]->last_access < oldest_time && cache.entries[i]->is_complete) {
            oldest_time = cache.entries[i]->last_access;
            oldest_index = i;
        }
    }
    free(cache.entries[oldest_index]->key);
    free(cache.entries[oldest_index]->data);
    free(cache.entries[oldest_index]);
    cache.entries[oldest_index] = entry;

    pthread_mutex_unlock(&cache.mutex);
    return entry;
}

// поток для загрузки данных с целевого сервера
void *loader_thread(void *arg) {
    loader_data_t *data = (loader_data_t *)arg;
    cache_entry_t *entry = cache_find(data->key);
    if (!entry) {
        goto cleanup;
    }

    // чтобы прокси мог соединиться с конечным/целевым сервером ему нужно получить IP‑адрес, разрешив его имя
    struct hostent *server = gethostbyname(data->host);
    if (!server) {
        pthread_mutex_lock(&entry->mutex);
        // ждущие по cd потоки проснутся, увидят изменение флага и завершатся
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    //----------------------------------------------------------------------------------------------------------------
    // создаю TCP сокет для соединения с конечным сервером
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    // настройка адреса сервера server_addr
    struct sockaddr_in server_addr;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет (IPv4)
    server_addr.sin_port = htons(data->port); // номер порта (в сетевом порядке байт) к которому будет привязан сокет

    // попытка соединения с конечным сервером
    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    // пересылка http-запроса клиента на конечный сервер
    if (write(server_fd, data->request, data->request_len) < 0) {
        close(server_fd);
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }
    //----------------------------------------------------------------------------------------------------------------

    // полученный от конечного сервера http-ответ сохраняется в кэш
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    pthread_mutex_lock(&entry->mutex);

    /* пример http-ответа из тетради
    1) строка статуса: HTTP/1.0 200 OK
    2) строки заголовков: 
        Header1: Value1
        Header2: Value2
    3) тело ответа...
    */
    while (bytes_read = read(server_fd, buffer, sizeof(buffer))) {
        if (bytes_read < 0) break;
        
        // если свободного места в текущем буфере < bytes_read, надо расширять буфер
        if (entry->size + bytes_read > entry->capacity) {
            size_t new_capacity = entry->capacity * 2;
            if (new_capacity < entry->size + bytes_read) {
                new_capacity = entry->size + bytes_read + BUFFER_SIZE;
            }
            
            char *new_data = realloc(entry->data, new_capacity);
            if (!new_data) break;
            
            entry->data = new_data;
            entry->capacity = new_capacity;
        }
        
        // копирую считанные bytes_read из buffer в кэш entry->data+entry->size
        memcpy(entry->data + entry->size, buffer, bytes_read);
        entry->size += bytes_read;
        
        // через cd оповещаю ждущие потоки о появлении новых данных
        pthread_cond_broadcast(&entry->cond);
    }
    
    // загрузка http-ответа в кэш завершена - оповещаю ждущие потоки
    entry->is_complete = 1;
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->mutex);
    
    close(server_fd);

cleanup:
    free(data->host);
    free(data->request);
    free(data);
    return NULL;
}

// обработка соединения с клиентом в отдельном потоке (вызывается когда в кэше уже лежит ключ)
void *handle_connection(void *arg) {
    client_data_t *data = (client_data_t *)arg;
    int client_fd = data->client_fd;
    char *key = data->key;
    free(data);

    cache_entry_t *entry = cache_find(key);
    if (!entry) {
        close(client_fd);
        return NULL;
    }

    pthread_mutex_lock(&entry->mutex);
    int sent = 0;
    
    while (1) {
        // отправка новых данных
        while (sent < entry->size) {
            int n = write(client_fd, entry->data + sent, entry->size - sent);
            if (n <= 0) {
                pthread_mutex_unlock(&entry->mutex);
                close(client_fd);
                return NULL;
            }

            sent += n;
        }
        
        if (entry->is_complete || entry->is_error) break;
        
        // жду сообщения от loader_thread что появились новые данные от конечного сервера
        pthread_cond_wait(&entry->cond, &entry->mutex);
    }
    
    while (sent < entry->size) {
        int n = write(client_fd, entry->data + sent, entry->size - sent);
        if (n <= 0) break;
        sent += n;
    }
    
    pthread_mutex_unlock(&entry->mutex);
    close(client_fd);
    return NULL;
}

int main() {
    cache_init();

    // создаем TCP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        AF_INET - протокол IPv4
        SOCK_STREAM - TCP
        0 - выбирается автоматически
    */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR позволяет переиспользовать порт сразу после завершения сервера. даже если он в состоянии TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // настройка адреса сервера server_addr
    struct sockaddr_in server_addr;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет (IPv4)
    server_addr.sin_port = htons(PORT); // номер порта 80 (в сетевом порядке байт) к которому будет привязан сокет

    // привязка сокета к адресу server_addr
    if (bind(server_fd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        printf("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // начало прослушивания (5 потому что в мане прочитал что обычно <= 5)
    if (listen(server_fd, 5) < 0) {
        printf("listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("[main] КЭШИРУЮЩИЙ прокси-сервер слушает порт %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // принятие нового соединения
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("accept failed");
            continue;
        }

        printf("[main] новое подключение от %s:%d\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);

        //---------------------------------------------------------------------------------------
        // чтение строки HTTP-запроса и заголовков из сокета клиента 
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0';

        // извлекаю имя хоста из строки заголовков 
        char* host_start = strstr(buffer, "Host: ");
        if (!host_start) {
            const char* msg = "HTTP/1.0 400 Bad Request\r\n\r\nInvalid request";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        host_start += 6; // "Host: "
        char* host_end = strstr(host_start, "\r\n");
        if (!host_end) {
            close(client_fd);
            continue;
        }

        // извлекаю имя сервера в host (дано: "Host: example.com", извлекаю: host="example.com") 
        char host[256];
        int host_len = host_end - host_start;
        if (host_len > sizeof(host) - 1) {
            host_len = sizeof(host) - 1;
        }
        strncpy(host, host_start, host_len); // копирую до host_len символов из host_start в host
        host[host_len] = '\0';
        //---------------------------------------------------------------------------------------
        
        // извлекаю тип и URL-путь для ключа из строки запроса (дано: "GET /courses/networks HTTP/1.1", извлекаю: method="GET", path="/courses/networks")
        char *method = strtok(buffer, " ");
        char *path = strtok(NULL, " ");
        if (!method || !path) {
            close(client_fd);
            continue;
        }
        
        // формирую ключ кэша вида: имя_сервера:тип(GET,POST,...) URL-путь <-> example.com:GET /courses/networks
        char key[512];
        snprintf(key, sizeof(key), "%s:%s %s", host, method, path);
        
        // проверка - есть ли ключ key в кэше?
        cache_entry_t *entry = cache_find(key);
        
        if (entry) {
            printf("[%s:%d] cache hit\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            // cache hit - создаю поток (раньше был fork()) для обработки нового соединения
            pthread_t tid;
            client_data_t* data = malloc(sizeof(client_data_t));
            data->client_fd = client_fd;
            data->key = strdup(key);

            if (pthread_create(&tid, NULL, handle_connection, data)) {
                printf("cache hit, pthread_create failed\n");
                free(data->key);
                free(data);
                close(client_fd);
            } else {
                pthread_detach(tid);
            }
        } else {
            printf("[%s:%d] cache miss\n", inet_ntoa(client_addr.sin_addr), client_addr.sin_port);
            // cache miss - создаю новую запись в кэше, запускаю loader_thread
            entry = cache_add(key);
            
            loader_data_t *loader = malloc(sizeof(loader_data_t));
            loader->key = strdup(key); // malloc-ом выделяю память под key
            loader->request = malloc(bytes_read);
            memcpy(loader->request, buffer, bytes_read);
            loader->request_len = bytes_read;
            loader->host = strdup(host);
            loader->port = PORT;
            
            // loader_thread соединится с целевым сервером, сохранит данные в кэш и закроет соединение
            pthread_t loader_tid;
            if (pthread_create(&loader_tid, NULL, loader_thread, loader)) {
                printf("cache miss, pthread_create for loader_thread failed\n");
                free(loader->key);
                free(loader->request);
                free(loader->host);
                free(loader);

                close(client_fd);
            } else {
                pthread_detach(loader_tid);
                
                // клиента обрабатываю в отдельном потоке, как при cache hit
                pthread_t tid;
                client_data_t *data = malloc(sizeof(client_data_t));
                data->client_fd = client_fd;
                data->key = strdup(key);
                
                if (pthread_create(&tid, NULL, handle_connection, data)) {
                    printf("cache miss, pthread_create for client failed\n");
                    free(data->key);
                    free(data);
                    close(client_fd);
                } else {
                    pthread_detach(tid);
                }
            }
        }
    }

    close(server_fd);
    return 0;
}