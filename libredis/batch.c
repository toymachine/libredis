/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <stdio.h>
#include <assert.h>
#include <string.h>

#include "common.h"
#include "alloc.h"
#include "batch.h"
#include "list.h"
#include "reply.h"
#include "connection.h"

#define BATCH_REPLY_ITERATOR_STACK_SIZE 2

struct _Batch
{
#ifdef SINGLETHREADED
    struct list_head list; //for creating lists of batches
#endif

    int num_commands;
    struct list_head reply_queue; //finished commands that have replies set

    Buffer *write_buffer;
    Buffer *read_buffer;

    //simple stack for iterating results
    int current_reply_sp;
    struct list_head *current_reply[BATCH_REPLY_ITERATOR_STACK_SIZE * 2];

    //error for aborted batch
    Buffer *error;
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

ALLOC_LIST_T(Reply, list)

Reply *Reply_new(ReplyType type, Buffer *buffer, size_t offset, size_t len)
{
    Reply *reply;
    Reply_list_alloc(&reply);
    DEBUG(("Reply_new, type: %d\n", type));
    reply->type = type;
    reply->buffer = buffer;
    reply->offset = offset;
    reply->len = len;
    INIT_LIST_HEAD(&reply->children);
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

void Reply_add_child(Reply *reply, Reply *child)
{
    list_add_tail(&child->list, &reply->children);
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
    assert(reply->buffer != NULL);
    return Buffer_data(reply->buffer) + reply->offset;
}

ReplyType Reply_type(Reply *reply)
{
    return reply->type;
}

void Reply_dump(Reply *reply) {
    ReplyType reply_type = reply->type;
    switch(reply_type) {
    case RT_OK: {
        printf("ok reply: %.*s\n", (int)reply->len, Reply_data(reply));
        break;
    }
    case RT_BULK: {
        printf("bulk reply: %.*s\n", (int)reply->len, Reply_data(reply));
        break;
    }
    case RT_BULK_NIL: {
        printf("bulk nil\n");
        break;
    }
    case RT_MULTIBULK: {
        printf("multi bulk reply, count: %d\n", (int)reply->len);
        Reply *child;
        list_for_each_entry(child, &reply->children, list) {
            assert(child != NULL);
            ReplyType child_type = child->type;
            if(RT_BULK == child_type) {
                printf("\tbulk reply: %.*s\n", (int)child->len, Reply_data(child));
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
}

ALLOC_LIST_T(Batch, list)

Batch *Batch_new()
{
    Batch *batch;
    if(Batch_list_alloc(&batch)) {
        batch->read_buffer = Buffer_new(DEFAULT_READ_BUFF_SIZE);
        batch->write_buffer = Buffer_new(DEFAULT_WRITE_BUFF_SIZE);
    }
    batch->num_commands = 0;
    INIT_LIST_HEAD(&batch->reply_queue);

    batch->current_reply_sp = 0;
    batch->current_reply[0] = &batch->reply_queue;
    batch->current_reply[1] = &batch->reply_queue;

    batch->error = NULL;

    return batch;
}

void _Batch_free(Batch *batch, int final)
{
    DEBUG(("_Batch_free\n"));
    while(!list_empty(&batch->reply_queue)) {
        Reply *reply = list_pop_T(Reply, list, &batch->reply_queue);
        Reply_free(reply);
    }
    if(final) {
        DEBUG(("_Batch_free final\n"));
        Buffer_free(batch->read_buffer);
        Buffer_free(batch->write_buffer);
    }
    else {
        DEBUG(("_Batch_free re-use\n"));
        Buffer_clear(batch->read_buffer);
        Buffer_clear(batch->write_buffer);
#ifndef NDEBUG
        Buffer_fill(batch->read_buffer, (Byte)0xEA);
        Buffer_fill(batch->write_buffer, (Byte)0xEA);
#else
        Buffer_fill(batch->read_buffer, 0);
        Buffer_fill(batch->write_buffer, 0);
#endif
    }
    if (batch->error) {
        Buffer_free(batch->error);
        batch->error = NULL;
    }
    Batch_list_free(batch, final);
}

void Batch_write(Batch *batch, const char *str, size_t str_len, int num_commands)
{
    if(str != NULL && str_len > 0) {
        Buffer_write(batch->write_buffer, str, str_len);
    }
    batch->num_commands += num_commands;
}

void Batch_write_decimal(Batch *batch, long decimal)
{
    char buff[32];
    int buff_len = snprintf(buff, 32, "%ld", decimal);
    Batch_write(batch, buff, buff_len, 0);
}

void Batch_write_set(Batch *batch, const char *key, int key_len, const char *value, int value_len)
{
    Batch_write(batch, "SET ", 4, 0);
    Batch_write(batch, key, key_len, 0);
    Batch_write(batch, " ", 1, 0);
    Batch_write_decimal(batch, value_len);
    Batch_write(batch, "\r\n", 2, 0);
    Batch_write(batch, value, value_len, 0);
    Batch_write(batch, "\r\n", 2, 1);
}

void Batch_write_get(Batch *batch, const char *key, int key_len)
{
    Batch_write(batch, "GET ", 4, 0);
    Batch_write(batch, key, key_len, 0);
    Batch_write(batch, "\r\n", 2, 1);
}



int Batch_has_command(Batch *batch)
{
    return batch->num_commands > 0;
}


void Batch_add_reply(Batch *batch, Reply *reply)
{
    DEBUG(("pop cmd from command queue\n"));
    DEBUG(("add reply/cmd back to reply queue\n"));
    batch->num_commands -= 1;
    list_add_tail(&reply->list, &batch->reply_queue);
}

char *Batch_error(Batch *batch)
{
    if(batch->error) {
        return Buffer_data(batch->error);
    }
    else {
        return NULL;
    }
}

void Batch_abort(Batch *batch, const char *error)
{
    DEBUG(("Batch abort\n"));
    assert(batch->error == NULL);
    int length = strlen(error) + 1; //include terminating null character
    batch->error = Buffer_new(length);
    Buffer_write(batch->error, error, length);
    while(Batch_has_command(batch)) {
        DEBUG(("Batch abort, adding error reply\n"));
        Batch_add_reply(batch, Reply_new(RT_ERROR, batch->error, 0, length));
    }
}


int Batch_next_reply(Batch *batch, ReplyType *reply_type, char **data, size_t *len)
{
    DEBUG(("Batch_next_reply\n"));

    if(reply_type == NULL || data == NULL || len == NULL) {
        Module_set_error(GET_MODULE(), "Invalid argument");
        return -1;
    }

    *reply_type = RT_NONE;
    *data = NULL;
    *len = 0;

    struct list_head *current = batch->current_reply[batch->current_reply_sp];
    struct list_head *last = batch->current_reply[batch->current_reply_sp + 1];

    current = current->next;

    batch->current_reply[batch->current_reply_sp] = current;

    if(current == last) {
        if(batch->current_reply_sp > 0) {
            DEBUG(("Batch_next_reply, current == last, pop\n"));
            batch->current_reply_sp -= 2;
            assert(batch->current_reply_sp >= 0);
            current = batch->current_reply[batch->current_reply_sp];
            last = batch->current_reply[batch->current_reply_sp + 1];
            if(current == last) {
                //ok were also at the end at this level
                return 0;
            }
        }
        else {
            DEBUG(("Batch_next_reply, current == last, end\n"));
            return 0;
        }
    }

    int level = (batch->current_reply_sp / 2) + 1;
    Reply *current_reply = list_entry(current, Reply, list);
    *reply_type = current_reply->type;
//		RT_NONE = 0,
//	    RT_OK = 1,
//		RT_ERROR = 2,
//	    RT_BULK_NIL = 3,
//	    RT_BULK = 4,
//	    RT_MULTIBULK_NIL = 5,
//	    RT_MULTIBULK = 6
//		RT_INTEGER = 7
    if(current_reply->type == RT_OK ||
       current_reply->type == RT_ERROR ||
       current_reply->type == RT_BULK ||
       current_reply->type == RT_INTEGER) {
        *data = Reply_data(current_reply);
    }
    else if(current_reply->type == RT_MULTIBULK) {
        *data = NULL;
        batch->current_reply[batch->current_reply_sp] = current->next;
        batch->current_reply_sp += 2;
        assert(batch->current_reply_sp <= (BATCH_REPLY_ITERATOR_STACK_SIZE * 2));
        batch->current_reply[batch->current_reply_sp] = &current_reply->children;
        batch->current_reply[batch->current_reply_sp + 1] = &current_reply->children;
    }
    else if(current_reply->type == RT_NONE ||
             current_reply->type == RT_BULK_NIL ||
             current_reply->type == RT_MULTIBULK_NIL) {
        *data = NULL;
    }
    else {
        assert(0);
    }
    *len = current_reply->len;
    return level;
}


Buffer *Batch_read_buffer(Batch *batch)
{
    return batch->read_buffer;
}

Buffer *Batch_write_buffer(Batch *batch)
{
    return batch->write_buffer;
}

