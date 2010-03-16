#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "buffer.h"
#include "connection.h"

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
	Buffer *read_buffer;
	Buffer *write_buffer;
	Buffer *command_buffer;
};

Buffer *Connection_command_buffer(Connection *connection)
{
	return connection->command_buffer;
}

Buffer *Connection_read_buffer(Connection *connection)
{
	return connection->read_buffer;
}

Buffer *Connection_write_buffer(Connection *connection)
{
	return connection->write_buffer;
}

void Connection_event_add(Connection *connection, struct event *event, long int tv_sec, long int tv_usec)
{
	struct timeval tv;
	tv.tv_sec = tv_sec;
	tv.tv_usec = tv_usec;
	int res = event_add(event, &tv);
	printf("connection ev add: fd: %d, res: %d\n", connection->sockfd, res);
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
	printf("connection write_data fd: %d\n", connection->sockfd);

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

	if(CS_CLOSED == connection->state) {
		if(-1 == Connection_connect(connection)) {
			//open the connection
			if(EINPROGRESS == errno) {
				//normal async connect
				connection->state = CS_CONNECTING;
				printf("async connecting\n");
				Connection_event_add(connection, &connection->event_write, 0, 400000);
				return;
			}
			else {
				printf("abort on connect, errno: %d\n", errno);
				abort(); //TODO
			}
		}
		else {
			//immediate connect succeeded
			printf("sync connected\n");
			connection->state = CS_CONNECTED;
		}
	}

	if(CS_CONNECTED == connection->state) {
		while(Buffer_remaining(connection->write_buffer)) {
			//still something to write
			size_t res = Buffer_send(connection->write_buffer, connection->sockfd);
			printf("bfr send res: %d\n", res);
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

/*
void Connection_read_data(Connection *connection)
{
	printf("connection read fd: %d\n", connection->sockfd);

	if(Buffer_remaining(connection->command_buffer) && readable) {
		Buffer_recv(connection->read_buffer, connection->sockfd, DEFAULT_READ_BUFF_SIZE);
	}

	//now

	if(Buffer_remaining(connection->command_buffer)) {
		Connection_event_add(connection, &connection->event_read, 0, 400000);
	}
}
*/

void Connection_handle_event(int fd, short flags, void *data)
{
	Connection *connection = (Connection *)data;

	printf("con event, fd: %d, state: %d, readable: %d, writeable: %d, timeout: %d\n", connection->sockfd,
			connection->state, (flags & EV_READ) ? 1 : 0, (flags & EV_WRITE) ? 1 : 0, (flags & EV_TIMEOUT) ? 1 : 0 );

	if(flags & EV_TIMEOUT) {
		abort();
	}

	if(flags & EV_WRITE) {
		Connection_write_data(connection);
	}
}


int Connection_write_command(Connection *connection, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Buffer_vprintf(connection->write_buffer, format, args);
	va_end(args);
	Buffer_printf(connection->command_buffer, "%c", 1);
	Connection_write_data(connection);
	return 0;
}

Connection *Connection_new(Alloc *alloc, const char *addr, int port)
{
	Connection *connection = (Connection *)alloc->alloc(sizeof(Connection));
	connection->state = CS_CLOSED;
	connection->addr = addr;
	connection->port = port;
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connection->read_buffer = Buffer_new(alloc, DEFAULT_READ_BUFF_SIZE);
	connection->write_buffer = Buffer_new(alloc, DEFAULT_WRITE_BUFF_SIZE);
	connection->command_buffer = Buffer_new(alloc, DEFAULT_COMMAND_BUFF_SIZE);
	event_set(&connection->event_read, connection->sockfd, EV_READ, &Connection_handle_event, (void *)connection);
	event_set(&connection->event_write, connection->sockfd, EV_WRITE, &Connection_handle_event, (void *)connection);
	int flags;
	//set socket in non-blocking mode
	if ((flags = fcntl(connection->sockfd, F_GETFL, 0)) < 0)
	{
	/* Handle error */
	}
	if (fcntl(connection->sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
	/* Handle error */
	}
	return connection;
}
