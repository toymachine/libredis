#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "buffer.h"

typedef struct _Connection Connection;

Connection *Connection_new(const char *addr, int port);

#endif

