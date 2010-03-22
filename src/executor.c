#include "common.h"
#include "executor.h"

/*
struct _Executor
{
	HashMethodDelegate hash_method;
	Hash server_batch;
};

void Executor_set_server_hash_method(Executor *executor, HashMethodDelegate hash_method)
{
	executor->hash_method = hash_method;
}

Executor *Executor_new()
{
	Executor *executor = Alloc_alloc_T(Executor);
	return executor;
}

void Executor_free(Executor *executor)
{
	Alloc_free_T(executor, Executor);
}

void Executor_write_command(Executor *executor, char *key, size_t key_len, const char *format, ...)
{
	char *addr = executor->hash_method->func(executor->hash_method->instance, key, key_len);
	Batch *batch = Hash_get(executor->server_batch, addr);
	if(batch == NULL) {
		batch = Batch_new();
		if(executor->batch_prefix != NULL) {
			Batch_printv(batch, "%s", executor->batch_prefix)
		}
		Hash_set(executor->server_batch, addr, batch)
	}
	Batch_write_command(batch, format, args);
}

int Executor_execute(Executor *executor)
{
	Hash_for_each(addr, batch) {
		if(executor->batch_postfix != NULL) {
			Batch_printv(batch, "%s", executor->batch_postfix)
		}
		Connection *connection = executor->connection_manager->get_connection(executor->connection_manager, addr);
		Connection_execute(batch, bfi);
	}
	return 0;
}

int Executor_has_reply()
{
	return 0;
}

Reply *Executor_next_reply()
{
	return NULL;
}
*/
