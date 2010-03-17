#include <event.h>

#include "connection.h"
#include "buffer.h"
#include "list.h"
#include "batch.h"

typedef enum _ReplyType
{
    RT_OK = 1,
	RT_ERROR = 2,
    RT_BULK_NIL = 3,
    RT_BULK = 4,
    RT_MULTIBULK_NIL = 5,
    RT_MULTIBULK = 6,
} ReplyType;

typedef struct _Reply
{
	struct list_head list;

	ReplyType type;
	Buffer *buffer;
	size_t offset;
	size_t len;

	struct list_head children;

} Reply;

Reply *Reply_new();

typedef struct _Command
{
	struct list_head list;

	Batch *batch;
	Buffer *write_buffer;
	Buffer *read_buffer;
	size_t offset;
	size_t len;

	Reply *reply;

} Command;

Command *Command_new();

int Connection_add_commands(Connection *connection, struct list_head *commands);

