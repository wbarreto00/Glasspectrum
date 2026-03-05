/*
 * Glasspectum — OFX Plugin Entry Point
 * Implements the OpenFX API for DaVinci Resolve 18.6+.
 *
 * GPU rendering (Metal) is the PRIMARY path.
 * CPU rendering is the FALLBACK for systems without capable GPUs.
 *
 * All parameters are animatable and compatible with Resolve masks/windows.
 */

#include "../include/ofx/ofxCore.h"
#include "../include/ofx/ofxImageEffect.h"
#include "../include/ofx/ofxMemory.h"
#include "../include/ofx/ofxParam.h"
#include "../include/ofx/ofxProperty.h"

#include "color_pipeline.h"
#include "glasspectum_processor.h"
#include "lens_profile_db.h"
#include "sensor_table.h"
#include "trait_mixer.h"

#ifdef __APPLE__
#include "gpu/metal_renderer.h"
#endif

#include <cmath>
#include <cstdio>
#include <cstring>

// Extra OFX property names not in our minimal vendored headers
#ifndef kOfxPluginPropGrouping
#define kOfxPluginPropGrouping "OfxPluginPropGrouping"
#endif
#ifndef kOfxPropPluginDescription
#define kOfxPropPluginDescription "OfxPropPluginDescription"
#endif
#ifndef kOfxImageEffectPropMetalRenderSupported
#define kOfxImageEffectPropMetalRenderSupported                                \
  "OfxImageEffectPropMetalRenderSupported"
#endif
#ifndef kOfxImageEffectPropMetalCommandQueue
#define kOfxImageEffectPropMetalCommandQueue                                   \
  "OfxImageEffectPropMetalCommandQueue"
#endif

namespace glasspectum {

// ── Global OFX state ──────────────────────────────────────────

static OfxHost *gHost = nullptr;
static const OfxPropertySuiteV1 *gPropSuite = nullptr;
static const OfxParameterSuiteV1 *gParamSuite = nullptr;
static const OfxImageEffectSuiteV1 *gEffectSuite = nullptr;
static const OfxMemorySuiteV1 *gMemorySuite = nullptr;

// ── Plugin Identity ───────────────────────────────────────────

static const char *kPluginId = "com.glasspectum.lensemulator";
static const char *kPluginLabel = "Glasspectum";
static const char *kPluginGroup = "Glasspectum";
static const int kVersionMajor = 1;
static const int kVersionMinor = 0;

// ── Parameter name constants ──────────────────────────────────

// Essential Controls (exact UI order)
#define P_INPUT_CS "inputColorSpace"
#define P_LENS "lensProfile"
#define P_MASTER_BLEND "masterBlend"
#define P_FSTOP_SHARP "fStopSharpener"
#define P_SENSOR_SIZE "sensorSize"
#define P_OVERDRIVE "overdrive"

// Group: Individual Controls
#define G_INDIVIDUAL "grpIndividual"

// Sub-groups inside Individual Controls
#define G_COMA "grpComa"
#define P_COMA_AMOUNT "comaAmount"
#define P_COMA_SRC "comaSource"
#define P_COMA_CUSTOM "comaCustomLens"

#define G_DISTORTION "grpDistortion"
#define P_DIST_AMOUNT "distortionAmount"
#define P_DIST_SRC "distortionSource"
#define P_DIST_CUSTOM "distortionCustomLens"

#define G_CA "grpCA"
#define P_CA_AMOUNT "caAmount"
#define P_CA_SRC "caSource"
#define P_CA_CUSTOM "caCustomLens"

#define G_BOKEH "grpBokeh"
#define P_BOKEH_AMOUNT "bokehAmount"
#define P_BOKEH_SRC "bokehSource"
#define P_BOKEH_CUSTOM "bokehCustomLens"

#define G_VIGNETTE "grpVignette"
#define P_VIG_AMOUNT "vignetteAmount"
#define P_VIG_SRC "vignetteSource"
#define P_VIG_CUSTOM "vignetteCustomLens"

#define G_BLOOM "grpBloom"
#define P_BLOOM_AMOUNT "bloomAmount"
#define P_BLOOM_SRC "bloomSource"
#define P_BLOOM_CUSTOM "bloomCustomLens"

#define G_FRINGING "grpFringing"
#define P_FRINGE_AMOUNT "fringeAmount"
#define P_FRINGE_SRC "fringeSource"
#define P_FRINGE_CUSTOM "fringeCustomLens"

// Group: Settings & Quality
#define G_SETTINGS "grpSettings"
#define P_SCALE_TO_FRAME "scaleToFrame"
#define P_ABERR_MAX_STEPS "aberrationMaxSteps"
#define P_DEPTH_GAIN "depthGain"
#define P_BOKEH_SWIRL "bokehSwirl"

// Group: Advanced
#define G_ADVANCED "grpAdvanced"
#define P_DEPTH_ENABLE "depthMapEnable"
#define P_DEPTH_GAMMA "depthMapGamma"
#define P_DEPTH_SCALE "depthMapScale"
#define P_FOCUS_DIST "focusDistance"
#define P_APERTURE "aperture"
#define P_FOCAL_LEN "focalLength"
#define P_BLADE_COUNT "bladeCount"
#define P_BLADE_CURVE "bladeCurvature"
#define P_CATEYE "catEyeStrength"
#define P_LONGI_CA "longiCA"
#define P_BREATHING "breathingAmount"
#define P_ANA_MODE "anamorphicMode"
#define P_ANA_SQUEEZE "anaSqueeze"
#define P_ANA_OVAL "anaOvalBokeh"
#define P_ANA_STREAK "anaStreakFlare"
#define P_FLARE_THRESHOLD "flareThreshold"
#define P_FLARE_GAIN "flareGain"

// ── Helper: define a choice parameter ─────────────────────────

static void defineChoice(OfxParamSetHandle paramSet, const char *name,
                         const char *label, const char *hint,
                         const char *parent, int defaultVal,
                         const char **options, int numOptions) {
  OfxPropertySetHandle props;
  gParamSuite->paramDefine(paramSet, kOfxParamTypeChoice, name, &props);
  gPropSuite->propSetString(props, kOfxPropLabel, 0, label);
  if (hint)
    gPropSuite->propSetString(props, kOfxParamPropHint, 0, hint);
  if (parent)
    gPropSuite->propSetString(props, kOfxParamPropParent, 0, parent);
  gPropSuite->propSetInt(props, kOfxParamPropDefault, 0, defaultVal);
  gPropSuite->propSetInt(props, kOfxParamPropAnimates, 0, 1);
  for (int i = 0; i < numOptions; ++i) {
    gPropSuite->propSetString(props, kOfxParamPropChoiceOption, i, options[i]);
  }
}

// ── Helper: define a double slider ────────────────────────────

static void defineDouble(OfxParamSetHandle paramSet, const char *name,
                         const char *label, const char *hint,
                         const char *parent, double defaultVal, double minVal,
                         double maxVal, double displayMin, double displayMax) {
  OfxPropertySetHandle props;
  gParamSuite->paramDefine(paramSet, kOfxParamTypeDouble, name, &props);
  gPropSuite->propSetString(props, kOfxPropLabel, 0, label);
  if (hint)
    gPropSuite->propSetString(props, kOfxParamPropHint, 0, hint);
  if (parent)
    gPropSuite->propSetString(props, kOfxParamPropParent, 0, parent);
  gPropSuite->propSetDouble(props, kOfxParamPropDefault, 0, defaultVal);
  gPropSuite->propSetDouble(props, kOfxParamPropMin, 0, minVal);
  gPropSuite->propSetDouble(props, kOfxParamPropMax, 0, maxVal);
  gPropSuite->propSetDouble(props, kOfxParamPropDisplayMin, 0, displayMin);
  gPropSuite->propSetDouble(props, kOfxParamPropDisplayMax, 0, displayMax);
  gPropSuite->propSetInt(props, kOfxParamPropAnimates, 0, 1);
}

// ── Helper: define an integer slider ──────────────────────────

static void defineInt(OfxParamSetHandle paramSet, const char *name,
                      const char *label, const char *hint, const char *parent,
                      int defaultVal, int minVal, int maxVal) {
  OfxPropertySetHandle props;
  gParamSuite->paramDefine(paramSet, kOfxParamTypeInteger, name, &props);
  gPropSuite->propSetString(props, kOfxPropLabel, 0, label);
  if (hint)
    gPropSuite->propSetString(props, kOfxParamPropHint, 0, hint);
  if (parent)
    gPropSuite->propSetString(props, kOfxParamPropParent, 0, parent);
  gPropSuite->propSetInt(props, kOfxParamPropDefault, 0, defaultVal);
  gPropSuite->propSetInt(props, kOfxParamPropMin, 0, minVal);
  gPropSuite->propSetInt(props, kOfxParamPropMax, 0, maxVal);
  gPropSuite->propSetInt(props, kOfxParamPropAnimates, 0, 1);
}

// ── Helper: define a boolean (checkbox) ───────────────────────

static void defineBool(OfxParamSetHandle paramSet, const char *name,
                       const char *label, const char *hint, const char *parent,
                       bool defaultVal) {
  OfxPropertySetHandle props;
  gParamSuite->paramDefine(paramSet, kOfxParamTypeBoolean, name, &props);
  gPropSuite->propSetString(props, kOfxPropLabel, 0, label);
  if (hint)
    gPropSuite->propSetString(props, kOfxParamPropHint, 0, hint);
  if (parent)
    gPropSuite->propSetString(props, kOfxParamPropParent, 0, parent);
  gPropSuite->propSetInt(props, kOfxParamPropDefault, 0, defaultVal ? 1 : 0);
  gPropSuite->propSetInt(props, kOfxParamPropAnimates, 0, 1);
}

// ── Helper: define a group ────────────────────────────────────

static void defineGroup(OfxParamSetHandle paramSet, const char *name,
                        const char *label, const char *parent, bool open) {
  OfxPropertySetHandle props;
  gParamSuite->paramDefine(paramSet, kOfxParamTypeGroup, name, &props);
  gPropSuite->propSetString(props, kOfxPropLabel, 0, label);
  if (parent)
    gPropSuite->propSetString(props, kOfxParamPropParent, 0, parent);
  gPropSuite->propSetInt(props, kOfxParamPropGroupOpen, 0, open ? 1 : 0);
}

// ── Helper: define a trait sub-group with Source dropdown ──────

static void defineTraitGroup(OfxParamSetHandle paramSet, const char *groupName,
                             const char *groupLabel, const char *amountName,
                             const char *amountLabel, const char *srcName,
                             const char *customName) {
  defineGroup(paramSet, groupName, groupLabel, G_INDIVIDUAL, false);

  defineDouble(paramSet, amountName, amountLabel,
               "Amount of this trait (0 = off, 1 = full profile value)",
               groupName, 1.0, 0.0, 3.0, 0.0, 2.0);

  const char *srcOptions[] = {"Base", "Custom Lens"};
  defineChoice(paramSet, srcName, "Trait Source",
               "Source lens for this trait: base profile or a custom override",
               groupName, 0, srcOptions, 2);

  // Build lens names array
  const char *lensNames[kLensProfileCount];
  for (int i = 0; i < kLensProfileCount; ++i)
    lensNames[i] = getLensProfile(i)->name;

  defineChoice(paramSet, customName, "Custom Lens",
               "Override lens for this trait group", groupName, 0, lensNames,
               kLensProfileCount);
}

// ══════════════════════════════════════════════════════════════
// ══  OFX ACTION HANDLERS  ═══════════════════════════════════
// ══════════════════════════════════════════════════════════════

// ── Action: Load ──────────────────────────────────────────────

static OfxStatus actionLoad() {
  // Fetch suites
  gPropSuite = (const OfxPropertySuiteV1 *)gHost->fetchSuite(
      gHost->host, kOfxPropertySuite, 1);
  gParamSuite = (const OfxParameterSuiteV1 *)gHost->fetchSuite(
      gHost->host, kOfxParameterSuite, 1);
  gEffectSuite = (const OfxImageEffectSuiteV1 *)gHost->fetchSuite(
      gHost->host, kOfxImageEffectSuite, 1);
  gMemorySuite = (const OfxMemorySuiteV1 *)gHost->fetchSuite(
      gHost->host, kOfxMemorySuite, 1);

  if (!gPropSuite || !gParamSuite || !gEffectSuite)
    return kOfxStatErrMissingHostFeature;

#ifdef __APPLE__
  metalInit();
#endif

  return kOfxStatOK;
}

// ── Action: Unload ────────────────────────────────────────────

static OfxStatus actionUnload() {
#ifdef __APPLE__
  metalShutdown();
#endif
  return kOfxStatOK;
}

// ── Action: Describe ──────────────────────────────────────────

static OfxStatus actionDescribe(OfxImageEffectHandle effect) {
  OfxPropertySetHandle effectProps;
  gEffectSuite->getPropertySet(effect, &effectProps);

  gPropSuite->propSetString(effectProps, kOfxPropLabel, 0, kPluginLabel);
  gPropSuite->propSetString(effectProps, kOfxPropShortLabel, 0, kPluginLabel);
  gPropSuite->propSetString(effectProps, kOfxPropLongLabel, 0,
                            "Glasspectum Lens Emulator");
  gPropSuite->propSetString(effectProps, kOfxPluginPropGrouping, 0,
                            kPluginGroup);
  gPropSuite->propSetString(
      effectProps, kOfxPropPluginDescription, 0,
      "Emulates optical lens characteristics in post-production. "
      "50 cinema lens presets with trait-level mixing, GPU-accelerated "
      "processing, "
      "and ACES-compatible color pipeline.");

  // Supported contexts
  gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts,
                            0, kOfxImageEffectContextFilter);
  gPropSuite->propSetString(effectProps, kOfxImageEffectPropSupportedContexts,
                            1, kOfxImageEffectContextGeneral);

  // Pixel depths: float only (linear pipeline)
  gPropSuite->propSetString(effectProps,
                            kOfxImageEffectPropSupportedPixelDepths, 0,
                            kOfxBitDepthFloat);

  // GPU support: Metal primary, CPU fallback
#ifdef __APPLE__
  gPropSuite->propSetString(effectProps,
                            kOfxImageEffectPropMetalRenderSupported, 0, "true");
#endif
  gPropSuite->propSetInt(effectProps,
                         kOfxImageEffectPropSupportsMultiResolution, 0, 1);
  gPropSuite->propSetInt(effectProps, kOfxImageEffectPropSupportsTiles, 0, 0);

  return kOfxStatOK;
}

// ── Action: DescribeInContext ─────────────────────────────────

static OfxStatus actionDescribeInContext(OfxImageEffectHandle effect,
                                         OfxPropertySetHandle inArgs) {
  // Define clips
  OfxPropertySetHandle clipProps;

  // Source clip
  gEffectSuite->clipDefine(effect, kOfxImageEffectSimpleSourceClipName,
                           &clipProps);
  gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedPixelDepths,
                            0, kOfxBitDepthFloat);
  gPropSuite->propSetString(clipProps, kOfxImageEffectPropComponents, 0,
                            kOfxImageComponentRGBA);

  // Output clip
  gEffectSuite->clipDefine(effect, kOfxImageEffectOutputClipName, &clipProps);
  gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedPixelDepths,
                            0, kOfxBitDepthFloat);
  gPropSuite->propSetString(clipProps, kOfxImageEffectPropComponents, 0,
                            kOfxImageComponentRGBA);

  // Optional depth map clip
  gEffectSuite->clipDefine(effect, "DepthMap", &clipProps);
  gPropSuite->propSetString(clipProps, kOfxImageEffectPropSupportedPixelDepths,
                            0, kOfxBitDepthFloat);
  gPropSuite->propSetInt(clipProps, kOfxImageClipPropOptional, 0, 1);

  // ── Define Parameters (EXACT UI ORDER) ────────────────────
  OfxParamSetHandle paramSet;
  gEffectSuite->getParamSet(effect, &paramSet);

  // 1. Input Color Space
  {
    const char *csNames[CS_COUNT];
    for (int i = 0; i < CS_COUNT; ++i)
      csNames[i] = colorSpaceName(static_cast<ColorSpaceIndex>(i));
    defineChoice(
        paramSet, P_INPUT_CS, "Input Color Space",
        "Color space of the input. All processing happens in scene-linear.",
        nullptr, 0, csNames, CS_COUNT);
  }

  // 2. Lens Profile
  {
    const char *lensNames[kLensProfileCount];
    for (int i = 0; i < kLensProfileCount; ++i)
      lensNames[i] = getLensProfile(i)->name;
    defineChoice(paramSet, P_LENS, "Lens", "Base lens profile preset", nullptr,
                 0, lensNames, kLensProfileCount);
  }

  // 3. Master Blend
  defineDouble(paramSet, P_MASTER_BLEND, "Master Blend",
               "Mix between original (0) and fully processed (1). Always "
               "blended in linear.",
               nullptr, 1.0, 0.0, 1.0, 0.0, 1.0);

  // 4. f-Stop Sharpener
  defineDouble(paramSet, P_FSTOP_SHARP, "f-Stop Sharpener",
               "Microcontrast restoration. Respects wide-open glow and "
               "protects highlights.",
               nullptr, 0.0, -1.0, 3.0, -0.5, 2.0);

  // 5. Sensor Size
  {
    const char *sensorNames[SENSOR_COUNT];
    for (int i = 0; i < SENSOR_COUNT; ++i)
      sensorNames[i] = kSensorPresets[i].name;
    defineChoice(paramSet, P_SENSOR_SIZE, "Sensor Size",
                 "Affects CoC, crop factor, and edge aberration scaling.",
                 nullptr, SENSOR_FULL_FRAME, sensorNames, SENSOR_COUNT);
  }

  // 6. Overdrive
  defineDouble(paramSet, P_OVERDRIVE, "Overdrive",
               "Global exaggeration with trait-specific scaling curves.",
               nullptr, 0.0, -1.0, 3.0, -0.5, 2.0);

  // ── 7. Individual Controls Group ──────────────────────────
  defineGroup(paramSet, G_INDIVIDUAL, "Individual Controls", nullptr, false);

  // 13. Coma
  defineTraitGroup(paramSet, G_COMA, "Coma", P_COMA_AMOUNT, "Coma Amount",
                   P_COMA_SRC, P_COMA_CUSTOM);

  // 14. Distortion
  defineTraitGroup(paramSet, G_DISTORTION, "Distortion", P_DIST_AMOUNT,
                   "Distortion Amount", P_DIST_SRC, P_DIST_CUSTOM);

  // 15. Chromatic Aberration
  defineTraitGroup(paramSet, G_CA, "Chromatic Aberration", P_CA_AMOUNT,
                   "CA Amount", P_CA_SRC, P_CA_CUSTOM);

  // 16. Bokeh Blur
  defineTraitGroup(paramSet, G_BOKEH, "Bokeh Blur", P_BOKEH_AMOUNT,
                   "Bokeh Amount", P_BOKEH_SRC, P_BOKEH_CUSTOM);

  // 17. Vignette & Color Cast
  defineTraitGroup(paramSet, G_VIGNETTE, "Vignette & Color Cast", P_VIG_AMOUNT,
                   "Vignette Amount", P_VIG_SRC, P_VIG_CUSTOM);

  // 18. Bloom
  defineTraitGroup(paramSet, G_BLOOM, "Bloom", P_BLOOM_AMOUNT, "Bloom Amount",
                   P_BLOOM_SRC, P_BLOOM_CUSTOM);

  // 19. Fringing
  defineTraitGroup(paramSet, G_FRINGING, "Fringing", P_FRINGE_AMOUNT,
                   "Fringing Amount", P_FRINGE_SRC, P_FRINGE_CUSTOM);

  // ── 8. Settings & Quality Group ───────────────────────────
  defineGroup(paramSet, G_SETTINGS, "Settings & Quality", nullptr, false);

  // 9. Scale to Frame
  defineBool(paramSet, P_SCALE_TO_FRAME, "Scale to Frame",
             "Zoom/crop after distortion to remove black borders.", G_SETTINGS,
             false);

  // 10. Aberration Max Steps
  defineInt(paramSet, P_ABERR_MAX_STEPS, "Aberration Max Steps",
            "Cap sampling steps for CA/coma/bokeh/bloom. Higher = better "
            "quality, slower.",
            G_SETTINGS, 32, 1, 128);

  // 11. Depth Gain
  defineDouble(paramSet, P_DEPTH_GAIN, "Depth Gain",
               "Artistic multiplier for DOF blur radius.", G_SETTINGS, 1.0, 0.0,
               10.0, 0.0, 5.0);

  // 12. Bokeh Swirl
  defineDouble(
      paramSet, P_BOKEH_SWIRL, "Bokeh Swirl",
      "Rotates bokeh kernel by radial position. Correlates with cat-eye.",
      G_SETTINGS, 0.0, -2.0, 2.0, -1.0, 1.0);

  // ── Advanced Section ──────────────────────────────────────
  defineGroup(paramSet, G_ADVANCED, "Advanced", nullptr, false);

  // A1: Depth Map controls
  defineBool(paramSet, P_DEPTH_ENABLE, "Depth Map Input Enable",
             "Enable external depth map input for DOF.", G_ADVANCED, false);
  defineDouble(paramSet, P_DEPTH_GAMMA, "Depth Map Gamma",
               "Gamma correction for depth map.", G_ADVANCED, 1.0, 0.1, 5.0,
               0.1, 3.0);
  defineDouble(paramSet, P_DEPTH_SCALE, "Depth Map Scale",
               "Scale multiplier for depth map values.", G_ADVANCED, 1.0, 0.01,
               100.0, 0.1, 10.0);

  // A2: Focus Distance
  defineDouble(paramSet, P_FOCUS_DIST, "Focus Distance (m)",
               "Focus distance in meters.", G_ADVANCED, 3.0, 0.1, 1000.0, 0.1,
               50.0);

  // A3: Aperture / T-stop
  defineDouble(paramSet, P_APERTURE, "Aperture / T-stop",
               "Aperture f-number. Affects DOF and wide-open glow.", G_ADVANCED,
               2.8, 0.7, 64.0, 0.7, 22.0);

  // A4: Focal Length
  defineDouble(paramSet, P_FOCAL_LEN, "Focal Length (mm)",
               "Focal length in millimeters.", G_ADVANCED, 50.0, 8.0, 600.0,
               12.0, 300.0);

  // A5: Blade Count, Curvature, Cat-eye
  defineInt(paramSet, P_BLADE_COUNT, "Blade Count",
            "Override aperture blade count (0 = use profile).", G_ADVANCED, 0,
            0, 32);
  defineDouble(
      paramSet, P_BLADE_CURVE, "Blade Curvature",
      "Override blade curvature (0=polygon, 1=circle, <0=use profile).",
      G_ADVANCED, -1.0, -1.0, 1.0, -1.0, 1.0);
  defineDouble(paramSet, P_CATEYE, "Cat-eye Strength",
               "Override cat-eye vignetting of bokeh (<0=use profile).",
               G_ADVANCED, -1.0, -1.0, 1.0, -1.0, 1.0);

  // A6: Longitudinal CA
  defineDouble(paramSet, P_LONGI_CA, "Longitudinal CA",
               "Axial fringing amount.", G_ADVANCED, 0.0, 0.0, 2.0, 0.0, 1.0);

  // A7: Lens Breathing
  defineDouble(paramSet, P_BREATHING, "Lens Breathing",
               "FOV change with focus distance.", G_ADVANCED, 0.0, 0.0, 2.0,
               0.0, 1.0);

  // A8: Anamorphic Mode
  defineBool(paramSet, P_ANA_MODE, "Anamorphic Mode",
             "Enable anamorphic squeeze, oval bokeh, and streak flare.",
             G_ADVANCED, false);
  defineDouble(paramSet, P_ANA_SQUEEZE, "Anamorphic Squeeze",
               "Squeeze factor (1.25x, 1.33x, 2.0x common).", G_ADVANCED, 2.0,
               1.0, 2.0, 1.0, 2.0);
  defineDouble(paramSet, P_ANA_OVAL, "Oval Bokeh", "Oval bokeh strength.",
               G_ADVANCED, 0.5, 0.0, 1.0, 0.0, 1.0);
  defineDouble(paramSet, P_ANA_STREAK, "Streak Flare",
               "Horizontal streak flare intensity.", G_ADVANCED, 0.5, 0.0, 1.0,
               0.0, 1.0);

  // A9: Flare/Ghosting/Veiling
  defineDouble(paramSet, P_FLARE_THRESHOLD, "Flare Threshold",
               "Linear threshold for flare/ghost extraction.", G_ADVANCED, 1.2,
               0.5, 5.0, 0.5, 3.0);
  defineDouble(paramSet, P_FLARE_GAIN, "Flare Gain",
               "Intensity of flare/ghosting/veiling glare.", G_ADVANCED, 0.0,
               0.0, 2.0, 0.0, 1.0);

  return kOfxStatOK;
}

// ── Helper: read param values at render time ──────────────────

static double getDouble(OfxParamSetHandle paramSet, const char *name,
                        double time) {
  OfxParamHandle param;
  gParamSuite->paramGetHandle(paramSet, name, &param, nullptr);
  double val;
  gParamSuite->paramGetValueAtTime(param, time, &val);
  return val;
}

static int getInt(OfxParamSetHandle paramSet, const char *name, double time) {
  OfxParamHandle param;
  gParamSuite->paramGetHandle(paramSet, name, &param, nullptr);
  int val;
  gParamSuite->paramGetValueAtTime(param, time, &val);
  return val;
}

static bool getBool(OfxParamSetHandle paramSet, const char *name, double time) {
  return getInt(paramSet, name, time) != 0;
}

static TraitOverride getTraitOverride(OfxParamSetHandle ps, const char *srcName,
                                      const char *customName, double time) {
  TraitOverride o;
  o.mode = static_cast<TraitSourceMode>(getInt(ps, srcName, time));
  o.customLensIndex = getInt(ps, customName, time);
  return o;
}

static RenderParams collectRenderParams(OfxParamSetHandle paramSet,
                                        double time) {
  RenderParams p = defaultRenderParams();

  p.inputColorSpace =
      static_cast<ColorSpaceIndex>(getInt(paramSet, P_INPUT_CS, time));
  p.lensIndex = getInt(paramSet, P_LENS, time);
  p.masterBlend = static_cast<float>(getDouble(paramSet, P_MASTER_BLEND, time));
  p.fStopSharpener =
      static_cast<float>(getDouble(paramSet, P_FSTOP_SHARP, time));
  p.sensorSizeIndex = getInt(paramSet, P_SENSOR_SIZE, time);
  p.overdrive = static_cast<float>(getDouble(paramSet, P_OVERDRIVE, time));

  p.scaleToFrame = getBool(paramSet, P_SCALE_TO_FRAME, time);
  p.aberrationMaxSteps = getInt(paramSet, P_ABERR_MAX_STEPS, time);
  p.depthGain = static_cast<float>(getDouble(paramSet, P_DEPTH_GAIN, time));
  p.bokehSwirl = static_cast<float>(getDouble(paramSet, P_BOKEH_SWIRL, time));

  p.depthMapEnabled = getBool(paramSet, P_DEPTH_ENABLE, time);
  p.depthMapGamma =
      static_cast<float>(getDouble(paramSet, P_DEPTH_GAMMA, time));
  p.depthMapScale =
      static_cast<float>(getDouble(paramSet, P_DEPTH_SCALE, time));
  p.focusDistance_m =
      static_cast<float>(getDouble(paramSet, P_FOCUS_DIST, time));
  p.aperture = static_cast<float>(getDouble(paramSet, P_APERTURE, time));
  p.focalLength_mm = static_cast<float>(getDouble(paramSet, P_FOCAL_LEN, time));
  p.bladeCountOverride = getInt(paramSet, P_BLADE_COUNT, time);
  p.bladeCurvatureOverride =
      static_cast<float>(getDouble(paramSet, P_BLADE_CURVE, time));
  p.catEyeStrengthOverride =
      static_cast<float>(getDouble(paramSet, P_CATEYE, time));
  p.longiCA = static_cast<float>(getDouble(paramSet, P_LONGI_CA, time));
  p.breathingAmount =
      static_cast<float>(getDouble(paramSet, P_BREATHING, time));

  p.anamorphicMode = getBool(paramSet, P_ANA_MODE, time);
  p.anaSqueezeOverride =
      static_cast<float>(getDouble(paramSet, P_ANA_SQUEEZE, time));
  p.anaOvalBokeh = static_cast<float>(getDouble(paramSet, P_ANA_OVAL, time));
  p.anaStreakFlare =
      static_cast<float>(getDouble(paramSet, P_ANA_STREAK, time));

  p.flareThreshold =
      static_cast<float>(getDouble(paramSet, P_FLARE_THRESHOLD, time));
  p.flareGain = static_cast<float>(getDouble(paramSet, P_FLARE_GAIN, time));

  // Trait amounts
  p.comaAmount = static_cast<float>(getDouble(paramSet, P_COMA_AMOUNT, time));
  p.distortionAmount =
      static_cast<float>(getDouble(paramSet, P_DIST_AMOUNT, time));
  p.caAmount = static_cast<float>(getDouble(paramSet, P_CA_AMOUNT, time));
  p.bokehAmount = static_cast<float>(getDouble(paramSet, P_BOKEH_AMOUNT, time));
  p.vignetteAmount =
      static_cast<float>(getDouble(paramSet, P_VIG_AMOUNT, time));
  p.bloomAmount = static_cast<float>(getDouble(paramSet, P_BLOOM_AMOUNT, time));
  p.fringingAmount =
      static_cast<float>(getDouble(paramSet, P_FRINGE_AMOUNT, time));

  // Trait source overrides
  p.traitMix.coma = getTraitOverride(paramSet, P_COMA_SRC, P_COMA_CUSTOM, time);
  p.traitMix.distortion =
      getTraitOverride(paramSet, P_DIST_SRC, P_DIST_CUSTOM, time);
  p.traitMix.chromaticAberration =
      getTraitOverride(paramSet, P_CA_SRC, P_CA_CUSTOM, time);
  p.traitMix.bokehBlur =
      getTraitOverride(paramSet, P_BOKEH_SRC, P_BOKEH_CUSTOM, time);
  p.traitMix.vignetteCast =
      getTraitOverride(paramSet, P_VIG_SRC, P_VIG_CUSTOM, time);
  p.traitMix.bloom =
      getTraitOverride(paramSet, P_BLOOM_SRC, P_BLOOM_CUSTOM, time);
  p.traitMix.fringing =
      getTraitOverride(paramSet, P_FRINGE_SRC, P_FRINGE_CUSTOM, time);

  return p;
}

// ── Action: Render ────────────────────────────────────────────

static OfxStatus actionRender(OfxImageEffectHandle effect,
                              OfxPropertySetHandle inArgs) {
  // Get render time
  double time;
  gPropSuite->propGetDouble(inArgs, kOfxPropTime, 0, &time);

  // Get render window
  int renderWindow[4];
  gPropSuite->propGetIntN(inArgs, kOfxImageEffectPropRenderWindow, 4,
                          renderWindow);

  // Get clips
  OfxImageClipHandle srcClip, dstClip, depthClip;
  gEffectSuite->clipGetHandle(effect, kOfxImageEffectSimpleSourceClipName,
                              &srcClip, nullptr);
  gEffectSuite->clipGetHandle(effect, kOfxImageEffectOutputClipName, &dstClip,
                              nullptr);

  // Fetch images
  OfxPropertySetHandle srcImg, dstImg;
  gEffectSuite->clipGetImage(srcClip, time, nullptr, &srcImg);
  gEffectSuite->clipGetImage(dstClip, time, nullptr, &dstImg);

  if (!srcImg || !dstImg) {
    if (srcImg)
      gEffectSuite->clipReleaseImage(srcImg);
    if (dstImg)
      gEffectSuite->clipReleaseImage(dstImg);
    return kOfxStatFailed;
  }

  // Get image data pointers
  void *srcData = nullptr;
  void *dstData = nullptr;
  gPropSuite->propGetPointer(srcImg, kOfxImagePropData, 0, &srcData);
  gPropSuite->propGetPointer(dstImg, kOfxImagePropData, 0, &dstData);

  int srcRowBytes, dstRowBytes;
  gPropSuite->propGetInt(srcImg, kOfxImagePropRowBytes, 0, &srcRowBytes);
  gPropSuite->propGetInt(dstImg, kOfxImagePropRowBytes, 0, &dstRowBytes);

  int srcBounds[4], dstBounds[4];
  gPropSuite->propGetIntN(srcImg, kOfxImagePropBounds, 4, srcBounds);
  gPropSuite->propGetIntN(dstImg, kOfxImagePropBounds, 4, dstBounds);

  int width = dstBounds[2] - dstBounds[0];
  int height = dstBounds[3] - dstBounds[1];

  // Collect parameters
  OfxParamSetHandle paramSet;
  gEffectSuite->getParamSet(effect, &paramSet);
  RenderParams params = collectRenderParams(paramSet, time);

  // Optional depth map
  const float *depthData = nullptr;
  OfxPropertySetHandle depthImg = nullptr;
  if (params.depthMapEnabled) {
    OfxStatus depthStat =
        gEffectSuite->clipGetHandle(effect, "DepthMap", &depthClip, nullptr);
    if (depthStat == kOfxStatOK) {
      OfxPropertySetHandle depthClipProps;
      gEffectSuite->clipGetPropertySet(depthClip, &depthClipProps);
      int connected = 0;
      gPropSuite->propGetInt(depthClipProps, kOfxImageClipPropConnected, 0,
                             &connected);
      if (connected) {
        gEffectSuite->clipGetImage(depthClip, time, nullptr, &depthImg);
        if (depthImg) {
          gPropSuite->propGetPointer(depthImg, kOfxImagePropData, 0,
                                     (void **)&depthData);
        }
      }
    }
  }

  // ── RENDER DISPATCH: GPU primary, CPU fallback ────────────

  bool gpuRendered = false;

#ifdef __APPLE__
  // Try Metal GPU rendering first (PRIMARY PATH)
  if (metalIsAvailable()) {
    // Check if Resolve provided a Metal command queue
    void *metalCmdQueue = nullptr;
    gPropSuite->propGetPointer(inArgs, kOfxImageEffectPropMetalCommandQueue, 0,
                               &metalCmdQueue);

    metalRender((const float *)srcData, (float *)dstData, depthData, width,
                height, params, metalCmdQueue);
    gpuRendered = true;
  }
#endif

  if (!gpuRendered) {
    // CPU FALLBACK — only when GPU is unavailable
    ImageBuffer src;
    src.data = (float *)srcData;
    src.width = width;
    src.height = height;
    src.rowBytes = srcRowBytes;

    ImageBuffer dst;
    dst.data = (float *)dstData;
    dst.width = width;
    dst.height = height;
    dst.rowBytes = dstRowBytes;

    processImage(src, dst, depthData, params);
  }

  // Release images
  gEffectSuite->clipReleaseImage(srcImg);
  gEffectSuite->clipReleaseImage(dstImg);
  if (depthImg)
    gEffectSuite->clipReleaseImage(depthImg);

  return kOfxStatOK;
}

// ── Action: CreateInstance ─────────────────────────────────────

static OfxStatus actionCreateInstance(OfxImageEffectHandle /*effect*/) {
  return kOfxStatOK;
}

// ── Action: DestroyInstance ────────────────────────────────────

static OfxStatus actionDestroyInstance(OfxImageEffectHandle /*effect*/) {
  return kOfxStatOK;
}

// ══════════════════════════════════════════════════════════════
// ══  MAIN ENTRY POINT  ═════════════════════════════════════
// ══════════════════════════════════════════════════════════════

static OfxStatus pluginMainEntry(const char *action, const void *handle,
                                 OfxPropertySetHandle inArgs,
                                 OfxPropertySetHandle outArgs) {
  (void)outArgs;

  if (std::strcmp(action, kOfxActionLoad) == 0) {
    return actionLoad();
  }
  if (std::strcmp(action, kOfxActionUnload) == 0) {
    return actionUnload();
  }
  if (std::strcmp(action, kOfxActionDescribe) == 0) {
    return actionDescribe((OfxImageEffectHandle)handle);
  }
  if (std::strcmp(action, kOfxImageEffectActionDescribeInContext) == 0) {
    return actionDescribeInContext((OfxImageEffectHandle)handle, inArgs);
  }
  if (std::strcmp(action, kOfxActionCreateInstance) == 0) {
    return actionCreateInstance((OfxImageEffectHandle)handle);
  }
  if (std::strcmp(action, kOfxActionDestroyInstance) == 0) {
    return actionDestroyInstance((OfxImageEffectHandle)handle);
  }
  if (std::strcmp(action, kOfxImageEffectActionRender) == 0) {
    return actionRender((OfxImageEffectHandle)handle, inArgs);
  }

  // Unhandled actions: return default
  return kOfxStatReplyDefault;
}

static void setHostFunc(OfxHost *host) { gHost = host; }

// ── Plugin Struct ─────────────────────────────────────────────

static OfxPlugin s_plugin = {kOfxImageEffectPluginApi,
                             kOfxImageEffectPluginApiVersion,
                             kPluginId,
                             (unsigned int)kVersionMajor,
                             (unsigned int)kVersionMinor,
                             setHostFunc,
                             pluginMainEntry};

} // namespace glasspectum

// ══════════════════════════════════════════════════════════════
// ══  EXPORTED SYMBOLS  ═════════════════════════════════════
// ══════════════════════════════════════════════════════════════

OfxExport int OfxGetNumberOfPlugins(void) { return 1; }

OfxExport OfxPlugin *OfxGetPlugin(int nth) {
  if (nth == 0)
    return &glasspectum::s_plugin;
  return nullptr;
}
