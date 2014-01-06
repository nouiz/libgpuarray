#ifndef TASKQ_H
#define TASKQ_H

#include <pthread.h>
#include <sys/queue.h>

typedef struct _taskq taskq;
typedef struct _task task;

taskq *taskq_create(void);
void taskq_process(taskq *);
void taskq_destroy(taskq *);

task *task_create(int (*)(void *), void *);
void task_add(taskq *, task *);
void task_wait(task *);
int task_res(task *);
void task_destroy(task *);

#endif
