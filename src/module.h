/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/
#ifndef MODULE_H_
#define MODULE_H_

#include "redis.h"

extern Module *g_module;

void Module_dispatch();

#endif /* MODULE_H_ */
