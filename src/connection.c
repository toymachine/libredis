#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <event.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "common.h"
#include "buffer.h"
#include "connection.h"

typedef enum _ConnectionState
{
    CS_INIT = 0,
    CS_CONNECTING = 1,
    CS_EXECUTING = 2
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



void Connection_connect(Connection *connection)
{
	struct sockaddr_in sa;

	sa.sin_family = AF_INET;
	sa.sin_port = htons(connection->port);
	inet_pton(AF_INET, connection->addr, &sa.sin_addr);
	if(-1 == connect(connection->sockfd, (struct sockaddr *) &sa, sizeof(struct sockaddr))) {
		if(EINPROGRESS == errno) {
			//normal async connect
			connection->state = CS_CONNECTING;
			Connection_event_add(connection, &connection->event_write, 0, 400000);
		}
		else {
			//die();
			printf("other rror");
		}
	}
	else {
		//immediate connect succeeded
		connection->state = CS_EXECUTING;
		printf("conn imm");
	}

}

void Connection_write(Connection *connection, int writeable)
{
	printf("connection write fd: %d\n", connection->sockfd);

	if(Buffer_remaining(connection->write_buffer)) {
		//still something to write
		if(writeable) {
			Buffer_send(connection->write_buffer, connection->sockfd);
		}
		else {
			Connection_event_add(connection, &connection->event_write, 0, 400000);
		}
	}
}

void Connection_read(Connection *connection, int readable)
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


void Connection_loop(Connection *connection, int readable, int writeable, int timeout)
{
	printf("con loop, fd: %d, state: %d,  readable: %d, writeable: %d, timeout: %d\n", connection->sockfd, connection->state, readable, writeable, timeout);

	if(CS_INIT == connection->state) {
		//open the connection
		Connection_connect(connection);
		if(CS_CONNECTING == connection->state) {
			return;
		}
	}

	if(CS_CONNECTING == connection->state) {
		if(writeable) {
			connection->state = CS_EXECUTING;
		}
	}

	if(CS_EXECUTING == connection->state) {
		Connection_write(connection, writeable);
		Connection_read(connection, readable);
	}
}

void Connection_event_callback(int fd, short flags, Connection *connection)
{
	Connection_loop(connection, (flags & EV_READ) ? 1 : 0, (flags & EV_WRITE) ? 1 : 0, (flags & EV_TIMEOUT) ? 1 : 0);
}


int Connection_write_command(Connection *connection, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	Buffer_vprintf(connection->write_buffer, format, args);
	va_end(args);
	Buffer_printf(connection->command_buffer, "%c", 1);
	return 0;
}

Connection *Connection_new(Alloc *alloc, const char *addr, int port)
{
	Connection *connection = (Connection *)alloc->alloc(sizeof(Connection));
	connection->state = CS_INIT;
	connection->addr = addr;
	connection->port = port;
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	connection->read_buffer = Buffer_new(alloc, DEFAULT_READ_BUFF_SIZE);
	connection->write_buffer = Buffer_new(alloc, DEFAULT_WRITE_BUFF_SIZE);
	connection->command_buffer = Buffer_new(alloc, DEFAULT_COMMAND_BUFF_SIZE);
	event_set(&connection->event_read, connection->sockfd, EV_READ, &Connection_event_callback, (void *)connection);
	event_set(&connection->event_write, connection->sockfd, EV_WRITE, &Connection_event_callback, (void *)connection);
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
