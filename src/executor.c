#include "common.h"
#include "executor.h"

struct _Executor
{
	int a;
};

Executor *Executor_new()
{
	Executor *executor = Alloc_alloc_T(Executor);
	return executor;
}

void Executor_free(Executor *executor)
{
	Alloc_free_T(executor, Executor);
}

void Executor_add_batch_connection(Executor *executor, Batch *batch, Connection *connection)
{
}

int Executor_execute(Executor *executor)
{
	return 0;
}

int Executor_has_batch(Executor *executor)
{
	return 0;
}

