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

typedef struct _Alloc
{
  void *(*alloc)(size_t size);
  void (*free)(void *address);
} Alloc;

typedef char Byte;

#endif
