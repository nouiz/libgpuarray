#include "private.h"
#include "private_opencl.h"

#include "loaders/libclblas.h"

#include "gpuarray/buffer_blas.h"
#include "gpuarray/error.h"

extern const gpuarray_buffer_ops opencl_ops;

static inline clblasOrder convO(cb_order order) {
  switch (order) {
  case cb_row:
    return clblasRowMajor;
  case cb_column:
    return clblasColumnMajor;
  default:
    return -1;
  }
}

static inline clblasTranspose convT(cb_transpose trans) {
  switch (trans) {
  case cb_no_trans:
    return clblasNoTrans;
  case cb_trans:
    return clblasTrans;
  case cb_conj_trans:
    return clblasConjTrans;
  default:
    return -1;
  }
}

static unsigned int refcnt = 0;

static int setup(gpucontext *ctx) {
  clblasStatus err;

  if (refcnt == 0) {
    err = clblasSetup();
    if (err != clblasSuccess)
      return GA_BLAS_ERROR;
  }

  if (ctx->blas_handle == NULL)
    ctx->blas_handle = &refcnt;
  refcnt++;
  return GA_NO_ERROR;
}

static void teardown(gpucontext *ctx) {
  if (ctx->blas_handle != NULL) {
    ctx->blas_handle = NULL;
    refcnt--;
  }
  if (refcnt == 0)
    clblasTeardown();
}

static const char *error(gpucontext *ctx) {
  return "(clblas) error in blas call, no details for now.";
}

#define ARRAY_INIT(A)                           \
  if (A->ev != NULL)                            \
    evl[num_ev++] = A->ev

#define ARRAY_FINI(A)                           \
  if (A->ev != NULL)                            \
    clReleaseEvent(A->ev);                      \
  A->ev = ev;                                   \
  clRetainEvent(A->ev)

static int hgemmBatch(cb_order order, cb_transpose transA, cb_transpose transB,
                      size_t M, size_t N, size_t K, float alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **B, size_t *offB, size_t ldb,
                      float beta, gpudata **C, size_t *offC, size_t ldc,
                      size_t batchCount) {
  return GA_DEVSUP_ERROR;
}

static int sgemmBatch(cb_order order, cb_transpose transA, cb_transpose transB,
                      size_t M, size_t N, size_t K, float alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **B, size_t *offB, size_t ldb,
                      float beta, gpudata **C, size_t *offC, size_t ldc,
                      size_t batchCount) {
  cl_ctx *ctx = A[0]->ctx;
  cl_event evl[3];
  cl_event ev;
  size_t i;
  cl_uint num_ev = 0;
  clblasStatus err;

  for (i = 0; i < batchCount; i++) {
    ARRAY_INIT(A[i]);
    ARRAY_INIT(B[i]);
    ARRAY_INIT(C[i]);
    err = clblasSgemm(convO(order), convT(transA), convT(transB), M, N, K,
                      alpha, A[i]->buf, offA[i], lda, B[i]->buf, offB[i], ldb,
                      beta, C[i]->buf, offB[i], ldc, 1, &ctx->q,
                      num_ev, num_ev == 0 ? NULL : evl, &ev);
    if (err != clblasSuccess)
      return GA_BLAS_ERROR;
    ARRAY_FINI(A[i]);
    ARRAY_FINI(B[i]);
    ARRAY_FINI(C[i]);
    clReleaseEvent(ev);
  }

  return GA_NO_ERROR;
}

static int dgemmBatch(cb_order order, cb_transpose transA, cb_transpose transB,
                      size_t M, size_t N, size_t K, double alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **B, size_t *offB, size_t ldb,
                      double beta, gpudata **C, size_t *offC, size_t ldc,
                      size_t batchCount) {
  cl_ctx *ctx = A[0]->ctx;
  cl_event evl[3];
  cl_event ev;
  size_t i;
  cl_uint num_ev = 0;
  clblasStatus err;

  for (i = 0; i < batchCount; i++) {
    ARRAY_INIT(A[i]);
    ARRAY_INIT(B[i]);
    ARRAY_INIT(C[i]);
    err = clblasDgemm(convO(order), convT(transA), convT(transB), M, N, K,
                      alpha, A[i]->buf, offA[i], lda, B[i]->buf, offB[i], ldb,
                      beta, C[i]->buf, offB[i], ldc, 1, &ctx->q,
                      num_ev, num_ev == 0 ? NULL : evl, &ev);
    if (err != clblasSuccess)
      return GA_BLAS_ERROR;
    ARRAY_FINI(A[i]);
    ARRAY_FINI(B[i]);
    ARRAY_FINI(C[i]);
    clReleaseEvent(ev);
  }

  return GA_NO_ERROR;
}

static int hgemvBatch(cb_order order, cb_transpose transA,
                      size_t M, size_t N, float alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **x, size_t *offX, size_t incX,
                      float beta, gpudata **y, size_t *offY, size_t incY,
                      size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int sgemvBatch(cb_order order, cb_transpose transA,
                      size_t M, size_t N, float alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **x, size_t *offX, size_t incX,
                      float beta, gpudata **y, size_t *offY, size_t incY,
                      size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int dgemvBatch(cb_order order, cb_transpose transA,
                      size_t M, size_t N, double alpha,
                      gpudata **A, size_t *offA, size_t lda,
                      gpudata **x, size_t *offX, size_t incX,
                      double beta, gpudata **y, size_t *offY, size_t incY,
                      size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int hgerBatch(cb_order order, size_t M, size_t N, float alpha,
                     gpudata **x, size_t *offX, size_t incX,
                     gpudata **y, size_t *offY, size_t incY,
                     gpudata **A, size_t *offA, size_t lda,
                     size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int sgerBatch(cb_order order, size_t M, size_t N, float alpha,
                     gpudata **x, size_t *offX, size_t incX,
                     gpudata **y, size_t *offY, size_t incY,
                     gpudata **A, size_t *offA, size_t lda,
                     size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int dgerBatch(cb_order order, size_t M, size_t N, double alpha,
                     gpudata **x, size_t *offX, size_t incX,
                     gpudata **y, size_t *offY, size_t incY,
                     gpudata **A, size_t *offA, size_t lda,
                     size_t batchCount, int flags) {
  return GA_DEVSUP_ERROR;
}

static int hdot(
        size_t N,
        gpudata *X, size_t offX, size_t incX,
        gpudata *Y, size_t offY, size_t incY,
        gpudata *Z, size_t offZ) {
    return GA_DEVSUP_ERROR;
}

static int sdot(
        size_t N,
        gpudata *X, size_t offX, size_t incX,
        gpudata *Y, size_t offY, size_t incY,
        gpudata *Z, size_t offZ) {
  cl_ctx *ctx = X->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;
  gpudata *wbuf;
  int alloc_err;

  wbuf = opencl_ops.buffer_alloc(
          (gpucontext*)ctx,
          N*sizeof(float), NULL, GA_BUFFER_READ_WRITE, &alloc_err);
  if (alloc_err != GA_NO_ERROR)
      return alloc_err;

  ARRAY_INIT(X);
  ARRAY_INIT(Y);
  ARRAY_INIT(Z);

  // TODO: a thread-safe static buffer or allocator?
  err = clblasSdot(
          N, Z->buf, offZ,
          X->buf, offX, incX,
          Y->buf, offY, incY,
          wbuf->buf, 1, &ctx->q,
          num_ev, num_ev ? evl : NULL, &ev);
  if (err != clblasSuccess)
      return GA_BLAS_ERROR;

  ARRAY_FINI(X);
  ARRAY_FINI(Y);
  ARRAY_FINI(Z);

  opencl_ops.buffer_release(wbuf);
  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int ddot(
        size_t N,
        gpudata *X, size_t offX, size_t incX,
        gpudata *Y, size_t offY, size_t incY,
        gpudata *Z, size_t offZ) {
  cl_ctx *ctx = X->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;
  gpudata *wbuf;
  int alloc_err;

  wbuf = opencl_ops.buffer_alloc(
          (gpucontext*)ctx,
          N*sizeof(double), NULL, GA_BUFFER_READ_WRITE, &alloc_err);
  if (alloc_err != GA_NO_ERROR)
      return alloc_err;

  ARRAY_INIT(X);
  ARRAY_INIT(Y);
  ARRAY_INIT(Z);

  err = clblasDdot(
          N, Z->buf, offZ,
          X->buf, offX, incX,
          Y->buf, offY, incY,
          wbuf->buf, 1, &ctx->q,
          num_ev, num_ev ? evl : NULL, &ev);
  if (err != clblasSuccess)
      return GA_BLAS_ERROR;

  ARRAY_FINI(X);
  ARRAY_FINI(Y);
  ARRAY_FINI(Z);

  opencl_ops.buffer_release(wbuf);
  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int hgemv(cb_order order, cb_transpose transA, size_t M, size_t N,
                 float alpha, gpudata *A, size_t offA, size_t lda,
                 gpudata *X, size_t offX, int incX, float beta,
                 gpudata *Y, size_t offY, int incY) {
  return GA_DEVSUP_ERROR;
}

static int sgemv(cb_order order, cb_transpose transA, size_t M, size_t N,
                 float alpha, gpudata *A, size_t offA, size_t lda,
                 gpudata *X, size_t offX, int incX, float beta,
                 gpudata *Y, size_t offY, int incY) {
  cl_ctx *ctx = A->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;

  ARRAY_INIT(A);
  ARRAY_INIT(X);
  ARRAY_INIT(Y);

  err = clblasSgemv(convO(order), convT(transA), M, N, alpha,
                    A->buf, offA, lda, X->buf, offX, incX,
                    beta, Y->buf, offY, incY, 1, &ctx->q,
                    num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(A);
  ARRAY_FINI(X);
  ARRAY_FINI(Y);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int dgemv(cb_order order, cb_transpose transA, size_t M, size_t N,
                 double alpha, gpudata *A, size_t offA, size_t lda,
                 gpudata *X, size_t offX, int incX, double beta,
                 gpudata *Y, size_t offY, int incY) {
  cl_ctx *ctx = A->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;

  ARRAY_INIT(A);
  ARRAY_INIT(X);
  ARRAY_INIT(Y);

  err = clblasDgemv(convO(order), convT(transA), M, N, alpha,
                    A->buf, offA, lda, X->buf, offX, incX,
                    beta, Y->buf, offY, incY, 1, &ctx->q,
                    num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(A);
  ARRAY_FINI(X);
  ARRAY_FINI(Y);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int hgemm(cb_order order, cb_transpose transA, cb_transpose transB,
                 size_t M, size_t N, size_t K, float alpha,
                 gpudata *A, size_t offA, size_t lda,
                 gpudata *B, size_t offB, size_t ldb, float beta,
                 gpudata *C, size_t offC, size_t ldc) {
  return GA_DEVSUP_ERROR;
}

static int sgemm(cb_order order, cb_transpose transA, cb_transpose transB,
                 size_t M, size_t N, size_t K, float alpha,
                 gpudata *A, size_t offA, size_t lda,
                 gpudata *B, size_t offB, size_t ldb, float beta,
                 gpudata *C, size_t offC, size_t ldc) {
  cl_ctx *ctx = A->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;

  ARRAY_INIT(A);
  ARRAY_INIT(B);
  ARRAY_INIT(C);

  err = clblasSgemm(convO(order), convT(transA), convT(transB), M, N, K,
                    alpha, A->buf, offA, lda, B->buf, offB, ldb,
                    beta, C->buf, offC, ldc, 1, &ctx->q,
                    num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(A);
  ARRAY_FINI(B);
  ARRAY_FINI(C);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int dgemm(cb_order order, cb_transpose transA, cb_transpose transB,
                 size_t M, size_t N, size_t K, double alpha,
                 gpudata *A, size_t offA, size_t lda,
                 gpudata *B, size_t offB, size_t ldb, double beta,
                 gpudata *C, size_t offC, size_t ldc) {
  cl_ctx *ctx = A->ctx;
  clblasStatus err;
  cl_uint num_ev = 0;
  cl_event evl[3];
  cl_event ev;

  ARRAY_INIT(A);
  ARRAY_INIT(B);
  ARRAY_INIT(C);

  err = clblasDgemm(convO(order), convT(transA), convT(transB), M, N, K,
                    alpha, A->buf, offA, lda, B->buf, offB, ldb,
                    beta, C->buf, offC, ldc, 1, &ctx->q,
                    num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(A);
  ARRAY_FINI(B);
  ARRAY_FINI(C);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int hger(cb_order order, size_t M, size_t N, float alpha,
                gpudata *X, size_t offX, int incX,
                gpudata *Y, size_t offY, int incY,
                gpudata *A, size_t offA, size_t lda) {
  return GA_DEVSUP_ERROR;
}

static int sger(cb_order order, size_t M, size_t N, float alpha,
                gpudata *X, size_t offX, int incX,
                gpudata *Y, size_t offY, int incY,
                gpudata *A, size_t offA, size_t lda) {
  cl_ctx *ctx = X->ctx;
  cl_event evl[3];
  cl_event ev;
  cl_uint num_ev = 0;
  clblasStatus err;

  ARRAY_INIT(X);
  ARRAY_INIT(Y);
  ARRAY_INIT(A);

  err = clblasSger(convO(order), M, N, alpha, X->buf, offX, incX,
                   Y->buf, offY, incY, A->buf, offA, lda, 1, &ctx->q,
                   num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(X);
  ARRAY_FINI(Y);
  ARRAY_FINI(A);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

static int dger(cb_order order, size_t M, size_t N, double alpha,
                gpudata *X, size_t offX, int incX,
                gpudata *Y, size_t offY, int incY,
                gpudata *A, size_t offA, size_t lda) {
  cl_ctx *ctx = X->ctx;
  cl_event evl[3];
  cl_event ev;
  cl_uint num_ev = 0;
  clblasStatus err;

  ARRAY_INIT(X);
  ARRAY_INIT(Y);
  ARRAY_INIT(A);

  err = clblasDger(convO(order), M, N, alpha, X->buf, offX, incX,
                   Y->buf, offY, incY, A->buf, offA, lda, 1, &ctx->q,
                   num_ev, num_ev == 0 ? NULL : evl, &ev);
  if (err != clblasSuccess)
    return GA_BLAS_ERROR;

  ARRAY_FINI(X);
  ARRAY_FINI(Y);
  ARRAY_FINI(A);

  clReleaseEvent(ev);

  return GA_NO_ERROR;
}

GPUARRAY_LOCAL gpuarray_blas_ops clblas_ops = {
  setup,
  teardown,
  error,
  hdot, /* TODO */
  sdot,
  ddot,
  hgemv, /* TODO */
  sgemv,
  dgemv,
  hgemm, /* TODO */
  sgemm,
  dgemm,
  hger, /* TODO */
  sger,
  dger,
  hgemmBatch, /* TODO */
  sgemmBatch,
  dgemmBatch,
  hgemvBatch, /* TODO */
  sgemvBatch, /* TODO */
  dgemvBatch, /* TODO */
  hgerBatch, /* TODO */
  sgerBatch, /* TODO */
  dgerBatch, /* TODO */
};
