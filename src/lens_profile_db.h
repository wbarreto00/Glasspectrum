#pragma once
/*
 * Glasspectrum — Lens Profile Database Header
 * Contains data structures and accessors for the 50 embedded lens presets.
 */

#include <cstdint>

namespace glasspectrum {

// ── Sub-structures ────────────────────────────────────────────────

struct DistortionModel {
  float k1, k2, k3; // radial (Brown–Conrady)
  float p1, p2;     // tangential
};

struct ComaParams {
  float strength;
  float anisotropy;
  float hue_bias_deg;
  float falloff;
};

struct VignetteParams {
  float vig_stops_at_edge;
  float power;
};

struct ColorCastParams {
  bool edge_only;
  float cast_delta_xy[2];
};

struct BloomParams {
  float bloom_threshold_linear;
  float gain;
  float falloff_exp;
  float tint_rgb[3];
};

struct FringingParams {
  float gain;
  float hue_deg;
  float width_px;
  float radial_bias;
};

struct BokehParams {
  int blade_count;
  float blade_curvature;
  float cat_eye_strength;
  float onion_ring_strength;
  float swirl_base;
};

struct SharpnessParams {
  float microcontrast;
  float wide_open_glow;
  float mtf_center;
  float mtf_edge;
};

struct AnamorphicParams {
  float squeeze;
  float oval_bokeh;
  float streak_flare;
  float blue_flare_bias;
};

// ── Evidence level ────────────────────────────────────────────────

enum EvidenceLevel {
  EVIDENCE_PUBLISHED = 0,
  EVIDENCE_VENDOR_DESCRIBED = 1,
  EVIDENCE_HEURISTIC = 2
};

// ── Main lens profile ─────────────────────────────────────────────

struct LensProfile {
  const char *name;
  EvidenceLevel evidence_level;
  float distortion_at_edge_percent;
  DistortionModel distortion_model;
  float ca_edge_px_at_4k;
  ComaParams coma;
  VignetteParams vignette;
  ColorCastParams color_cast;
  BloomParams bloom;
  FringingParams fringing;
  BokehParams bokeh;
  SharpnessParams sharpness;
  float breathing_percent;
  bool has_anamorphic;
  AnamorphicParams anamorphic;
};

// ── Public API ────────────────────────────────────────────────────

constexpr int kLensProfileCount = 50;

// Returns the full array of 50 lens profiles.
const LensProfile *getLensProfiles();

// Returns a single profile by index [0..49]. Returns nullptr if out of range.
const LensProfile *getLensProfile(int index);

// Returns a profile by name (case-sensitive). Returns nullptr if not found.
const LensProfile *getLensProfileByName(const char *name);

// Returns 50 (or asserts at compile time).
int getLensProfileCount();

} // namespace glasspectrum
