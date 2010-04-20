/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#ifndef REPLY_H_
#define REPLY_H_

#include "redis.h"
#include "common.h"
#include "buffer.h"

Reply *Reply_new(ReplyType type, Buffer *buffer, size_t offset, size_t len);
void Reply_free(Reply *reply);
void Reply_free_final();

void Reply_add_child(Reply *reply, Reply *child);
int Reply_has_child(Reply *reply);
Reply *Reply_pop_child(Reply *reply);

void Reply_dump(Reply *reply);

#endif /* REPLY_H_ */
