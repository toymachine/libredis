/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <stdio.h>

#include "libredis/redis.h"

int main(int argc, char *argv[])
{
	int error = 0;

	Module *module = Module_new();
	Module_init(module);

	//create our basic object
	Batch *batch = Batch_new();
	Connection *connection = Connection_new("127.0.0.1:6379");
	Executor *executor = Executor_new();

	//setup some Redis commands
	char *cmd;
	cmd = "SET foo 3\r\nbar\r\n";
	Batch_write(batch, cmd, strlen(cmd), 1);
	cmd = "GET foo\r\n";
	Batch_write(batch, cmd, strlen(cmd), 1);

	//associate batch with connections
	Executor_add(executor, connection, batch);

	//execute it
	if(Executor_execute(executor, 500) <= 0) {
		printf("error: %s", Module_last_error(module));
		error = 1;
	}
	else {
		//read out replies
		ReplyType reply_type;
		char *reply_data;
		size_t reply_len;
		int level;
		while((level = Batch_next_reply(batch, &reply_type, &reply_data, &reply_len))) {
			printf("level: %d, reply type: %d, data: '%.*s'\n", level, (int)reply_type, reply_len, reply_data);
		}
	}

	//release all resources
	Executor_free(executor);
	Batch_free(batch);
	Connection_free(connection);
	Module_free(module);

	return error;
}
