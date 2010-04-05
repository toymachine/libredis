/**
* Copyright (C) 2010, Hyves (Startphone Ltd.)
*
* This module is part of Libredis (http://github.com/toymachine/libredis) and is released under
* the New BSD License: http://www.opensource.org/licenses/bsd-license.php
*
*/

#include "module.h"
#include "reply.h"
#include "batch.h"

Module *g_module;
Module g_default_module;

void Module_init(Module *module)
{
	if(module == NULL) {
		g_module = &g_default_module;
	}
	else {
		g_module = module;
	}
	DEBUG(("Module init\n"));
	if(g_module->alloc_malloc == NULL) {
		g_module->alloc_malloc = malloc;
	}
	if(g_module->alloc_realloc == NULL) {
		g_module->alloc_realloc = realloc;
	}
	if(g_module->alloc_free == NULL) {
		g_module->alloc_free = free;
	}
	DEBUG(("start alloc: %d\n", g_module->allocated));
}

void Module_free()
{
	DEBUG(("Module free\n"));
	//release the freelists
	Reply_free_final();
//	Command_free_final();
	Batch_free_final();

	DEBUG(("final alloc: %d\n", g_module->allocated));
}
