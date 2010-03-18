#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "redis.h"

struct _Redis
{
};

static Redis redis_instance;

Redis *Redis_get_instance()
{
	return &redis_instance;
}

void *_Redis_alloc(Redis *redis, size_t size)
{
	DEBUG(("alloc: %d\n", size));
	return malloc(size);
}

void _Redis_free(Redis *redis, void *obj, size_t size)
{
	DEBUG(("dealloc: %d\n", size));
	free(obj);
}



