#pragma once
/*
 * Glasspectum — Metal GPU Renderer
 * Primary rendering path for macOS. Dispatches Metal compute kernels
 * for the full pipeline.
 */

#ifdef __APPLE__

namespace glasspectum {

struct RenderParams; // forward

// Initialize Metal rendering resources. Call once during plugin load.
// Returns true if Metal is available and initialized.
bool metalInit();

// Release Metal resources. Call during plugin unload.
void metalShutdown();

// Returns true if Metal GPU rendering is available.
bool metalIsAvailable();

// Render using Metal compute kernels.
// inputData/outputData: RGBA float pixel buffers
// depthData: optional single-channel float depth map (or nullptr)
// All buffers must be width*height*4 floats (or width*height for depth).
void metalRender(const float *inputData, float *outputData,
                 const float *depthData, int width, int height,
                 const RenderParams &params,
                 void *metalCmdQueue // id<MTLCommandQueue> from Resolve
);

} // namespace glasspectum

#endif // __APPLE__
