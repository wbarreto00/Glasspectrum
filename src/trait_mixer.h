#pragma once
/*
 * Glasspectum — Trait Mixer
 * Implements the mix-and-match system where each trait group can source
 * its parameters from either the base lens or a custom lens override.
 */

#include "lens_profile_db.h"

namespace glasspectum {

// Each trait group can be sourced from the base lens or overridden by another.
enum TraitSourceMode { TRAIT_SOURCE_BASE = 0, TRAIT_SOURCE_CUSTOM = 1 };

// One entry per trait group in the Individual Controls section.
struct TraitOverride {
  TraitSourceMode mode;
  int customLensIndex; // only used when mode == TRAIT_SOURCE_CUSTOM
};

// All trait overrides for the seven groups.
struct TraitMixConfig {
  TraitOverride coma;
  TraitOverride distortion;
  TraitOverride chromaticAberration;
  TraitOverride bokehBlur;
  TraitOverride vignetteCast;
  TraitOverride bloom;
  TraitOverride fringing;
};

// Resolved parameters after mixing: contains one complete LensProfile
// assembled from potentially different source lenses per trait group.
struct ResolvedProfile {
  // Base profile name (for display)
  const char *baseName;

  // Individual resolved traits (may come from different lenses)
  DistortionModel distortionModel;
  float distortionAtEdge;
  float caEdgePx;
  ComaParams coma;
  BokehParams bokeh;
  VignetteParams vignette;
  ColorCastParams colorCast;
  BloomParams bloom;
  FringingParams fringing;
  SharpnessParams sharpness;
  float breathingPercent;
  bool hasAnamorphic;
  AnamorphicParams anamorphic;
};

// Resolve a complete profile from base lens + trait overrides.
ResolvedProfile resolveProfile(int baseLensIndex,
                               const TraitMixConfig &overrides);

// Apply overdrive scaling to a resolved profile.
// Modifies the profile in-place with trait-specific curves.
void applyOverdrive(ResolvedProfile &profile, float overdrive);

} // namespace glasspectum
