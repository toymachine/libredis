#include "module.h"
#include "event.h"
#include "reply.h"
#include "batch.h"

void Module_init()
{
	DEBUG(("Module init\n"));
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
}
