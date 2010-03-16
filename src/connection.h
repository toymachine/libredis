#include "buffer.h"

typedef struct _Connection Connection;

Buffer *Connection_command_buffer(Connection *connection);
Buffer *Connection_read_buffer(Connection *connection);
Buffer *Connection_write_buffer(Connection *connection);

int Connection_write_command(Connection *connection, const char *format, ...);

Connection *Connection_new(Alloc *alloc, const char *addr, int port);

