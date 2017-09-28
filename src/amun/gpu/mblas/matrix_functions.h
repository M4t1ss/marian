#pragma once

#define MAX_THREADS 512
#define MAX_BLOCKS 65535

#include <cuda_fp16.h>
#include <cmath>
#include <cublas_v2.h>
#include <thrust/execution_policy.h>
#include <thrust/functional.h>
#include <iostream>

#include "gpu/mblas/thrust_functions.h"
#include "gpu/mblas/matrix.h"
#include "gpu/mblas/matrix_wrapper.h"
#include "gpu/mblas/handles.h"
#include "gpu/mblas/half.h"

namespace amunmt {
namespace GPU {
namespace mblas {

template <class M>
void Debug(const M& m, size_t pos = 0, size_t l = 8) {
  std::cerr << m.dim(0) << " " << m.dim(1) << std::endl;
  for(size_t i = 0; i < m.dim(0); ++i) {
    std::cerr << i << ": ";
    for(size_t j = pos; j < m.dim(1) && j < pos + l; ++j) {
      std::cerr << m.GetVec()[i * m.dim(1) + j] << " ";
    }
    std::cerr << " ... ";

    for(size_t j = m.dim(1) - l; j < m.dim(1);  ++j) {
      std::cerr << m.GetVec()[i * m.dim(1) + j] << " ";
    }
    std::cerr << std::endl;
    // if(i == 4)
      // break;
  }
}

template<typename T>
std::string Debug(const DeviceVector<T> &vec, size_t verbosity = 1)
{
  std::stringstream strm;

  strm << "size=" << vec.size();

  if (verbosity) {
    T sum(0);
    for (size_t i = 0; i < vec.size(); ++i) {
      sum += vec[i];
    }
    strm << " sum=" << sum;
  }

  if (verbosity == 2) {
    for (size_t i = 0; i < vec.size(); ++i) {
      strm << " " << vec[i];
    }
  }

  return strm.str();
}

template<typename T>
std::string Debug(const HostVector<T> &vec, size_t verbosity = 1)
{
  std::stringstream strm;

  strm << "size=" << vec.size();

  if (verbosity) {
    T sum = Sum(vec.data(), vec.size());
    strm << " sum=" << sum;
  }

  if (verbosity == 2) {
    for (size_t i = 0; i < vec.size(); ++i) {
      strm << " " << vec[i];
    }
  }

  return strm.str();
}

/////////////////////////////////////////////////////////////////////////////

std::ostream& operator<<(std::ostream& os, const half &val);

/////////////////////////////////////////////////////////////////////////////

template<typename Tout, typename Tin>
__global__ void gCopyMatrix(MatrixWrapper<Tout> out,
                                const MatrixWrapper<Tin> in)
{
  int id = threadIdx.x + blockIdx.x * blockDim.x;
  if (id < in.size()) {
    uint indices[SHAPE_SIZE];
    in.id2Indices(id, indices);

    out(indices[0], indices[1], indices[2], indices[3])
      = in(indices[0], indices[1], indices[2], indices[3]);
  }

}

template<typename Tout, typename Tin>
void CopyMatrix(TMatrix<Tout> &out, const TMatrix<Tin> &in)
{
  if (in.size() == 0) {
    return;
  }
  //cerr << "out=" << out.Debug(0) << endl;
  //cerr << "in=" << in.Debug(0) << endl;

  assert(out.dim(0) == in.dim(0));
  assert(out.dim(1) == in.dim(1));
  assert(out.dim(2) == in.dim(2));
  assert(out.dim(3) == in.dim(3));

  uint size = in.size();
  uint threads = std::min(size, (uint) MAX_THREADS);
  uint blocks  = (size / threads) + 1;

  const cudaStream_t &stream = CudaStreamHandler::GetStream();
  MatrixWrapper<Tout> outWrap(out);
  const MatrixWrapper<Tin> inWrap(in);

  gCopyMatrix<<<blocks, threads, 0, stream>>>(outWrap, inWrap);
}

/////////////////////////////////////////////////////////////////////////////

template<typename Tout, typename Tin>
__global__ void gCopyVector(Tout *out,
                            const Tin *in,
                            size_t size)
{
  int id = threadIdx.x + blockIdx.x * blockDim.x;
  if (id < size) {
    out[id] = in[id];
  }

}

template<typename Tout, typename Tin>
void CopyVector(DeviceVector<Tout> &out, const DeviceVector<Tin> &in)
{
  if (in.size() == 0) {
    return;
  }
  //cerr << "out=" << out.Debug(0) << endl;
  //cerr << "in=" << in.Debug(0) << endl;

  assert(out.size() == in.size());

  uint size = in.size();
  uint threads = std::min(size, (uint) MAX_THREADS);
  uint blocks  = (size / threads) + 1;

  const cudaStream_t &stream = CudaStreamHandler::GetStream();

  gCopyVector<<<blocks, threads, 0, stream>>>(
      thrust::raw_pointer_cast(out.data()),
      thrust::raw_pointer_cast(in.data()),
      size);
}

/////////////////////////////////////////////////////////////////////////////


template<typename T>
void copy(const T *in, size_t count, T *out,  cudaMemcpyKind kind)
{
  HANDLE_ERROR( cudaMemcpyAsync(out, in, count * sizeof(T), kind, CudaStreamHandler::GetStream()) );
}

/////////////////////////////////////////////////////////////////////////////

inline void copy(const float *in, size_t count, half *out,  cudaMemcpyKind kind)
{
  assert(kind == cudaMemcpyHostToDevice);

  HostVector<half> hostVec(count);
  for (size_t i = 0; i < count; ++i) {
    hostVec[i] = float2half_rn(in[i]);
  }

  HANDLE_ERROR( cudaMemcpyAsync(out,
                                thrust::raw_pointer_cast(hostVec.data()),
                                count * sizeof(half),
                                kind,
                                CudaStreamHandler::GetStream()) );
}

/////////////////////////////////////////////////////////////////////////////

void Fill(Matrix& In, float value=0.0f);

Matrix& Swap(Matrix& Out, Matrix& In);

void Mean(Matrix& Out, const Matrix& In, const IMatrix &sentencesMask);

void WeightedMean(Matrix& Out,const Matrix& Weights, const Matrix& In, const DeviceVector<uint>& mapping);

Matrix& Transpose(Matrix& Out, const Matrix& In);

Matrix& Transpose(Matrix& Out);

Matrix& Copy(Matrix& Out, const Matrix& In);

Matrix& PasteRow(Matrix& Out,
                 const Matrix& In,
                 const size_t r = 0,
                 const size_t c = 0);
void PasteRows(Matrix& Out, const Matrix& In, const size_t rowNo, size_t colNo=0);

Matrix& CopyRow(Matrix& Out,
                const Matrix& In,
                const size_t r = 0,
                const size_t c = 0);

Matrix& Concat(Matrix& Out, const Matrix& In);

void MapMatrix(Matrix& state,
              const mblas::IMatrix &sentencesMask,
              size_t i);

Matrix& CopyRows(Matrix& Out,
                 const Matrix& In,
                 const DeviceVector<uint>& indices);

Matrix& Assemble(Matrix& Out,
                 const Matrix& In,
                 const DeviceVector<uint>& indices);

Matrix& Slice(Matrix& Out,
              const Matrix& In,
              size_t n, size_t dim);

Matrix& Prod(Matrix& C, const Matrix& A, const Matrix& B,
             bool transA = false, bool transB = false);

Matrix& Softmax(Matrix& Out, const DeviceVector<uint>& batchIds, const mblas::IMatrix &sentencesMask, size_t batchSize);

Matrix& LogSoftmax(Matrix& Out);

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gBroadcast(Functor functor,
                           MatrixWrapper<half> outWrap,
                           const MatrixWrapper<half> in1Wrap,
                           const MatrixWrapper<half> in2Wrap,
                           const MatrixWrapper<uint> batchMappingWrap)
{
  size_t srcSize = outWrap.dim(2);
  size_t inRows = in2Wrap.dim(0);
  size_t cols  = in1Wrap.dim(1);

  int id = threadIdx.x + blockIdx.x * blockDim.x;
  if (id < outWrap.size()) {
    /*
    size_t indices[SHAPE_SIZE];
    outWrap.id2Indices(id, indices);

    int row = id / cols; // len * batch for in1
    int srcId = row % srcSize;  // source pos for in1

    int batchMappingIdx = row / srcSize; // batch for in1
    int batchIdx = batchMappingWrap[batchMappingIdx]; // batch id for in1

    outWrap[id] = functor(in1Wrap(srcId, indices[1], 0, batchIdx),
                          in2Wrap(batchMappingIdx, indices[1], 0, 0) );
    */

    int row = id / cols;
    int stateIdx = id % cols;

    int beamIdx = row / srcSize;
    int srcId = row % srcSize;

    int batchIdx = batchMappingWrap[beamIdx];

    outWrap[id] = functor(in1Wrap[(batchIdx * srcSize + srcId) * cols + stateIdx],
                          in2Wrap[beamIdx * cols + stateIdx]);
  }
}

template <class Functor>
HalfMatrix& Broadcast(Functor functor,
                      HalfMatrix& OutOrig,
                      const HalfMatrix& In,
                      const DeviceVector<uint>& batchMapping,
                      size_t srcSize)
{
  size_t sumOfBeamSizes = In.dim(0);

  //size_t rows = srcSize * sumOfBeamSizes;
  size_t cols  = OutOrig.dim(1);

  thread_local static HalfMatrix OutNew;
  OutNew.NewSize(sumOfBeamSizes, cols, srcSize);

  MatrixWrapper<half> outWrap(OutNew);
  const MatrixWrapper<half> in1Wrap(OutOrig);
  const MatrixWrapper<half> in2Wrap(In);
  const MatrixWrapper<uint> batchMappingWrap(batchMapping);

  int threads = MAX_THREADS;
  int blocks  = (OutNew.size() / threads) + ((OutNew.size() % threads == 0) ?  0 : 1);

  gBroadcast<<<blocks, threads, 0, CudaStreamHandler::GetStream()>>>
    (functor, outWrap, in1Wrap, in2Wrap, batchMappingWrap);

  /*
  std::cerr << "nBlocks=" << blocks << std::endl;
  std::cerr << "nThreads=" << threads << std::endl;
  std::cerr << "outWrap=" << outWrap.Debug() << std::endl;
  std::cerr << "in1Wrap=" << in1Wrap.Debug() << std::endl;
  std::cerr << "in2Wrap=" << in2Wrap.Debug() << std::endl;
  std::cerr << "batchMapping=" << Debug(batchMapping, 2) << std::endl;
  std::cerr << "srcSize=" << srcSize << std::endl;
  std::cerr << "sumOfBeamSizes=" << sumOfBeamSizes << std::endl;
  std::cerr << std::endl;

  HANDLE_ERROR(cudaDeviceSynchronize());
  */

  OutOrig.swap(OutNew);
  return OutOrig;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gBroadcastVecColumn(Functor functor,
                                    MatrixWrapper<half> outWrap,
                                    const MatrixWrapper<half> inWrap)
{
  extern __shared__ half sdataOrigHalf[];

  size_t rows  = outWrap.dim(0);
  size_t cols = outWrap.dim(1);

  MatrixWrapper<half> sdata(sdataOrigHalf, rows);

  if (threadIdx.x == 0) {
    for (int i = 0; i < rows; ++i)
      sdata[i] = inWrap[i];
  }
  __syncthreads();

  int noColumn = threadIdx.x + blockDim.x * blockIdx.x;
  if (noColumn < cols) {
    for (int noRow = 0; noRow < rows; ++noRow) {
      half &val = outWrap(noRow, noColumn, 0, 0);
      val = functor(val, sdata[noRow]);
    }
  }
}

template <class Functor>
HalfMatrix& BroadcastVecColumn(Functor functor,
                              HalfMatrix& Out,
                              const DeviceVector<half>& In)
{
  size_t rows  = Out.dim(0);
  size_t cols = Out.dim(1);

  MatrixWrapper<half> outWrap(Out);
  const MatrixWrapper<half> inWrap(In);

  int threads = std::min(MAX_THREADS, (int)cols);
  int blocks  = cols / threads  + ((cols % threads == 0) ?  0 : 1);

  gBroadcastVecColumn<<<blocks, threads, rows * sizeof(half), CudaStreamHandler::GetStream()>>>
    (functor, outWrap, inWrap);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
Matrix& BroadcastVecColumn(Functor functor,
                          Matrix& Out,
                          const DeviceVector<float>& In)
{
  /*
  size_t rows  = Out.dim(0);
  size_t cols = Out.dim(1);

  MatrixWrapper<float> outWrap(Out);
  const MatrixWrapper<float> inWrap(In);

  int threads = std::min(MAX_THREADS, (int)cols);
  int blocks  = cols / threads  + ((cols % threads == 0) ?  0 : 1);

  gBroadcastVecColumn<<<blocks, threads, rows * sizeof(float), CudaStreamHandler::GetStream()>>>
    (functor, outWrap, inWrap);
  */

  HalfMatrix halfOut(Out.dim(0), Out.dim(1), Out.dim(2), Out.dim(3));
  CopyMatrix(halfOut, Out);

  DeviceVector<half> halfIn(In.size());
  CopyVector(halfIn, In);

  BroadcastVecColumn(functor, halfOut, halfIn);

  CopyMatrix(Out, halfOut);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gBroadcastVec(Functor functor,
                              MatrixWrapper<half> outWrap,
                              const MatrixWrapper<half> inWrap)
{
  size_t cols = outWrap.dim(1);

  int noColumn = threadIdx.x + blockDim.x * blockIdx.x;
  if (noColumn < cols) {
    float vecValue = inWrap(0, noColumn, 0, 0);

    for (int dim0 = 0; dim0 < outWrap.dim(0); ++dim0) {
      for (int dim2 = 0; dim2 < outWrap.dim(2); ++dim2) {
        for (int dim3 = 0; dim3 < outWrap.dim(3); ++dim3) {
          half &val = outWrap(dim0, noColumn, dim2, dim3);
          val = functor(val, vecValue);
        }
      }
    }

  }
}

template <class Functor>
HalfMatrix& BroadcastVec(Functor functor, HalfMatrix& Out, const HalfMatrix& In)
{
  //std::cerr << "Out=" << Out.Debug() << std::endl;
  //std::cerr << "In=" << In.Debug() << std::endl;

  size_t cols = Out.dim(1);

  MatrixWrapper<half> outWrap(Out);
  const MatrixWrapper<half> inWrap(In);

  int threads = std::min(MAX_THREADS, (int)cols);
  int blocks  = cols / threads  + ((cols % threads == 0) ?  0 : 1);
  const cudaStream_t& stream = CudaStreamHandler::GetStream();

  gBroadcastVec<<<blocks, threads, 0, stream>>>
    (functor, outWrap, inWrap);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gElement(Functor functor,
                         MatrixWrapper<half> outWrap)
{
  size_t ind = blockIdx.x * blockDim.x + threadIdx.x;
  if (ind < outWrap.size()) {
    outWrap[ind] = functor(outWrap[ind]);
  }
}

template <class Functor>
HalfMatrix& Element(Functor functor,
                HalfMatrix& Out)
{
  int threads = MAX_THREADS;
  int blocks  = Out.size() / threads + ((Out.size() % threads == 0) ?  0 : 1);
  const cudaStream_t& stream = CudaStreamHandler::GetStream();

  MatrixWrapper<half> outWrap(Out);

  gElement<<<blocks, threads, 0, stream>>>
    (functor, outWrap);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gElement(Functor functor,
                         MatrixWrapper<half> outWrap,
                         const MatrixWrapper<half> inWrap)
{
  size_t ind = blockIdx.x * blockDim.x + threadIdx.x;
  if (ind < outWrap.size()) {
    outWrap[ind] = functor(outWrap[ind], inWrap[ind]);
  }
}

template <class Functor>
HalfMatrix& Element(Functor functor,
                HalfMatrix& Out, const HalfMatrix& In)
{
  assert(Out.size() == In.size());

  int threads = MAX_THREADS;
  int blocks  = Out.size() / threads + ((Out.size() % threads == 0) ?  0 : 1);
  const cudaStream_t& stream = CudaStreamHandler::GetStream();

  MatrixWrapper<half> outWrap(Out);
  const MatrixWrapper<half> inWrap(In);

  gElement<<<blocks, threads, 0, stream>>>
    (functor, outWrap, inWrap);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

template <class Functor>
__global__ void gElement(Functor functor,
                         MatrixWrapper<half> outWrap,
                         const MatrixWrapper<half> in1Wrap,
                         const MatrixWrapper<half> in2Wrap)
{
  size_t ind = blockIdx.x * blockDim.x + threadIdx.x;
  if (ind < outWrap.size()) {
    outWrap[ind] = functor(outWrap[ind], in1Wrap[ind], in2Wrap[ind]);
  }
}

template <class Functor>
HalfMatrix& Element(Functor functor,
                HalfMatrix& Out,
                const HalfMatrix& In1,
                const HalfMatrix& In2)
{
  assert(Out.size() == In1.size());
  assert(Out.size() == In2.size());

  int threads = MAX_THREADS;
  int blocks  = Out.size() / threads + ((Out.size() % threads == 0) ?  0 : 1);
  const cudaStream_t& stream = CudaStreamHandler::GetStream();

  MatrixWrapper<half> outWrap(Out);
  const MatrixWrapper<half> in1Wrap(In1);
  const MatrixWrapper<half> in2Wrap(In2);

  gElement<<<blocks, threads, 0, stream>>>
    (functor, outWrap, in1Wrap, in2Wrap);

  return Out;
}

/////////////////////////////////////////////////////////////////////////////

void SetColumn(Matrix& In, int noColumn, float value);

void Normalization(Matrix& out, const Matrix& in, const Matrix& alpha, const Matrix& beta,
                   float eps);

void Normalization(Matrix& out, const Matrix& in, const Matrix& alpha, float eps);


} // namespace mblas
} // namespace GPU
}
