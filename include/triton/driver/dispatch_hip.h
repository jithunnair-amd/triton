// !!! This is a file automatically generated by hipify!!!
#pragma once

#ifndef _TRITON_DRIVER_DISPATCH_H_
#define _TRITON_DRIVER_DISPATCH_H_

#include <type_traits>
#include <dlfcn.h>

//HIP Backend
#define __HIP_PLATFORM_HCC__
#include "hip/hip_runtime.h"


// #include "triton/external/CUDA/hip.h"
#include "triton/external/CUDA/nvml_hip.h"

//Exceptions
#include <iostream>
#include <stdexcept>

namespace llvm {
class PassRegistry;
class Module;
}

namespace triton
{
namespace driver
{

class cu_context;

template<class T> void check(T){}
void check(hipError_t err);

class dispatch
{
protected:
  template <class F>
  struct return_type;

  template <class R, class... A>
  struct return_type<R (*)(A...)>
  { typedef R type; };

  typedef bool (*f_init_t)();

  template<f_init_t initializer, typename FunPtrT, typename... Args>
  static typename return_type<FunPtrT>::type f_impl(void*& lib_h, FunPtrT, void*& cache, const char * name, Args... args)
  {
    std::cout << "f_impl: " << name << std::endl;
    initializer(); // cuinit
    // std::cout << "f_impl: after cuinit"<< std::endl;

    if(cache == nullptr){
      // std::cout << "f_impl: cache empty"<< std::endl;
      cache = dlsym(lib_h, name);
			if(cache == 0){
        std::cout << "dlsym cannot find " << name << std::endl;
        throw std::runtime_error("dlsym unable to load HIP function");
      }
    }
    FunPtrT fptr;
    *reinterpret_cast<void **>(&fptr) = cache;
    typename return_type<FunPtrT>::type res = (*fptr)(args...);
    // std::cout << "f_impl:res " << res << std::endl;
    check(res);
    return res;
  }

public:
  static bool nvmlinit();
  static bool cuinit();
  static bool spvllvminit();
  static void release();

  // CUDA
  static hipError_t hipCtxGetCurrent(hipCtx_t *pctx);
  static hipError_t hipCtxSetCurrent(hipCtx_t ctx);
  static hipError_t hipCtxDestroy(hipCtx_t ctx);
  static hipError_t hipEventCreate(hipEvent_t *phEvent, unsigned int Flags);
  static hipError_t hipGetDevice(hipDevice_t *device, int ordinal);
  static hipError_t hipMemcpyDtoH(void *dstHost, hipDeviceptr_t srcDevice, size_t ByteCount);
  static hipError_t hipStreamCreate__(hipStream_t *phStream, unsigned int Flags); // replace with hipStreamCreateWithFlags
  static hipError_t hipEventElapsedTime(float *pMilliseconds, hipEvent_t hStart, hipEvent_t hEnd);
  static hipError_t hipFree(hipDeviceptr_t dptr);
  static hipError_t hipMemcpyDtoHAsync(void *dstHost, hipDeviceptr_t srcDevice, size_t ByteCount, hipStream_t hStream);
  static hipError_t hipDriverGetVersion(int *driverVersion);
  static hipError_t hipDeviceGetName(char *name, int len, int dev);
  static hipError_t hipDeviceGetPCIBusId(char *id, int len, int dev);
  static hipError_t hipModuleGetGlobal(hipDeviceptr_t *dptr, size_t* bytes, hipModule_t hmod, const char *name);
  static hipError_t hipMemcpyHtoDAsync(hipDeviceptr_t dstDevice, const void *srcHost, size_t ByteCount, hipStream_t hStream);
  static hipError_t hipModuleLoad(hipModule_t *module, const char *fname);
  static hipError_t hipModuleLaunchKernel(hipFunction_t f, unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ, unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ, unsigned int sharedMemBytes, hipStream_t hStream, void **kernelParams, void **extra);
  static hipError_t hipModuleUnload(hipModule_t hmod);
  static hipError_t hipModuleLoadDataEx(hipModule_t *module, const void *image, unsigned int numOptions, hipJitOption *options, void **optionValues);
  static hipError_t hipDeviceGetAttribute(int *pi, hipDeviceAttribute_t attrib, int dev);
  static hipError_t hipGetDeviceCount(int *count);
  static hipError_t hipMemcpyHtoD(hipDeviceptr_t dstDevice, const void *srcHost, size_t ByteCount);
  static hipError_t hipInit(unsigned int Flags);
  static hipError_t hipEventRecord(hipEvent_t hEvent, hipStream_t hStream);
  static hipError_t hipCtxCreate(hipCtx_t *pctx, unsigned int flags, hipDevice_t dev);
  static hipError_t hipCtxPushCurrent(hipCtx_t ctx);
  static hipError_t hipCtxPopCurrent(hipCtx_t *pctx);
  static hipError_t hipModuleGetFunction(hipFunction_t *hfunc, hipModule_t hmod, const char *name);
  static hipError_t hipStreamSynchronize(hipStream_t hStream);
  static hipError_t cuStreamGetCtx(hipStream_t hStream, hipCtx_t* pctx);
  static hipError_t hipStreamDestroy(hipStream_t hStream);
  static hipError_t hipEventDestroy(hipEvent_t hEvent);
  static hipError_t hipMalloc(hipDeviceptr_t *dptr, size_t bytesize);
  static hipError_t hipPointerGetAttribute(void * data, hipPointerAttribute_t attribute, hipDeviceptr_t ptr);
  static hipError_t hipCtxGetDevice(hipDevice_t* result);
  static hipError_t hipMemsetD8Async(hipDeviceptr_t dst, unsigned char x, size_t N, hipStream_t stream);
  // static hipError_t hipFuncGetAttribute(int* pi, hipFuncAttribute_t attrib, hipFunction_t hfunc);
  // static hipError_t hipFuncSetAttribute(hipFunction_t hfunc, hipFuncAttribute_t attrib, int  value);
  // static hipError_t hipFuncSetCacheConfig (hipFunction_t hfunc, hipFuncCache config);
  // NVML
  static nvmlReturn_t nvmlDeviceGetHandleByPciBusId_v2( const char* pciBusId, nvmlDevice_t* device);
  static nvmlReturn_t nvmlDeviceGetClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int *clock);
  static nvmlReturn_t nvmlDeviceGetMaxClockInfo(nvmlDevice_t device, nvmlClockType_t type, unsigned int *clock);
  static nvmlReturn_t nvmlDeviceSetApplicationsClocks(nvmlDevice_t device, unsigned int mem_clock, unsigned int sm_clock);


  // SPIR-V libraries
  static int initializeLLVMToSPIRVPass(llvm::PassRegistry &);
  static bool writeSpirv(llvm::Module *M, std::ostream &OS, std::string &ErrMsg);


private:

  // Libraries
  static void* cuda_;
  static void* hip_;
  static void* nvml_;
  static void* vulkan_;
  static void* spvllvm_;
  static void* spvcross_;
  static void* opengl_;


  // CUDA functions
  static void* hipCtxGetCurrent_;
  static void* hipCtxSetCurrent_;
  static void* hipCtxDestroy_;
  static void* hipEventCreate_;
  static void* hipGetDevice_;
  static void* hipMemcpyDtoH_;
  static void* hipStreamCreate___;
  static void* hipEventElapsedTime_;
  static void* hipFree_;
  static void* hipMemcpyDtoHAsync_;
  static void* hipDriverGetVersion_;
  static void* hipDeviceGetName_;
  static void* hipDeviceGetPCIBusId_;
  static void* hipModuleGetGlobal_;
  static void* hipMemcpyHtoDAsync_;
  static void* hipModuleLoad_;
  static void* hipModuleLaunchKernel_;
  static void* hipModuleUnload_;
  static void* hipModuleLoadDataEx_;
  static void* hipDeviceGetAttribute_;
  static void* hipGetDeviceCount_;
  static void* hipMemcpyHtoD_;
  static void* hipInit_;
  static void* hipEventRecord_;
  static void* hipCtxCreate_;
  static void* hipModuleGetFunction_;
  static void* hipStreamSynchronize_;
  static void* hipStreamDestroy_;
  static void* cuStreamGetCtx_;
  static void* hipEventDestroy_;
  static void* hipMalloc_;
  static void* hipPointerGetAttribute_;
  static void* hipCtxGetDevice_;
  static void* hipMemsetD8Async_;
  static void* hipCtxPushCurrent_;
  static void* hipCtxPopCurrent_;
  static void* hipFuncGetAttribute_;
  static void* cuFuncSetAttribute_;
  static void* hipFuncSetCacheConfig_;
  // NVML
  static void* nvmlInit_v2_;
  static void* nvmlDeviceGetHandleByPciBusId_v2_;
  static void* nvmlDeviceGetClockInfo_;
  static void* nvmlDeviceGetMaxClockInfo_;
  static void* nvmlDeviceSetApplicationsClocks_;

  // LLVM to SPIR-V
  static void* initializeLLVMToSPIRVPass_;
  static void* writeSpirv_;
};

}
}


#endif
