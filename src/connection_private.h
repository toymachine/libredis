#include <event.h>

#include "connection.h"
#include "buffer.h"
#include "list.h"
#include "batch.h"

typedef struct _Command
{
	struct list_head list;

	Batch *batch;
	Buffer *buffer;
	Buffer *read_buffer;
	size_t offset;
	size_t len;
} Command;

int Connection_add_commands(Connection *connection, struct list_head *commands);

