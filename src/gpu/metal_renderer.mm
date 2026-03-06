/*
 * Glasspectrum — Metal GPU Renderer Implementation
 * Objective-C++ file that interfaces with Metal framework.
 * This is the PRIMARY rendering path on macOS.
 */

#ifdef __APPLE__

#import <Foundation/Foundation.h>
#import <Metal/Metal.h>

#include "../glasspectrum_processor.h"
#include "../sensor_table.h"
#include "../trait_mixer.h"
#include "metal_renderer.h"

#include <algorithm>
#include <cmath>

namespace glasspectrum {

// ── Static state ──────────────────────────────────────────────

static id<MTLDevice> s_device = nil;
static id<MTLLibrary> s_library = nil;
static id<MTLComputePipelineState> s_distortionPSO = nil;
static id<MTLComputePipelineState> s_caPSO = nil;
static id<MTLComputePipelineState> s_comaPSO = nil;
static id<MTLComputePipelineState> s_vignettePSO = nil;
static id<MTLComputePipelineState> s_bloomExtractPSO = nil;
static id<MTLComputePipelineState> s_bloomBlurHPSO = nil;
static id<MTLComputePipelineState> s_bloomBlurVPSO = nil;
static id<MTLComputePipelineState> s_bloomRecombinePSO = nil;
static id<MTLComputePipelineState> s_fringingPSO = nil;
static id<MTLComputePipelineState> s_sharpenerPSO = nil;
static id<MTLComputePipelineState> s_bokehPSO = nil;
static id<MTLComputePipelineState> s_blendPSO = nil;
static id<MTLComputePipelineState> s_colorConvertPSO = nil;
static bool s_initialized = false;

// ── GPU pipeline parameter struct (must match Metal shader) ────

struct GlassPipelineParams {
  uint32_t imageSize[2];
  float center[2];
  float maxR;

  float dist_k1, dist_k2, dist_k3;
  float dist_p1, dist_p2;
  float distortionAmount;
  int32_t scaleToFrame;

  float caEdgePx;
  float caAmount;
  float caResScale;

  float comaStrength;
  float comaAnisotropy;
  float comaHueBias;
  float comaFalloff;
  float comaAmount;

  float vigStops;
  float vigPower;
  float vigAmount;
  float castDeltaX, castDeltaY;

  float bloomThreshold;
  float bloomGain;
  float bloomFalloffExp;
  float bloomTint[3];
  float bloomAmount;

  float fringeGain;
  float fringeHueDeg;
  float fringeWidthPx;
  float fringeRadialBias;
  float fringeAmount;

  float sharpMicrocontrast;
  float sharpWideOpenGlow;
  float fStopSharpener;
  float aperture;

  float focusDistance;
  float focalLength;
  float sensorWidth;
  float depthGain;
  float bokehAmount;
  float swirlBase;
  float swirlAmount;
  int32_t bladeCount;
  float bladeCurvature;
  float catEyeStrength;
  int32_t maxSteps;

  int32_t anamorphicEnabled;
  float anaSqueeze;
  float anaOval;

  float masterBlend;
  float cropFactor;
};

// ── Helper: create PSO from function name ─────────────────────

static id<MTLComputePipelineState> createPSO(NSString *name) {
  id<MTLFunction> func = [s_library newFunctionWithName:name];
  if (!func) {
    NSLog(@"[Glasspectrum] Metal function '%@' not found", name);
    return nil;
  }
  NSError *error = nil;
  id<MTLComputePipelineState> pso =
      [s_device newComputePipelineStateWithFunction:func error:&error];
  if (error) {
    NSLog(@"[Glasspectrum] Failed to create PSO for '%@': %@", name, error);
    return nil;
  }
  return pso;
}

// ── Public API ────────────────────────────────────────────────

bool metalInit() {
  if (s_initialized)
    return true;

  s_device = MTLCreateSystemDefaultDevice();
  if (!s_device) {
    NSLog(@"[Glasspectrum] No Metal device available");
    return false;
  }

  // Load the compiled metallib from the bundle
  NSBundle *bundle = [NSBundle
      bundleForClass:[NSObject class]]; // Will be adjusted for OFX bundle
  NSString *libPath = [bundle pathForResource:@"glasspectrum"
                                       ofType:@"metallib"];

  if (!libPath) {
    // Try loading from the plugin bundle's Resources
    // OFX plugins on macOS are bundles at
    // /Library/OFX/Plugins/Glasspectrum.ofx.bundle
    NSString *pluginPath = @"/Library/OFX/Plugins/Glasspectrum.ofx.bundle/"
                           @"Contents/Resources/glasspectrum.metallib";
    if ([[NSFileManager defaultManager] fileExistsAtPath:pluginPath]) {
      libPath = pluginPath;
    }
  }

  NSError *error = nil;
  if (libPath) {
    NSURL *libURL = [NSURL fileURLWithPath:libPath];
    s_library = [s_device newLibraryWithURL:libURL error:&error];
  }

  if (!s_library) {
    // Fallback: try compiling from source at runtime
    NSString *srcPath =
        @"/Library/OFX/Plugins/Glasspectrum.ofx.bundle/Contents/"
        @"Resources/glasspectrum.metal";
    NSString *src = [NSString stringWithContentsOfFile:srcPath
                                              encoding:NSUTF8StringEncoding
                                                 error:&error];
    if (src) {
      MTLCompileOptions *opts = [[MTLCompileOptions alloc] init];
      opts.languageVersion = MTLLanguageVersion2_0;
      s_library = [s_device newLibraryWithSource:src options:opts error:&error];
    }
  }

  if (!s_library) {
    NSLog(@"[Glasspectrum] Failed to load Metal library: %@", error);
    return false;
  }

  // Create all pipeline state objects
  s_distortionPSO = createPSO(@"distortionKernel");
  s_caPSO = createPSO(@"chromaticAberrationKernel");
  s_comaPSO = createPSO(@"comaKernel");
  s_vignettePSO = createPSO(@"vignetteKernel");
  s_bloomExtractPSO = createPSO(@"bloomExtractKernel");
  s_bloomBlurHPSO = createPSO(@"bloomBlurHKernel");
  s_bloomBlurVPSO = createPSO(@"bloomBlurVKernel");
  s_bloomRecombinePSO = createPSO(@"bloomRecombineKernel");
  s_fringingPSO = createPSO(@"fringingKernel");
  s_sharpenerPSO = createPSO(@"sharpenerKernel");
  s_bokehPSO = createPSO(@"bokehBlurKernel");
  s_blendPSO = createPSO(@"masterBlendKernel");
  s_colorConvertPSO = createPSO(@"colorConvertKernel");

  s_initialized = true;
  NSLog(@"[Glasspectrum] Metal initialized successfully on %@", s_device.name);
  return true;
}

void metalShutdown() {
  s_distortionPSO = nil;
  s_caPSO = nil;
  s_comaPSO = nil;
  s_vignettePSO = nil;
  s_bloomExtractPSO = nil;
  s_bloomBlurHPSO = nil;
  s_bloomBlurVPSO = nil;
  s_bloomRecombinePSO = nil;
  s_fringingPSO = nil;
  s_sharpenerPSO = nil;
  s_bokehPSO = nil;
  s_blendPSO = nil;
  s_colorConvertPSO = nil;
  s_library = nil;
  s_device = nil;
  s_initialized = false;
}

bool metalIsAvailable() { return s_initialized && s_device != nil; }

// ── Helper: dispatch a simple kernel with 2 textures + params ─

static void dispatchKernel(id<MTLComputeCommandEncoder> encoder,
                           id<MTLComputePipelineState> pso,
                           id<MTLTexture> inTex, id<MTLTexture> outTex,
                           id<MTLBuffer> paramBuf, uint32_t width,
                           uint32_t height) {
  [encoder setComputePipelineState:pso];
  [encoder setTexture:inTex atIndex:0];
  [encoder setTexture:outTex atIndex:1];
  [encoder setBuffer:paramBuf offset:0 atIndex:0];

  MTLSize threadGroupSize = MTLSizeMake(16, 16, 1);
  MTLSize gridSize =
      MTLSizeMake((width + threadGroupSize.width - 1) / threadGroupSize.width *
                      threadGroupSize.width,
                  (height + threadGroupSize.height - 1) /
                      threadGroupSize.height * threadGroupSize.height,
                  1);
  [encoder dispatchThreads:MTLSizeMake(width, height, 1)
      threadsPerThreadgroup:threadGroupSize];
}

// ── Main GPU Render ───────────────────────────────────────────

void metalRender(const float *inputData, float *outputData,
                 const float *depthData, int width, int height,
                 const RenderParams &params, void *metalCmdQueue) {
  if (!inputData || !outputData || width <= 0 || height <= 0) {
    return;
  }

  id<MTLCommandQueue> cmdQueue = (__bridge id<MTLCommandQueue>)metalCmdQueue;
  if (!cmdQueue) {
    // If no command queue from host, create our own
    cmdQueue = [s_device newCommandQueue];
  }
  if (!cmdQueue)
    return;

  int pixelCount = width * height;

  // Resolve profile for parameter filling
  ResolvedProfile profile = resolveProfile(params.lensIndex, params.traitMix);
  applyOverdrive(profile, params.overdrive);

  // Fill GPU parameter buffer
  GlassPipelineParams gpuParams;
  memset(&gpuParams, 0, sizeof(gpuParams));

  float cx = width * 0.5f;
  float cy = height * 0.5f;
  float maxR = sqrtf(cx * cx + cy * cy);

  gpuParams.imageSize[0] = width;
  gpuParams.imageSize[1] = height;
  gpuParams.center[0] = cx;
  gpuParams.center[1] = cy;
  gpuParams.maxR = maxR;

  // Distortion
  gpuParams.dist_k1 = profile.distortionModel.k1;
  gpuParams.dist_k2 = profile.distortionModel.k2;
  gpuParams.dist_k3 = profile.distortionModel.k3;
  gpuParams.dist_p1 = profile.distortionModel.p1;
  gpuParams.dist_p2 = profile.distortionModel.p2;
  gpuParams.distortionAmount = params.distortionAmount;
  gpuParams.scaleToFrame = params.scaleToFrame ? 1 : 0;

  // CA
  gpuParams.caEdgePx = profile.caEdgePx;
  gpuParams.caAmount = params.caAmount;
  gpuParams.caResScale = static_cast<float>(width) / 3840.0f;

  // Coma
  gpuParams.comaStrength = profile.coma.strength;
  gpuParams.comaAnisotropy = profile.coma.anisotropy;
  gpuParams.comaHueBias = profile.coma.hue_bias_deg;
  gpuParams.comaFalloff = profile.coma.falloff;
  gpuParams.comaAmount = params.comaAmount;

  // Vignette
  gpuParams.vigStops = profile.vignette.vig_stops_at_edge;
  gpuParams.vigPower = profile.vignette.power;
  gpuParams.vigAmount = params.vignetteAmount;
  gpuParams.castDeltaX = profile.colorCast.cast_delta_xy[0];
  gpuParams.castDeltaY = profile.colorCast.cast_delta_xy[1];

  // Bloom
  gpuParams.bloomThreshold = profile.bloom.bloom_threshold_linear;
  gpuParams.bloomGain = profile.bloom.gain;
  gpuParams.bloomFalloffExp = profile.bloom.falloff_exp;
  gpuParams.bloomTint[0] = profile.bloom.tint_rgb[0];
  gpuParams.bloomTint[1] = profile.bloom.tint_rgb[1];
  gpuParams.bloomTint[2] = profile.bloom.tint_rgb[2];
  gpuParams.bloomAmount = params.bloomAmount;

  // Fringing
  gpuParams.fringeGain = profile.fringing.gain;
  gpuParams.fringeHueDeg = profile.fringing.hue_deg;
  gpuParams.fringeWidthPx = profile.fringing.width_px;
  gpuParams.fringeRadialBias = profile.fringing.radial_bias;
  gpuParams.fringeAmount = params.fringingAmount;

  // Sharpener
  gpuParams.sharpMicrocontrast = profile.sharpness.microcontrast;
  gpuParams.sharpWideOpenGlow = profile.sharpness.wide_open_glow;
  gpuParams.fStopSharpener = params.fStopSharpener;
  gpuParams.aperture = params.aperture;

  // DOF
  gpuParams.focusDistance = params.focusDistance_m;
  gpuParams.focalLength = params.focalLength_mm;
  {
    int sIdx = params.sensorSizeIndex;
    if (sIdx < 0)
      sIdx = 0;
    if (sIdx >= (int)SENSOR_COUNT)
      sIdx = (int)SENSOR_COUNT - 1;
    gpuParams.sensorWidth = kSensorPresets[sIdx].width_mm;
  }
  gpuParams.depthGain = params.depthGain;
  gpuParams.bokehAmount = params.bokehAmount;
  gpuParams.swirlBase = profile.bokeh.swirl_base;
  gpuParams.swirlAmount = params.bokehSwirl;
  gpuParams.bladeCount = (params.bladeCountOverride > 0)
                             ? params.bladeCountOverride
                             : profile.bokeh.blade_count;
  gpuParams.bladeCurvature = (params.bladeCurvatureOverride >= 0)
                                 ? params.bladeCurvatureOverride
                                 : profile.bokeh.blade_curvature;
  gpuParams.catEyeStrength = (params.catEyeStrengthOverride >= 0)
                                 ? params.catEyeStrengthOverride
                                 : profile.bokeh.cat_eye_strength;
  gpuParams.maxSteps = params.aberrationMaxSteps;

  // Anamorphic
  bool doAna = params.anamorphicMode || profile.hasAnamorphic;
  gpuParams.anamorphicEnabled = doAna ? 1 : 0;
  gpuParams.anaSqueeze =
      doAna ? (profile.hasAnamorphic ? profile.anamorphic.squeeze
                                     : params.anaSqueezeOverride)
            : 1.0f;
  gpuParams.anaOval =
      doAna ? (profile.hasAnamorphic ? profile.anamorphic.oval_bokeh
                                     : params.anaOvalBokeh)
            : 0.0f;

  gpuParams.masterBlend = params.masterBlend;
  gpuParams.cropFactor = cropFactor(params.sensorSizeIndex);

  // Create Metal textures
  MTLTextureDescriptor *texDesc = [MTLTextureDescriptor
      texture2DDescriptorWithPixelFormat:MTLPixelFormatRGBA32Float
                                   width:width
                                  height:height
                               mipmapped:NO];
  texDesc.usage = MTLTextureUsageShaderRead | MTLTextureUsageShaderWrite;
  texDesc.storageMode = MTLStorageModePrivate;

  id<MTLTexture> texA = [s_device newTextureWithDescriptor:texDesc];
  id<MTLTexture> texB = [s_device newTextureWithDescriptor:texDesc];
  id<MTLTexture> origTex =
      [s_device newTextureWithDescriptor:texDesc]; // keep original for blend

  // Depth texture (single channel)
  id<MTLTexture> depthTex = nil;
  if (depthData) {
    MTLTextureDescriptor *depthDesc = [MTLTextureDescriptor
        texture2DDescriptorWithPixelFormat:MTLPixelFormatR32Float
                                     width:width
                                    height:height
                                 mipmapped:NO];
    depthDesc.usage = MTLTextureUsageShaderRead;
    depthDesc.storageMode = MTLStorageModePrivate;
    depthTex = [s_device newTextureWithDescriptor:depthDesc];
  }

  // Command buffer
  id<MTLBuffer> paramBuf =
      [s_device newBufferWithBytes:&gpuParams
                            length:sizeof(GlassPipelineParams)
                           options:MTLResourceStorageModeShared];

  // Command buffer
  id<MTLCommandBuffer> cmdBuf = [cmdQueue commandBuffer];

  // Upload to texA via a buffer to avoid replaceRegion crash on unaligned host
  // memory
  {
    size_t rowBytes = width * 4 * sizeof(float);
    size_t imgBytes = rowBytes * height;
    id<MTLBuffer> stagingImgBuf =
        [s_device newBufferWithBytes:inputData
                              length:imgBytes
                             options:MTLResourceStorageModeShared];

    id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
    [blit copyFromBuffer:stagingImgBuf
               sourceOffset:0
          sourceBytesPerRow:rowBytes
        sourceBytesPerImage:imgBytes
                 sourceSize:MTLSizeMake(width, height, 1)
                  toTexture:texA
           destinationSlice:0
           destinationLevel:0
          destinationOrigin:MTLOriginMake(0, 0, 0)];

    [blit copyFromBuffer:stagingImgBuf
               sourceOffset:0
          sourceBytesPerRow:rowBytes
        sourceBytesPerImage:imgBytes
                 sourceSize:MTLSizeMake(width, height, 1)
                  toTexture:origTex
           destinationSlice:0
           destinationLevel:0
          destinationOrigin:MTLOriginMake(0, 0, 0)];

    if (depthData && depthTex) {
      size_t depthRowBytes = width * sizeof(float);
      size_t depthImgBytes = depthRowBytes * height;
      id<MTLBuffer> stagingDepthBuf =
          [s_device newBufferWithBytes:depthData
                                length:depthImgBytes
                               options:MTLResourceStorageModeShared];
      [blit copyFromBuffer:stagingDepthBuf
                 sourceOffset:0
            sourceBytesPerRow:depthRowBytes
          sourceBytesPerImage:depthImgBytes
                   sourceSize:MTLSizeMake(width, height, 1)
                    toTexture:depthTex
             destinationSlice:0
             destinationLevel:0
            destinationOrigin:MTLOriginMake(0, 0, 0)];
    }

    [blit endEncoding];
  }

  // ── Pipeline dispatch ─────────────────────────────────────
  // Each stage: texA → kernel → texB, then swap

  auto swapTextures = [&]() {
    id<MTLTexture> tmp = texA;
    texA = texB;
    texB = tmp;
  };

  // Note: Color space conversion to linear should happen first,
  // but for GPU path we assume input is already in the designated space
  // and convert on the fly. Full conversion kernels are available.

  // Stage 1: Color convert to linear (if needed)
  if (params.inputColorSpace != CS_LINEAR &&
      params.inputColorSpace != CS_ACESCG) {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:s_colorConvertPSO];
    [enc setTexture:texA atIndex:0];
    [enc setTexture:texB atIndex:1];
    int32_t csIdx = static_cast<int32_t>(params.inputColorSpace);
    int32_t dir = 0; // to linear
    uint32_t sz[2] = {(uint32_t)width, (uint32_t)height};
    [enc setBytes:&csIdx length:sizeof(int32_t) atIndex:0];
    [enc setBytes:&dir length:sizeof(int32_t) atIndex:1];
    [enc setBytes:sz length:sizeof(uint32_t) * 2 atIndex:2];
    MTLSize tgs = MTLSizeMake(16, 16, 1);
    [enc dispatchThreads:MTLSizeMake(width, height, 1)
        threadsPerThreadgroup:tgs];
    [enc endEncoding];
    swapTextures();

    // Update origTex to linear version
    id<MTLBlitCommandEncoder> blit2 = [cmdBuf blitCommandEncoder];
    [blit2 copyFromTexture:texA toTexture:origTex];
    [blit2 endEncoding];
  }

  // Stage 2: Distortion
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_distortionPSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 3: Chromatic Aberration
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_caPSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 4: Coma
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_comaPSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 5: Bokeh / DOF (only if depth map available)
  if (depthTex) {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:s_bokehPSO];
    [enc setTexture:texA atIndex:0];
    [enc setTexture:depthTex atIndex:1];
    [enc setTexture:texB atIndex:2];
    [enc setBuffer:paramBuf offset:0 atIndex:0];
    MTLSize tgs = MTLSizeMake(16, 16, 1);
    [enc dispatchThreads:MTLSizeMake(width, height, 1)
        threadsPerThreadgroup:tgs];
    [enc endEncoding];
    swapTextures();
  }

  // Stage 6: Vignette
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_vignettePSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 7: Bloom (3-pass: extract → blur → recombine)
  if (params.bloomAmount > 0.001f) {
    // Bloom texture
    id<MTLTexture> bloomTex = [s_device newTextureWithDescriptor:texDesc];
    id<MTLTexture> bloomTemp = [s_device newTextureWithDescriptor:texDesc];

    // 7a: Extract highlights
    {
      id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
      [enc setComputePipelineState:s_bloomExtractPSO];
      [enc setTexture:texA atIndex:0];
      [enc setTexture:bloomTex atIndex:1];
      [enc setBuffer:paramBuf offset:0 atIndex:0];
      MTLSize tgs = MTLSizeMake(16, 16, 1);
      [enc dispatchThreads:MTLSizeMake(width, height, 1)
          threadsPerThreadgroup:tgs];
      [enc endEncoding];
    }

    // 7b: Multi-scale blur
    int numScales = (params.aberrationMaxSteps / 4 + 1);
    if (numScales > 6)
      numScales = 6;
    for (int scale = 0; scale < numScales; ++scale) {
      int32_t radius = (1 << (scale + 1));
      uint32_t sz[2] = {(uint32_t)width, (uint32_t)height};

      // Horizontal
      {
        id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
        [enc setComputePipelineState:s_bloomBlurHPSO];
        [enc setTexture:bloomTex atIndex:0];
        [enc setTexture:bloomTemp atIndex:1];
        [enc setBytes:&radius length:sizeof(int32_t) atIndex:0];
        [enc setBytes:sz length:sizeof(uint32_t) * 2 atIndex:1];
        MTLSize tgs = MTLSizeMake(16, 16, 1);
        [enc dispatchThreads:MTLSizeMake(width, height, 1)
            threadsPerThreadgroup:tgs];
        [enc endEncoding];
      }
      // Vertical
      {
        id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
        [enc setComputePipelineState:s_bloomBlurVPSO];
        [enc setTexture:bloomTemp atIndex:0];
        [enc setTexture:bloomTex atIndex:1];
        [enc setBytes:&radius length:sizeof(int32_t) atIndex:0];
        [enc setBytes:sz length:sizeof(uint32_t) * 2 atIndex:1];
        MTLSize tgs = MTLSizeMake(16, 16, 1);
        [enc dispatchThreads:MTLSizeMake(width, height, 1)
            threadsPerThreadgroup:tgs];
        [enc endEncoding];
      }
    }

    // 7c: Recombine
    {
      id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
      [enc setComputePipelineState:s_bloomRecombinePSO];
      [enc setTexture:texA atIndex:0];
      [enc setTexture:bloomTex atIndex:1];
      [enc setTexture:texB atIndex:2];
      [enc setBuffer:paramBuf offset:0 atIndex:0];
      MTLSize tgs = MTLSizeMake(16, 16, 1);
      [enc dispatchThreads:MTLSizeMake(width, height, 1)
          threadsPerThreadgroup:tgs];
      [enc endEncoding];
      swapTextures();
    }
  }

  // Stage 8: Fringing
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_fringingPSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 9: Sharpener
  {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    dispatchKernel(enc, s_sharpenerPSO, texA, texB, paramBuf, width, height);
    [enc endEncoding];
    swapTextures();
  }

  // Stage 10: Master Blend
  if (params.masterBlend < 0.999f) {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:s_blendPSO];
    [enc setTexture:origTex atIndex:0];
    [enc setTexture:texA atIndex:1];
    [enc setTexture:texB atIndex:2];
    [enc setBuffer:paramBuf offset:0 atIndex:0];
    MTLSize tgs = MTLSizeMake(16, 16, 1);
    [enc dispatchThreads:MTLSizeMake(width, height, 1)
        threadsPerThreadgroup:tgs];
    [enc endEncoding];
    swapTextures();
  }

  // Stage 11: Convert back from linear
  if (params.inputColorSpace != CS_LINEAR &&
      params.inputColorSpace != CS_ACESCG) {
    id<MTLComputeCommandEncoder> enc = [cmdBuf computeCommandEncoder];
    [enc setComputePipelineState:s_colorConvertPSO];
    [enc setTexture:texA atIndex:0];
    [enc setTexture:texB atIndex:1];
    int32_t csIdx = static_cast<int32_t>(params.inputColorSpace);
    int32_t dir = 1; // from linear
    uint32_t sz[2] = {(uint32_t)width, (uint32_t)height};
    [enc setBytes:&csIdx length:sizeof(int32_t) atIndex:0];
    [enc setBytes:&dir length:sizeof(int32_t) atIndex:1];
    [enc setBytes:sz length:sizeof(uint32_t) * 2 atIndex:2];
    MTLSize tgs = MTLSizeMake(16, 16, 1);
    [enc dispatchThreads:MTLSizeMake(width, height, 1)
        threadsPerThreadgroup:tgs];
    [enc endEncoding];
    swapTextures();
  }

  // Read back result
  {
    MTLTextureDescriptor *readDesc = [texDesc copy];
    readDesc.storageMode = MTLStorageModeManaged;
    id<MTLTexture> readTex = [s_device newTextureWithDescriptor:readDesc];

    id<MTLBlitCommandEncoder> blit = [cmdBuf blitCommandEncoder];
    [blit copyFromTexture:texA toTexture:readTex];
    [blit synchronizeTexture:readTex slice:0 level:0];
    [blit endEncoding];

    [cmdBuf commit];
    [cmdBuf waitUntilCompleted];

    [readTex getBytes:outputData
          bytesPerRow:width * 4 * sizeof(float)
           fromRegion:MTLRegionMake2D(0, 0, width, height)
          mipmapLevel:0];
  }
}

} // namespace glasspectrum

#endif // __APPLE__
