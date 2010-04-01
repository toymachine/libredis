#ifndef REDIS_H
#define REDIS_H

#include <string.h>

#define MAX_ERROR_SIZE 255

typedef struct _Module
{
	size_t size; //size of module structure
	void * (*alloc_malloc)(size_t size);
    void * (*alloc_realloc)(void *ptr, size_t size);
    void (*alloc_free)(void *ptr);
    size_t allocated;
    char error[MAX_ERROR_SIZE];
} Module;

void Module_init(Module *module);
void Module_free();

typedef struct _Batch Batch;
typedef struct _Connection Connection;
typedef struct _Ketama Ketama;
typedef struct _Executor Executor;

typedef enum _ReplyType
{
	RT_ERROR = -1,
	RT_NONE = 0,
    RT_OK = 1,
    RT_BULK_NIL = 2,
    RT_BULK = 3,
    RT_MULTIBULK_NIL = 4,
    RT_MULTIBULK = 5,
    RT_INTEGER = 6
} ReplyType;

Connection *Connection_new(const char *addr);
void Connection_free(Connection *connection);

Batch *Batch_new();
void Batch_free(Batch *batch);
void Batch_write(Batch *batch, const char *str, size_t str_len, int num_commands);
//reading out replies
int Batch_next_reply(Batch *batch, ReplyType *reply_type, char **data, size_t *len);
char *Batch_error(Batch *batch);

Executor *Executor_new();
void Executor_free(Executor *executor);
int Executor_add(Executor *executor, Connection *connection, Batch *batch);
int Executor_execute(Executor *executor);

//ketama hashing
Ketama *Ketama_new();
void Ketama_free(Ketama *ketama);
void Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight);
void Ketama_create_continuum(Ketama *ketama);
int Ketama_get_server(Ketama *ketama, char* key, size_t key_len);
char *Ketama_get_server_addr(Ketama *ketama, int ordinal);

#endif
