#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common.h"
#include "event.h"
#include "batch.h"
#include "parser.h"
#include "connection.h"
#include "reply.h"
#include "assert.h"
#include "ketama.h"

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

void test_simple()
{
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
}

void test_ketama()
{
	Ketama *ketama = Ketama_new();

	Ketama_add_server(ketama, "10.0.1.1", 11211, 600);
	Ketama_add_server(ketama, "10.0.1.2", 11211, 300);
	Ketama_add_server(ketama, "10.0.1.3", 11211, 200);
	Ketama_add_server(ketama, "10.0.1.4", 11211, 350);
	Ketama_add_server(ketama, "10.0.1.5", 11211, 1000);
	Ketama_add_server(ketama, "10.0.1.6", 11211, 800);
	Ketama_add_server(ketama, "10.0.1.7", 11211, 950);
	Ketama_add_server(ketama, "10.0.1.8", 11211, 100);

	Ketama_create_continuum(ketama);

	Ketama_free(ketama);
}

int main(void) {
	event_init();

	test_ketama();

	//release the freelists
	Reply_free_final();
	Command_free_final();
	Batch_free_final();

	printf("normal main done! allocated: %d\n", allocated);

	return 0;
}
