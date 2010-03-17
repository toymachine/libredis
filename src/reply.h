#ifndef REPLY_H_
#define REPLY_H_

#include "common.h"

typedef enum _ReplyType
{
    RT_OK = 1,
	RT_ERROR = 2,
    RT_BULK_NIL = 3,
    RT_BULK = 4,
    RT_MULTIBULK_NIL = 5,
    RT_MULTIBULK = 6,
} ReplyType;

Reply *Reply_new(ReplyType type, Buffer *buffer, size_t offset, size_t len);
ReplyType Reply_type(Reply *reply);

#endif /* REPLY_H_ */
