#include "taskq.h"

#define TQ_RUNNING 0x1
#define TQ_QUIT    0x2
#define TQ_DONE    0x4

typedef struct _task {
  TAILQ_ENTRY(task) entry;
  void (*func)(void *);
  void *arg;
} task;

typedef struct _event {
  pthread_mutex_t m;
  pthread_cond_t c;
  volatile int res;
};

struct _taskq {
  TAILQ_HEAD(, task) l;
  pthread_mutex_t m;
  pthread_cond_t c;
  volatile int flags;
};

#define lock(tq) do {				\
    int err;					\
    err = pthread_mutex_lock(&tq->m);		\
    if (err)					\
      abort();					\
  } while (0)

#define unlock(tq) {				\
    int err;					\
    err = pthread_mutex_unlock(&tq->m);		\
    if (err)					\
      abort();					\
  } while (0)

#define sleep(tq) {				\
    int err;					\
    err = pthread_cond_wait(&tq->c, &tq->m);	\
    if (err)					\
      abort();					\
  } while (0)

#define wakeup(tq) {				\
    int err;					\
    err = pthread_cond_signal(&tq->c);		\
    if (err)					\
      abort();					\
  } while (0)

#define wakeup_all(tq) {			\
    int err;					\
    err = pthread_cond_broadcast(&tq->c);	\
    if (err)					\
      abort();					\
  } while (0)

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
  int fl;
  lock(tq);
  tq->flags |= TQ_QUIT;
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

static void event_task(void *e) {
  event_post((event *)e, 1);
}

int task_event(taskq *tq, event *ev) {
  return taskq_add(tq, event_task, ev);
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

event *event_create(void) {
  int err;
  event *res = malloc();
  if (res == NULL)
    return NULL;
  res->done = 0;

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

void event_destroy(event *ev) {
  pthread_mutex_destroy(&ev->m);
  pthread_cond_destroy(&ev->c);
  free(ev);
}

int event_wait(event *ev) {
  lock(ev);
  while (ev->done == 0)
    sleep(ev);
  unlock(ev);
}

void event_post(event *ev, int val) {
  if (val == 0) val = 1;
  lock(ev);
  ev->done = val;
  unlock(ev);
  wakeup_all(ev);
}

void event_reset(event *ev) {
  ev->done = 0;
}
