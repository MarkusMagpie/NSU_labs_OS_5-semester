#include "mycache.h"

cache_t cache;

// чтение HTTP запроса до конца заголовков
ssize_t read_http_request(int fd, char *buffer, size_t max_size) {
    size_t total_read = 0;
    ssize_t bytes;
    char *headers_end = NULL;
    
    // читаю пока не найду конец заголовков или не заполнится буфер
    while (total_read < max_size - 1) {
        bytes = recv(fd, buffer + total_read, max_size - 1 - total_read, 0);
        if (bytes <= 0) {
            // error или соединение закрыто клиентом
            return -1;
        }
        
        total_read += bytes;
        buffer[total_read] = '\0'; // строка завершенная
        
        // поиск конца заголовков "\r\n\r\n" в том что уже прочтено
        headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end) {
            return total_read;
        }
        
        // запрос слишком длинный и конца заголовков нет -> защита
        if (total_read > 16 * 1024) { // НАПРИМЕР лимит 16KB на строку запроса
            printf("[main] Слишком длинные заголовки запроса\n");
            return -1;
        }
    }
    
    // буфер переполнен, а конца заголовков так и не нашел
    printf("[main] Буфер переполнен или некорректный запрос\n");
    return -1;
}

// модификация HTTP запроса
void modify_http_request(char *request, size_t *request_len, const char *path) {
    char buffer[BUFFER_SIZE];
    char *first_line_end = strstr(request, "\r\n");
    
    if (!first_line_end) return;
    
    // формирование новой первой строки
    char method[16];
    sscanf(request, "%15s", method);
    
    snprintf(buffer, sizeof(buffer), "%s %s HTTP/1.0\r\n", method, path);
    // копия остальных заголовков
    strcat(buffer, first_line_end + 2);

    if (!strstr(buffer, "Connection:")) {
        char *headers_end = strstr(buffer, "\r\n\r\n");
        if (headers_end) {
            char temp[BUFFER_SIZE];
            strcpy(temp, headers_end);
            strcpy(headers_end, "\r\nConnection: close");
            strcat(buffer, temp);
        }
    }

    // копия обратно в request
    size_t new_len = strlen(buffer);
    if (new_len < BUFFER_SIZE) {
        memcpy(request, buffer, new_len + 1);
        *request_len = new_len;
    }
}

void *loader_thread(void *arg) {
    loader_data_t *data = (loader_data_t *)arg;
    printf("[loader_thread] Начало загрузки ключа: %s\n", data->key);
    cache_entry_t *entry = cache_find(data->key);
    if (!entry) {
        free(data->host);
        free(data->request);
        free(data);
        return NULL;
    }

    // получаю IP адрес целевого сервераы
    struct hostent *server = gethostbyname(data->host);
    if (!server) {
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    // создаю соединение с целевым сервером
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    struct sockaddr_in server_addr;
    memcpy(&server_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    server_addr.sin_family = AF_INET; // IP адрес к которому будет привязан сокет (IPv4)
    server_addr.sin_port = htons(data->port); // номер порта (в сетевом порядке байт) к которому будет привязан сокет

    if (connect(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        close(server_fd);
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    // отправка модифицированного запроса
    if (write(server_fd, data->request, data->request_len) < 0) {
        close(server_fd);
        pthread_mutex_lock(&entry->mutex);
        entry->is_error = 1;
        pthread_cond_broadcast(&entry->cond);
        pthread_mutex_unlock(&entry->mutex);
        goto cleanup;
    }

    // чтение ответа и сохранение в кэш
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    
    pthread_mutex_lock(&entry->mutex);
    
    while ((bytes_read = read(server_fd, buffer, sizeof(buffer))) > 0) {
        // увеличение буфера при необходимости
        if (entry->size + bytes_read > entry->capacity) {
            size_t new_capacity = entry->capacity * 2;
            if (new_capacity < entry->size + bytes_read) {
                new_capacity = entry->size + bytes_read + BUFFER_SIZE;
            }
            
            char *new_data = realloc(entry->data, new_capacity);
            if (!new_data) {
                printf("[loader_thread] realloc failed  для ключа: %s\n", data->key);
                entry->is_error = 1;
                break;
            }
            if (!new_data) break;
            
            entry->data = new_data;
            entry->capacity = new_capacity;
        }
        
        // копия данных в кэш
        memcpy(entry->data + entry->size, buffer, bytes_read);
        entry->size += bytes_read;
        
        // опопвещение ждущих потоков о новых данных
        pthread_cond_broadcast(&entry->cond);
    }
    if (bytes_read < 0) {
        printf("[loader_thread] read failed из сокета для ключа: %s\n", data->key);
        entry->is_error = 1;
    }

    if (!entry->is_error) {
        entry->is_complete = 1;
        printf("[loader_thread] завершена загрузка ключа: %s, размер: %d байт\n", data->key, entry->size);
    } else {
        printf("[loader_thread] загрузка ключа: %s завершилась с ошибкой\n", data->key);
    }
    
    // тут уже загрузка завершена
    // entry->is_complete = 1;
    entry->last_access = time(NULL);
    pthread_cond_broadcast(&entry->cond);
    pthread_mutex_unlock(&entry->mutex);
    
    close(server_fd);

cleanup:
    free(data->host);
    free(data->request);
    free(data);
    return NULL;
}

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
    
    // ожидание появления данных
    while (!entry->is_complete && !entry->is_error && entry->size == 0) {
        pthread_cond_wait(&entry->cond, &entry->mutex);
    }
    
    if (entry->is_error) {
        const char *error_msg = "HTTP/1.0 502 Bad Gateway\r\n\r\nError loading resource";
        write(client_fd, error_msg, strlen(error_msg));
        pthread_mutex_unlock(&entry->mutex);
        close(client_fd);
        return NULL;
    }
    
    // отправка данных клиенту
    int sent = 0;
    while (sent < entry->size) {
        int n = write(client_fd, entry->data + sent, entry->size - sent);
        if (n <= 0) break;
        sent += n;
        
        // если данные еще загружаются -> жду новых
        if (!entry->is_complete && sent == entry->size) {
            pthread_cond_wait(&entry->cond, &entry->mutex);
        }
    }
    
    pthread_mutex_unlock(&entry->mutex);
    close(client_fd);
    return NULL;
}

// для извлечения относительного пути из URL
void extract_path_from_url(const char *url, char *path, size_t path_size) {
    // если это абсолютный URL (начинается с http://) то хуйня надо менять
    if (strncmp(url, "http://", 7) == 0) {
        const char *path_start = strchr(url + 7, '/'); // пропуск "http://", посик первой "/" -> начало пути в URL
        /*
        url = "http://example.com/images/123.png"
        path_start = "/images/123.png"
        */
        if (path_start) {
            snprintf(path, path_size, "%s", path_start); // копирует path_start в path не превышая path_size байт
            // path = "/images/123.png"
        } else {
            snprintf(path, path_size, "/");
        }
    } else {
        // относительный путь: path = "/"
        snprintf(path, path_size, "%s", url);
    }
}

int main() {
    cache_init();

    // создаем TCP сокет
    /*
    socket() создает сокет, возвращает дескриптор сокета
        __domain - AF_INET - протокол IPv4
        __type - SOCK_STREAM - TCP
        __protocol - 0 - выбирается автоматически
    */
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("socket creation failed\n");
        exit(EXIT_FAILURE);
    }

    // SO_REUSEADDR позволяет переиспользовать порт сразу после завершения сервера. даже если он в состоянии TIME_WAIT
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        printf("setsockopt failed\n");
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

    printf("[main] Кэширующий прокси-сервер слушает порт %d...\n", PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        // принятие нового соединения
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("accept failed\n");
            continue;
        }

        printf("[main] Новое подключение от %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        // читаю HTTP-запрос
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read = read_http_request(client_fd, buffer, sizeof(buffer));
        if (bytes_read <= 0) {
            printf("[main] Не удалось прочитать запрос или соединение закрыто\n");
            close(client_fd);
            continue;
        }
        buffer[bytes_read] = '\0';

        // извлекаю имя хоста из строки заголовков 
        char *host_start = strstr(buffer, "Host: ");
        if (!host_start) {
            const char *msg = "HTTP/1.0 400 Bad Request\r\n\r\nHost header required";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        host_start += 6;
        char *host_end = strstr(host_start, "\r\n");
        if (!host_end) {
            close(client_fd);
            continue;
        }

        char host[256];
        int host_len = host_end - host_start;
        strncpy(host, host_start, host_len);
        host[host_len] = '\0';
        
        int port = PORT;

        // извлекаю метод/тип_запроса и URL
        char method[16], url[1024], version[16];
        if (sscanf(buffer, "%15s %1023s %15s", method, url, version) != 3) {
            const char *msg = "HTTP/1.0 400 Bad Request\r\n\r\nInvalid request line";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        // извлекаю относительный путь из URL
        char path[1024];
        extract_path_from_url(url, path, sizeof(path));

        // формирование ключ кэша
        char key[2048];
        snprintf(key, sizeof(key), "%s:%s %s", host, method, path);
        printf("[main] Ключ кэша: %s\n", key);

        // модификация клиентского запроса для отправки на целевой сервер
        size_t modified_len = bytes_read;
        modify_http_request(buffer, &modified_len, path);

        // проверка наличия кэш
        cache_entry_t *entry = cache_find(key);
        
        if (entry && entry->is_complete) {
            printf("[main] Cache HIT для ключа: %s\n", key);
            
            // поток для клиента
            client_data_t *client_data = malloc(sizeof(client_data_t));
            client_data->client_fd = client_fd;
            client_data->key = strdup(key);
            
            pthread_t client_tid;
            if (pthread_create(&client_tid, NULL, handle_connection, client_data)) {
                printf("pthread_create handle_connection failed\n");
                free(client_data->key);
                free(client_data);
                close(client_fd);
            } else {
                pthread_detach(client_tid);
            }
        } else {
            printf("[main] Cache MISS для ключа: %s\n", key);
            
            // добавляю запись в кэш если ее тама еще нема
            if (!entry) {
                // entry = cache_add(key);
                entry = cache_get_or_add(key);
            }

            // НОВОЕ: атомарная проверка: начата ли уже загрузка?
            pthread_mutex_lock(&entry->mutex);
            int should_create_loader = 0;
            if (!entry->is_complete && !entry->is_loading) {
                entry->is_loading = 1;  // <-- отмечаем, что загрузка начата
                should_create_loader = 1;
            }
            pthread_mutex_unlock(&entry->mutex);
            
            if (should_create_loader) {
                // поток для загрузки 
                loader_data_t *loader = malloc(sizeof(loader_data_t));
                loader->key = strdup(key);
                loader->request = malloc(modified_len);
                memcpy(loader->request, buffer, modified_len);
                loader->request_len = modified_len;
                loader->host = strdup(host);
                loader->port = port;
                
                pthread_t loader_tid;
                if (pthread_create(&loader_tid, NULL, loader_thread, loader)) {
                    printf("pthread_create loader_thread failed\n");
                    free(loader->key);
                    free(loader->request);
                    free(loader->host);
                    free(loader);
                } else {
                    pthread_detach(loader_tid);
                }
            } else {
                printf("[main] Загрузка для ключа %s уже начата другим потоком\n", key);
            }
            
            // поток для клиента
            client_data_t *client_data = malloc(sizeof(client_data_t));
            client_data->client_fd = client_fd;
            client_data->key = strdup(key);
            
            pthread_t client_tid;
            if (pthread_create(&client_tid, NULL, handle_connection, client_data)) {
                printf("pthread_create handle_connection failed\n");
                free(client_data->key);
                free(client_data);
                close(client_fd);
            } else {
                pthread_detach(client_tid);
            }
        }
    }

    close(server_fd);
    return 0;
}