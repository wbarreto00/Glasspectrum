/*
 * Glasspectum — Lens Profile Database Test
 * Validates:
 *   - Exactly 50 presets (build fails if not)
 *   - Range validation for all numeric fields
 *   - Name uniqueness
 *   - Evidence level validity
 */

#include "lens_profile_db.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>

using namespace glasspectum;

static int g_errors = 0;

#define CHECK(cond, ...)                                                       \
  do {                                                                         \
    if (!(cond)) {                                                             \
      printf("  FAIL: ");                                                      \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
      ++g_errors;                                                              \
    }                                                                          \
  } while (0)

#define CHECK_RANGE(val, lo, hi, name, preset)                                 \
  CHECK((val) >= (lo) && (val) <= (hi),                                        \
        "[%s] %s = %.4f out of range [%.4f, %.4f]", preset, name,              \
        (float)(val), (float)(lo), (float)(hi))

static void validateProfile(const LensProfile &p, int index) {
  const char *n = p.name;

  // Name must not be empty
  CHECK(n && strlen(n) > 0, "[%d] Empty name", index);

  // Evidence level
  CHECK(p.evidence_level >= EVIDENCE_PUBLISHED &&
            p.evidence_level <= EVIDENCE_HEURISTIC,
        "[%s] Invalid evidence_level %d", n, p.evidence_level);

  // Distortion
  CHECK_RANGE(p.distortion_at_edge_percent, 0.0f, 10.0f,
              "distortion_at_edge_percent", n);
  CHECK_RANGE(p.distortion_model.k1, -0.1f, 0.1f, "k1", n);
  CHECK_RANGE(p.distortion_model.k2, -0.01f, 0.01f, "k2", n);

  // CA
  CHECK_RANGE(p.ca_edge_px_at_4k, 0.0f, 5.0f, "ca_edge_px_at_4k", n);

  // Coma
  CHECK_RANGE(p.coma.strength, 0.0f, 2.0f, "coma.strength", n);
  CHECK_RANGE(p.coma.anisotropy, 0.0f, 1.0f, "coma.anisotropy", n);
  CHECK_RANGE(p.coma.hue_bias_deg, -180.0f, 180.0f, "coma.hue_bias_deg", n);
  CHECK_RANGE(p.coma.falloff, 0.0f, 5.0f, "coma.falloff", n);

  // Vignette
  CHECK_RANGE(p.vignette.vig_stops_at_edge, 0.0f, 5.0f, "vig_stops_at_edge", n);
  CHECK_RANGE(p.vignette.power, 0.5f, 5.0f, "vig_power", n);

  // Bloom
  CHECK_RANGE(p.bloom.bloom_threshold_linear, 0.1f, 5.0f, "bloom_threshold", n);
  CHECK_RANGE(p.bloom.gain, 0.0f, 3.0f, "bloom_gain", n);
  CHECK_RANGE(p.bloom.falloff_exp, 0.5f, 5.0f, "bloom_falloff_exp", n);
  CHECK_RANGE(p.bloom.tint_rgb[0], 0.5f, 2.0f, "bloom_tint_r", n);
  CHECK_RANGE(p.bloom.tint_rgb[1], 0.5f, 2.0f, "bloom_tint_g", n);
  CHECK_RANGE(p.bloom.tint_rgb[2], 0.5f, 2.0f, "bloom_tint_b", n);

  // Fringing
  CHECK_RANGE(p.fringing.gain, 0.0f, 2.0f, "fringe_gain", n);
  CHECK_RANGE(p.fringing.hue_deg, 0.0f, 360.0f, "fringe_hue_deg", n);
  CHECK_RANGE(p.fringing.width_px, 0.0f, 10.0f, "fringe_width_px", n);
  CHECK_RANGE(p.fringing.radial_bias, 0.0f, 1.0f, "fringe_radial_bias", n);

  // Bokeh
  CHECK_RANGE(p.bokeh.blade_count, 3, 32, "blade_count", n);
  CHECK_RANGE(p.bokeh.blade_curvature, 0.0f, 1.0f, "blade_curvature", n);
  CHECK_RANGE(p.bokeh.cat_eye_strength, 0.0f, 1.0f, "cat_eye_strength", n);
  CHECK_RANGE(p.bokeh.onion_ring_strength, 0.0f, 1.0f, "onion_ring_strength",
              n);
  CHECK_RANGE(p.bokeh.swirl_base, 0.0f, 1.0f, "swirl_base", n);

  // Sharpness
  CHECK_RANGE(p.sharpness.microcontrast, 0.0f, 1.0f, "microcontrast", n);
  CHECK_RANGE(p.sharpness.wide_open_glow, 0.0f, 1.0f, "wide_open_glow", n);
  CHECK_RANGE(p.sharpness.mtf_center, 0.0f, 1.0f, "mtf_center", n);
  CHECK_RANGE(p.sharpness.mtf_edge, 0.0f, 1.0f, "mtf_edge", n);

  // Breathing
  CHECK_RANGE(p.breathing_percent, 0.0f, 20.0f, "breathing_percent", n);

  // Anamorphic (if present)
  if (p.has_anamorphic) {
    CHECK_RANGE(p.anamorphic.squeeze, 1.0f, 2.5f, "ana_squeeze", n);
    CHECK_RANGE(p.anamorphic.oval_bokeh, 0.0f, 1.0f, "ana_oval_bokeh", n);
    CHECK_RANGE(p.anamorphic.streak_flare, 0.0f, 1.0f, "ana_streak_flare", n);
    CHECK_RANGE(p.anamorphic.blue_flare_bias, 0.0f, 1.0f, "ana_blue_flare_bias",
                n);
  }
}

int main() {
  printf("=== Glasspectum Lens Profile DB Test ===\n\n");

  // Test 1: Count == 50
  int count = getLensProfileCount();
  printf("Test 1: Profile count\n");
  CHECK(count == 50, "Expected 50 profiles, got %d", count);
  printf("  Profiles: %d\n\n", count);

  // Test 2: All profiles accessible
  printf("Test 2: Profile accessibility\n");
  const LensProfile *profiles = getLensProfiles();
  CHECK(profiles != nullptr, "getLensProfiles() returned nullptr");
  for (int i = 0; i < count; ++i) {
    const LensProfile *p = getLensProfile(i);
    CHECK(p != nullptr, "getLensProfile(%d) returned nullptr", i);
  }
  printf("  All %d profiles accessible.\n\n", count);

  // Test 3: Range validation
  printf("Test 3: Range validation\n");
  for (int i = 0; i < count; ++i) {
    validateProfile(*getLensProfile(i), i);
  }
  printf("  Range validation complete.\n\n");

  // Test 4: Name uniqueness
  printf("Test 4: Name uniqueness\n");
  std::set<std::string> names;
  for (int i = 0; i < count; ++i) {
    std::string name = getLensProfile(i)->name;
    CHECK(names.find(name) == names.end(), "Duplicate name: %s", name.c_str());
    names.insert(name);
  }
  printf("  %zu unique names.\n\n", names.size());

  // Test 5: Lookup by name
  printf("Test 5: Name lookup\n");
  const LensProfile *master = getLensProfileByName("ARRI/Zeiss Master Prime");
  CHECK(master != nullptr, "Failed to find 'ARRI/Zeiss Master Prime'");
  const LensProfile *unknown = getLensProfileByName("Nonexistent Lens");
  CHECK(unknown == nullptr, "Found nonexistent lens");
  printf("  Name lookup OK.\n\n");

  // Test 6: Out-of-range access
  printf("Test 6: Bounds checking\n");
  CHECK(getLensProfile(-1) == nullptr, "getLensProfile(-1) should be nullptr");
  CHECK(getLensProfile(50) == nullptr, "getLensProfile(50) should be nullptr");
  CHECK(getLensProfile(100) == nullptr,
        "getLensProfile(100) should be nullptr");
  printf("  Bounds checking OK.\n\n");

  // Summary
  printf("=== RESULTS: %d errors ===\n", g_errors);
  if (g_errors > 0) {
    printf("FAILED\n");
    return 1;
  }
  printf("ALL TESTS PASSED\n");
  return 0;
}
