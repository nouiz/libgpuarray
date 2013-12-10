#include "taskq.h"

#define TQ_RUNNING 0x1
#define TQ_QUIT    0x2
#define TQ_DONE    0x4

struct _taskq {
  TAILQ_HEAD(, task) l;
  pthread_mutex_t m;
  pthread_cond_t c;
  int flags;
};

static inline void lock(taskq *tq) {
  int err;
  err = pthread_mutex_lock(&tq->m);
  if (err)
    abort();
}

static inline void unlock(taskq *tq) {
  int err;
  err = pthread_mutex_unlock(&tq->m);
  if (err)
    abort();
}

static inline void sleep(taskq *tq) {
  int err;
  err = pthread_cond_wait(&tq->c, &tq->m);
  if (err)
    abort();
}

static inline void wakeup(taskq *tq) {
  int err;
  err = pthread_cond_signal(&tq->c);
  if (err)
    abort();
}

static inline task *get() {
  return malloc(sizeof(task));
}

static inline void put(task *t) {
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
  lock(tq);
  tq->flags |= TQ_QUIT;
  if (tq->flags & TQ_RUNNING) {
    unlock(tq);
    wakeup(tq);
    lock(tq);
    while (!(tq->flags & TQ_DONE)) {
      unlock(tq);
      lock(tq);
    }
  }
  unlock(tq);
}

void taskq_destroy(taskq *tq) {
  task *t;
  taskq_stop(tq);
  /* Drain the queue */
  while ((t = TAILQ_FISRT(&tq->l)) != NULL) {
    TAILQ_REMOVE(&t->l, t, entry);
    put(t);
  }
  pthread_cond_destroy(&tq->c);
  pthread_mutex_destroy(&tq->m);
  free(tq);
}

int task_add(taskq *tq, void (*fn)(void *), void *arg) {
  int rv = 0;

  task *t = get();
  t->func = fn;
  t->arg = arg;

  lock(tq);
  TAILQ_INSERT_TAIL(&tq->l, t, entry);
  unlock(tq);
  wakeup(tq);

  return rv;
}

static task *taskq_next_work(taskq *tq) {
  task *next;

  lock(tq);
  while ((next = TAILQ_FIRST(&tq->l)) == NULL) {
    if (tq->flags & TQ_QUIT) {
      tq->flags & TQ_DONE;
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

  tq->flags = TQ_RUNNING;

  while ((work = taskq_next_work(tq)) != NULL) {
    (work->func)(work->arg);
    put(work);
  }
}
