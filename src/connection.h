#ifndef __CONNECTION_H
#define __CONNECTION_H

#include "redis.h"
#include "common.h"
#include "buffer.h"
#include "event.h"

void Connection_abort(Connection *connection, const char *format,  ...);
void Connection_execute(Connection *connection, Batch *batch);
void Connection_event_add(Connection *connection, struct event *event, long int tv_sec, long int tv_usec);
void Connection_write_data(Connection *connection);
void Connection_read_data(Connection *connection);

#endif

