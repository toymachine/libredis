#include "common.h"
#include "batch.h"
#include "list.h"
#include "command.h"
#include "reply.h"
#include <stdio.h>

struct _Batch
{
	struct list_head write_queue; //commands queued for writing
	struct list_head read_queue; //commands retired after reading
	Buffer *write_buffer;
	Buffer *read_buffer;
};

struct _Command
{
	struct list_head list;

	Batch *batch;
	Buffer *write_buffer;
	Buffer *read_buffer;
	size_t offset;
	size_t len;

	Reply *reply;

};

struct _Reply
{
	struct list_head list;

	ReplyType type;
	Buffer *buffer;
	size_t offset;
	size_t len;

	struct list_head children;

};

Batch *Batch_new()
{
	Batch *batch = Redis_alloc_T(Batch);
	batch->read_buffer = Buffer_new(DEFAULT_READ_BUFF_SIZE);
	batch->write_buffer = Buffer_new(DEFAULT_WRITE_BUFF_SIZE);
	INIT_LIST_HEAD(&batch->write_queue);
	INIT_LIST_HEAD(&batch->read_queue);
	return batch;
}

Reply *Reply_new(ReplyType type, Buffer *buffer, size_t offset, size_t len)
{
	Reply *reply = Redis_alloc_T(Reply);
	reply->type = type;
	reply->buffer = buffer;
	reply->offset = offset;
	reply->len = len;
	INIT_LIST_HEAD(&reply->children);
	return reply;
}

ReplyType Reply_type(Reply *reply)
{
	return reply->type;
}


Command *Command_new()
{
	Command *command = Redis_alloc_T(Command);
	return command;
}

Command *Command_list_last(struct list_head *head)
{
	struct list_head *pos = list_last(head);
	return list_entry(pos, Command, list);
}

Command *Command_list_pop(struct list_head *head)
{
	struct list_head *pos = list_pop(head);
	return list_entry(pos, Command, list);
}

Buffer *Command_read_buffer(Command *cmd)
{
	return cmd->read_buffer;
}

Buffer *Command_write_buffer(Command *cmd)
{
	return cmd->write_buffer;
}

int Command_flip_buffer(Command *cmd)
{
	Buffer *buffer = cmd->write_buffer;
	Buffer_set_position(buffer, cmd->offset);
	Buffer_set_limit(buffer, cmd->offset + cmd->len);
}

int Command_reply(Command *cmd, Reply *reply)
{
	cmd->reply = reply;
	list_add_tail(&(cmd->list), &(cmd->batch->read_queue));
	return 0;
}

int Batch_write_command(Batch *batch, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	size_t offset = Buffer_position(batch->write_buffer);
	Buffer_vprintf(batch->write_buffer, format, args);
	size_t len = Buffer_position(batch->write_buffer) - offset;
	va_end(args);

	Command *cmd = Command_new();
	cmd->batch = batch;
	cmd->write_buffer = batch->write_buffer;
	cmd->read_buffer = batch->read_buffer;
	cmd->offset = offset;
	cmd->len = len;

	list_add(&(cmd->list), &(batch->write_queue));

	return 0;
}

int Batch_execute(Batch *batch, Connection *connection)
{
	printf("batch execute\n");
	Connection_add_commands(connection, &batch->write_queue);
	return 0;
}

int Batch_has_result(Batch *batch)
{
	return !list_empty(&(batch->read_queue));
}

Command *Batch_next_result(Batch *batch)
{
	if(Batch_has_result(batch)) {
		return list_pop_T(Command, list, &(batch->read_queue));
	}
	else {
		return NULL;
	}
}


