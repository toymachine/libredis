#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "common.h"
#include "buffer.h"

Connection *Connection_new(const char *addr, int port);
void Connection_free(Connection *connection);
int Connection_execute(Connection *connection, Batch *batch);

#endif

