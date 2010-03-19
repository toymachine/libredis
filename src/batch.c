#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "batch.h"
#include "list.h"
#include "command.h"
#include "reply.h"

struct _Batch
{
	struct list_head cmd_queue; //commands queued for writing
	struct list_head reply_queue; //finished commands that have replies set

	Buffer *write_buffer;
	Buffer *read_buffer;
};

struct _Command
{
	struct list_head list;

	Batch *batch;
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
	DEBUG(("alloc Batch\n"));
	Batch *batch = Redis_alloc_T(Batch);
	batch->read_buffer = Buffer_new(DEFAULT_READ_BUFF_SIZE);
	batch->write_buffer = Buffer_new(DEFAULT_WRITE_BUFF_SIZE);
	INIT_LIST_HEAD(&batch->cmd_queue);
	INIT_LIST_HEAD(&batch->reply_queue);
	return batch;
}

int Batch_free(Batch *batch)
{
	assert(list_empty(&batch->cmd_queue));
	assert(list_empty(&batch->reply_queue));
	Buffer_free(batch->read_buffer);
	Buffer_free(batch->write_buffer);
	DEBUG(("dealloc Batch\n"));
	Redis_free_T(batch, Batch);
	return 0;
}


Reply *Reply_new(ReplyType type, Byte *data, size_t offset, size_t len)
{
	DEBUG(("alloc Reply\n"));
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

int Reply_free(Reply *reply)
{
	while(!list_empty(&reply->children)) {
		Reply *child = list_pop_T(Reply, list, &reply->children);
		Reply_free(child);
	}
	if(reply->cmd) {
		Command_free(reply->cmd);
	}
	DEBUG(("dealloc Reply\n"));
	Redis_free_T(reply, Reply);
	return 0;
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
	DEBUG(("alloc Command\n"));
	Command *command = Redis_alloc_T(Command);
	return command;
}

int Command_free(Command *command)
{
	DEBUG(("dealloc Command\n"));
	Redis_free_T(command, Command);
	return 0;
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

int Batch_write_command(Batch *batch, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	size_t offset = Buffer_position(batch->write_buffer);
	Buffer_vprintf(batch->write_buffer, format, args);
	va_end(args);

	Command *cmd = Command_new();
	cmd->batch = batch;

	list_add(&(cmd->list), &(batch->cmd_queue));

	return 0;
}

int Batch_has_command(Batch *batch)
{
	return !list_empty(&batch->cmd_queue);
}

Command *Batch_next_command(Batch *batch)
{
	if(Batch_has_command(batch)) {
		return list_pop_T(Command, list, &batch->cmd_queue);
	}
	else {
		return NULL;
	}
}

int Batch_add_reply(Batch *batch, Reply *reply)
{
	DEBUG(("pop cmd from command queue\n"));
	Command *cmd = Command_list_pop(&batch->cmd_queue);
	cmd->reply = reply;
	reply->cmd = cmd;
	DEBUG(("add reply/cmd back to reply queue\n"));
	list_add(&cmd->list, &batch->reply_queue);
	return 0;
}


int Batch_has_reply(Batch *batch)
{
	return !list_empty(&batch->reply_queue);
}

Reply *Batch_next_reply(Batch *batch)
{
	if(Batch_has_reply(batch)) {
		Command *cmd = list_pop_T(Command, list, &batch->reply_queue);
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



