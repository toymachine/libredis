#ifndef REDIS_H
#define REDIS_H

void Module_init();
void Module_dispatch();
void Module_free();

typedef struct _Batch Batch;
typedef struct _Connection Connection;
typedef struct _Ketama Ketama;

Batch *Batch_new();
void Batch_free(Batch *batch);

Ketama *Ketama_new();
void Ketama_free(Ketama *ketama);
void Ketama_add_server(Ketama *ketama, const char *addr, int port, unsigned long weight);
void Ketama_create_continuum(Ketama *ketama);
void Ketama_print_continuum(Ketama *ketama);
int Ketama_get_server(Ketama *ketama, char* key, size_t key_len);
char *Ketama_get_server_addr(Ketama *ketama, int ordinal);

#endif
