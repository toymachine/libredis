#include "module.h"
#include "event.h"
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

	event_init();
}

void Module_dispatch()
{
	DEBUG(("Module before dispatch\n"));
	event_dispatch();
	DEBUG(("Module after dispatch\n"));
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
