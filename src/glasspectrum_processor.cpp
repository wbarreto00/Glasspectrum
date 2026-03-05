/*
 * Glasspectrum — CPU Image Processor Implementation
 * Full 11-stage rendering pipeline operating in scene-linear.
 *
 * Pipeline order:
 *   1) Input conversion to working linear
 *   2) Distortion warp (Brown–Conrady)
 *   3) Chromatic aberration (lateral radial RGB displacement)
 *   4) Coma (directional anisotropic blur)
 *   5) DOF bokeh blur (depth-based CoC)
 *   6) Vignette & edge color cast
 *   7) Bloom / veiling glare
 *   8) Fringing (edge-based colored fringe)
 *   9) f-Stop Sharpener (MTF microcontrast)
 *  10) Master Blend
 *  11) Output conversion
 */

#include "glasspectrum_processor.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace glasspectrum {

// ── Utility ───────────────────────────────────────────────────────

static inline float clamp01(float v) {
  return std::max(0.0f, std::min(1.0f, v));
}
static inline float lerp(float a, float b, float t) { return a + (b - a) * t; }

static inline float *pixelAt(ImageBuffer &img, int x, int y) {
  return reinterpret_cast<float *>(reinterpret_cast<char *>(img.data) +
                                   y * img.rowBytes) +
         x * 4;
}

static inline const float *pixelAt(const ImageBuffer &img, int x, int y) {
  return reinterpret_cast<const float *>(
             reinterpret_cast<const char *>(img.data) + y * img.rowBytes) +
         x * 4;
}

// Bilinear sample (clamp edges)
static void sampleBilinear(const ImageBuffer &img, float fx, float fy,
                           float *out) {
  int x0 = static_cast<int>(std::floor(fx));
  int y0 = static_cast<int>(std::floor(fy));
  float tx = fx - x0;
  float ty = fy - y0;

  int x1 = x0 + 1;
  int y1 = y0 + 1;

  x0 = std::max(0, std::min(x0, img.width - 1));
  x1 = std::max(0, std::min(x1, img.width - 1));
  y0 = std::max(0, std::min(y0, img.height - 1));
  y1 = std::max(0, std::min(y1, img.height - 1));

  const float *p00 = pixelAt(img, x0, y0);
  const float *p10 = pixelAt(img, x1, y0);
  const float *p01 = pixelAt(img, x0, y1);
  const float *p11 = pixelAt(img, x1, y1);

  for (int c = 0; c < 4; ++c) {
    float top = lerp(p00[c], p10[c], tx);
    float bottom = lerp(p01[c], p11[c], tx);
    out[c] = lerp(top, bottom, ty);
  }
}

// ── Default Render Params ─────────────────────────────────────────

RenderParams defaultRenderParams() {
  RenderParams p;
  std::memset(&p, 0, sizeof(p));

  p.inputColorSpace = CS_LINEAR;
  p.lensIndex = 0;
  p.masterBlend = 1.0f;
  p.fStopSharpener = 0.0f;
  p.sensorSizeIndex = SENSOR_FULL_FRAME;
  p.overdrive = 0.0f;

  p.scaleToFrame = false;
  p.aberrationMaxSteps = 32;
  p.depthGain = 1.0f;
  p.bokehSwirl = 0.0f;

  p.depthMapEnabled = false;
  p.depthMapGamma = 1.0f;
  p.depthMapScale = 1.0f;
  p.focusDistance_m = 3.0f;
  p.aperture = 2.8f;
  p.focalLength_mm = 50.0f;
  p.bladeCountOverride = 0;
  p.bladeCurvatureOverride = -1.0f;
  p.catEyeStrengthOverride = -1.0f;

  p.longiCA = 0.0f;
  p.breathingAmount = 0.0f;

  p.anamorphicMode = false;
  p.anaSqueezeOverride = 2.0f;
  p.anaOvalBokeh = 0.5f;
  p.anaStreakFlare = 0.5f;

  p.flareThreshold = 1.2f;
  p.flareGain = 0.0f;
  p.flareTint[0] = p.flareTint[1] = p.flareTint[2] = 1.0f;

  // Default: all trait groups from base lens
  p.traitMix.coma = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.distortion = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.chromaticAberration = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.bokehBlur = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.vignetteCast = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.bloom = {TRAIT_SOURCE_BASE, 0};
  p.traitMix.fringing = {TRAIT_SOURCE_BASE, 0};

  p.comaAmount = 1.0f;
  p.distortionAmount = 1.0f;
  p.caAmount = 1.0f;
  p.bokehAmount = 1.0f;
  p.vignetteAmount = 1.0f;
  p.bloomAmount = 1.0f;
  p.fringingAmount = 1.0f;

  return p;
}

// ══════════════════════════════════════════════════════════════════
// ══  STAGE IMPLEMENTATIONS  ═════════════════════════════════════
// ══════════════════════════════════════════════════════════════════

namespace stages {

// ── Stage 0,11: Color Space Conversion ──────────────────────────

void convertToLinear(ImageBuffer &img, ColorSpaceIndex cs) {
  if (cs == CS_LINEAR || cs == CS_ACESCG)
    return;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float *px = pixelAt(img, x, y);
      pixelToLinear(px, cs);
    }
  }
}

void convertFromLinear(ImageBuffer &img, ColorSpaceIndex cs) {
  if (cs == CS_LINEAR || cs == CS_ACESCG)
    return;
  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float *px = pixelAt(img, x, y);
      pixelFromLinear(px, cs);
    }
  }
}

// ── Stage 2: Distortion Warp (Brown–Conrady) ────────────────────

void applyDistortion(const ImageBuffer &src, ImageBuffer &dst,
                     const DistortionModel &model, float amount,
                     bool scaleToFrame, int sensorIndex) {
  if (amount < 0.001f) {
    // Copy source to dest
    if (src.data != dst.data)
      std::memcpy(dst.data, src.data, src.height * src.rowBytes);
    return;
  }

  float cx = src.width * 0.5f;
  float cy = src.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);
  float invMaxR = 1.0f / maxR;

  // Scale k values by amount
  float k1 = model.k1 * amount;
  float k2 = model.k2 * amount;
  float k3 = model.k3 * amount;
  float p1 = model.p1 * amount;
  float p2 = model.p2 * amount;

  // Compute scale factor for "scale to frame" mode
  float scaleFactor = 1.0f;
  if (scaleToFrame) {
    // Compute distortion at corner to find scale
    float cornerR = 1.0f; // normalized
    float cornerR2 = cornerR * cornerR;
    float radial = 1.0f + k1 * cornerR2 + k2 * cornerR2 * cornerR2 +
                   k3 * cornerR2 * cornerR2 * cornerR2;
    scaleFactor = 1.0f / radial;
  }

  for (int y = 0; y < dst.height; ++y) {
    for (int x = 0; x < dst.width; ++x) {
      // Normalized coords relative to center
      float dx = (x - cx) * invMaxR;
      float dy = (y - cy) * invMaxR;

      // Apply scale for frame fitting
      dx *= scaleFactor;
      dy *= scaleFactor;

      float r2 = dx * dx + dy * dy;
      float r4 = r2 * r2;
      float r6 = r4 * r2;

      // Radial distortion
      float radial = 1.0f + k1 * r2 + k2 * r4 + k3 * r6;

      // Tangential distortion
      float tx = 2.0f * p1 * dx * dy + p2 * (r2 + 2.0f * dx * dx);
      float ty = p1 * (r2 + 2.0f * dy * dy) + 2.0f * p2 * dx * dy;

      float srcX = (dx * radial + tx) * maxR + cx;
      float srcY = (dy * radial + ty) * maxR + cy;

      float *dstPx = pixelAt(dst, x, y);
      sampleBilinear(src, srcX, srcY, dstPx);
    }
  }
}

// ── Stage 3: Chromatic Aberration (Lateral) ─────────────────────

void applyChromaticAberration(ImageBuffer &img, float caEdgePx, float amount,
                              int maxSteps, int sensorIndex) {
  float totalCA = caEdgePx * amount;
  if (totalCA < 0.01f)
    return;

  // Scale for resolution (caEdgePx is specified at 4K width)
  float resScale = static_cast<float>(img.width) / 3840.0f;
  totalCA *= resScale;

  // Scale with sensor crop factor (larger sensors = more CA at edges)
  float cf = cropFactor(sensorIndex);
  totalCA /= cf; // full frame has cf=1, smaller sensors show less edge

  float cx = img.width * 0.5f;
  float cy = img.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);

  // We need a copy for sampling
  std::vector<float> temp(img.width * img.height * 4);
  std::memcpy(temp.data(), img.data,
              img.width * img.height * 4 * sizeof(float));

  ImageBuffer tempBuf;
  tempBuf.data = temp.data();
  tempBuf.width = img.width;
  tempBuf.height = img.height;
  tempBuf.rowBytes = img.width * 4 * sizeof(float);

  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float dx = x - cx;
      float dy = y - cy;
      float r = std::sqrt(dx * dx + dy * dy) / maxR; // 0..1
      float r2 = r * r;

      // R shifts outward, B shifts inward (smooth quadratic radial)
      float rShift = totalCA * r2;
      float bShift = -totalCA * r2 * 0.7f; // B shifts slightly less

      // Direction from center
      float nx = 0.0f, ny = 0.0f;
      if (r > 0.001f) {
        nx = dx / (r * maxR);
        ny = dy / (r * maxR);
      }

      float rPx[4], bPx[4];
      sampleBilinear(tempBuf, x + nx * rShift, y + ny * rShift, rPx);
      sampleBilinear(tempBuf, x + nx * bShift, y + ny * bShift, bPx);

      float *px = pixelAt(img, x, y);
      const float *origPx = pixelAt(tempBuf, x, y);
      px[0] = rPx[0];    // R from shifted position
      px[1] = origPx[1]; // G stays
      px[2] = bPx[2];    // B from shifted position
                         // Alpha stays
    }
  }
}

// ── Stage 4: Coma ───────────────────────────────────────────────

void applyComa(ImageBuffer &img, const ComaParams &coma, float amount,
               int maxSteps, int sensorIndex) {
  float strength = coma.strength * amount;
  if (strength < 0.001f)
    return;

  float cx = img.width * 0.5f;
  float cy = img.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);

  // Coma is stronger toward edges, scale with crop factor
  float cf = cropFactor(sensorIndex);
  float edgeScale = 1.0f / cf;

  // Coma blur radius in pixels at the edge
  float maxRadius = strength * 8.0f * (img.width / 3840.0f) * edgeScale;

  int steps = std::min(maxSteps, std::max(1, static_cast<int>(maxRadius * 2)));

  std::vector<float> temp(img.width * img.height * 4);
  std::memcpy(temp.data(), img.data,
              img.width * img.height * 4 * sizeof(float));

  ImageBuffer tempBuf;
  tempBuf.data = temp.data();
  tempBuf.width = img.width;
  tempBuf.height = img.height;
  tempBuf.rowBytes = img.width * 4 * sizeof(float);

  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float dx = x - cx;
      float dy = y - cy;
      float r = std::sqrt(dx * dx + dy * dy) / maxR;

      // Radial falloff per profile
      float falloff = std::pow(r, coma.falloff);
      float radius = maxRadius * falloff;

      if (radius < 0.5f)
        continue; // no blur needed here

      // Radial direction (coma is oriented radially)
      float nx = 0.0f, ny = 0.0f;
      if (r > 0.001f) {
        nx = dx / (r * maxR);
        ny = dy / (r * maxR);
      }

      // Tangential direction for anisotropy
      float tx_dir = -ny;
      float ty_dir = nx;

      float accum[4] = {0, 0, 0, 0};
      float totalWeight = 0.0f;

      for (int s = 0; s < steps; ++s) {
        float t = (static_cast<float>(s) / std::max(1, steps - 1)) * 2.0f -
                  1.0f; // -1..1
        // Anisotropic kernel: stretch more along radial direction
        float radialOff = t * radius;
        float tangentialOff = t * radius * (1.0f - coma.anisotropy);

        float sx = x + nx * radialOff + tx_dir * tangentialOff * 0.3f;
        float sy = y + ny * radialOff + ty_dir * tangentialOff * 0.3f;

        float sample[4];
        sampleBilinear(tempBuf, sx, sy, sample);

        // Gaussian-ish weight
        float w = std::exp(-t * t * 2.0f);
        for (int c = 0; c < 4; ++c)
          accum[c] += sample[c] * w;
        totalWeight += w;
      }

      float *px = pixelAt(img, x, y);
      if (totalWeight > 0.0f) {
        for (int c = 0; c < 4; ++c)
          px[c] = accum[c] / totalWeight;
      }
    }
  }
}

// ── Stage 5: Depth-of-Field Bokeh Blur ──────────────────────────

void applyBokehBlur(ImageBuffer &img, const float *depthMap,
                    const BokehParams &bokeh, const DOFParams &dof,
                    float swirlAmount, float amount, int maxSteps,
                    bool anamorphic, float anaSqueeze, float anaOval) {
  if (amount < 0.001f)
    return;
  if (!depthMap)
    return; // no depth = no DOF blur (fallback disabled)

  float cx = img.width * 0.5f;
  float cy = img.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);

  std::vector<float> temp(img.width * img.height * 4);
  std::memcpy(temp.data(), img.data,
              img.width * img.height * 4 * sizeof(float));

  ImageBuffer tempBuf;
  tempBuf.data = temp.data();
  tempBuf.width = img.width;
  tempBuf.height = img.height;
  tempBuf.rowBytes = img.width * 4 * sizeof(float);

  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float depth = depthMap[y * img.width + x];

      // Compute CoC for this pixel
      float subjectDist =
          depth * dof.focusDistance_m * 3.0f + 0.1f; // rough mapping
      float coc_mm = computeCoC_mm(subjectDist, dof);
      float blurRadius =
          cocToPixelRadius(coc_mm, dof.sensorIndex, img.width) * amount;

      if (blurRadius < 0.5f)
        continue;

      // Clamp blur radius for performance
      blurRadius = std::min(blurRadius, static_cast<float>(maxSteps));

      // Simple disc blur approximation (proper aperture shape is GPU-side)
      int samples =
          std::min(maxSteps, std::max(4, static_cast<int>(blurRadius * 4)));

      // Swirl: rotate kernel by angle proportional to radial position
      float dx_c = x - cx;
      float dy_c = y - cy;
      float r_norm = std::sqrt(dx_c * dx_c + dy_c * dy_c) / maxR;
      float swirlAngle = swirlAmount * bokeh.swirl_base * r_norm * r_norm *
                         static_cast<float>(M_PI);

      float accum[4] = {0, 0, 0, 0};
      float totalWeight = 0.0f;

      // Poisson-disc approximation (golden angle spiral)
      float goldenAngle = 2.39996323f;
      for (int s = 0; s < samples; ++s) {
        float sr = std::sqrt((s + 0.5f) / samples) * blurRadius;
        float sa = s * goldenAngle + swirlAngle;

        float sx_off = std::cos(sa) * sr;
        float sy_off = std::sin(sa) * sr;

        // Anamorphic oval: stretch vertically
        if (anamorphic) {
          sy_off *= anaSqueeze * anaOval;
        }

        float sample[4];
        sampleBilinear(tempBuf, x + sx_off, y + sy_off, sample);

        float w = 1.0f;
        for (int c = 0; c < 4; ++c)
          accum[c] += sample[c] * w;
        totalWeight += w;
      }

      float *px = pixelAt(img, x, y);
      if (totalWeight > 0.0f) {
        for (int c = 0; c < 4; ++c)
          px[c] = accum[c] / totalWeight;
      }
    }
  }
}

// ── Stage 6: Vignette & Color Cast ──────────────────────────────

void applyVignette(ImageBuffer &img, const VignetteParams &vig,
                   const ColorCastParams &cast, float amount, int sensorIndex) {
  if (amount < 0.001f)
    return;

  float cx = img.width * 0.5f;
  float cy = img.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);

  float vigStops = vig.vig_stops_at_edge * amount;
  float cf = cropFactor(sensorIndex);
  // Larger sensors have same vignette, smaller sensors see less edge
  float edgeScale = 1.0f / cf;

  for (int y = 0; y < img.height; ++y) {
    for (int x = 0; x < img.width; ++x) {
      float dx = x - cx;
      float dy = y - cy;
      float r = std::sqrt(dx * dx + dy * dy) / maxR; // 0..1
      r *= edgeScale;

      // Exposure falloff: cos^n-like model
      float rn = std::pow(r, vig.power);
      float stopLoss = vigStops * rn;
      float multiplier = std::pow(2.0f, -stopLoss); // exposure stops

      float *px = pixelAt(img, x, y);
      px[0] *= multiplier;
      px[1] *= multiplier;
      px[2] *= multiplier;

      // Color cast (shift chromaticity at edges)
      if (std::abs(cast.cast_delta_xy[0]) > 0.0001f ||
          std::abs(cast.cast_delta_xy[1]) > 0.0001f) {
        float castR = rn * amount;
        // Simple approximation: shift R and B channels
        px[0] *= (1.0f + cast.cast_delta_xy[0] * castR * 50.0f);
        px[2] *= (1.0f - cast.cast_delta_xy[1] * castR * 50.0f);
      }
    }
  }
}

// ── Stage 7: Bloom / Veiling Glare ──────────────────────────────

void applyBloom(ImageBuffer &img, const BloomParams &bloom, float amount,
                int maxSteps) {
  if (amount < 0.001f || bloom.gain < 0.001f)
    return;

  float threshold = bloom.bloom_threshold_linear;
  float gain = bloom.gain * amount;
  int w = img.width;
  int h = img.height;

  // Extract highlights into a separate buffer
  std::vector<float> highlights(w * h * 4, 0.0f);

  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const float *px = pixelAt(img, x, y);
      float luma = px[0] * 0.2126f + px[1] * 0.7152f + px[2] * 0.0722f;
      if (luma > threshold) {
        float excess = (luma - threshold) / luma;
        int idx = (y * w + x) * 4;
        highlights[idx + 0] = px[0] * excess;
        highlights[idx + 1] = px[1] * excess;
        highlights[idx + 2] = px[2] * excess;
        highlights[idx + 3] = 0.0f;
      }
    }
  }

  // Multi-scale Gaussian blur with exponential falloff
  int numScales = std::min(6, maxSteps / 4 + 1);
  std::vector<float> blurred(highlights);
  (void)blurred;

  // Simple box blur approximation for each scale
  std::vector<float> tempH(w * h * 4);
  std::vector<float> blurAccum(w * h * 4, 0.0f);

  for (int scale = 0; scale < numScales; ++scale) {
    int radius = (1 << (scale + 1)); // 2, 4, 8, 16, 32, 64
    float scaleWeight = std::pow(bloom.falloff_exp, -static_cast<float>(scale));

    // Use highlights as source for each scale
    // Horizontal pass
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float accum[4] = {0, 0, 0, 0};
        int count = 0;
        for (int kx = -radius; kx <= radius; kx += std::max(1, radius / 4)) {
          int sx = std::max(0, std::min(w - 1, x + kx));
          int idx = (y * w + sx) * 4;
          accum[0] += highlights[idx + 0];
          accum[1] += highlights[idx + 1];
          accum[2] += highlights[idx + 2];
          count++;
        }
        int idx = (y * w + x) * 4;
        tempH[idx + 0] = accum[0] / count;
        tempH[idx + 1] = accum[1] / count;
        tempH[idx + 2] = accum[2] / count;
      }
    }

    // Vertical pass
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        float accum[4] = {0, 0, 0, 0};
        int count = 0;
        for (int ky = -radius; ky <= radius; ky += std::max(1, radius / 4)) {
          int sy = std::max(0, std::min(h - 1, y + ky));
          int idx = (sy * w + x) * 4;
          accum[0] += tempH[idx + 0];
          accum[1] += tempH[idx + 1];
          accum[2] += tempH[idx + 2];
          count++;
        }
        int idx = (y * w + x) * 4;
        blurAccum[idx + 0] += accum[0] / count * scaleWeight;
        blurAccum[idx + 1] += accum[1] / count * scaleWeight;
        blurAccum[idx + 2] += accum[2] / count * scaleWeight;
      }
    }
  }

  // Recombine with tint
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      float *px = pixelAt(img, x, y);
      int idx = (y * w + x) * 4;
      px[0] += blurAccum[idx + 0] * gain * bloom.tint_rgb[0];
      px[1] += blurAccum[idx + 1] * gain * bloom.tint_rgb[1];
      px[2] += blurAccum[idx + 2] * gain * bloom.tint_rgb[2];
      // No clamping — preserve linear HDR values
    }
  }
}

// ── Stage 8: Fringing ───────────────────────────────────────────

void applyFringing(ImageBuffer &img, const FringingParams &fringe, float amount,
                   int sensorIndex) {
  if (amount < 0.001f || fringe.gain < 0.001f)
    return;

  float fringeGain = fringe.gain * amount;
  float cx = img.width * 0.5f;
  float cy = img.height * 0.5f;
  float maxR = std::sqrt(cx * cx + cy * cy);
  float resScale = img.width / 3840.0f;
  float widthPx = fringe.width_px * resScale;

  // Convert hue to RGB
  float hueRad = fringe.hue_deg * static_cast<float>(M_PI) / 180.0f;
  float fringeR = 0.5f + 0.5f * std::cos(hueRad);
  float fringeG = 0.5f + 0.5f * std::cos(hueRad - 2.094395f);
  float fringeB = 0.5f + 0.5f * std::cos(hueRad + 2.094395f);

  std::vector<float> temp(img.width * img.height * 4);
  std::memcpy(temp.data(), img.data,
              img.width * img.height * 4 * sizeof(float));

  for (int y = 1; y < img.height - 1; ++y) {
    for (int x = 1; x < img.width - 1; ++x) {
      // Compute luma gradient (Sobel)
      int idx = (y * img.width + x) * 4;
      int idxL = (y * img.width + (x - 1)) * 4;
      int idxR = (y * img.width + (x + 1)) * 4;
      int idxU = ((y - 1) * img.width + x) * 4;
      int idxD = ((y + 1) * img.width + x) * 4;

      float lumaC = temp[idx] * 0.2126f + temp[idx + 1] * 0.7152f +
                    temp[idx + 2] * 0.0722f;
      float lumaL = temp[idxL] * 0.2126f + temp[idxL + 1] * 0.7152f +
                    temp[idxL + 2] * 0.0722f;
      float lumaR = temp[idxR] * 0.2126f + temp[idxR + 1] * 0.7152f +
                    temp[idxR + 2] * 0.0722f;
      float lumaU = temp[idxU] * 0.2126f + temp[idxU + 1] * 0.7152f +
                    temp[idxU + 2] * 0.0722f;
      float lumaD = temp[idxD] * 0.2126f + temp[idxD + 1] * 0.7152f +
                    temp[idxD + 2] * 0.0722f;

      float gx = (lumaR - lumaL) * 0.5f;
      float gy = (lumaD - lumaU) * 0.5f;
      float edgeMag = std::sqrt(gx * gx + gy * gy);

      if (edgeMag < 0.01f)
        continue;

      // Radial bias: more fringing at edges
      float dx = x - cx;
      float dy = y - cy;
      float r = std::sqrt(dx * dx + dy * dy) / maxR;
      float radialMix = lerp(1.0f - fringe.radial_bias, 1.0f, r);

      float fringeStrength = edgeMag * fringeGain * radialMix;
      fringeStrength = std::min(fringeStrength, 0.5f); // cap to avoid artifacts

      float *px = pixelAt(img, x, y);
      px[0] += fringeStrength * fringeR;
      px[1] += fringeStrength * fringeG;
      px[2] += fringeStrength * fringeB;
    }
  }
}

// ── Stage 9: f-Stop Sharpener ───────────────────────────────────

void applySharpener(ImageBuffer &img, const SharpnessParams &sharpness,
                    float fStopSharpener, float aperture) {
  if (std::abs(fStopSharpener) < 0.001f)
    return;

  // When aperture is wide open, profile may have wide_open_glow
  // Sharpener restores microcontrast without removing glow entirely
  float wideOpenFactor = 1.0f;
  if (aperture < 2.0f) {
    wideOpenFactor = lerp(0.5f, 1.0f, (aperture - 0.7f) / 1.3f);
  }

  float sharpenAmount =
      fStopSharpener * sharpness.microcontrast * wideOpenFactor;

  // MTF-shaped unsharp mask: use small radius for microcontrast
  int w = img.width;
  int h = img.height;

  std::vector<float> temp(w * h * 4);
  std::memcpy(temp.data(), img.data, w * h * 4 * sizeof(float));

  for (int y = 1; y < h - 1; ++y) {
    for (int x = 1; x < w - 1; ++x) {
      float *px = pixelAt(img, x, y);
      int idx = (y * w + x) * 4;

      // Compute local average (3x3)
      float avg[3] = {0, 0, 0};
      for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
          int si = ((y + ky) * w + (x + kx)) * 4;
          avg[0] += temp[si + 0];
          avg[1] += temp[si + 1];
          avg[2] += temp[si + 2];
        }
      }
      avg[0] /= 9.0f;
      avg[1] /= 9.0f;
      avg[2] /= 9.0f;

      // Unsharp mask with highlight protection
      for (int c = 0; c < 3; ++c) {
        float detail = temp[idx + c] - avg[c];

        // Highlight protection: reduce sharpening for very bright pixels
        float luma = temp[idx] * 0.2126f + temp[idx + 1] * 0.7152f +
                     temp[idx + 2] * 0.0722f;
        float protection = (luma > 1.0f) ? 1.0f / (luma * 0.5f + 0.5f) : 1.0f;

        px[c] = temp[idx + c] + detail * sharpenAmount * protection;
      }
    }
  }
}

// ── Stage 10: Master Blend ──────────────────────────────────────

void applyMasterBlend(const ImageBuffer &original, ImageBuffer &processed,
                      float blend) {
  if (blend >= 0.999f)
    return; // fully processed, no blend needed

  for (int y = 0; y < processed.height; ++y) {
    for (int x = 0; x < processed.width; ++x) {
      const float *orig = pixelAt(original, x, y);
      float *proc = pixelAt(processed, x, y);

      // Linear blend in linear light
      proc[0] = lerp(orig[0], proc[0], blend);
      proc[1] = lerp(orig[1], proc[1], blend);
      proc[2] = lerp(orig[2], proc[2], blend);
      // Alpha: blend as well
      proc[3] = lerp(orig[3], proc[3], blend);
    }
  }
}

} // namespace stages

// ══════════════════════════════════════════════════════════════════
// ══  MAIN PROCESS FUNCTION  ═════════════════════════════════════
// ══════════════════════════════════════════════════════════════════

void processImage(const ImageBuffer &src, ImageBuffer &dst,
                  const float *depthMap, const RenderParams &params) {
  int w = src.width;
  int h = src.height;
  int pixelCount = w * h;

  // Resolve the trait mix
  ResolvedProfile profile = resolveProfile(params.lensIndex, params.traitMix);

  // Apply overdrive to the resolved profile
  applyOverdrive(profile, params.overdrive);

  // 1) Copy source → working buffer and convert to linear
  std::vector<float> workBuf(pixelCount * 4);
  std::memcpy(workBuf.data(), src.data, pixelCount * 4 * sizeof(float));

  ImageBuffer work;
  work.data = workBuf.data();
  work.width = w;
  work.height = h;
  work.rowBytes = w * 4 * sizeof(float);

  // Keep original (in linear) for master blend
  std::vector<float> originalLinear(pixelCount * 4);

  stages::convertToLinear(work, params.inputColorSpace);
  std::memcpy(originalLinear.data(), workBuf.data(),
              pixelCount * 4 * sizeof(float));

  ImageBuffer origBuf;
  origBuf.data = originalLinear.data();
  origBuf.width = w;
  origBuf.height = h;
  origBuf.rowBytes = w * 4 * sizeof(float);

  // 2) Distortion warp
  {
    std::vector<float> distBuf(pixelCount * 4);
    ImageBuffer distDst;
    distDst.data = distBuf.data();
    distDst.width = w;
    distDst.height = h;
    distDst.rowBytes = w * 4 * sizeof(float);

    stages::applyDistortion(work, distDst, profile.distortionModel,
                            params.distortionAmount, params.scaleToFrame,
                            params.sensorSizeIndex);
    std::memcpy(workBuf.data(), distBuf.data(), pixelCount * 4 * sizeof(float));
  }

  // 3) Chromatic aberration (lateral)
  stages::applyChromaticAberration(work, profile.caEdgePx, params.caAmount,
                                   params.aberrationMaxSteps,
                                   params.sensorSizeIndex);

  // 4) Coma
  stages::applyComa(work, profile.coma, params.comaAmount,
                    params.aberrationMaxSteps, params.sensorSizeIndex);

  // 5) DOF bokeh blur
  {
    DOFParams dof;
    dof.focusDistance_m = params.focusDistance_m;
    dof.aperture = params.aperture;
    dof.focalLength_mm = params.focalLength_mm;
    dof.sensorIndex = params.sensorSizeIndex;
    dof.depthGain = params.depthGain;

    BokehParams bk = profile.bokeh;
    if (params.bladeCountOverride > 0)
      bk.blade_count = params.bladeCountOverride;
    if (params.bladeCurvatureOverride >= 0.0f)
      bk.blade_curvature = params.bladeCurvatureOverride;
    if (params.catEyeStrengthOverride >= 0.0f)
      bk.cat_eye_strength = params.catEyeStrengthOverride;

    bool doAna = params.anamorphicMode || profile.hasAnamorphic;
    float anaSq = doAna ? (profile.hasAnamorphic ? profile.anamorphic.squeeze
                                                 : params.anaSqueezeOverride)
                        : 1.0f;
    float anaOv = doAna ? (profile.hasAnamorphic ? profile.anamorphic.oval_bokeh
                                                 : params.anaOvalBokeh)
                        : 0.0f;

    stages::applyBokehBlur(work, depthMap, bk, dof, params.bokehSwirl,
                           params.bokehAmount, params.aberrationMaxSteps, doAna,
                           anaSq, anaOv);
  }

  // 6) Vignette & color cast
  stages::applyVignette(work, profile.vignette, profile.colorCast,
                        params.vignetteAmount, params.sensorSizeIndex);

  // 7) Bloom / veiling glare
  stages::applyBloom(work, profile.bloom, params.bloomAmount,
                     params.aberrationMaxSteps);

  // 8) Fringing
  stages::applyFringing(work, profile.fringing, params.fringingAmount,
                        params.sensorSizeIndex);

  // 9) f-Stop Sharpener
  stages::applySharpener(work, profile.sharpness, params.fStopSharpener,
                         params.aperture);

  // 10) Master Blend (mix in linear)
  stages::applyMasterBlend(origBuf, work, params.masterBlend);

  // 11) Convert back from linear if needed
  stages::convertFromLinear(work, params.inputColorSpace);

  // Copy to output
  std::memcpy(dst.data, workBuf.data(), pixelCount * 4 * sizeof(float));
}

} // namespace glasspectrum
