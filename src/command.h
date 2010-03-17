#ifndef __COMMAND_H
#define __COMMAND_H

#include <event.h>

#include "connection.h"
#include "buffer.h"
#include "list.h"
#include "batch.h"

typedef struct _Command
{
	struct list_head list;

	Batch *batch;
	Buffer *write_buffer;
	Buffer *read_buffer;
	size_t offset;
	size_t len;

	Reply *reply;

} Command;

Command *Command_new();

int Connection_add_commands(Connection *connection, struct list_head *commands);
int Batch_add_reply(Batch *batch, Command *cmd);

#endif
