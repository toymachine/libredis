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
#include "executor.h"
#include "module.h"

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

/*
void test_executor()
{
	Ketama *ketama = Ketama_new();
	Ketama_add_server(ketama, "10.0.1.1", 11211, 600);
	Ketama_add_server(ketama, "10.0.1.2", 11211, 600);

	Executor *executor = Executor_new();

	Executor_set_server_hash_method(executor, Ketama_get_hash_method(ketama));

	Executor_start_command(executor, "MGET ");

	int ord = 0;
	Executor_write(executor, ord++, "blaat1", 6, "%s ", "blaat1");
	Executor_write(executor, ord++, "blaat2", 6, "%s ", "blaat1");
	Executor_write(executor, ord++, "blaat3", 6, "%s ", "blaat1");
	Executor_write(executor, ord++, "blaat4", 6, "%s ", "blaat1");

	Executor_end_command(executor, "\r\n");

	//Executor_write_command(executor, ord++, "blaat1", 6, "GET %s\r\n", "blaat1");
	//Executor_write_command(executor, ord++, "blaat2", 6, "GET %s\r\n", "blaat2");
	//Executor_write_command(executor, ord++, "blaat3", 6, "GET %s\r\n", "blaat3");

	Executor_execute(executor);
	while(Executor_has_reply(executor)) {
		Reply *reply = Executor_next_reply(executor);
#ifndef NDEBUG
		Reply_dump(reply);
#endif
		Reply_free(reply);
	}

	Executor_free(executor);
	Ketama_free(ketama);
}
*/

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

#ifndef NDEBUG
	Ketama_print_continuum(ketama);
#endif

	assert(0 == strcmp("10.0.1.7:11211", Ketama_get_server(ketama, "12936", 5)));
	assert(0 == strcmp("10.0.1.5:11211", Ketama_get_server(ketama, "27804", 5)));
	assert(0 == strcmp("10.0.1.2:11211", Ketama_get_server(ketama, "37045", 5)));
	assert(0 == strcmp("10.0.1.1:11211", Ketama_get_server(ketama, "50829", 5)));
	assert(0 == strcmp("10.0.1.6:11211", Ketama_get_server(ketama, "65422", 5)));
	assert(0 == strcmp("10.0.1.6:11211", Ketama_get_server(ketama, "74912", 5)));

	Ketama_free(ketama);
}

int main(void) {
	Module_init();

	test_ketama();
	//test_executor();

	Module_free();

	printf("normal main done! allocated: %d\n", allocated);

	return 0;
}
