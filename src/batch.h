#ifndef __BATCH_H
#define __BATCH_H

#include "common.h"
#include "connection.h"
#include "command.h"

Batch *Batch_new();
int Batch_free(Batch *batch);

int Batch_write_command(Batch *batch, const char *format, ...);
int Batch_has_command(Batch *batch);
Command *Batch_next_command(Batch *batch);

int Batch_add_reply(Batch *batch, Reply *reply);
int Batch_has_reply(Batch *batch);
Reply *Batch_next_reply(Batch *batch);
Buffer *Batch_read_buffer(Batch *batch);
Buffer *Batch_write_buffer(Batch *batch);

#endif
