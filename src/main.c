#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <event.h>

#include "batch.h"
#include "parser.h"
#include "reply.h"
#include "assert.h"

#define N 10
#define M 1

int main(void) {
	event_init();

	Connection *connection = Connection_new("127.0.0.1", 6379);
//  Connection *connection2 = Connection_new("127.0.0.1", 6380);

	int i,j;
	for(i = 0; i < N; i++) {
		DEBUG(("round %d\n", i));

		Batch *batch = Batch_new();
		//Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "blaat", 3, "aap");
		//Batch_write_command(batch, "%s %s %d\r\n%s\r\n", "SET", "piet", 7, "jaapaap");
		for(j = 0; j < M; j++) {
			Batch_write_command(batch, "GET %s\r\n", "blaat");
		}
		//Batch_write_command(batch, "MGET %s %s %s\r\n", "blaat", "piet", "boe");
		//Batch_write_command(batch, "GET %s\r\n", "blaat2");

		Connection_execute(connection, batch);

		DEBUG(("before dispatch\n"));

		event_dispatch();

		DEBUG(("after dispatch\n"));

		while(Batch_has_reply(batch)) {
			Reply *reply = Batch_next_reply(batch);
#ifndef NDEBUG
			Reply_dump(reply);
#endif
			Reply_free(reply);
		}

		Batch_free(batch);
	}

	Connection_free(connection);

	printf("normal main done! %d\n", N * M);

	return 0;
}
