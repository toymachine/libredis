#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "event.h"
#include "batch.h"
#include "parser.h"
#include "connection.h"
#include "reply.h"
#include "assert.h"

#ifdef NDEBUG
	#define N 10000
	#define M 100
#else
	#define N 2
	#define M 1
#endif

Batch *create_get_batch(int n)
{
	Batch *batch = Batch_new();
	//Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "blaat", 3, "aap");
	//Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "piet", 7, "jaapaap");
	int i;
	for(i = 0; i < n; i++) {
		Batch_write_command(batch, "GET %s\r\n", "blaat");
	}
	//Batch_write_command(batch, "MGET %s %s %s\r\n", "blaat", "piet", "boe");
	//Batch_write_command(batch, "GET %s\r\n", "blaat2");
	return batch;
}

void read_replies_and_free_batch(Batch *batch)
{
	while(Batch_has_reply(batch)) {
		Reply *reply = Batch_next_reply(batch);
#ifndef NDEBUG
		Reply_dump(reply);
#endif
		Reply_free(reply);
	}

	Batch_free(batch);
}

int main(void) {
	event_init();

	Connection *connection = Connection_new("127.0.0.1", 6379);
    Connection *connection2 = Connection_new("127.0.0.1", 6380);

	int i;
	for(i = 0; i < N; i++) {
		DEBUG(("round %d\n", i));

		Batch *batch = create_get_batch(M);
		Batch *batch2 = create_get_batch(M);

		Connection_execute(connection, batch);
		Connection_execute(connection2, batch2);

		DEBUG(("before dispatch\n"));

		event_dispatch();

		DEBUG(("after dispatch\n"));

		read_replies_and_free_batch(batch);
		read_replies_and_free_batch(batch2);
	}

	Connection_free(connection);
	Connection_free(connection2);

	//release the freelists
	Reply_free_final();
	Command_free_final();
	Batch_free_final();

	printf("normal main done! %d\n", N * M * 2);

	return 0;
}
