#pragma once
/*
 * Glasspectrum — Sensor Size Presets Table
 * Physical sensor dimensions for CoC, crop factor, and edge-aberration scaling.
 */

#include <cmath>

namespace glasspectrum {

struct SensorPreset {
  const char *name;
  float width_mm;
  float height_mm;
  float crop_factor; // relative to Full Frame 36×24
  float diagonal_mm; // derived
};

// Index constants matching the choice parameter
enum SensorSizeIndex {
  SENSOR_PHONE_1_3 = 0,
  SENSOR_PHONE_1_2_3 = 1,
  SENSOR_1_INCH = 2,
  SENSOR_MFT = 3,
  SENSOR_APS_C = 4,
  SENSOR_SUPER35 = 5,
  SENSOR_FULL_FRAME = 6,
  SENSOR_LARGE_FORMAT = 7,
  SENSOR_IMAX_65 = 8,
  SENSOR_COUNT = 9
};

inline float diag(float w, float h) { return std::sqrt(w * w + h * h); }

static const SensorPreset kSensorPresets[SENSOR_COUNT] = {
    // name                      width   height  crop     diagonal
    {"Phone 1/3\"", 4.80f, 3.60f, 7.50f, diag(4.80f, 3.60f)},
    {"Phone 1/2.3\"", 6.17f, 4.55f, 5.64f, diag(6.17f, 4.55f)},
    {"1\"", 13.20f, 8.80f, 2.73f, diag(13.20f, 8.80f)},
    {"Micro Four Thirds", 17.30f, 13.00f, 2.00f, diag(17.30f, 13.00f)},
    {"APS-C", 23.60f, 15.60f, 1.53f, diag(23.60f, 15.60f)},
    {"Super 35", 24.89f, 18.66f, 1.39f, diag(24.89f, 18.66f)},
    {"Full Frame 36x24", 36.00f, 24.00f, 1.00f, diag(36.00f, 24.00f)},
    {"Large Format 65 class", 52.50f, 23.00f, 0.68f, diag(52.50f, 23.00f)},
    {"IMAX 65 class", 70.41f, 52.63f, 0.47f, diag(70.41f, 52.63f)},
};

// Circle of confusion for "acceptable sharpness" in mm
// Standard formula: CoC = diagonal / 1500 (Zeiss formula)
inline float cocForSensor(int sensorIndex) {
  if (sensorIndex < 0 || sensorIndex >= SENSOR_COUNT)
    sensorIndex = SENSOR_FULL_FRAME;
  return kSensorPresets[sensorIndex].diagonal_mm / 1500.0f;
}

// Crop factor for scaling edge-dependent aberrations
inline float cropFactor(int sensorIndex) {
  if (sensorIndex < 0 || sensorIndex >= SENSOR_COUNT)
    sensorIndex = SENSOR_FULL_FRAME;
  return kSensorPresets[sensorIndex].crop_factor;
}

} // namespace glasspectrum
