#pragma once
/*
 * Glasspectum — OpenCL GPU Renderer (Stub)
 * TODO: Implement OpenCL kernels as Windows GPU fallback.
 */

#ifdef GLASSPECTU_HAS_OPENCL

namespace glasspectum {

struct RenderParams;

bool openclInit();
void openclShutdown();
bool openclIsAvailable();

void openclRender(const float *inputData, float *outputData,
                  const float *depthData, int width, int height,
                  const RenderParams &params, void *clCommandQueue);

} // namespace glasspectum

#endif // GLASSPECTU_HAS_OPENCL
