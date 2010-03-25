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

#endif
