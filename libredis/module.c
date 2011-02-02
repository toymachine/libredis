/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include "alloc.h"
#include "module.h"
#include "reply.h"
#include "batch.h"

Module g_module;
static THREADLOCAL char error[MAX_ERROR_SIZE];

Module *Module_new()
{
	return &g_module;
}

int Module_init(Module *module)
{
	assert(module == &g_module);//for now...

	DEBUG(("Module init\n"));
	if(module->alloc_malloc == NULL) {
		module->alloc_malloc = malloc;
	}
	if(module->alloc_realloc == NULL) {
		module->alloc_realloc = realloc;
	}
	if(module->alloc_free == NULL) {
		module->alloc_free = free;
	}
	DEBUG(("start alloc: %d\n", module->allocated));
	return 0;
}

void Module_set_alloc_alloc(Module *module, void * (*alloc_malloc)())
{
	module->alloc_malloc = alloc_malloc;
}

void Module_set_alloc_realloc(Module *module, void * (*alloc_realloc)(void *, size_t))
{
	module->alloc_realloc = alloc_realloc;
}

void Module_set_alloc_free(Module *module, void (*alloc_free)(void *))
{
	module->alloc_free= alloc_free;
}

size_t Module_get_allocated(Module *module)
{
	return module->allocated;
}

char *Module_last_error(Module *module)
{
	return error;
}

void Module_set_error(Module *module, char *format, ...)
{
	va_list args;
	va_start(args, format);
	vsnprintf(error, MAX_ERROR_SIZE, format, args);
	va_end(args);
}

void Module_free(Module *module)
{
	assert(module == &g_module);//for now...

	DEBUG(("Module free\n"));
	//release the freelists
	Reply_free_final();
//	Command_free_final();
	Batch_free_final();

	DEBUG(("final alloc: %d\n", module->allocated));
}
