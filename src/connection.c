/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
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


#define ADDR_SIZE 22 //max size of ip:port addr string

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
	char addr[ADDR_SIZE];
	struct sockaddr_in sa; //parsed addres
	int sockfd;
	ConnectionState state;
	Batch *current_batch;
	Executor *current_executor;
	ReplyParser *parser;
};

//forward decls.
void Connection_abort(Connection *connection, const char *format,  ...);
void Connection_execute_start(Connection *connection, Executor *executor, Batch *batch);
void Connection_write_data(Connection *connection);
void Connection_read_data(Connection *connection);

void Executor_notify_event(Executor *executor, Connection *connection, EventType event);

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

	//copy socket addr
	if(ADDR_SIZE < snprintf(connection->addr, ADDR_SIZE, "%s", in_addr)) {
		Module_set_error(GET_MODULE(), "Invalid address for Connection");
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
		Module_set_error(GET_MODULE(), "Could not parse ip address");
		Connection_free(connection);
		return NULL;
	}
	DEBUG(("Connection ip: '%s', port: %d\n", addr, port));

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
	snprintf(error2, MAX_ERROR_SIZE, "Connection error %s [addr: %s]", error1, connection->addr);

	DEBUG(("Connection aborting: %s\n", error2));

	Batch_abort(connection->current_batch, error2);
	connection->current_batch = NULL;
	connection->current_executor = NULL;

	if(CS_CONNECTED == connection->state ||
	   CS_CONNECTING == connection->state) {
		assert(connection->sockfd > 0);

		//close the socket
		close(connection->sockfd);
	}

	connection->sockfd = 0;
	connection->state = CS_ABORTED;

	DEBUG(("Connection aborted\n"));
}

void Connection_execute_start(Connection *connection, Executor *executor, Batch *batch)
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
	Connection_write_data(connection);

	//kick off reading when socket becomes readable
	if(CS_CONNECTED == connection->state) {
		Executor_notify_event(connection->current_executor, connection, EVENT_READ);
	}
}

void Connection_write_data(Connection *connection)
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
		if(-1 == connect(connection->sockfd, (struct sockaddr *) &connection->sa, sizeof(struct sockaddr))) {
			//open the connection
			if(EINPROGRESS == errno) {
				//normal async connect
				connection->state = CS_CONNECTING;
				DEBUG(("async connecting, adding write event\n"));
				Executor_notify_event(connection->current_executor, connection, EVENT_WRITE);
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
			Executor_notify_event(connection->current_executor, connection, EVENT_READ);
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
					Executor_notify_event(connection->current_executor, connection, EVENT_WRITE);
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
	assert(connection->current_executor != NULL);
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
					Executor_notify_event(connection->current_executor, connection, EVENT_READ);
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
			Batch_add_reply(connection->current_batch, reply);
			break;
		}
		default:
			Connection_abort(connection, "unexpected result parser result, rpres: %d", rp_res);
			return;
		}
	}
}

void Connection_handle_event(Connection *connection, EventType event)
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
		Connection_write_data(connection);
	}

	if(event & EVENT_READ) {
		Connection_read_data(connection);
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
	int max_fd;
	int numevents;
	fd_set readfds;
	fd_set writefds;
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
	executor->max_fd = 0;
	FD_ZERO(&executor->readfds);
	FD_ZERO(&executor->writefds);
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
	executor->numpairs += 1;
	DEBUG(("Executor add, total: %d\n", executor->numpairs));
	return 0;
}

#define TIMESPEC_TO_MS(tm) (((double)tm.tv_sec) * 1000.0) + (((double)tm.tv_nsec) / 1000000.0)

int Executor_current_timeout(Executor *executor, struct timeval *tv)
{
	struct timespec tm;
	clock_gettime(CLOCK_MONOTONIC, &tm);
	double cur_tm_ms = TIMESPEC_TO_MS(tm);
	DEBUG(("Executor cur_tm: %3.2f\n", cur_tm_ms));
	double left_ms = executor->end_tm_ms - cur_tm_ms;
	DEBUG(("Time left: %3.2f\n", left_ms));
	tv->tv_sec = (time_t)left_ms / 1000.0;
	tv->tv_usec = (left_ms - (tv->tv_sec * 1000.0)) * 1000.0;
	DEBUG(("Timeout: %d sec, %d usec\n", (int)tv->tv_sec, (int)tv->tv_usec));
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
		Connection_execute_start(pair->connection, executor, pair->batch);
	}

	int select_result = 1;
	//for as long there are outstanding events and no error or timeout occurred:
	while(executor->numevents > 0 && select_result > 0) {

		//figure out how many ms left for this execution
		struct timeval tv;
		Executor_current_timeout(executor, &tv);

		//copy filedes. sets, because select is going to modify them
		fd_set readfds;
		fd_set writefds;
		readfds = executor->readfds;
		writefds = executor->writefds;

		//do the select
		DEBUG(("Executor start select max_fd %d, num_events: %d\n", executor->max_fd, executor->numevents));
		select_result = select(executor->max_fd + 1, &readfds, &writefds, NULL, &tv);

		DEBUG(("Executor select res %d\n", select_result));

		for(int i = 0; i < executor->numpairs; i++) {

			struct _Pair *pair = &executor->pairs[i];
			Connection *connection = pair->connection;
			Batch *batch = pair->batch;

			EventType event = 0;
			if(FD_ISSET(connection->sockfd, &readfds)) {
				event |= EVENT_READ;
				FD_CLR(connection->sockfd, &executor->readfds);
				executor->numevents -= 1;
			}
			if(FD_ISSET(connection->sockfd, &writefds)) {
				event |= EVENT_WRITE;
				FD_CLR(connection->sockfd, &executor->writefds);
				executor->numevents -= 1;
			}

			if(select_result == 0) {
				event = EVENT_TIMEOUT;
			}
			else if(select_result < 0) {
				event = EVENT_ERROR;
			}

			if(event > 0 && Batch_has_command(batch)) {
				//there is an event, and batch is not finished
				Connection_handle_event(connection, event);
			}
		}
	}

	if(select_result > 1) {
		select_result = 1;
	}
	if(select_result < 0) {
		Module_set_error(GET_MODULE(), "Execute select error, errno: [%d] %s", errno, strerror(errno));
	}
	else if(select_result == 0) {
		Module_set_error(GET_MODULE(), "Execute timeout");
	}
	DEBUG(("Executor execute done\n"));
	return select_result;
}

void Executor_notify_event(Executor *executor, Connection *connection, EventType event)
{
	assert(executor != NULL);
	assert(connection != NULL);
	assert(connection->sockfd != 0);
	assert(connection->state != CS_ABORTED);

	if(connection->sockfd > executor->max_fd) {
		executor->max_fd = connection->sockfd;
	}

	if(event & EVENT_READ) {
		FD_SET(connection->sockfd, &executor->readfds);
		executor->numevents += 1;
	}

	if(event & EVENT_WRITE) {
		FD_SET(connection->sockfd, &executor->writefds);
		executor->numevents += 1;
	}

	DEBUG(("executor notify event added: fd: %d, type: %c, max_fd: %d, num_events: %d\n", connection->sockfd, event == EVENT_READ ? 'R' : 'W', executor->max_fd, executor->numevents));
}




