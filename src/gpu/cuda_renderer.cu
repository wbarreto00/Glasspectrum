/*
 * Glasspectum — CUDA GPU Renderer (Stub)
 * TODO: Port Metal compute kernels to CUDA.
 * All kernel logic mirrors glasspectum.metal exactly.
 */

#ifdef GLASSPECTU_HAS_CUDA

#include "cuda_renderer.h"
#include <cstdio>

namespace glasspectum {

bool cudaInit() {
  // TODO: Initialize CUDA device, compile kernels
  fprintf(
      stderr,
      "[Glasspectum] CUDA renderer not yet implemented. Using CPU fallback.\n");
  return false;
}

void cudaShutdown() {}
bool cudaIsAvailable() { return false; }

void cudaRender(const float *, float *, const float *, int, int,
                const RenderParams &, void *) {
  // Stub — CPU fallback handles rendering
}

} // namespace glasspectum

#endif // GLASSPECTU_HAS_CUDA
