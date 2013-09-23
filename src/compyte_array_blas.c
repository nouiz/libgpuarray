#include "compyte/blas.h"
#include "compyte/buffer_blas.h"
#include "compyte/types.h"
#include "compyte/util.h"

int GpuArray_sgemv(cb_transpose t, float alpha, GpuArray *A, GpuArray *X,
		   float beta, GpuArray *Y) {
  compyte_blas_ops *blas;
  size_t elsize;
  size_t n, m;
  cb_order o;
  int err;

  if (A->nd != 2 || X->nd != 1 || Y->nd != 1 ||
      A->typecode != GA_FLOAT || X->typecode != GA_FLOAT ||
      Y->typecode != GA_FLOAT)
    return GA_VALUE_ERROR;

  if (A->flags & GA_F_CONTIGUOUS)
    o = cb_fortran;
  else if (A->flags & GA_C_CONTIGUOUS)
    o = cb_c;
  else
    return GA_VALUE_ERROR;

  if (t == cb_no_trans) {
    if (A->dimensions[1] != X->dimensions[0])
      return GA_VALUE_ERROR;
    n = X->dimensions[0];
  } else {
    if (A->dimensions[1] != Y->dimensions[0])
      return GA_VALUE_ERROR;
    m = Y->dimensions[0];
  }

  elsize = compyte_get_elsize(GA_FLOAT);

  if (A->offset % elsize != 0 || X->offset % elsize != 0 ||
      Y->offset % elsize != 0 || X->strides[0] % elsize != 0 ||
      Y->strides[0] % elsize != 0)
    return GA_VALUE_ERROR;

  err = A->ops->property(A->ctx, NULL, NULL, GA_CTX_PROP_BLAS_OPS, &blas);
  if (err != GA_NO_ERROR)
    return err;

  return blas->sgemv(o, t, m, n, alpha, A->data, A->offset / elsize,
		     A->dimensions[0], X->data, X->offset / elsize,
		     X->strides[0] / elsize, beta, Y->data, Y->offset / elsize,
		     Y->strides[0] / elsize);
}