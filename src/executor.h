/*
 * executor.h
 *
 *  Created on: Mar 22, 2010
 *      Author: henk
 */

#ifndef EXECUTOR_H_
#define EXECUTOR_H_

Executor *Executor_new();
void Executor_free(Executor *executor);

void Executor_add_batch_connection(Executor *executor, Batch *batch, Connection *connection);
int Executor_execute(Executor *executor);

int Executor_has_batch(Executor *executor);

#endif /* EXECUTOR_H_ */
