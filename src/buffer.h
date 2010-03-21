#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdarg.h>

#include "common.h"

typedef struct _Buffer Buffer;

Buffer *Buffer_new(size_t size);
int Buffer_free(Buffer *buffer);
Byte *Buffer_data(Buffer *buffer);
int Buffer_dump(Buffer *buffer, int limit);
int Buffer_flip(Buffer *buffer);
void Buffer_clear(Buffer *buffer);
void Buffer_fill(Buffer *buffer, Byte b);
size_t Buffer_position(Buffer *buffer);
int Buffer_set_position(Buffer *buffer, size_t position);
int Buffer_set_limit(Buffer *buffer, int limit);
int Buffer_remaining(Buffer *buffer);
int Buffer_printf(Buffer *buffer, const char *format, ...);
int Buffer_vprintf(Buffer *buffer, const char *format, va_list args);
size_t Buffer_recv(Buffer *buffer, int fd);
size_t Buffer_send(Buffer *buffer, int fd);

#endif




