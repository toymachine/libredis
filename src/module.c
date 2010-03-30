#include "module.h"
#include "event.h"
#include "reply.h"
#include "batch.h"

Module *g_module;

void Module_init(Module *module)
{
	if(module == NULL) {
		abort();
	}
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
	g_module = module;
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
