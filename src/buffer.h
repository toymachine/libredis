/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifndef __BUFFER_H
#define __BUFFER_H

#include <stdarg.h>

#include "common.h"

typedef struct _Buffer Buffer;

Buffer *Buffer_new(size_t size);
void Buffer_free(Buffer *buffer);
Byte *Buffer_data(Buffer *buffer);
void Buffer_dump(Buffer *buffer, size_t limit);
void Buffer_flip(Buffer *buffer);
void Buffer_clear(Buffer *buffer);
void Buffer_fill(Buffer *buffer, Byte b);
size_t Buffer_position(Buffer *buffer);
void Buffer_set_position(Buffer *buffer, size_t position);
void Buffer_set_limit(Buffer *buffer, size_t limit);
size_t Buffer_remaining(Buffer *buffer);
void Buffer_write(Buffer *buffer, const char *data, size_t len);
size_t Buffer_recv(Buffer *buffer, int fd);
size_t Buffer_send(Buffer *buffer, int fd);

#endif




