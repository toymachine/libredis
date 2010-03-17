#ifndef __COMMAND_H
#define __COMMAND_H

#include "common.h"
#include "list.h"

Command *Command_new();
Command *Command_list_last(struct list_head *head);
Command *Command_list_pop(struct list_head *head);

Reply *Command_reply(Command *cmd);
Batch *Command_batch(Command *cmd);

int Command_prepare_buffer(Command *cmd);
int Command_add_reply(Command *cmd, Reply *reply);

int Connection_add_commands(Connection *connection, struct list_head *commands);

#endif
