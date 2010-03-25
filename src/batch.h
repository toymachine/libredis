#ifndef __BATCH_H
#define __BATCH_H

#include "redis.h"
#include "common.h"
#include "command.h"
#include "reply.h"
#include "buffer.h"

void Batch_free_final();

//commands
int Batch_has_command(Batch *batch);
Command *Batch_next_command(Batch *batch);

//replies
void Batch_add_reply(Batch *batch, Reply *reply);
int Batch_has_reply(Batch *batch);
Reply *Batch_pop_reply(Batch *batch);

//buffers (private interface to connection)
Buffer *Batch_read_buffer(Batch *batch);
Buffer *Batch_write_buffer(Batch *batch);

#endif
