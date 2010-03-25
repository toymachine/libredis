#ifndef REDIS_H
#define REDIS_H

void Module_init();
void Module_dispatch();
void Module_free();

typedef struct _Batch Batch;
typedef struct _Connection Connection;
typedef struct _Ketama Ketama;

Connection *Connection_new(const char *addr);
void Connection_free(Connection *connection);
int Connection_execute(Connection *connection, Batch *batch);

Batch *Batch_new();
void Batch_free(Batch *batch);
void Batch_write(Batch *batch, const char *str, size_t str_len);
void Batch_writef(Batch *batch, const char *format, ...);
void Batch_add_command(Batch *batch);

Ketama *Ketama_new();
void Ketama_free(Ketama *ketama);
void Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight);
void Ketama_create_continuum(Ketama *ketama);
void Ketama_print_continuum(Ketama *ketama);
int Ketama_get_server(Ketama *ketama, char* key, size_t key_len);
char *Ketama_get_server_addr(Ketama *ketama, int ordinal);

#endif
