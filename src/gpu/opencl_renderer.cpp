/*
 * Glasspectum — OpenCL GPU Renderer (Stub)
 * TODO: Port Metal compute kernels to OpenCL.
 */

#ifdef GLASSPECTU_HAS_OPENCL

#include "opencl_renderer.h"
#include <cstdio>

namespace glasspectum {

bool openclInit() {
  fprintf(stderr, "[Glasspectum] OpenCL renderer not yet implemented. Using "
                  "CPU fallback.\n");
  return false;
}

void openclShutdown() {}
bool openclIsAvailable() { return false; }

void openclRender(const float *, float *, const float *, int, int,
                  const RenderParams &, void *) {
  // Stub
}

} // namespace glasspectum

#endif // GLASSPECTU_HAS_OPENCL
