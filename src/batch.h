/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

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
