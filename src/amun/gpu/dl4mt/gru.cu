#include "gru.h"

namespace amunmt {
namespace GPU {

__global__ void gElementwiseOps(mblas::MatrixWrapper<float> outWrap,
                                const mblas::MatrixWrapper<float> stateWrap,
                                const mblas::MatrixWrapper<float> ruhWrap,
                                const mblas::MatrixWrapper<float> tempWrap,
                                const mblas::MatrixWrapper<float> bWrap,
                                const mblas::MatrixWrapper<float> bx1Wrap,
                                const mblas::MatrixWrapper<float> bx2Wrap)
{
  const uint rows = stateWrap.dim(0);
  const uint cols = stateWrap.dim(1);
  assert(blockIdx.x < rows);
  assert(ruhWrap.dim(1) == cols * 3);

  for(int tid = 0; tid < cols; tid += blockDim.x) {
    int i = tid + threadIdx.x;
    if(i < cols) {
      float ev1 = expf(-(ruhWrap(blockIdx.x, i, 0, 0)
                         + bWrap[i]
                         + tempWrap(blockIdx.x, i, 0, 0)
                        )
                      );
      float r = 1.0f / (1.0f + ev1);

      int k = i + cols;
      float ev2 = expf(-(ruhWrap(blockIdx.x, k, 0, 0)
                         + bWrap[k]
                         + tempWrap(blockIdx.x, k, 0, 0)
                        )
                      );
      float u = 1.0f / (1.0f + ev2);

      float hv = ruhWrap(blockIdx.x, 2*cols + i, 0, 0)
               + bx1Wrap[i];

      float t2v = tempWrap(blockIdx.x, 2*cols + i, 0, 0)
                + bx2Wrap[i];

      hv = tanhf(hv + r * t2v);
      outWrap(blockIdx.x, i, 0, 0) = (1.0f - u) * hv + u * stateWrap(blockIdx.x, i, 0, 0);
    }
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////
__device__
half htanh(const half x)
{
  //half ret = ((half)1.0f - hexp((half)-2.0f * x)) / ((half)1.0f + hexp((half)-2.0f * x));
  //half ret = (hexp((half)2.0f * x) - (half)1.0f) / (hexp((half)2.0f * x) + (half)1.0f);
  //half ret = (hexp(x) - hexp(-x)) / (hexp(x) + hexp(-x));
  half ret = tanhf(x);

  return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

__global__ void gElementwiseOps(mblas::MatrixWrapper<half> outWrap,
                                const mblas::MatrixWrapper<half> stateWrap,
                                const mblas::MatrixWrapper<half> ruhWrap,
                                const mblas::MatrixWrapper<half> tempWrap,
                                const mblas::MatrixWrapper<half> bWrap,
                                const mblas::MatrixWrapper<half> bx1Wrap,
                                const mblas::MatrixWrapper<half> bx2Wrap)
{
  const uint rows = stateWrap.dim(0);
  const uint cols = stateWrap.dim(1);
  assert(blockIdx.x < rows);
  assert(ruhWrap.dim(1) == cols * 3);

  for(int tid = 0; tid < cols; tid += blockDim.x) {
    int i = tid + threadIdx.x;
    if(i < cols) {
      half ev1 = hexp(-(ruhWrap(blockIdx.x, i, 0, 0)
                         + bWrap[i]
                         + tempWrap(blockIdx.x, i, 0, 0)
                        )
                      );
      half r = ((half)1.0f) / ((half)1.0f + ev1);

      int k = i + cols;
      half ev2 = hexp(-(ruhWrap(blockIdx.x, k, 0, 0)
                         + bWrap[k]
                         + tempWrap(blockIdx.x, k, 0, 0)
                        )
                      );
      half u = ((half)1.0f) / ((half)1.0f + ev2);

      half hv = ruhWrap(blockIdx.x, 2*cols + i, 0, 0)
               + bx1Wrap[i];

      half t2v = tempWrap(blockIdx.x, 2*cols + i, 0, 0)
                + bx2Wrap[i];

      hv = htanh(hv + r * t2v);
      //hv = tanhf(hv + r * t2v);
      outWrap(blockIdx.x, i, 0, 0) = ((half)1.0f - u) * hv + u * stateWrap(blockIdx.x, i, 0, 0);
    }
  }
}

}
}

