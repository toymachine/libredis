#ifndef __BATCH_H
#define __BATCH_H

#include "redis.h"
#include "common.h"
#include "reply.h"
#include "buffer.h"

void Batch_free_final();

//commands
int Batch_has_command(Batch *batch);

//replies
void Batch_add_reply(Batch *batch, Reply *reply);

//buffers (private interface to connection)
Buffer *Batch_read_buffer(Batch *batch);
Buffer *Batch_write_buffer(Batch *batch);

void Batch_abort(Batch *batch, const char *error);

#endif
