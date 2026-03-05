/*
 * Glasspectrum — CUDA GPU Renderer (Stub)
 * TODO: Port Metal compute kernels to CUDA.
 * All kernel logic mirrors glasspectrum.metal exactly.
 */

#ifdef GLASSPECTRU_HAS_CUDA

#include "cuda_renderer.h"
#include <cstdio>

namespace glasspectrum {

bool cudaInit() {
  // TODO: Initialize CUDA device, compile kernels
  fprintf(
      stderr,
      "[Glasspectrum] CUDA renderer not yet implemented. Using CPU fallback.\n");
  return false;
}

void cudaShutdown() {}
bool cudaIsAvailable() { return false; }

void cudaRender(const float *, float *, const float *, int, int,
                const RenderParams &, void *) {
  // Stub — CPU fallback handles rendering
}

} // namespace glasspectrum

#endif // GLASSPECTRU_HAS_CUDA
