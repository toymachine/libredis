#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

#include "event.h"
#include "common.h"
#include "buffer.h"
#include "connection.h"
#include "reply.h"
#include "parser.h"
#include "batch.h"

#define ADDR_SIZE 22 //max size of ip:port addr string

typedef enum _ConnectionState
{
    CS_CLOSED = 0,
    CS_CONNECTING = 1,
    CS_CONNECTED = 2,
    CS_ABORTED = 3
} ConnectionState;


struct _Connection
{
	char addr[ADDR_SIZE];
	struct sockaddr_in sa; //parsed addres
	int sockfd;
	ConnectionState state;
	struct event event_read;
	struct event event_write;
	Batch *current_batch;
	ReplyParser *parser;
};

void Connection_handle_event(int fd, short flags, void *data);

Connection *Connection_new(const char *in_addr)
{
	DEBUG(("alloc Connection\n"));
	Connection *connection = Alloc_alloc_T(Connection);
	if(connection == NULL) {
		SETERROR(("Out of memory while allocating Connection"));
		return NULL;
	}
	connection->state = CS_CLOSED;
	connection->current_batch = NULL;
	connection->parser = ReplyParser_new();
	if(connection->parser == NULL) {
		Connection_free(connection);
		return NULL;
	}

	//copy socket addr
	if(ADDR_SIZE < snprintf(connection->addr, ADDR_SIZE, "%s", in_addr)) {
		SETERROR(("Invalid address for Connection"));
		Connection_free(connection);
		return NULL;
	}

	//parse addr
	int port = DEFAULT_IP_PORT;
	char *pport;
	char addr[ADDR_SIZE];
	DEBUG(("parsing connection addr: %s\n", connection->addr));
	if(NULL == (pport = strchr(connection->addr, ':'))) {
		port = DEFAULT_IP_PORT;
		snprintf(addr, ADDR_SIZE, "%s", connection->addr);
	}
	else {
		port = atoi(pport + 1);
		snprintf(addr, (pport - connection->addr + 1), "%s", connection->addr);
	}
	connection->sa.sin_family = AF_INET;
	connection->sa.sin_port = htons(port);
	if(!inet_pton(AF_INET, addr, &connection->sa.sin_addr)) {
		SETERROR(("Could not parse ip address"));
		Connection_free(connection);
		return NULL;
	}
	DEBUG(("Connection ip: '%s', port: %d\n", addr, port));

	connection->sockfd = 0;

	//
	return connection;
}

void Connection_free(Connection *connection)
{
	if(connection == NULL) {
		return;
	}
	if(connection->parser != NULL) {
		ReplyParser_free(connection->parser);
	}
	DEBUG(("dealloc Connection\n"));
	Alloc_free_T(connection, Connection);
}

int Connection_create_socket(Connection *connection)
{
	assert(connection != NULL);
	assert(CS_CLOSED == connection->state);

	//create socket
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(connection->sockfd == -1) {
		Connection_abort(connection, "could not create socket");
		return -1;
	}

	//prepare libevent structures
	event_set(&connection->event_read, connection->sockfd, EV_READ, &Connection_handle_event, (void *)connection);
	event_set(&connection->event_write, connection->sockfd, EV_WRITE, &Connection_handle_event, (void *)connection);

	//set socket in non-blocking mode
	int flags;
	if ((flags = fcntl(connection->sockfd, F_GETFL, 0)) < 0)
	{
		Connection_abort(connection, "could not get socket flags for");
		return -1;
	}
	if (fcntl(connection->sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		Connection_abort(connection, "could not set socket to non-blocking mode");
		return -1;
	}
	return 0;
}

void Connection_abort(Connection *connection, const char *format,  ...)
{
	if(CS_ABORTED == connection->state) {
		return;
	}

	DEBUG(("Connection aborting\n"));

	//abort batch
	char error1[MAX_ERROR_SIZE];
	char error2[MAX_ERROR_SIZE];
	va_list args;
	va_start(args, format);
	vsnprintf(error1, MAX_ERROR_SIZE, format, args);
	va_end(args);
	snprintf(error2, MAX_ERROR_SIZE, "Connection error %s [addr: %s]", error1, connection->addr);
	Batch_abort(connection->current_batch, error2);
	connection->current_batch = NULL;

	if(CS_CONNECTED == connection->state ||
	   CS_CONNECTING == connection->state) {
		assert(connection->sockfd > 0);

		//unset outstanding events
		event_del(&connection->event_read);
		event_del(&connection->event_write);

		//close the socket
		close(connection->sockfd);
	}

	connection->sockfd = 0;
	connection->state = CS_ABORTED;

	DEBUG(("Connection aborted: %s\n", error2));
}

void Connection_execute(Connection *connection, Batch *batch)
{
	DEBUG(("Connection exec\n"));

	connection->current_batch = batch;

	if(CS_ABORTED == connection->state) {
		connection->state = CS_CLOSED;
	}

	ReplyParser_reset(connection->parser);
	Buffer_flip(Batch_write_buffer(batch));

	DEBUG(("Connection exec write buff:\n"));
#ifndef NDEBUG
	Buffer_dump(Batch_write_buffer(batch), 128);
#endif

	//kick off writing:
	Connection_write_data(connection);

	//kick off reading when socket becomes readable
	if(CS_CONNECTED == connection->state) {
		Connection_event_add(connection, &connection->event_read, 0, 400000);
	}
}

void Connection_event_add(Connection *connection, struct event *event, long int tv_sec, long int tv_usec)
{
	if(CS_ABORTED == connection->state) {
		return;
	}

	struct timeval tv;
	tv.tv_sec = tv_sec;
	tv.tv_usec = tv_usec;
	if(-1 == event_add(event, &tv)) {
		Connection_abort(connection, "could not add event");
		return;
	}
	DEBUG(("connection ev add: fd: %d, type: %c\n", connection->sockfd, event == &connection->event_read ? 'R' : 'W'));
}

void Connection_write_data(Connection *connection)
{
	DEBUG(("connection write_data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);
	if(CS_ABORTED == connection->state) {
		return;
	}

	if(CS_CLOSED == connection->state) {
		if(-1 == Connection_create_socket(connection)) {
			//already aborted in create_socket
			return;
		}
		//connect the socket
		if(-1 == connect(connection->sockfd, (struct sockaddr *) &connection->sa, sizeof(struct sockaddr))) {
			//open the connection
			if(EINPROGRESS == errno) {
				//normal async connect
				connection->state = CS_CONNECTING;
				DEBUG(("async connecting, adding write event\n"));
				Connection_event_add(connection, &connection->event_write, 0, 400000);
				DEBUG(("write event added, now returning\n"));
				return;
			}
			else {
				Connection_abort(connection, "connect error 1, errno [%d] %s", errno, strerror(errno));
				return;
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
		if(-1 == getsockopt(connection->sockfd, SOL_SOCKET, SO_ERROR, &error, &len)) {
			Connection_abort(connection, "getsockopt error for connect result, errno: [%d] %s", errno, strerror(errno));
			return;
		}
		if(error != 0) {
			Connection_abort(connection, "connect error 2, errno: [%d] %s", errno, strerror(errno));
			return;
		}
		else {
			connection->state = CS_CONNECTED;
			Connection_event_add(connection, &connection->event_read, 0, 400000);
		}
	}

	if(CS_CONNECTED == connection->state) {

		Buffer *buffer = Batch_write_buffer(connection->current_batch);
		assert(buffer != NULL);
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
					Connection_abort(connection, "write error, errno: [%d] %s", errno, strerror(errno));
					return;
				}
			}
		}
	}
}

void Connection_read_data(Connection *connection)
{
	if(CS_ABORTED == connection->state) {
		return;
	}

	DEBUG(("connection read data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);
	assert(CS_CONNECTED == connection->state);

	Buffer *buffer = Batch_read_buffer(connection->current_batch);
	assert(buffer != NULL);

	while(Batch_has_command(connection->current_batch)) {
		DEBUG(("exec rp\n"));
		Reply *reply = NULL;
		ReplyParserResult rp_res = ReplyParser_execute(connection->parser, Buffer_data(buffer), Buffer_position(buffer), &reply);
		switch(rp_res) {
		case RPR_ERROR: {
			Connection_abort(connection, "result parse error");
			return;
		}
		case RPR_MORE: {
			size_t res = Buffer_recv(buffer, connection->sockfd);
#ifndef NDEBUG
		Buffer_dump(buffer, 128);
#endif
			if(res == -1) {
				if(errno == EAGAIN) {
					Connection_event_add(connection, &connection->event_read, 0, 400000);
					return;
				}
				else {
					Connection_abort(connection, "read error, errno: [%d] %s", errno, strerror(errno));
					return;
				}
			}
			break;
		}
		case RPR_REPLY: {
			Batch_add_reply(connection->current_batch, reply);
			break;
		}
		default:
			Connection_abort(connection, "unexpected result parser result, rpres: %d", rp_res);
			return;
		}
	}
}

void Connection_handle_event(int fd, short flags, void *data)
{
	Connection *connection = (Connection *)data;

	if(CS_ABORTED == connection->state) {
		return;
	}

	DEBUG(("con event, fd: %d, state: %d, flags: %d, readable: %d, writeable: %d, timeout: %d\n", connection->sockfd,
			connection->state, flags, (flags & EV_READ) ? 1 : 0, (flags & EV_WRITE) ? 1 : 0, (flags & EV_TIMEOUT) ? 1 : 0 ));

	if(flags & EV_TIMEOUT) {
		Connection_abort(connection, "Connection r/w timeout");
		return;
	}

	if(flags & EV_WRITE) {
		Connection_write_data(connection);
	}

	if(flags & EV_READ) {
		Connection_read_data(connection);
	}

}




