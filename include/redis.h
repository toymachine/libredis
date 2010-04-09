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
 * Connection *connection = Connection_new("127.0.0.1:6379");
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
 * int level;
 * while(level = Batch_next_reply(batch, &reply_type, &reply_data, &reply_len)) {
 * 		printf("level: %d, reply type: %d, data: '%.*s'\n", level, (int)reply_type, reply_len, reply_data);
 * }
 *
 * If a function returns an error code (e.g. -1). You can call Module_last_error to get a textual description of the error.
 *
 * If a reply is an error reply (RT_ERROR type), the textual description will be in the reply_data.
 */
#ifndef REDIS_H
#define REDIS_H

#include <string.h>

typedef struct _Module Module;
typedef struct _Batch Batch;
typedef struct _Connection Connection;
typedef struct _Ketama Ketama;
typedef struct _Executor Executor;


/*
 * Create a new instance of the library. (Currently this just returns the same global instance. This might change in the future)
 */
Module *Module_new();

/**
 * Override various memory-management functons to be used by libredis. You don't have to set these,
 * libredis will use normal alloc/realloc/free in that case.
 */
void Module_set_alloc_alloc(Module *module, void * (*alloc_malloc)());
void Module_set_alloc_realloc(Module *module, void * (*alloc_realloc)(void *, size_t));
void Module_set_alloc_free(Module *module, void (*alloc_free)(void *));

/**
 * Initialise the libredis module once all properties have been set. The library is now ready to be used.
 * Returns -1 if there is an error, 0 if all is ok.
 */
int Module_init(Module *module);

/**
 * Gets the amount of heap memory currently allocated by the libredis module. This should return 0 after the module has been freed.
 */
size_t Module_get_allocated(Module *module);

/**
 * Gets a textual description of the last error that occurred.
 */
char *Module_last_error(Module *module);

/**
 * Release all resources still held by libredis. The library cannot be used anymore after this call.
 */
void Module_free(Module *module);

/**
 * Create a new connection to a Redis instance. addr should be of form 'xxx.xxx.xxx.xxx:y' where xxx is an ip-address and y is the port
 * to connect to. if the port is not given the default Redis port of 6379 will be used.
 * Note that the actual connection will not be made at this point. It will open the connection as soon as the first command
 * will be written to Redis.
 */
Connection *Connection_new(const char *addr);

/**
 * Release all resources held by the connection.
 */
void Connection_free(Connection *connection);

/**
 * Enumerates the type of replies that can be read from a Batch.
 */
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

/**
 * Create a new Batch of redis commands
 */
Batch *Batch_new();

/**
 * Release all resources of the given batch
 */
void Batch_free(Batch *batch);

/**
 * Writes a command or part of a command into the batch. The batch will keep an internal pointer to the last written
 * position, so that the next write will be appended from there. The batch will automatically grow in size.
 * The num_commands argument specifies how many commands are added by this write. It might be 0 if you are writing a command in parts
 * It is possible to call the write method without a string, just to set the number of commands in the batch.
 * In that case pass NULL for str and 0 for str_len.
 */
void Batch_write(Batch *batch, const char *str, size_t str_len, int num_commands);

/**
 * Write a decimal into the batch (as a string, like using %d in a printf call).
 */
void Batch_write_decimal(Batch *batch, long decimal);

/**
 * Reads the next reply from the batch. This will return the replies in the order the commands were given.
 * Call repeatedly until all replies have been read (it will return 0 when there are no more replies left).
 * For some reply types, data will point to the content of the reply (RT_BULK, RT_OK, RT_ERROR). In that
 * case the len argument will contain the length of this data (e.g. the data is NOT null terminated).
 * In the case of a multibulk reply RT_MULTIBULK, the len argument will contain the number of bulk replies that follow.
 * Note that any data pointed to by the data argument is only valid as long as the batch is not freed.
 * If you want to do something with it later on, you need to copy it yourself.
 */
int Batch_next_reply(Batch *batch, ReplyType *reply_type, char **data, size_t *len);

/**
 * If a batch was aborted (maybe because a connection went down or timed-out), there will be an error message
 * associated with the batch. Use this function to retrieve it.
 * Returns NULL if there was no error in the batch.
 */
char *Batch_error(Batch *batch);


/**
 * Creates a new empty Executor
 */
Executor *Executor_new();

/**
 * Frees any resources held by the Executor
 */
void Executor_free(Executor *executor);

/**
 * Associate a batch with a connection. When execute is called the commands from the batch
 * will be executed on the given connection.
 * Returns 0 if all ok, -1 if there was an error making the association.
 */
int Executor_add(Executor *executor, Connection *connection, Batch *batch);

/**
 * Execute all associated (connection, batch) pairs within the given timeout. The commands
 * contained by the batches will be send to their respective connections, and the replies to these commands
 * will be gathered in their respective batches.
 * When all batches complete within the timeout, the result of this function is 1.
 * If a timeout occurs before completion. the result of this function is 0, and all commands in all batches that
 * were not completed at the time of timeout will get an error reply.
 * If there is an error with this method itself, it will return -1.
 */
int Executor_execute(Executor *executor, int timeout_ms);


//ketama hashing
Ketama *Ketama_new();
void Ketama_free(Ketama *ketama);
void Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight);
void Ketama_create_continuum(Ketama *ketama);
int Ketama_get_server(Ketama *ketama, char* key, size_t key_len);
char *Ketama_get_server_addr(Ketama *ketama, int ordinal);

#endif
