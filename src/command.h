#ifndef __COMMAND_H
#define __COMMAND_H

#include "common.h"
#include "list.h"

Command *Command_new();
Command *Command_list_last(struct list_head *head);
Command *Command_list_pop(struct list_head *head);

Buffer *Command_read_buffer(Command *cmd);
Buffer *Command_write_buffer(Command *cmd);
int Command_flip_buffer(Command *cmd);

int Command_reply(Command *cmd, Reply *reply);
int Connection_add_commands(Connection *connection, struct list_head *commands);

#endif
