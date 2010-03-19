#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "batch.h"
#include "list.h"
#include "command.h"
#include "reply.h"

struct _Batch
{
	struct list_head list; //for creating lists of batches

	struct list_head cmd_queue; //commands queued for writing
	struct list_head reply_queue; //finished commands that have replies set

	Buffer *write_buffer;
	Buffer *read_buffer;
};

struct _Command
{
	struct list_head list;
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

ALLOC_LIST_T(Batch, list);

Batch *Batch_new()
{
	Batch *batch;
	if(Batch_alloc(&batch)) {
		batch->read_buffer = Buffer_new(DEFAULT_READ_BUFF_SIZE);
		batch->write_buffer = Buffer_new(DEFAULT_WRITE_BUFF_SIZE);
	}
	else {
		Buffer_clear(batch->read_buffer);
		Buffer_clear(batch->write_buffer);
	}
	INIT_LIST_HEAD(&batch->cmd_queue);
	INIT_LIST_HEAD(&batch->reply_queue);
	return batch;
}

int Batch_free(Batch *batch)
{
	assert(list_empty(&batch->cmd_queue));
	assert(list_empty(&batch->reply_queue));
	//note that we don't free the buffers, because we will re-use them
	//TODO ungrow buffers here
	Batch_dealloc(batch);
	return 0;
}

//static struct list_head reply_free_list = LIST_HEAD_INIT(reply_free_list);
ALLOC_LIST_T(Reply, list)

Reply *Reply_new(ReplyType type, Byte *data, size_t offset, size_t len)
{
	//Alloc_alloc_list_T(Reply, list, reply, &reply_free_list);
	Reply *reply;
	Reply_alloc(&reply);
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
	Reply_dealloc(reply);
	//Alloc_free_list_T(Reply, list, reply, &reply_free_list);
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

ALLOC_LIST_T(Command, list);

Command *Command_new()
{
	Command *command;
	Command_alloc(&command);
	command->reply = NULL;
	return command;
}

int Command_free(Command *command)
{
	Command_dealloc(command);
	return 0;
}


int Batch_write_command(Batch *batch, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	size_t offset = Buffer_position(batch->write_buffer);
	Buffer_vprintf(batch->write_buffer, format, args);
	va_end(args);

	Command *cmd = Command_new();

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
	Command *cmd = list_pop_T(Command, list, &batch->cmd_queue);
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



