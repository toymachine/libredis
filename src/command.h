#ifndef __COMMAND_H
#define __COMMAND_H

#include "common.h"
#include "list.h"

Command *Command_new();
int Command_free(Command *command);

#endif
