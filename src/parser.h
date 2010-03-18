#ifndef __PARSER_H
#define __PARSER_H

#include "reply.h"

typedef struct _ReplyParser ReplyParser;

typedef enum _ReplyParserResult
{
	RPR_NONE = 0,
    RPR_DONE = 1,
    RPR_ERROR = 2,
    RPR_MORE = 3,
    RPR_REPLY = 4,

} ReplyParserResult;

ReplyParser *ReplyParser_new();
int ReplyParser_free(ReplyParser *rp);

ReplyParserResult ReplyParser_execute(ReplyParser *rp, Byte *buffer, size_t len, Reply **reply);

#endif
