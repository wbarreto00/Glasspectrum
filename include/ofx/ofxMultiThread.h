#ifndef OFX_MULTI_THREAD_H
#define OFX_MULTI_THREAD_H

#include "ofxCore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*OfxThreadFunctionV1)(unsigned int threadIndex,
                                    unsigned int threadMax, void *customArg);

typedef struct OfxMultiThreadSuiteV1 {
  OfxStatus (*multiThread)(OfxThreadFunctionV1 func, unsigned int nThreads,
                           void *customArg);
  OfxStatus (*multiThreadNumCPUs)(unsigned int *nCPUs);
  OfxStatus (*multiThreadIndex)(unsigned int *threadIndex);
  int (*multiThreadIsSpawnedThread)(void);
  OfxStatus (*mutexCreate)(OfxMutexHandle *mutex, int lockCount);
  OfxStatus (*mutexDestroy)(const OfxMutexHandle mutex);
  OfxStatus (*mutexLock)(const OfxMutexHandle mutex);
  OfxStatus (*mutexUnLock)(const OfxMutexHandle mutex);
  OfxStatus (*mutexTryLock)(const OfxMutexHandle mutex);
} OfxMultiThreadSuiteV1;

typedef void *OfxMutexHandle;

#ifdef __cplusplus
}
#endif

#endif /* OFX_MULTI_THREAD_H */
