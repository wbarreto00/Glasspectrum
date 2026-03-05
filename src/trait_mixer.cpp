/*
 * Glasspectrum — Trait Mixer Implementation
 */

#include "trait_mixer.h"
#include <algorithm>
#include <cmath>

namespace glasspectrum {

// ── Helper: get a profile, falling back to index 0 ────────────────

static const LensProfile &safeProfile(int index) {
  const LensProfile *p = getLensProfile(index);
  if (!p)
    p = getLensProfile(0);
  return *p;
}

// ── Resolve Profile ───────────────────────────────────────────────

ResolvedProfile resolveProfile(int baseLensIndex,
                               const TraitMixConfig &overrides) {
  const LensProfile &base = safeProfile(baseLensIndex);

  ResolvedProfile r;
  r.baseName = base.name;

  // Distortion
  {
    const LensProfile &src =
        (overrides.distortion.mode == TRAIT_SOURCE_CUSTOM)
            ? safeProfile(overrides.distortion.customLensIndex)
            : base;
    r.distortionModel = src.distortion_model;
    r.distortionAtEdge = src.distortion_at_edge_percent;
  }

  // Chromatic Aberration
  {
    const LensProfile &src =
        (overrides.chromaticAberration.mode == TRAIT_SOURCE_CUSTOM)
            ? safeProfile(overrides.chromaticAberration.customLensIndex)
            : base;
    r.caEdgePx = src.ca_edge_px_at_4k;
  }

  // Coma
  {
    const LensProfile &src = (overrides.coma.mode == TRAIT_SOURCE_CUSTOM)
                                 ? safeProfile(overrides.coma.customLensIndex)
                                 : base;
    r.coma = src.coma;
  }

  // Bokeh
  {
    const LensProfile &src =
        (overrides.bokehBlur.mode == TRAIT_SOURCE_CUSTOM)
            ? safeProfile(overrides.bokehBlur.customLensIndex)
            : base;
    r.bokeh = src.bokeh;
  }

  // Vignette & Color Cast
  {
    const LensProfile &src =
        (overrides.vignetteCast.mode == TRAIT_SOURCE_CUSTOM)
            ? safeProfile(overrides.vignetteCast.customLensIndex)
            : base;
    r.vignette = src.vignette;
    r.colorCast = src.color_cast;
  }

  // Bloom
  {
    const LensProfile &src = (overrides.bloom.mode == TRAIT_SOURCE_CUSTOM)
                                 ? safeProfile(overrides.bloom.customLensIndex)
                                 : base;
    r.bloom = src.bloom;
  }

  // Fringing
  {
    const LensProfile &src =
        (overrides.fringing.mode == TRAIT_SOURCE_CUSTOM)
            ? safeProfile(overrides.fringing.customLensIndex)
            : base;
    r.fringing = src.fringing;
  }

  // Sharpness always comes from base
  r.sharpness = base.sharpness;
  r.breathingPercent = base.breathing_percent;
  r.hasAnamorphic = base.has_anamorphic;
  r.anamorphic = base.anamorphic;

  return r;
}

// ── Overdrive Scaling ─────────────────────────────────────────────
// Each trait has a specific scaling curve so overdrive feels natural.
// overdrive = 0 means profile defaults; overdrive = 1 means 2× the effect,
// overdrive = -1 means near-zero. We use a power curve.

static float overdriveScale(float overdrive, float speed) {
  // speed controls how fast this trait reacts to overdrive
  // result: multiplier on the trait's parameters
  // overdrive 0 → 1.0, overdrive 1 → 2^speed, overdrive -1 → ~0
  return std::pow(2.0f, overdrive * speed);
}

void applyOverdrive(ResolvedProfile &profile, float overdrive) {
  if (std::abs(overdrive) < 0.001f)
    return;

  // Distortion scales faster (speed 1.5)
  float distScale = overdriveScale(overdrive, 1.5f);
  profile.distortionModel.k1 *= distScale;
  profile.distortionModel.k2 *= distScale;
  profile.distortionModel.k3 *= distScale;
  profile.distortionAtEdge *= distScale;

  // CA scales at normal speed (1.0)
  float caScale = overdriveScale(overdrive, 1.0f);
  profile.caEdgePx *= caScale;

  // Coma scales at normal speed (1.0) with stability
  float comaScale = overdriveScale(overdrive, 1.0f);
  profile.coma.strength *= comaScale;

  // Bokeh: swirl and cat-eye scale with overdrive
  float bokehScale = overdriveScale(overdrive, 0.8f);
  profile.bokeh.swirl_base *= bokehScale;
  profile.bokeh.cat_eye_strength =
      std::min(1.0f, profile.bokeh.cat_eye_strength * bokehScale);

  // Vignette scales moderately (0.7)
  float vigScale = overdriveScale(overdrive, 0.7f);
  profile.vignette.vig_stops_at_edge *= vigScale;

  // Bloom scales with threshold protection
  float bloomScale = overdriveScale(overdrive, 0.6f);
  profile.bloom.gain *= bloomScale;
  // Lower threshold slightly when overdriving up
  if (overdrive > 0.0f)
    profile.bloom.bloom_threshold_linear *= (1.0f - overdrive * 0.15f);

  // Fringing scales at normal speed
  float fringeScale = overdriveScale(overdrive, 1.0f);
  profile.fringing.gain *= fringeScale;

  // Cast scales slowly (0.5)
  float castScale = overdriveScale(overdrive, 0.5f);
  profile.colorCast.cast_delta_xy[0] *= castScale;
  profile.colorCast.cast_delta_xy[1] *= castScale;
}

} // namespace glasspectrum
