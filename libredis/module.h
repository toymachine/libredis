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

#define MAX_ERROR_SIZE 255

struct _Module
{
	size_t size; //size of module structure
	void * (*alloc_malloc)(size_t size);
    void * (*alloc_realloc)(void *ptr, size_t size);
    void (*alloc_free)(void *ptr);
    size_t allocated;
};

extern Module g_module;
#define GET_MODULE() ((Module *)&g_module)

void Module_set_error(Module *module, char *format, ...);

#endif /* MODULE_H_ */
