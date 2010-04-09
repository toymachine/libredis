/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

/*
 * This is the public C API of the libredis library.
 *
 * The library should be initialized first using the 'Module_new' function to get reference to the library.
 * Then you can set some optional properties (using 'Module_set_XXX' functions) to configure the library.
 * Finally you should call Module_init to initialize the library proper.
 * At the end of your program, call 'Module_free' to release all resources in use by the library.
 *
 * The API is written in a 'pseudo' object oriented style, using only opaque references to the Batch, Connection and Executor 'Classes'.
 * Each of these will be created and destroyed using their respective XXX_new() and XXX_free methods.
 *
 * In order to communicate with a Redis server you will need at least to create an instance of Batch, Connection and Executor. e.g.:
 *
 * Batch *batch = Batch_new();
 * Connection *connection = Connection_new('127.0.0.1:6379');
 * Executor *executor = Executor_new();
 *
 * One or more Redis commands can be written into the batch using the Batch_write_XXX functions:
 *
 * Batch_write(batch, "GET foo\r\n", 9, 1);
 *
 * Then we tell the library that we want to execute these commands on a specific Redis server by associating the Batch
 * and Connection using the 'Executor_add' function.
 *
 * Executor_add(executor, connection, batch);
 *
 * Note that we can associate multiple (connection, batch) pairs by calling the Executor_add function for each pair.
 *
 * Then we will execute all of the commands against all of the connections in parallel by calling Executer_execute,
 * with a timeout specified in milliseconds.
 *
 * Executor_execute(executor, 500);
 *
 * This method returns when all replies have been received, or a timeout has occurred.
 *
 * If everything went well we can now read the replies from our batch (or batches):
 *
 * ReplyType reply_type;
 * char *reply_data;
 * size_t reply_len;
 * while(int level = Batch_next_reply(batch, &reply_type, &reply_data, &reply_len)) {
 * 		printf("reply type: %d, data: '%s', len: %d\n", (int)reply_type, reply_data, reply_len);
 * }
 */
#ifndef REDIS_H
#define REDIS_H

#include <string.h>

typedef struct _Module Module;
typedef struct _Batch Batch;
typedef struct _Connection Connection;
typedef struct _Ketama Ketama;
typedef struct _Executor Executor;


Module *Module_new();
void Module_set_alloc_alloc(Module *module, void * (*alloc_malloc)());
void Module_set_alloc_realloc(Module *module, void * (*alloc_realloc)(void *, size_t));
void Module_set_alloc_free(Module *module, void (*alloc_free)(void *));
int Module_init(Module *module);

size_t Module_get_allocated(Module *module);
char *Module_last_error(Module *module);
void Module_free(Module *module);

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
void Batch_write_decimal(Batch *batch, long decimal);

//reading out replies
int Batch_next_reply(Batch *batch, ReplyType *reply_type, char **data, size_t *len);
char *Batch_error(Batch *batch);

Executor *Executor_new();
void Executor_free(Executor *executor);
int Executor_add(Executor *executor, Connection *connection, Batch *batch);
int Executor_execute(Executor *executor, int timeout_ms);

//ketama hashing
Ketama *Ketama_new();
void Ketama_free(Ketama *ketama);
void Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight);
void Ketama_create_continuum(Ketama *ketama);
int Ketama_get_server(Ketama *ketama, char* key, size_t key_len);
char *Ketama_get_server_addr(Ketama *ketama, int ordinal);

#endif
