#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "redis.h"

struct _Redis
{
	size_t allocated;
};

static Redis redis_instance;

Redis *Redis_get_instance()
{
	return &redis_instance;
}

void *_Redis_alloc(Redis *redis, size_t size)
{
	redis->allocated += size;
	DEBUG(("alloc: %d total now: %d\n", size, redis->allocated));
	return malloc(size);
}

void _Redis_free(Redis *redis, void *obj, size_t size)
{
	redis->allocated -= size;
	DEBUG(("dealloc: %d total left: %d\n", size, redis->allocated));
	free(obj);
}



