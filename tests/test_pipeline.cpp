/*
 * Glasspectum — Pipeline Stage Tests
 * Tests individual processing stages with known inputs.
 */

#include "color_pipeline.h"
#include "dof_engine.h"
#include "glasspectum_processor.h"
#include "lens_profile_db.h"
#include "sensor_table.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace glasspectum;

static int g_errors = 0;
static int g_tests = 0;

#define EXPECT_NEAR(val, expected, eps, ...)                                   \
  do {                                                                         \
    ++g_tests;                                                                 \
    float _v = (float)(val);                                                   \
    float _e = (float)(expected);                                              \
    if (std::abs(_v - _e) > (eps)) {                                           \
      printf("  FAIL: ");                                                      \
      printf(__VA_ARGS__);                                                     \
      printf(" — got %.6f, expected %.6f (±%.6f)\n", _v, _e, (float)(eps));    \
      ++g_errors;                                                              \
    }                                                                          \
  } while (0)

#define EXPECT_TRUE(cond, ...)                                                 \
  do {                                                                         \
    ++g_tests;                                                                 \
    if (!(cond)) {                                                             \
      printf("  FAIL: ");                                                      \
      printf(__VA_ARGS__);                                                     \
      printf("\n");                                                            \
      ++g_errors;                                                              \
    }                                                                          \
  } while (0)

// Helper: create a solid-color test image
static std::vector<float> makeTestImage(int w, int h, float r, float g, float b,
                                        float a = 1.0f) {
  std::vector<float> data(w * h * 4);
  for (int i = 0; i < w * h; ++i) {
    data[i * 4 + 0] = r;
    data[i * 4 + 1] = g;
    data[i * 4 + 2] = b;
    data[i * 4 + 3] = a;
  }
  return data;
}

static ImageBuffer wrapImage(std::vector<float> &data, int w, int h) {
  ImageBuffer buf;
  buf.data = data.data();
  buf.width = w;
  buf.height = h;
  buf.rowBytes = w * 4 * sizeof(float);
  return buf;
}

// ── Test: Color Space Roundtrips ──────────────────────────────

static void testColorRoundtrip() {
  printf("Test: Color space roundtrips\n");

  float testValues[] = {0.0f, 0.01f, 0.1f, 0.18f, 0.5f, 0.9f, 1.0f, 2.0f};

  for (int cs = 0; cs < CS_COUNT; ++cs) {
    ColorSpaceIndex csi = static_cast<ColorSpaceIndex>(cs);
    for (float v : testValues) {
      float encoded = fromLinear(v, csi);
      float decoded = toLinear(encoded, csi);
      EXPECT_NEAR(decoded, v, 0.001f, "%s roundtrip v=%.2f",
                  colorSpaceName(csi), v);
    }
  }
  printf("  Color roundtrips done.\n\n");
}

// ── Test: Vignette ────────────────────────────────────────────

static void testVignette() {
  printf("Test: Vignette\n");

  int w = 64, h = 64;
  auto data = makeTestImage(w, h, 1.0f, 1.0f, 1.0f);
  auto buf = wrapImage(data, w, h);

  VignetteParams vig = {1.0f, 2.0f};
  ColorCastParams cast = {false, {0.0f, 0.0f}};

  stages::applyVignette(buf, vig, cast, 1.0f, SENSOR_FULL_FRAME);

  // Center should be unaffected
  float *center = buf.data + (h / 2 * w + w / 2) * 4;
  EXPECT_NEAR(center[0], 1.0f, 0.05f, "Center R after vignette");

  // Corner should be darker
  float *corner = buf.data; // (0,0)
  EXPECT_TRUE(corner[0] < 0.9f, "Corner should be darker than 0.9, got %.3f",
              corner[0]);

  printf("  Vignette OK.\n\n");
}

// ── Test: Master Blend ────────────────────────────────────────

static void testMasterBlend() {
  printf("Test: Master blend\n");

  int w = 4, h = 4;
  auto origData = makeTestImage(w, h, 1.0f, 0.0f, 0.0f);
  auto procData = makeTestImage(w, h, 0.0f, 1.0f, 0.0f);
  auto origBuf = wrapImage(origData, w, h);
  auto procBuf = wrapImage(procData, w, h);

  stages::applyMasterBlend(origBuf, procBuf, 0.5f);

  EXPECT_NEAR(procBuf.data[0], 0.5f, 0.001f, "Blend 50%% R");
  EXPECT_NEAR(procBuf.data[1], 0.5f, 0.001f, "Blend 50%% G");

  printf("  Master blend OK.\n\n");
}

// ── Test: DOF Engine ──────────────────────────────────────────

static void testDOFEngine() {
  printf("Test: DOF Engine\n");

  DOFParams dof;
  dof.focusDistance_m = 3.0f;
  dof.aperture = 2.8f;
  dof.focalLength_mm = 50.0f;
  dof.sensorIndex = SENSOR_FULL_FRAME;
  dof.depthGain = 1.0f;

  // At focus distance, CoC should be zero
  float cocAtFocus = computeCoC_mm(3.0f, dof);
  EXPECT_NEAR(cocAtFocus, 0.0f, 0.001f, "CoC at focus distance");

  // At infinity, CoC should be non-zero
  float cocAtInf = computeCoC_mm(100.0f, dof);
  EXPECT_TRUE(cocAtInf > 0.0f, "CoC at infinity should be > 0, got %.4f",
              cocAtInf);

  // Hyperfocal
  float H = hyperfocalDistance_m(dof);
  EXPECT_TRUE(H > 5.0f && H < 100.0f, "Hyperfocal unreasonable: %.1f m", H);

  // Focus limits
  float nearL, farL;
  focusLimits(dof, nearL, farL);
  EXPECT_TRUE(nearL < dof.focusDistance_m, "Near limit should be < focus");
  EXPECT_TRUE(farL > dof.focusDistance_m, "Far limit should be > focus");

  printf("  DOF Engine OK.\n\n");
}

// ── Test: Trait Mixer ─────────────────────────────────────────

static void testTraitMixer() {
  printf("Test: Trait mixer\n");

  TraitMixConfig mix;
  mix.coma = {TRAIT_SOURCE_BASE, 0};
  mix.distortion = {TRAIT_SOURCE_CUSTOM, 5}; // Cooke Speed Panchro
  mix.chromaticAberration = {TRAIT_SOURCE_BASE, 0};
  mix.bokehBlur = {TRAIT_SOURCE_BASE, 0};
  mix.vignetteCast = {TRAIT_SOURCE_BASE, 0};
  mix.bloom = {TRAIT_SOURCE_BASE, 0};
  mix.fringing = {TRAIT_SOURCE_BASE, 0};

  auto resolved = resolveProfile(0, mix); // Base = Master Prime

  // Distortion should come from Cooke Speed Panchro (index 5)
  const LensProfile *panchro = getLensProfile(5);
  EXPECT_NEAR(resolved.distortionModel.k1, panchro->distortion_model.k1,
              0.0001f, "Mixed distortion k1");

  // Coma should come from Master Prime (index 0)
  const LensProfile *master = getLensProfile(0);
  EXPECT_NEAR(resolved.coma.strength, master->coma.strength, 0.0001f,
              "Base coma strength");

  printf("  Trait mixer OK.\n\n");
}

// ── Test: Sensor Table ────────────────────────────────────────

static void testSensorTable() {
  printf("Test: Sensor table\n");

  EXPECT_NEAR(kSensorPresets[SENSOR_FULL_FRAME].width_mm, 36.0f, 0.01f,
              "FF width");
  EXPECT_NEAR(kSensorPresets[SENSOR_FULL_FRAME].crop_factor, 1.0f, 0.01f,
              "FF crop");

  float coc = cocForSensor(SENSOR_FULL_FRAME);
  EXPECT_TRUE(coc > 0.02f && coc < 0.04f, "CoC for FF: %.4f mm", coc);

  printf("  Sensor table OK.\n\n");
}

// ── Test: processImage smoke test ─────────────────────────────

static void testProcessImageSmoke() {
  printf("Test: processImage smoke test\n");

  int w = 32, h = 32;
  auto srcData = makeTestImage(w, h, 0.5f, 0.5f, 0.5f);
  auto dstData = makeTestImage(w, h, 0.0f, 0.0f, 0.0f);
  auto srcBuf = wrapImage(srcData, w, h);
  auto dstBuf = wrapImage(dstData, w, h);

  RenderParams params = defaultRenderParams();
  params.aberrationMaxSteps = 4; // keep it fast for test
  params.lensIndex = 0;

  processImage(srcBuf, dstBuf, nullptr, params);

  // Output should be non-zero (something was written)
  float sum = 0.0f;
  for (int i = 0; i < w * h * 4; ++i)
    sum += std::abs(dstData[i]);
  EXPECT_TRUE(sum > 0.0f, "Output should be non-zero");

  printf("  processImage smoke test OK.\n\n");
}

// ── Main ──────────────────────────────────────────────────────

int main() {
  printf("=== Glasspectum Pipeline Tests ===\n\n");

  testColorRoundtrip();
  testVignette();
  testMasterBlend();
  testDOFEngine();
  testTraitMixer();
  testSensorTable();
  testProcessImageSmoke();

  printf("=== RESULTS: %d/%d tests passed, %d errors ===\n", g_tests - g_errors,
         g_tests, g_errors);

  if (g_errors > 0) {
    printf("FAILED\n");
    return 1;
  }
  printf("ALL TESTS PASSED\n");
  return 0;
}
