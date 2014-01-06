#include "taskq.h"

#include <stdlib.h>

#define T_AUTODEL 0x1
#define T_DONE    0x2

struct _task {
  TAILQ_ENTRY(_task) entry;
  int (*func)(void *);
  void *arg;
  pthread_mutex_t m;
  pthread_cond_t c;
  volatile int flags;
  int res;
};

#define TQ_RUNNING 0x1
#define TQ_QUIT    0x2
#define TQ_DONE    0x4

struct _taskq {
  TAILQ_HEAD(, _task) l;
  pthread_mutex_t m;
  pthread_cond_t c;
  volatile int flags;
};

#define set(tq, v) ((tq)->flags |= (v))
#define clear(tq, v) ((tq)->flags &= ~(v))
#define test(tq, v) ((tq)->flags & (v))

#define lock(tq) do {				\
    int err;					\
    err = pthread_mutex_lock(&tq->m);		\
    if (err)					\
      abort();					\
  } while (0)

#define unlock(tq) do {				\
    int err;					\
    err = pthread_mutex_unlock(&tq->m);		\
    if (err)					\
      abort();					\
  } while (0)

#define sleep(tq) do {				\
    int err;					\
    err = pthread_cond_wait(&tq->c, &tq->m);	\
    if (err)					\
      abort();					\
  } while (0)

#define wakeup(tq) do {				\
    int err;					\
    err = pthread_cond_signal(&tq->c);		\
    if (err)					\
      abort();					\
  } while (0)

#define wakeup_all(tq) do {			\
    int err;					\
    err = pthread_cond_broadcast(&tq->c);	\
    if (err)					\
      abort();					\
  } while (0)

static inline task *get() {
  int err;
  task *res = malloc(sizeof(task));
  if (res == NULL) return NULL;
  err = pthread_mutex_init(&res->m, NULL);
  if (err) {
    free(res);
    return NULL;
  }
  err = pthread_cond_init(&res->c, NULL);
  if (err) {
    pthread_mutex_destroy(&res->m);
    free(res);
    return NULL;
  }
  return res;
}

static inline void put(task *t) {
  pthread_mutex_destroy(&t->m);
  pthread_cond_destroy(&t->c);
  free(t);
}

taskq *taskq_create() {
  taskq *tq;
  int err;
  tq = malloc(sizeof(*tq));
  if (tq == NULL)
    return NULL;
  tq->flags = 0;

  err = pthread_mutex_init(&tq->m, NULL);
  if (err) {
    free(tq);
    return NULL;
  }

  err = pthread_cond_init(&tq->c, NULL);
  if (err) {
    pthread_mutex_destroy(&tq->m);
    free(tq);
    return NULL;
  }

  TAILQ_INIT(&tq->l);

  return tq;
}

static void taskq_stop(taskq* tq) {
  int fl;
  lock(tq);
  set(tq, TQ_QUIT);
  fl = tq->flags;
  if (fl & TQ_RUNNING) {
    wakeup(tq);
    while (!(fl & TQ_DONE)) {
      unlock(tq);
      lock(tq);
      fl = tq->flags;
    }
  }
  unlock(tq);
}

void taskq_destroy(taskq *tq) {
  task *t;
  taskq_stop(tq);
  /* Drain the queue */
  while ((t = TAILQ_FIRST(&tq->l)) != NULL) {
    TAILQ_REMOVE(&tq->l, t, entry);
    put(t);
  }
  pthread_cond_destroy(&tq->c);
  pthread_mutex_destroy(&tq->m);
  free(tq);
}

task *task_create(int (*fn)(void *), void *arg) {
  task *t = get();
  if (t == NULL)
    return NULL;
  t->func = fn;
  t->arg = arg;
  t->flags = 0;
  return t;
}

void task_add(taskq *tq, task *t) {
  lock(tq);
  TAILQ_INSERT_TAIL(&tq->l, t, entry);
  unlock(tq);
  wakeup(tq);
}

void task_wait(task *t) {
  assert(!test(t, T_AUTODEL));
  lock(t);
  while (!test(t, T_DONE))
    sleep(t);
  unlock(t);
}

int task_res(task *t) {
  task_wait(t);
  return t->res;
}

void task_destroy(task *t) {
  task_wait(t);
  put(t);
}

static task *taskq_next_work(taskq *tq) {
  task *next;

  lock(tq);
  while ((next = TAILQ_FIRST(&tq->l)) == NULL) {
    if (test(tq, TQ_QUIT)) {
      set(tq, TQ_DONE);
      unlock(tq);
      return NULL;
    }
    sleep(tq);
  }

  TAILQ_REMOVE(&tq->l, next, entry);
  unlock(tq);

  return next;
}

void taskq_process(taskq *tq) {
  task *work;
  int fl;

  set(tq, TQ_RUNNING);

  while ((work = taskq_next_work(tq)) != NULL) {
    work->res = (work->func)(work->arg);
    lock(work);
    set(work, T_DONE);
    fl = work->flags;
    unlock(work);
    if (fl & T_AUTODEL) {
      put(work);
    } else {
      wakeup_all(work);
    }
  }
}
