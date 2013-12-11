#ifndef TASKQ_H
#define TASKQ_H

#include <pthread.h>
#include <sys/queue.h>

typedef struct _taskq taskq;
typedef struct _event event;

taskq *taskq_create(void);
void taskq_destroy(taskq *);
int task_add(taskq *, void (*)(void *), void *);
int task_event(taskq *, event *);
void taskq_process(taskq *);

event *event_create(void);
void event_destroy(event *);
int event_wait(event *);
void event_post(event *, int);
void event_reset(event *);

#endif
