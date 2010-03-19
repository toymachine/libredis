#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <event.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include "common.h"
#include "buffer.h"
#include "connection.h"
#include "command.h"
#include "reply.h"
#include "parser.h"
#include "batch.h"

typedef enum _ConnectionState
{
    CS_CLOSED = 0,
    CS_CONNECTING = 1,
    CS_CONNECTED = 2
} ConnectionState;


struct _Connection
{
	const char *addr;
	int port;
	int sockfd;
	ConnectionState state;
	struct event event_read;
	struct event event_write;
	Batch *current_batch;
	ReplyParser *parser;
};

void Connection_handle_event(int fd, short flags, void *data);

Connection *Connection_new(const char *addr, int port)
{
	DEBUG(("alloc Connection\n"));
	Connection *connection = Alloc_alloc_T(Connection);
	connection->state = CS_CLOSED;

	connection->current_batch = NULL;
	connection->parser = ReplyParser_new();

	//socket stuff:
	connection->addr = addr;
	connection->port = port;
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connection->sockfd == -1) {
		printf("could not create socket\n");
		abort();
	}
	event_set(&connection->event_read, connection->sockfd, EV_READ, &Connection_handle_event, (void *)connection);
	event_set(&connection->event_write, connection->sockfd, EV_WRITE, &Connection_handle_event, (void *)connection);
	//set socket in non-blocking mode
	int flags;
	if ((flags = fcntl(connection->sockfd, F_GETFL, 0)) < 0)
	{
		printf("TODO error on nonblock fcntl\n");
		abort();
	}
	if (fcntl(connection->sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		printf("TODO error on nonblock fcntl\n");
		abort();
	}
	//set nodelay option
	/*
	int nodelay = 1;
	setsockopt(connection->sockfd, SOL_TCP, TCP_NODELAY, &nodelay, sizeof(nodelay));
	*/

	return connection;
}

int Connection_free(Connection *connection)
{
	ReplyParser_free(connection->parser);
	DEBUG(("dealloc Connection\n"));
	Alloc_free_T(connection, Connection);
	return 0;
}

void Connection_event_add(Connection *connection, struct event *event, long int tv_sec, long int tv_usec)
{
	struct timeval tv;
	tv.tv_sec = tv_sec;
	tv.tv_usec = tv_usec;
	int res = event_add(event, &tv);
	DEBUG(("connection ev add: fd: %d, type: %c, res: %d\n", connection->sockfd, event == &connection->event_read ? 'R' : 'W', res));
}



int Connection_connect(Connection *connection)
{
	struct sockaddr_in sa;
	sa.sin_family = AF_INET;
	sa.sin_port = htons(connection->port);
	inet_pton(AF_INET, connection->addr, &sa.sin_addr);
	return connect(connection->sockfd, (struct sockaddr *) &sa, sizeof(struct sockaddr));
}

void Connection_write_data(Connection *connection)
{
	DEBUG(("connection write_data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);

	if(CS_CLOSED == connection->state) {
		if(-1 == Connection_connect(connection)) {
			//open the connection
			if(EINPROGRESS == errno) {
				//normal async connect
				connection->state = CS_CONNECTING;
				DEBUG(("async connecting\n"));
				Connection_event_add(connection, &connection->event_write, 0, 400000);
				return;
			}
			else {
				DEBUG(("abort on connect, errno: %d\n", errno));
				abort(); //TODO
			}
		}
		else {
			//immediate connect succeeded
			DEBUG(("sync connected\n"));
			connection->state = CS_CONNECTED;
		}
	}

	if(CS_CONNECTING == connection->state) {
		//now check for error to see if we are really connected
		int error;
		socklen_t len = sizeof(int);
		getsockopt(connection->sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
		if(error != 0) {
			printf("connect error: %d\n", error);
			abort();
		}
		else {
			connection->state = CS_CONNECTED;
		}
	}

	if(CS_CONNECTED == connection->state) {

		Buffer *buffer = Batch_write_buffer(connection->current_batch);
		while(Buffer_remaining(buffer)) {
			//still something to write
			size_t res = Buffer_send(buffer, connection->sockfd);
			DEBUG(("bfr send res: %d\n", res));
			if(res == -1) {
				if(errno == EAGAIN) {
					Connection_event_add(connection, &connection->event_write, 0, 400000);
					return;
				}
				else {
					printf("send error, errno: %d\n", errno);
					abort();
				}
			}
		}

	}
}

int Connection_execute(Connection *connection, Batch *batch)
{
	DEBUG(("Connection exec\n"));

	connection->current_batch = batch;
	ReplyParser_reset(connection->parser);
	Buffer_flip(Batch_write_buffer(batch));

	DEBUG(("Connection exec write buff:\n"));
#ifndef NDEBUG
	Buffer_dump(Batch_write_buffer(batch), 128);
#endif

	//kick off writing:
	Connection_write_data(connection);
	//kick off reading:
	Connection_read_data(connection);

	return 0;
}

void Connection_read_data(Connection *connection)
{
	DEBUG(("connection read data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);

	Buffer *buffer = Batch_read_buffer(connection->current_batch);
	assert(buffer != NULL);

	while(Batch_has_command(connection->current_batch)) {
		DEBUG(("exec rp\n"));
		Reply *reply = NULL;
		ReplyParserResult rp_res = ReplyParser_execute(connection->parser, Buffer_data(buffer), Buffer_position(buffer), &reply);
		switch(rp_res) {
		case RPR_ERROR: {
			printf("result parse error!");
			abort();
		}
		case RPR_MORE: {
			size_t res = Buffer_recv(buffer, connection->sockfd);
#ifndef NDEBUG
		Buffer_dump(buffer, 128);
#endif
			if(res == -1) {
				if(errno == EAGAIN) {
					Connection_event_add(connection, &connection->event_read, 0, 400000);
					goto exit;
				}
				else {
					printf("unhandeld read io err: %d\n", errno);
					abort(); //TODO
				}
			}
			break;
		}
		case RPR_REPLY: {
			Batch_add_reply(connection->current_batch, reply);
			break;
		}
		default:
			printf("unexpected/unhandled rp result: %d\n", rp_res);
			abort();
		}

	}
exit:
	DEBUG(("connection read data exit fd: %d\n", connection->sockfd));
}

void Connection_handle_event(int fd, short flags, void *data)
{
	Connection *connection = (Connection *)data;

	DEBUG(("con event, fd: %d, state: %d, readable: %d, writeable: %d, timeout: %d\n", connection->sockfd,
			connection->state, (flags & EV_READ) ? 1 : 0, (flags & EV_WRITE) ? 1 : 0, (flags & EV_TIMEOUT) ? 1 : 0 ));

	if(flags & EV_WRITE) {
		if(flags & EV_TIMEOUT) {
			printf("TODO write timeout");
			abort();
		}
		Connection_write_data(connection);
	}

	if(flags & EV_READ) {
		if(flags & EV_TIMEOUT) {
			printf("TODO read timeout");
			abort();
		}
		Connection_read_data(connection);
	}

}




