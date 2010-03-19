#include <string.h>

#include "common.h"

static size_t allocated;

void *_Alloc_alloc(size_t size)
{
	allocated += size;
	DEBUG(("alloc: %d total now: %d\n", size, allocated));
	return malloc(size);
}

void _Alloc_free(void *obj, size_t size)
{
	allocated -= size;
	DEBUG(("dealloc: %d total left: %d\n", size, allocated));
	free(obj);
}

