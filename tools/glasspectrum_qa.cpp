/*
 * Glasspectrum QA Tool
 *
 * CLI tool for image quality comparison and lens calibration.
 *
 * Usage:
 *   glasspectrum_qa compare <reference.raw> <test.raw> <width> <height>
 *     → Computes SSIM and DSSIM between two RGBA float raw images.
 *
 *   glasspectrum_qa calibrate <reference.raw> <width> <height> <preset_index>
 *     → Suggests parameter adjustments to match reference.
 *
 *   glasspectrum_qa list
 *     → Lists all available lens presets.
 *
 * Image format: flat binary RGBA float32 (width * height * 4 * sizeof(float))
 */

#include "color_pipeline.h"
#include "glasspectrum_processor.h"
#include "lens_profile_db.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

using namespace glasspectrum;

// ── SSIM Implementation ──────────────────────────────────────

struct SSIMResult {
  float ssim;  // 0..1 (1 = identical)
  float dssim; // distance = (1 - ssim) / 2
};

static float mean(const float *data, int count) {
  double s = 0;
  for (int i = 0; i < count; ++i)
    s += data[i];
  return (float)(s / count);
}

static float variance(const float *data, int count, float m) {
  double s = 0;
  for (int i = 0; i < count; ++i) {
    float d = data[i] - m;
    s += d * d;
  }
  return (float)(s / count);
}

static float covariance(const float *a, const float *b, int count, float ma,
                        float mb) {
  double s = 0;
  for (int i = 0; i < count; ++i) {
    s += (a[i] - ma) * (b[i] - mb);
  }
  return (float)(s / count);
}

SSIMResult computeSSIM(const float *refData, const float *testData, int width,
                       int height) {
  // Compute per-channel SSIM on luminance
  int count = width * height;
  std::vector<float> refLuma(count);
  std::vector<float> testLuma(count);

  for (int i = 0; i < count; ++i) {
    refLuma[i] = refData[i * 4] * 0.2126f + refData[i * 4 + 1] * 0.7152f +
                 refData[i * 4 + 2] * 0.0722f;
    testLuma[i] = testData[i * 4] * 0.2126f + testData[i * 4 + 1] * 0.7152f +
                  testData[i * 4 + 2] * 0.0722f;
  }

  // SSIM constants
  float C1 = 0.01f * 0.01f; // (k1*L)^2 with L=1 for float
  float C2 = 0.03f * 0.03f;

  // Global SSIM (window = entire image; for per-window, use 8x8 blocks)
  // Using 8x8 block-based mean
  float totalSSIM = 0.0f;
  int numBlocks = 0;
  int blockSize = 8;

  for (int by = 0; by + blockSize <= height; by += blockSize) {
    for (int bx = 0; bx + blockSize <= width; bx += blockSize) {
      float blockRef[64], blockTest[64];
      int k = 0;
      for (int y = by; y < by + blockSize; ++y) {
        for (int x = bx; x < bx + blockSize; ++x) {
          blockRef[k] = refLuma[y * width + x];
          blockTest[k] = testLuma[y * width + x];
          k++;
        }
      }

      float mR = mean(blockRef, 64);
      float mT = mean(blockTest, 64);
      float vR = variance(blockRef, 64, mR);
      float vT = variance(blockTest, 64, mT);
      float cov = covariance(blockRef, blockTest, 64, mR, mT);

      float ssim = (2.0f * mR * mT + C1) * (2.0f * cov + C2) /
                   ((mR * mR + mT * mT + C1) * (vR + vT + C2));

      totalSSIM += ssim;
      numBlocks++;
    }
  }

  SSIMResult result;
  result.ssim = (numBlocks > 0) ? totalSSIM / numBlocks : 0.0f;
  result.dssim = (1.0f - result.ssim) / 2.0f;
  return result;
}

// ── Edge-based perceptual metric ─────────────────────────────
// Simple edge-aware metric as LPIPS alternative:
// Computes mean absolute error on Sobel-filtered luminance (edge map).

static float computeEdgeMAE(const float *refData, const float *testData,
                            int width, int height) {
  int count = width * height;
  std::vector<float> refLuma(count), testLuma(count);

  for (int i = 0; i < count; ++i) {
    refLuma[i] = refData[i * 4] * 0.2126f + refData[i * 4 + 1] * 0.7152f +
                 refData[i * 4 + 2] * 0.0722f;
    testLuma[i] = testData[i * 4] * 0.2126f + testData[i * 4 + 1] * 0.7152f +
                  testData[i * 4 + 2] * 0.0722f;
  }

  double totalError = 0;
  int count2 = 0;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      // Sobel on ref
      float rGx =
          refLuma[(y - 1) * width + (x + 1)] -
          refLuma[(y - 1) * width + (x - 1)] +
          2 * (refLuma[y * width + (x + 1)] - refLuma[y * width + (x - 1)]) +
          refLuma[(y + 1) * width + (x + 1)] -
          refLuma[(y + 1) * width + (x - 1)];
      float rGy =
          refLuma[(y + 1) * width + (x - 1)] -
          refLuma[(y - 1) * width + (x - 1)] +
          2 * (refLuma[(y + 1) * width + x] - refLuma[(y - 1) * width + x]) +
          refLuma[(y + 1) * width + (x + 1)] -
          refLuma[(y - 1) * width + (x + 1)];
      float rEdge = std::sqrt(rGx * rGx + rGy * rGy);

      // Sobel on test
      float tGx =
          testLuma[(y - 1) * width + (x + 1)] -
          testLuma[(y - 1) * width + (x - 1)] +
          2 * (testLuma[y * width + (x + 1)] - testLuma[y * width + (x - 1)]) +
          testLuma[(y + 1) * width + (x + 1)] -
          testLuma[(y + 1) * width + (x - 1)];
      float tGy =
          testLuma[(y + 1) * width + (x - 1)] -
          testLuma[(y - 1) * width + (x - 1)] +
          2 * (testLuma[(y + 1) * width + x] - testLuma[(y - 1) * width + x]) +
          testLuma[(y + 1) * width + (x + 1)] -
          testLuma[(y - 1) * width + (x + 1)];
      float tEdge = std::sqrt(tGx * tGx + tGy * tGy);

      totalError += std::abs(rEdge - tEdge);
      count2++;
    }
  }

  return (count2 > 0) ? (float)(totalError / count2) : 0.0f;
}

// ── CLI Entry Point ──────────────────────────────────────────

static void printUsage(const char *prog) {
  printf("Glasspectrum QA Tool v1.0\n\n");
  printf("Usage:\n");
  printf("  %s compare <reference.raw> <test.raw> <width> <height>\n", prog);
  printf("  %s calibrate <reference.raw> <width> <height> <preset_index>\n",
         prog);
  printf("  %s list\n\n", prog);
  printf("Image format: RGBA float32 raw binary\n");
}

static bool loadRawImage(const char *path, std::vector<float> &data, int width,
                         int height) {
  FILE *f = fopen(path, "rb");
  if (!f) {
    printf("Error: Cannot open %s\n", path);
    return false;
  }

  size_t expected = width * height * 4 * sizeof(float);
  data.resize(width * height * 4);
  size_t read = fread(data.data(), 1, expected, f);
  fclose(f);

  if (read != expected) {
    printf("Error: %s has %zu bytes, expected %zu\n", path, read, expected);
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printUsage(argv[0]);
    return 1;
  }

  if (strcmp(argv[1], "list") == 0) {
    printf("Available lens presets (%d):\n", getLensProfileCount());
    for (int i = 0; i < getLensProfileCount(); ++i) {
      const LensProfile *p = getLensProfile(i);
      const char *ev = "???";
      switch (p->evidence_level) {
      case EVIDENCE_PUBLISHED:
        ev = "published";
        break;
      case EVIDENCE_VENDOR_DESCRIBED:
        ev = "vendor";
        break;
      case EVIDENCE_HEURISTIC:
        ev = "heuristic";
        break;
      }
      printf("  [%2d] %-45s (%s)%s\n", i, p->name, ev,
             p->has_anamorphic ? " [ANA]" : "");
    }
    return 0;
  }

  if (strcmp(argv[1], "compare") == 0) {
    if (argc < 6) {
      printUsage(argv[0]);
      return 1;
    }

    int w = atoi(argv[4]);
    int h = atoi(argv[5]);

    std::vector<float> refData, testData;
    if (!loadRawImage(argv[2], refData, w, h))
      return 1;
    if (!loadRawImage(argv[3], testData, w, h))
      return 1;

    SSIMResult ssim = computeSSIM(refData.data(), testData.data(), w, h);
    float edgeMAE = computeEdgeMAE(refData.data(), testData.data(), w, h);

    printf("=== QA Comparison Results ===\n");
    printf("  SSIM:     %.6f\n", ssim.ssim);
    printf("  DSSIM:    %.6f\n", ssim.dssim);
    printf("  Edge MAE: %.6f\n", edgeMAE);
    printf("\n");

    if (ssim.ssim > 0.95f)
      printf("  Quality: EXCELLENT match\n");
    else if (ssim.ssim > 0.90f)
      printf("  Quality: GOOD match\n");
    else if (ssim.ssim > 0.80f)
      printf("  Quality: FAIR match\n");
    else
      printf("  Quality: POOR match — consider adjusting parameters\n");

    return 0;
  }

  if (strcmp(argv[1], "calibrate") == 0) {
    if (argc < 5) {
      printUsage(argv[0]);
      return 1;
    }

    int w = atoi(argv[3]);
    int h = atoi(argv[4]);
    int presetIdx = (argc > 5) ? atoi(argv[5]) : 0;

    std::vector<float> refData;
    if (!loadRawImage(argv[2], refData, w, h))
      return 1;

    const LensProfile *preset = getLensProfile(presetIdx);
    if (!preset) {
      printf("Error: Invalid preset index %d\n", presetIdx);
      return 1;
    }

    printf("=== Calibration Mode ===\n");
    printf("  Reference: %s (%dx%d)\n", argv[2], w, h);
    printf("  Base preset: [%d] %s\n\n", presetIdx, preset->name);

    // Generate a test image using the preset
    std::vector<float> testData(w * h * 4);
    std::memcpy(testData.data(), refData.data(), w * h * 4 * sizeof(float));

    ImageBuffer src, dst;
    src.data = refData.data();
    src.width = w;
    src.height = h;
    src.rowBytes = w * 4 * sizeof(float);
    dst.data = testData.data();
    dst.width = w;
    dst.height = h;
    dst.rowBytes = w * 4 * sizeof(float);

    RenderParams params = defaultRenderParams();
    params.lensIndex = presetIdx;
    params.aberrationMaxSteps = 8;
    processImage(src, dst, nullptr, params);

    SSIMResult ssim = computeSSIM(refData.data(), testData.data(), w, h);
    float edgeMAE = computeEdgeMAE(refData.data(), testData.data(), w, h);

    printf("  Baseline SSIM:  %.6f\n", ssim.ssim);
    printf("  Baseline Edge:  %.6f\n\n", edgeMAE);

    printf("  Suggested adjustments (tune these in Resolve):\n");
    if (edgeMAE > 0.05f)
      printf("    - Increase/decrease distortion amount\n");
    if (ssim.dssim > 0.05f) {
      printf("    - Adjust bloom gain\n");
      printf("    - Adjust vignette amount\n");
    }
    printf("    - Fine-tune CA amount based on edge color fringing\n");
    printf("    - Adjust fringing gain to match edge color artifacts\n\n");

    printf("  To save tuned parameters, create a JSON file:\n");
    printf("  {\n");
    printf("    \"name\": \"My Custom %s\",\n", preset->name);
    printf("    \"base_preset\": %d,\n", presetIdx);
    printf("    \"overrides\": { ... }\n");
    printf("  }\n");

    return 0;
  }

  printUsage(argv[0]);
  return 1;
}
