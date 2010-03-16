#include "buffer.h"

typedef struct _Connection Connection;

typedef enum _ConnectionState
{
    CS_INIT = 0,
    CS_CONNECTING = 1,
    CS_EXECUTING = 2
} ConnectionState;

Buffer *Connection_command_buffer(Connection *connection);
Buffer *Connection_read_buffer(Connection *connection);
Buffer *Connection_write_buffer(Connection *connection);
int Connection_command_bulk(Connection *connection,
                             const char *command, int command_len,
                             const char *key, int key_len,
                             const char *value, int value_len);
Connection *Connection_new(Alloc *alloc, const char *addr, int port);

/*
void Connection_connect(Connection *connection)
void Connection_write(Connection *connection, int writeable)
void Connection_read(Connection *connection, int readable)
void Connection_loop(Connection *connection, int readable, int writeable, int timeout)
void Connection_event_callback(int fd, short flags, Connection *connection)
*/
