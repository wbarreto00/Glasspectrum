#pragma once
/*
 * Glasspectum — CPU Image Processor Header
 * Full rendering pipeline: 11 stages from input conversion to master blend.
 */

#include "color_pipeline.h"
#include "dof_engine.h"
#include "lens_profile_db.h"
#include "sensor_table.h"
#include "trait_mixer.h"

namespace glasspectum {

// ── Render parameters collected from OFX UI ──────────────────────

struct RenderParams {
  // Essential controls
  ColorSpaceIndex inputColorSpace;
  int lensIndex;
  float masterBlend;    // 0..1
  float fStopSharpener; // -1..2 (negative = soften)
  int sensorSizeIndex;
  float overdrive; // -1..2

  // Settings & Quality
  bool scaleToFrame;
  int aberrationMaxSteps; // 1..128
  float depthGain;        // 0..5
  float bokehSwirl;       // -1..1

  // DOF / Advanced
  bool depthMapEnabled;
  float depthMapGamma;
  float depthMapScale;
  float focusDistance_m;
  float aperture;
  float focalLength_mm;
  int bladeCountOverride;       // 0 = use profile
  float bladeCurvatureOverride; // <0 = use profile
  float catEyeStrengthOverride; // <0 = use profile

  // Longitudinal CA
  float longiCA; // 0..1

  // Lens breathing
  float breathingAmount; // 0..1

  // Anamorphic override
  bool anamorphicMode;
  float anaSqueezeOverride;
  float anaOvalBokeh;
  float anaStreakFlare;

  // Flare / Ghosting / Veiling
  float flareThreshold;
  float flareGain;
  float flareTint[3];

  // Trait mix config
  TraitMixConfig traitMix;

  // Individual trait group amounts (user sliders in each sub-group, 0..1 scale)
  float comaAmount;
  float distortionAmount;
  float caAmount;
  float bokehAmount;
  float vignetteAmount;
  float bloomAmount;
  float fringingAmount;
};

// Set defaults for render params.
RenderParams defaultRenderParams();

// ── Per-pixel intermediate data ──────────────────────────────────

struct ImageBuffer {
  float *data; // RGBA float interleaved
  int width;
  int height;
  int rowBytes; // in bytes
};

// ── Processing pipeline ──────────────────────────────────────────

// Full CPU render: source → dest, using given params.
// depthMap is optional (can be nullptr). If provided, single-channel float.
void processImage(
    const ImageBuffer &src, ImageBuffer &dst,
    const float *depthMap, // optional, width*height floats, or nullptr
    const RenderParams &params);

// Individual pipeline stages (exposed for testing)
namespace stages {

void convertToLinear(ImageBuffer &img, ColorSpaceIndex cs);
void convertFromLinear(ImageBuffer &img, ColorSpaceIndex cs);

void applyDistortion(const ImageBuffer &src, ImageBuffer &dst,
                     const DistortionModel &model, float amount,
                     bool scaleToFrame, int sensorIndex);

void applyChromaticAberration(ImageBuffer &img, float caEdgePx, float amount,
                              int maxSteps, int sensorIndex);

void applyComa(ImageBuffer &img, const ComaParams &coma, float amount,
               int maxSteps, int sensorIndex);

void applyBokehBlur(ImageBuffer &img, const float *depthMap,
                    const BokehParams &bokeh, const DOFParams &dof,
                    float swirlAmount, float amount, int maxSteps,
                    bool anamorphic, float anaSqueeze, float anaOval);

void applyVignette(ImageBuffer &img, const VignetteParams &vig,
                   const ColorCastParams &cast, float amount, int sensorIndex);

void applyBloom(ImageBuffer &img, const BloomParams &bloom, float amount,
                int maxSteps);

void applyFringing(ImageBuffer &img, const FringingParams &fringe, float amount,
                   int sensorIndex);

void applySharpener(ImageBuffer &img, const SharpnessParams &sharpness,
                    float fStopSharpener, float aperture);

void applyMasterBlend(const ImageBuffer &original, ImageBuffer &processed,
                      float blend);

} // namespace stages

} // namespace glasspectum
