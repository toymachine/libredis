#ifndef REPLY_H_
#define REPLY_H_

#include "redis.h"
#include "common.h"

Reply *Reply_new(ReplyType type, Byte *data, size_t offset, size_t len);
void Reply_free(Reply *reply);
void Reply_free_final();

int Reply_add_child(Reply *reply, Reply *child);
int Reply_has_child(Reply *reply);
Reply *Reply_pop_child(Reply *reply);

/*
ReplyType Reply_type(Reply *reply);
size_t Reply_length(Reply *reply);
Byte *Reply_data(Reply *reply);
*/

int Reply_dump(Reply *reply);

#endif /* REPLY_H_ */
