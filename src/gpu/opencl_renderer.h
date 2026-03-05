#pragma once
/*
 * Glasspectrum — OpenCL GPU Renderer (Stub)
 * TODO: Implement OpenCL kernels as Windows GPU fallback.
 */

#ifdef GLASSPECTRU_HAS_OPENCL

namespace glasspectrum {

struct RenderParams;

bool openclInit();
void openclShutdown();
bool openclIsAvailable();

void openclRender(const float *inputData, float *outputData,
                  const float *depthData, int width, int height,
                  const RenderParams &params, void *clCommandQueue);

} // namespace glasspectrum

#endif // GLASSPECTRU_HAS_OPENCL
