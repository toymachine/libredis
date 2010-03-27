#include <stdio.h>
#include <assert.h>

#include "common.h"
#include "batch.h"
#include "list.h"
#include "reply.h"
#include "connection.h"

struct _Batch
{
	struct list_head list; //for creating lists of batches

	int commands;
	struct list_head reply_queue; //finished commands that have replies set

	Buffer *write_buffer;
	Buffer *read_buffer;

	struct list_head *current; //reply iterator
};

struct _Reply
{
	struct list_head list;

	ReplyType type;

	Byte *data;
	size_t offset;
	size_t len;

	struct list_head children;
	struct list_head *current; //child iterator
};

ALLOC_LIST_T(Reply, list)

Reply *Reply_new(ReplyType type, Byte *data, size_t offset, size_t len)
{
	Reply *reply;
	Reply_list_alloc(&reply);
	reply->type = type;
	reply->data = data;
	reply->offset = offset;
	reply->len = len;
	INIT_LIST_HEAD(&reply->children);
	reply->current = &reply->children;
	return reply;
}

void _Reply_free(Reply *reply, int final)
{
	if(!final) {
		while(!list_empty(&reply->children)) {
			Reply *child = list_pop_T(Reply, list, &reply->children);
			Reply_free(child);
		}
	}
	Reply_list_free(reply, final);
}

int Reply_add_child(Reply *reply, Reply *child)
{
	list_add(&child->list, &reply->children);
	return 0;
}

int Reply_has_child(Reply *reply)
{
	return !list_empty(&reply->children);
}

Reply *Reply_pop_child(Reply *reply)
{
	return list_pop_T(Reply, list, &reply->children);
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
	ReplyType reply_type = reply->type;
	switch(reply_type) {
	case RT_OK: {
		printf("ok reply: %.*s\n", reply->len, Reply_data(reply));
		break;
	}
	case RT_BULK: {
		printf("bulk reply: %.*s\n", reply->len, Reply_data(reply));
		break;
	}
	case RT_BULK_NIL: {
		printf("bulk nil\n");
		break;
	}
	case RT_MULTIBULK: {
		printf("multi bulk reply, count: %d\n", reply->len);
		Reply *child;
		list_for_each_entry(child, &reply->children, list) {
			assert(child != NULL);
			ReplyType child_type = child->type;
			if(RT_BULK == child_type) {
				printf("\tbulk reply: %.*s\n", child->len, Reply_data(child));
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

ALLOC_LIST_T(Batch, list)

Batch *Batch_new()
{
	Batch *batch;
	if(Batch_list_alloc(&batch)) {
		batch->read_buffer = Buffer_new(DEFAULT_READ_BUFF_SIZE);
		batch->write_buffer = Buffer_new(DEFAULT_WRITE_BUFF_SIZE);
	}
	else {
		Buffer_clear(batch->read_buffer);
		Buffer_clear(batch->write_buffer);
	}
	batch->commands = 0;
	INIT_LIST_HEAD(&batch->reply_queue);
	batch->current = &batch->reply_queue;
	return batch;
}

void _Batch_free(Batch *batch, int final)
{
//	while(Batch_has_command(batch)) {
//		Command *command = Batch_next_command(batch);
//		Command_free(command);
//	}
	while(!list_empty(&batch->reply_queue)) {
		Reply *reply = list_pop_T(Reply, list, &batch->reply_queue);
		Reply_free(reply);
	}
	if(final) {
		Buffer_free(batch->read_buffer);
		Buffer_free(batch->write_buffer);
	}
	else {
		//note that we don't free the buffers, because we will re-use them
		//TODO ungrow buffers here when not final
#ifndef NDEBUG
		Buffer_fill(batch->read_buffer, (Byte)0xEA);
		Buffer_fill(batch->write_buffer, (Byte)0xEA);
#endif
	}
	Batch_list_free(batch, final);
}

void Batch_writef(Batch *batch, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Buffer_vprintf(batch->write_buffer, format, args);
	va_end(args);
}

void Batch_write(Batch *batch, const char *str, size_t str_len)
{
	Buffer_printf(batch->write_buffer, "%.*s", str_len, str);
}

void Batch_finalize(Batch *batch, int num_commands)
{
	batch->commands = num_commands;
}

int Batch_has_command(Batch *batch)
{
	return batch->commands > 0;
}


void Batch_add_reply(Batch *batch, Reply *reply)
{
	DEBUG(("pop cmd from command queue\n"));
	DEBUG(("add reply/cmd back to reply queue\n"));
	batch->commands -= 1;
	list_add(&reply->list, &batch->reply_queue);
}


int Batch_next_reply(Batch *batch, ReplyType *reply_type, char **data, size_t *len)
{
	batch->current = batch->current->next;
	if(batch->current == &batch->reply_queue) {
		*reply_type = RT_NONE;
		*data = NULL;
		*len = 0;
		return 0;
	}
	else {
		Reply *current = list_entry(batch->current, Reply, list);
		*reply_type = current->type;
//		RT_NONE = 0,
//	    RT_OK = 1,
//		RT_ERROR = 2,
//	    RT_BULK_NIL = 3,
//	    RT_BULK = 4,
//	    RT_MULTIBULK_NIL = 5,
//	    RT_MULTIBULK = 6
		if(current->type == RT_OK ||
		   current->type == RT_ERROR ||
		   current->type == RT_BULK) {
			*data = Reply_data(current);
		}
		else {
			*data = NULL;
		}
		*len = current->len;
		return 1;
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

