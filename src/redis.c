#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#include <unistd.h>

#include "common.h"
#include "connection.h"

typedef struct _Redis
{
	Connection *connections[MAX_CONNECTIONS]; //list of connections
	int connection_count;
} Redis;

Redis *Redis_new(Alloc *alloc)
{
	Redis *redis = alloc->alloc(sizeof(Redis));
	int i;
	for(i = 0; i < MAX_CONNECTIONS; i++) {
		redis->connections[i] = NULL;
	}
	redis->connection_count = 0;
	return redis;
}

int Redis_add_connection(Redis *redis, Connection *connection)
{
	redis->connections[redis->connection_count] = connection;
	redis->connection_count++;
}

int Redis_execute(Redis *redis)
{
	int i;
	//prepare all connections for writing
	for(i = 0; i < redis->connection_count; i++) {
		Connection *connection = redis->connections[i];
		//Buffer_flip(&connection->buffer);
	}
}


