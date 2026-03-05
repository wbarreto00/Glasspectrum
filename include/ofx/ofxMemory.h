#ifndef OFX_MEMORY_H
#define OFX_MEMORY_H

#include "ofxCore.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct OfxMemorySuiteV1 {
  OfxStatus (*memoryAlloc)(void *handle, size_t nBytes, void **allocatedData);
  OfxStatus (*memoryFree)(void *allocatedData);
} OfxMemorySuiteV1;

#ifdef __cplusplus
}
#endif

#endif /* OFX_MEMORY_H */
