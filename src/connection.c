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
#include "command.h"
#include "parser.h"

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
	struct list_head write_queue; //commands queued for writing
	struct list_head read_queue; //commands queued for reading
	ReplyParser *parser;
};

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

int Connection_buffer_next_command(Connection *connection)
{
	if(!list_empty(&connection->write_queue)) {
		struct list_head *pos = list_last(&connection->write_queue);
		Command *cmd = list_entry(pos, Command, list);
		Buffer_set_position(cmd->write_buffer, cmd->offset);
		Buffer_set_limit(cmd->write_buffer, cmd->offset + cmd->len);
		return 1;
	}
	else {
		return 0;
	}
}

void Connection_write_data(Connection *connection)
{
	printf("connection write_data fd: %d\n", connection->sockfd);

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

		while(!list_empty(&connection->write_queue)) {
			struct list_head *pos = list_last(&connection->write_queue);
			Command *cmd = list_entry(pos, Command, list);
			printf("offset= %d len= %d\n", cmd->offset, cmd->len);
			while(Buffer_remaining(cmd->write_buffer)) {
				//still something to write
				size_t res = Buffer_send(cmd->write_buffer, connection->sockfd);
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
			//command written
			pos = list_pop(&connection->write_queue);
			list_add(pos, &connection->read_queue);
			Connection_event_add(connection, &connection->event_read, 0, 400000);
			Connection_buffer_next_command(connection);
		}
	}
}

int Connection_add_commands(Connection *connection, struct list_head *commands)
{
	list_splice_init(commands, &connection->write_queue);
	if(Connection_buffer_next_command(connection)) {
		Connection_write_data(connection);
	}
	return 0;
}

void Connection_read_data(Connection *connection)
{
	printf("connection read fd: %d\n", connection->sockfd);

	while(!list_empty(&connection->read_queue)) {
		struct list_head *pos = list_last(&connection->read_queue);
		Command *cmd = list_entry(pos, Command, list);
		Buffer *read_buffer = cmd->read_buffer;
		size_t res = Buffer_recv(read_buffer, connection->sockfd, DEFAULT_READ_BUFF_SIZE);
		if(res == -1) {
			abort(); //TODO
		}
		Buffer_dump(read_buffer, 64);
		while(1) {
			ReplyParserResult rp_res = ReplyParser_execute(connection->parser, Buffer_data(read_buffer), Buffer_position(read_buffer));
			switch(rp_res) {
			case RPR_OK_LINE: {
				cmd->reply = Reply_new();
				cmd->reply->type = RT_OK;
				cmd->reply->buffer = read_buffer;
				cmd->reply->offset = ReplyParser_offset(connection->parser);
				cmd->reply->len = ReplyParser_length(connection->parser);
				break;
			}
			default:
				printf("unhandled rp result: %d\n", rp_res);
				abort();
			}
		}

		break;
	}
}

void Connection_handle_event(int fd, short flags, void *data)
{
	Connection *connection = (Connection *)data;

	printf("con event, fd: %d, state: %d, readable: %d, writeable: %d, timeout: %d\n", connection->sockfd,
			connection->state, (flags & EV_READ) ? 1 : 0, (flags & EV_WRITE) ? 1 : 0, (flags & EV_TIMEOUT) ? 1 : 0 );

	if(flags & EV_WRITE) {
		if(flags & EV_TIMEOUT) {
			abort();
		}
		Connection_write_data(connection);
	}

	if(flags & EV_READ) {
		if(flags & EV_TIMEOUT) {
			abort();
		}
		Connection_read_data(connection);
	}

}



Connection *Connection_new(const char *addr, int port)
{
	Connection *connection = REDIS_ALLOC_T(Connection);
	connection->state = CS_CLOSED;

	//cmd queues
	INIT_LIST_HEAD(&connection->write_queue);
	INIT_LIST_HEAD(&connection->read_queue);

	connection->parser = ReplyParser_new();

	//socket stuff:
	connection->addr = addr;
	connection->port = port;
	connection->sockfd = socket(AF_INET, SOCK_STREAM, 0);
	event_set(&connection->event_read, connection->sockfd, EV_READ, &Connection_handle_event, (void *)connection);
	event_set(&connection->event_write, connection->sockfd, EV_WRITE, &Connection_handle_event, (void *)connection);
	//set socket in non-blocking mode
	int flags;
	if ((flags = fcntl(connection->sockfd, F_GETFL, 0)) < 0)
	{
		abort();
	}
	if (fcntl(connection->sockfd, F_SETFL, flags | O_NONBLOCK) < 0)
	{
		abort();
	}

	return connection;
}
