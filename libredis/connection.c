/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <poll.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <time.h>

#include "redis.h"
#include "common.h"
#include "alloc.h"
#include "buffer.h"
#include "connection.h"
#include "reply.h"
#include "parser.h"
#include "batch.h"


#ifndef CLOCK_MONOTONIC
#ifdef __APPLE__

#define CLOCK_MONOTONIC 1
#include <mach/mach_time.h>
typedef int clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp) {
    if(clk_id != CLOCK_MONOTONIC)
        return -1;

    uint64_t t = mach_absolute_time();
    static mach_timebase_info_data_t info = {0,0};
    mach_timebase_info(&info);
    uint64_t t_nano = t * (info.numer / info.denom);

    tp->tv_sec = t_nano * 1e-9;
    tp->tv_nsec = t_nano - (tp->tv_sec * 1e9);

    return 0;
}

#endif
#endif


typedef enum _ConnectionState
{
    CS_CLOSED = 0,
    CS_CONNECTING = 1,
    CS_CONNECTED = 2,
    CS_ABORTED = 3
} ConnectionState;

typedef enum _EventType
{
	EVENT_READ = 1,
	EVENT_WRITE = 2,
	EVENT_TIMEOUT = 4,
	EVENT_ERROR = 8,
} EventType;

struct _Connection
{
	char addr[ADDR_SIZE]; //host name/ip address
	char serv[SERV_SIZE]; //port number
	struct addrinfo *addrinfo;
	int sockfd;
	ConnectionState state;
	Batch *current_batch;
	Executor *current_executor;
	ReplyParser *parser;
};

//forward decls.
void Connection_abort(Connection *connection, const char *format,  ...);
void Connection_execute_start(Connection *connection, Executor *executor, Batch *batch, int ordinal);
void Connection_write_data(Connection *connection, int ordinal);
void Connection_read_data(Connection *connection, int ordinal);
void Connection_close(Connection *connection);

void Executor_notify_event(Executor *executor, Connection *connection, EventType event, int ordinal);

Connection *Connection_new(const char *in_addr)
{
	DEBUG(("alloc Connection\n"));
	Connection *connection = Alloc_alloc_T(Connection);
	if(connection == NULL) {
		Module_set_error(GET_MODULE(), "Out of memory while allocating Connection");
		return NULL;
	}
	connection->state = CS_CLOSED;
	connection->current_batch = NULL;
	connection->current_executor = NULL;
	connection->parser = ReplyParser_new();
	if(connection->parser == NULL) {
		Connection_free(connection);
		return NULL;
	}

	//copy address
	int invalid_address = 0;
	char *service;
	if (NULL == (service = strchr(in_addr, ':'))) {
		invalid_address = ADDR_SIZE < snprintf(connection->addr, ADDR_SIZE, "%s", in_addr)
				|| SERV_SIZE < snprintf(connection->serv, SERV_SIZE, "%s", XSTR(DEFAULT_IP_PORT));
	}
	else {
		int addr_length = service - in_addr + 1;
		if (ADDR_SIZE < addr_length) {
			invalid_address = 1;
		}
		else {
			snprintf(connection->addr, addr_length, "%s", in_addr);
			invalid_address = SERV_SIZE < snprintf(connection->serv, SERV_SIZE, "%s", service + 1);
		}
	}
	if (invalid_address) {
		Module_set_error(GET_MODULE(), "Invalid address for Connection");
		Connection_free(connection);
		return NULL;
	}
	DEBUG(("Connection address: '%s', service: '%s'\n", connection->addr, connection->serv));

	connection->addrinfo = NULL;
	connection->sockfd = 0;

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
	Connection_close(connection);

	DEBUG(("dealloc Connection\n"));
	Alloc_free_T(connection, Connection);
}

//TODO make connection close public?, in that case make sure
//state is CS_CLOSE after the method is finished
void Connection_close(Connection *connection)
{
	assert(connection != NULL);

	if(connection->sockfd > 0) {
		//close the socket
		close(connection->sockfd);
		connection->sockfd = 0;
	}

	if (connection->addrinfo != NULL) {
		freeaddrinfo(connection->addrinfo);
		connection->addrinfo = NULL;
	}
}

int Connection_create_socket(Connection *connection)
{
	assert(connection != NULL);
	assert(CS_CLOSED == connection->state);

	//resolve address
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;
	if (getaddrinfo(connection->addr, connection->serv, &hints, &connection->addrinfo)) {
		Connection_abort(connection, "could not resolve address");
		return -1;
	}

	//create socket
	struct addrinfo *addrinfo = connection->addrinfo;
	connection->sockfd = socket(addrinfo->ai_family, addrinfo->ai_socktype, addrinfo->ai_protocol);
	if(connection->sockfd == -1) {
		Connection_abort(connection, "could not create socket");
		return -1;
	}

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

	//abort batch
	char error1[MAX_ERROR_SIZE];
	char error2[MAX_ERROR_SIZE];
	va_list args;
	va_start(args, format);
	vsnprintf(error1, MAX_ERROR_SIZE, format, args);
	va_end(args);
	snprintf(error2, MAX_ERROR_SIZE, "Connection error %s [addr: %s:%s]", error1, connection->addr, connection->serv);

	DEBUG(("Connection aborting: %s\n", error2));

	Batch_abort(connection->current_batch, error2);
	connection->current_batch = NULL;
	connection->current_executor = NULL;

	Connection_close(connection);

	connection->state = CS_ABORTED;

	DEBUG(("Connection aborted\n"));
}

void Connection_execute_start(Connection *connection, Executor *executor, Batch *batch, int ordinal)
{
	DEBUG(("Connection exec\n"));

	connection->current_batch = batch;
	connection->current_executor = executor;

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
	Connection_write_data(connection, ordinal);

	//kick off reading when socket becomes readable
	if(CS_CONNECTED == connection->state) {
		Executor_notify_event(connection->current_executor, connection, EVENT_READ, ordinal);
	}
}

void Connection_write_data(Connection *connection, int ordinal)
{
	DEBUG(("connection write_data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);
	assert(connection->current_executor != NULL);
	if(CS_ABORTED == connection->state) {
		return;
	}

	if(CS_CLOSED == connection->state) {
		if(-1 == Connection_create_socket(connection)) {
			//already aborted in create_socket
			return;
		}
		//connect the socket
		struct addrinfo *addrinfo = connection->addrinfo;
		if(-1 == connect(connection->sockfd, addrinfo->ai_addr, addrinfo->ai_addrlen)) {
			//open the connection
			if(EINPROGRESS == errno) {
				//normal async connect
				connection->state = CS_CONNECTING;
				DEBUG(("async connecting, adding write event\n"));
				Executor_notify_event(connection->current_executor, connection, EVENT_WRITE, ordinal);
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
			Executor_notify_event(connection->current_executor, connection, EVENT_READ, ordinal);
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
					Executor_notify_event(connection->current_executor, connection, EVENT_WRITE, ordinal);
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

void Connection_read_data(Connection *connection, int ordinal)
{
	if(CS_ABORTED == connection->state) {
		return;
	}

	DEBUG(("connection read data fd: %d\n", connection->sockfd));
	assert(connection->current_batch != NULL);
	assert(connection->current_executor != NULL);
	assert(CS_CONNECTED == connection->state);

	Buffer *buffer = Batch_read_buffer(connection->current_batch);
	assert(buffer != NULL);

	while(Batch_has_command(connection->current_batch)) {
		DEBUG(("exec rp\n"));
		Reply *reply = NULL;
		ReplyParserResult rp_res = ReplyParser_execute(connection->parser, buffer, Buffer_position(buffer), &reply);
		switch(rp_res) {
		case RPR_ERROR: {
			Connection_abort(connection, "result parse error");
			return;
		}
		case RPR_MORE: {
			DEBUG(("read data RPR_MORE buf recv\n"));
			size_t res = Buffer_recv(buffer, connection->sockfd);
#ifndef NDEBUG
		Buffer_dump(buffer, 128);
#endif
			if(res == -1) {
				if(errno == EAGAIN) {
 					DEBUG(("read data expecting more data in future, adding event\n"));
					Executor_notify_event(connection->current_executor, connection, EVENT_READ, ordinal);
					return;
				}
				else {
					Connection_abort(connection, "read error, errno: [%d] %s", errno, strerror(errno));
					return;
				}
			}
			else if(res == 0) {
				Connection_abort(connection, "read eof");
				return;
			}
			break;
		}
		case RPR_REPLY: {
			DEBUG(("read data RPR_REPLY batch add reply\n"));
			Batch_add_reply(connection->current_batch, reply);
			break;
		}
		default:
			Connection_abort(connection, "unexpected result parser result, rpres: %d", rp_res);
			return;
		}
	}
}

void Connection_handle_event(Connection *connection, EventType event, int ordinal)
{
	if(CS_ABORTED == connection->state) {
		return;
	}

	DEBUG(("con event, fd: %d, state: %d, event: %d, readable: %d, writeable: %d, timeout: %d\n", connection->sockfd,
			connection->state, event, (event & EVENT_READ) ? 1 : 0, (event & EVENT_WRITE) ? 1 : 0, (event & EVENT_TIMEOUT) ? 1 : 0 ));

	if(event & EVENT_ERROR) {
		Connection_abort(connection, "event error");
		return;
	}

	if(event & EVENT_TIMEOUT) {
		if(CS_CONNECTING == connection->state) {
			Connection_abort(connection, "connect timeout");
		}
		else {
			Connection_abort(connection, "read/write timeout");
		}
		return;
	}

	if(event & EVENT_WRITE) {
		Connection_write_data(connection, ordinal);
	}

	if(event & EVENT_READ) {
		Connection_read_data(connection, ordinal);
	}

}

/************************************ EXECUTOR ***************************************/

#define MAX_PAIRS 1024

struct _Pair
{
	Batch *batch;
	Connection *connection;
};

struct _Executor
{
	int numpairs;
	int numevents;
	struct pollfd fds[MAX_PAIRS];
	struct _Pair pairs[MAX_PAIRS];
	double end_tm_ms;
};

Executor *Executor_new()
{
	DEBUG(("alloc Executor\n"));
	Executor *executor = Alloc_alloc_T(Executor);
	if(executor == NULL) {
		Module_set_error(GET_MODULE(), "Out of memory while allocating Executor");
		return NULL;
	}
	executor->numpairs = 0;
	executor->numevents = 0;
	return executor;
}

void Executor_free(Executor *executor)
{
	if(executor == NULL) {
		return;
	}
	DEBUG(("dealloc Executor\n"));
	Alloc_free_T(executor, Executor);
}

int Executor_add(Executor *executor, Connection *connection, Batch *batch)
{
	assert(executor != NULL);
	assert(connection != NULL);
	assert(batch != NULL);

	if(executor->numpairs >= MAX_PAIRS) {
		Module_set_error(GET_MODULE(), "Executor is full, max = %d", MAX_PAIRS);
		return -1;
	}
	struct _Pair *pair = &executor->pairs[executor->numpairs];
	pair->batch = batch;
	pair->connection = connection;
	struct pollfd *fd = &executor->fds[executor->numpairs];
	fd->fd = 0;
	fd->events = fd->revents = 0;
	executor->numpairs += 1;
	DEBUG(("Executor add, total: %d\n", executor->numpairs));
	return 0;
}

#define TIMESPEC_TO_MS(tm) (((double)tm.tv_sec) * 1000.0) + (((double)tm.tv_nsec) / 1000000.0)

int Executor_current_timeout(Executor *executor, int *timeout)
{
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	double cur_tm_ms = TIMESPEC_TO_MS(tm);
	DEBUG(("Executor cur_tm: %3.2f\n", cur_tm_ms));
	double left_ms = executor->end_tm_ms - cur_tm_ms;
	DEBUG(("Time left: %3.2f\n", left_ms));
	if (left_ms < 0.0) {
		return -1;
	}
	*timeout = (int)left_ms;
	DEBUG(("Timeout: %d msec\n", *timeout));
	return 0;
}

int Executor_execute(Executor *executor, int timeout_ms)
{
	DEBUG(("Executor execute start\n"));

	//determine max endtime based on timeout
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	DEBUG(("Executor start_tm_ms: %3.2f\n", TIMESPEC_TO_MS(tm)));
	executor->end_tm_ms = TIMESPEC_TO_MS(tm) + ((float)timeout_ms);
	DEBUG(("Executor end_tm_ms: %3.2f\n", executor->end_tm_ms));

	executor->numevents = 0;
	for(int i = 0; i < executor->numpairs; i++) {
		struct _Pair *pair = &executor->pairs[i];
		Connection_execute_start(pair->connection, executor, pair->batch, i);
	}

	int poll_result = 1;
	//for as long there are outstanding events and no error or timeout occurred:
	while(executor->numevents > 0 && poll_result > 0) {
		//figure out how many ms left for this execution
		int timeout;
		if (Executor_current_timeout(executor, &timeout) == -1) {
			//if no time is left, force a timeout
			poll_result = 0;
		} else {
			//do the poll
			DEBUG(("Executor start poll num_events: %d\n", executor->numevents));
			poll_result = poll(executor->fds, executor->numpairs, timeout);

			DEBUG(("Executor select res %d\n", poll_result));
		}

		for(int i = 0; i < executor->numpairs; i++) {

			struct _Pair *pair = &executor->pairs[i];
			Connection *connection = pair->connection;
			Batch *batch = pair->batch;
			struct pollfd *fd = &executor->fds[i];

			EventType event = 0;
			if(fd->revents & POLLIN) {
				event |= EVENT_READ;
				fd->events &= ~POLLIN;
				fd->revents &= ~POLLIN;
				executor->numevents -= 1;
			}
			if(fd->revents & POLLOUT) {
				event |= EVENT_WRITE;
				fd->events &= ~POLLOUT;
				fd->revents &= ~POLLOUT;
				executor->numevents -= 1;
			}

			if(poll_result == 0) {
				event = EVENT_TIMEOUT;
			}
			else if(poll_result < 0) {
				event = EVENT_ERROR;
			}

			if(event > 0 && Batch_has_command(batch)) {
				//there is an event, and batch is not finished
				Connection_handle_event(connection, event, i);
			}
		}
	}

	if(poll_result > 1) {
		poll_result = 1;
	}
	if(poll_result < 0) {
		Module_set_error(GET_MODULE(), "Execute select error, errno: [%d] %s", errno, strerror(errno));
	}
	else if(poll_result == 0) {
		Module_set_error(GET_MODULE(), "Execute timeout");
	}
	DEBUG(("Executor execute done\n"));
	return poll_result;
}

void Executor_notify_event(Executor *executor, Connection *connection, EventType event, int ordinal)
{
	assert(executor != NULL);
	assert(connection != NULL);
	assert(connection->sockfd != 0);
	assert(connection->state != CS_ABORTED);

	executor->fds[ordinal].fd = connection->sockfd;

	if(event & EVENT_READ) {
		executor->fds[ordinal].events |= POLLIN;
		executor->numevents += 1;
	}

	if(event & EVENT_WRITE) {
		executor->fds[ordinal].events |= POLLOUT;
		executor->numevents += 1;
	}

	DEBUG(("executor notify event added: fd: %d, type: %c, max_fd: %d, num_events: %d\n", connection->sockfd, event == EVENT_READ ? 'R' : 'W', executor->max_fd, executor->numevents));
}




