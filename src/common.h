#ifndef __COMMON_H
#define __COMMON_H

#include <string.h>

#define DEFAULT_WRITE_BUFF_SIZE (1024 * 4)
#define DEFAULT_READ_BUFF_SIZE (1024 * 12)
#define DEFAULT_COMMAND_BUFF_SIZE 64
#define MAX_BUFF_SIZE (1024 * 1024 * 4)
#define MAX_CONNECTIONS 1024
#define MIN(a,b) ((a)>(b)?(b):(a))

#ifndef NDEBUG
#define DEBUG(args) (printf("DEBUG: "), printf args)
#else
#define DEBUG(args)
#endif

#define CR '\r'
#define LF '\n'
#define CRLF '\r\n'

typedef char Byte;

typedef struct _Redis Redis;
typedef struct _Batch Batch;
typedef struct _Command Command;
typedef struct _Connection Connection;
typedef struct _Reply Reply;

Redis *Redis_get_instance();
void *_Redis_alloc(Redis *redis, size_t size);
void _Redis_free(Redis *redis, void *obj, size_t size);

#define Redis_alloc(SIZE) _Redis_alloc(Redis_get_instance(), SIZE)
#define Redis_alloc_T(T) (T *)_Redis_alloc(Redis_get_instance(), sizeof(T))
#define Redis_free_T(PT,T) _Redis_free(Redis_get_instance(), PT, sizeof(T))

#endif
