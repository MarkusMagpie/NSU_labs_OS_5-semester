#pragma once

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

#define STORAGE_CAPACITY 10

#define ASC 0
#define DESC 1
#define EQ 2
#define SWAP 3

#define MAX_STRING_LENGTH 100

typedef struct Node {
    struct Node *next;
    int counter_swap, counter_asc, counter_dsc, counter_eq;
    char value[MAX_STRING_LENGTH];
    // pthread_mutex_t sync;
    pthread_spinlock_t sync;
    // pthread_rwlock_t sync;
} Node;

typedef struct _Storage {
    Node *first;
    int capacity;
    // pthread_mutex_t sync;
    pthread_spinlock_t sync;
    //pthread_rwlock_t sync;
} Storage;

// типы считывающих потоков: хранилище с которым поток работает + тип сравнения: ASC, DESC, EQ
typedef struct _ThreadData {
    Storage *storage;
    int type;
} ThreadData;

Storage *init_storage(int capacity);
void add_node(Storage *storage, const char *value);
Node* create_node(const char *value);
void fill_storage(Storage *storage);