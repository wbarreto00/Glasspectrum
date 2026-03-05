#pragma once
/*
 * Glasspectrum — CUDA GPU Renderer (Stub)
 * TODO: Implement CUDA kernels mirroring the Metal compute shaders.
 * This is the preferred GPU path on Windows with NVIDIA GPUs.
 */

#ifdef GLASSPECTRU_HAS_CUDA

namespace glasspectrum {

struct RenderParams;

bool cudaInit();
void cudaShutdown();
bool cudaIsAvailable();

void cudaRender(const float *inputData, float *outputData,
                const float *depthData, int width, int height,
                const RenderParams &params, void *cudaStream);

} // namespace glasspectrum

#endif // GLASSPECTRU_HAS_CUDA
