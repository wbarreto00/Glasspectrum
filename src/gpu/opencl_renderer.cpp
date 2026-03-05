/*
 * Glasspectrum — OpenCL GPU Renderer (Stub)
 * TODO: Port Metal compute kernels to OpenCL.
 */

#ifdef GLASSPECTRU_HAS_OPENCL

#include "opencl_renderer.h"
#include <cstdio>

namespace glasspectrum {

bool openclInit() {
  fprintf(stderr, "[Glasspectrum] OpenCL renderer not yet implemented. Using "
                  "CPU fallback.\n");
  return false;
}

void openclShutdown() {}
bool openclIsAvailable() { return false; }

void openclRender(const float *, float *, const float *, int, int,
                  const RenderParams &, void *) {
  // Stub
}

} // namespace glasspectrum

#endif // GLASSPECTRU_HAS_OPENCL
