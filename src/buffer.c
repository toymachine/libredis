#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#include "common.h"
#include "buffer.h"
#include "list.h"

struct _Buffer
{
	Byte *data;
	Byte *buff;
	size_t buff_size;
	size_t position;
	int limit;
	int capacity;
	int mark;
};

Buffer *Buffer_new(size_t size)
{
	Buffer *buffer = Alloc_alloc_T(Buffer);
	buffer->buff_size = size;
	buffer->buff = Alloc_alloc(size);
	buffer->data = buffer->buff;
	buffer->position = 0;
	buffer->capacity = size;
	buffer->limit = buffer->capacity;
	buffer->mark = 0;
#ifndef NDEBUG
	Buffer_fill(buffer, (Byte)0xEA);
#endif
	return buffer;
}

void Buffer_fill(Buffer *buffer, Byte b)
{
	for(int i = 0; i < buffer->capacity; i++) {
		buffer->buff[i] = b;
	}
}

void Buffer_clear(Buffer *buffer)
{
	buffer->position = 0;
	buffer->limit = buffer->capacity;
	buffer->mark = 0;
}

int Buffer_free(Buffer *buffer)
{
	Alloc_free(buffer->buff, buffer->capacity);
	DEBUG(("dealloc Buffer\n"));
	Alloc_free_T(buffer, Buffer);
	return 0;
}


Byte *Buffer_data(Buffer *buffer)
{
	return buffer->data;
}

int Buffer_dump(Buffer *buffer, int limit)
{
	int i, j;
	if(limit == -1) {
		limit = buffer->capacity;
	}
	printf("buffer cap: %d, limit: %d, pos: %d\n", buffer->capacity, buffer->limit, buffer->position);
	for(i = 0; i < limit; i+=16) {
		for(j = 0; j < 16; j++) {
			printf("%02X ", ((unsigned char *)buffer->data)[i + j]);
		}
		for(j = 0; j < 16; j++) {
			int c = ((unsigned char *)buffer->data)[i + j];
			if(isprint(c)) {
				printf("%c", c);
			}
			else {
				printf(".");
			}
		}
		printf("\n");
	}
	return 0;
}

int Buffer_remaining(Buffer *buffer)
{
    return (buffer->limit - buffer->position);
}

size_t Buffer_position(Buffer *buffer)
{
	return buffer->position;
}

int Buffer_set_position(Buffer *buffer, size_t position)
{
	buffer->position = position;
	return 0;
}

int Buffer_set_limit(Buffer *buffer, int limit)
{
	buffer->limit = limit;
	return 0;
}

int Buffer_grow(Buffer *buffer)
{
	size_t min_remaining = (buffer->capacity * 256) / 2048;
	size_t remaining = Buffer_remaining(buffer);
	DEBUG(("min remaining: %d, rem: %d\n", min_remaining, remaining));
	if(remaining < min_remaining) {
		printf("TODO grow buffer\n");
		abort();//TODO
	};
	return Buffer_remaining(buffer);
}


size_t Buffer_send(Buffer *buffer, int fd)
{
	DEBUG(("Buffer_send fd: %d, position: %d, limit: %d, remaining: %d\n", fd, buffer->position, buffer->limit, Buffer_remaining(buffer)));
	size_t bytes_written = write(fd, buffer->data + buffer->position, Buffer_remaining(buffer));
	DEBUG(("Buffer_send fd: %d, bytes_written: %d\n", fd, bytes_written));
	if(bytes_written != -1) {
		buffer->position += bytes_written;
	}
	return bytes_written;
}

size_t Buffer_recv(Buffer *buffer, int fd)
{
	Buffer_grow(buffer);
	DEBUG(("Buffer_recv fd: %d, position: %d, limit: %d, remaining: %d\n", fd, buffer->position, buffer->limit, Buffer_remaining(buffer)));
	size_t bytes_read = read(fd, buffer->data + buffer->position, Buffer_remaining(buffer));
	DEBUG(("Buffer_recv fd: %d, bytes_read: %d\n", fd, bytes_read));
	if(bytes_read != -1) {
		buffer->position += bytes_read;
	}
	return bytes_read;
}

int Buffer_flip(Buffer *buffer)
{
	buffer->limit = buffer->position;
	buffer->position = 0;
	return 0;
}

int Buffer_vprintf(Buffer *buffer, const char *format, va_list args)
{
	//TODO what about \0 that vsnprintf adds?
	int remaining = buffer->limit - buffer->position;
	int written = vsnprintf(buffer->data + buffer->position, remaining, format, args);
	if(written > remaining) {
		Buffer_grow(buffer);
		remaining = buffer->limit - buffer->position;
		written = vsnprintf(buffer->data + buffer->position, remaining, format, args);
		//TODO check written again
		printf("TODO!\n");
		abort();
	}
	buffer->position += written;
	return 0;
}

int Buffer_printf(Buffer *buffer, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int res = Buffer_vprintf(buffer, format, args);
	va_end(args);
	return res;
}

