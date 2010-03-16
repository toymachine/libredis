#ifndef __BUFFER_H
#define __BUFFER_H

#include "common.h"

typedef struct _Buffer Buffer;

Buffer *Buffer_new(Alloc *alloc, size_t size);
int Buffer_dump(Buffer *buffer, int limit);
int Buffer_flip(Buffer *buffer);
int Buffer_remaining(Buffer *buffer);
int Buffer_printf(Buffer *buffer, const char *format, ...);
int Buffer_recv(Buffer *buffer, int fd, size_t len);
int Buffer_send(Buffer *buffer, int fd);

#endif




