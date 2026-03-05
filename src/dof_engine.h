#pragma once
/*
 * Glasspectum — Depth-of-Field Engine
 * Physically grounded Circle of Confusion computation.
 */

#include "sensor_table.h"

namespace glasspectum {

// ── DOF computation parameters ─────────────────────────────────────

struct DOFParams {
  float focusDistance_m; // meters
  float aperture;        // f-number (e.g. 1.4, 2.8, 5.6)
  float focalLength_mm;  // mm
  int sensorIndex;       // index into kSensorPresets
  float depthGain;       // artistic multiplier for blur radius
};

// Compute the circle of confusion (CoC) in mm for a point at distance Z
// given the focus, aperture, and focal length.
// Standard thin-lens formula:
//   CoC = |S2 - S1| / S2 * f^2 / (N * (S1 - f))
// where S1 = focus distance, S2 = subject distance, f = focal length, N =
// f-number
inline float computeCoC_mm(float subjectDistance_m, const DOFParams &params) {
  float f = params.focalLength_mm * 0.001f; // convert to meters
  float S1 = params.focusDistance_m;
  float S2 = subjectDistance_m;
  float N = params.aperture;

  if (S1 <= f || S2 <= 0.0f || N <= 0.0f)
    return 0.0f;

  // Magnification at focus distance
  float m = f / (S1 - f);

  // CoC in meters
  float coc = std::abs(S2 - S1) / S2 * f * m / N;

  // Convert to mm and apply depth gain
  return coc * 1000.0f * params.depthGain;
}

// Convert CoC in mm to pixel blur radius given sensor and image dimensions.
inline float cocToPixelRadius(float coc_mm, int sensorIndex, int imageWidth) {
  if (sensorIndex < 0 || sensorIndex >= SENSOR_COUNT)
    sensorIndex = SENSOR_FULL_FRAME;
  float sensorWidth_mm = kSensorPresets[sensorIndex].width_mm;
  if (sensorWidth_mm <= 0.0f)
    return 0.0f;
  return coc_mm / sensorWidth_mm * static_cast<float>(imageWidth);
}

// Compute the acceptable CoC for this sensor (Zeiss formula: diagonal/1500)
inline float acceptableCoC_mm(int sensorIndex) {
  return cocForSensor(sensorIndex);
}

// Hyperfocal distance in meters
inline float hyperfocalDistance_m(const DOFParams &params) {
  float f = params.focalLength_mm;
  float N = params.aperture;
  float c = acceptableCoC_mm(params.sensorIndex);
  if (c <= 0.0f || N <= 0.0f)
    return 1e6f;
  return (f * f) / (N * c) * 0.001f + params.focalLength_mm * 0.001f;
}

// Near and far focus limits in meters
inline void focusLimits(const DOFParams &params, float &nearLimit,
                        float &farLimit) {
  float H = hyperfocalDistance_m(params);
  float s = params.focusDistance_m;
  float f = params.focalLength_mm * 0.001f;

  nearLimit = (s * (H - f)) / (H + s - 2.0f * f);
  if (nearLimit < 0.0f)
    nearLimit = 0.0f;

  if (s >= H) {
    farLimit = 1e6f; // infinity
  } else {
    farLimit = (s * (H - f)) / (H - s);
  }
}

} // namespace glasspectum
