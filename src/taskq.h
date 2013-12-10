#ifndef TASKQ_H
#define TASKQ_H

#include <pthread.h>
#include <sys/queue.h>

typedef struct _task {
  TAILQ_ENTRY(task) entry;
  void (*func)(void *);
  void *arg;
} task;

typedef struct _taskq taskq;

taskq *taskq_create();
void taskq_destroy(taskq *);
int task_add(taskq *, void (*)(void *), void *);
void taskq_process(taskq *tq);

#endif
