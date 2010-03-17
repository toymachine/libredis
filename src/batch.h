#ifndef __BATCH_H
#define __BATCH_H

#include "common.h"
#include "connection.h"
#include "command.h"

Batch *Batch_new();
int Batch_write_command(Batch *batch, const char *format, ...);
int Batch_execute(Batch *batch, Connection *connection);
int Batch_has_result(Batch *batch);
Command *Batch_next_result(Batch *batch);

#endif
