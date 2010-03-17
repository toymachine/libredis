#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "batch.h"
#include "list.h"
#include "command.h"
#include "reply.h"

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

	size_t offset;
	size_t len;

	Reply *reply;

};

struct _Reply
{
	struct list_head list;

	ReplyType type;
	Command *cmd;

	Byte *data;
	size_t offset;
	size_t len;

	struct list_head children;
	struct list_head *current;
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

Reply *Reply_new(ReplyType type, Byte *data, size_t offset, size_t len)
{
	Reply *reply = Redis_alloc_T(Reply);
	reply->type = type;
	reply->data = data;
	reply->offset = offset;
	reply->len = len;
	reply->cmd = NULL;
	INIT_LIST_HEAD(&reply->children);
	reply->current = &reply->children;
	return reply;
}

int Reply_add_child(Reply *reply, Reply *child)
{
	list_add_tail(&child->list, &reply->children);
	return 0;
}

int Reply_next_child(Reply *reply, Reply **child)
{
	reply->current = reply->current->next;
	if(reply->current == &reply->children) {
		*child = NULL;
		return 0;
	}
	else {
		*child = list_entry(reply->current, Reply, list);
		return 1;
	}
}

size_t Reply_length(Reply *reply)
{
	return reply->len;
}

Byte *Reply_data(Reply *reply)
{
	assert(reply->data != NULL);
	return reply->data + reply->offset;
}

ReplyType Reply_type(Reply *reply)
{
	return reply->type;
}

int Reply_dump(Reply *reply) {
	ReplyType reply_type = Reply_type(reply);
	switch(reply_type) {
	case RT_OK: {
		printf("ok reply: %.*s\n", Reply_length(reply), Reply_data(reply));
		break;
	}
	case RT_BULK: {
		printf("bulk reply: %.*s\n", Reply_length(reply), Reply_data(reply));
		break;
	}
	case RT_BULK_NIL: {
		printf("bulk nil\n");
		break;
	}
	case RT_MULTIBULK: {
		printf("multi bulk reply, count: %d\n", Reply_length(reply));
		Reply *child = NULL;
		while(Reply_next_child(reply, &child)) {
			assert(child != NULL);
			ReplyType child_type = Reply_type(child);
			if(RT_BULK == child_type) {
				printf("\tbulk reply: %.*s\n", Reply_length(child), Reply_data(child));
			}
			else if(RT_BULK_NIL == child_type) {
				printf("\tbulk nil\n");
			}
			else {
				assert(0);
			}
		}
		break;
	}
	default:
		printf("unknown reply %d\n", reply_type);
	}
	return 0;
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

Reply *Command_reply(Command *cmd)
{
	return cmd->reply;
}

Batch *Command_batch(Command *cmd)
{
	return cmd->batch;
}

int Command_prepare_buffer(Command *cmd)
{
	Buffer *buffer = cmd->batch->write_buffer;
	Buffer_set_position(buffer, cmd->offset);
	Buffer_set_limit(buffer, cmd->offset + cmd->len);
	return 0;
}

int Command_add_reply(Command *cmd, Reply *reply)
{
	cmd->reply = reply;
	reply->cmd = cmd;
	printf("add cmd back to batch, len: %d, off: %d\n", cmd->len, cmd->offset);
	list_add(&cmd->list, &cmd->batch->read_queue);
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
	return !list_empty(&batch->read_queue);
}

Reply *Batch_next_result(Batch *batch)
{
	if(Batch_has_result(batch)) {
		Command *cmd = list_pop_T(Command, list, &batch->read_queue);
		return cmd->reply;
	}
	else {
		return NULL;
	}
}

Buffer *Batch_read_buffer(Batch *batch)
{
	return batch->read_buffer;
}

Buffer *Batch_write_buffer(Batch *batch)
{
	return batch->write_buffer;
}



