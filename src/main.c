#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <event.h>

#include "redis.h"
#include "batch.h"
#include "parser.h"
#include "reply.h"
#include "assert.h"

int main(void) {
	event_init();

	Connection *connection = Connection_new("127.0.0.1", 6379);

	Batch *batch = Batch_new();
	Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "blaat", 3, "aap");
	Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "piet", 7, "jaapaap");
	Batch_write_command(batch, "GET %s\r\n", "blaat");
	Batch_write_command(batch, "MGET %s %s %s\r\n", "blaat", "piet", "boe");
	Batch_write_command(batch, "GET %s\r\n", "blaat2");

	Batch_execute(batch, connection);

	event_dispatch();

	printf("after dispatch\n");

	while(Batch_has_result(batch)) {
		Reply *reply = Batch_next_result(batch);
		Reply_dump(reply);
		Reply_free(reply);
	}

	printf("normal main done!\n");

	return 0;
}
