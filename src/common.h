#ifndef __COMMON_H
#define __COMMON_H

#include <string.h>

#define DEFAULT_WRITE_BUFF_SIZE (1024 * 4)
#define DEFAULT_READ_BUFF_SIZE (1024 * 12)
#define DEFAULT_COMMAND_BUFF_SIZE 64
#define MAX_BUFF_SIZE (1024 * 1024 * 4)
#define MAX_CONNECTIONS 1024
#define MIN(a,b) ((a)>(b)?(b):(a))

#define CR '\r'
#define LF '\n'
#define CRLF '\r\n'

typedef struct _Redis Redis;

Redis *Redis_get_instance();
void *Redis_alloc(Redis *redis, size_t size);

#define REDIS_ALLOC(SIZE) Redis_alloc(Redis_get_instance(), SIZE)
#define REDIS_ALLOC_T(T) (T *)Redis_alloc(Redis_get_instance(), sizeof(T))

/*
{
  void *(*alloc)(size_t size);
  void (*free)(void *address);
} Alloc;
*/

typedef char Byte;

#endif
