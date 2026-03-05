#pragma once
/*
 * Glasspectrum — Color Pipeline
 * Bidirectional conversion between camera/display color spaces and scene-linear
 * working space. Preserves out-of-range values (no clamping).
 */

namespace glasspectrum {

enum ColorSpaceIndex {
  CS_LINEAR = 0,
  CS_SRGB = 1,
  CS_REC709 = 2,
  CS_REC2020_HLG = 3,
  CS_ACESCG = 4, // passthrough (already linear)
  CS_ACESCC = 5,
  CS_ACESCCT = 6,
  CS_LOG_C = 7, // ARRI LogC3
  CS_SLOG3 = 8, // Sony S-Log3
  CS_VLOG = 9,  // Panasonic V-Log
  CS_CANON_LOG = 10,
  CS_COUNT = 11
};

// Convert a single float channel value from the given color space to
// scene-linear.
float toLinear(float v, ColorSpaceIndex cs);

// Convert a single float channel value from scene-linear back to the given
// color space.
float fromLinear(float v, ColorSpaceIndex cs);

// Convert RGBA pixel in-place to linear. Alpha is untouched.
void pixelToLinear(float *rgba, ColorSpaceIndex cs);

// Convert RGBA pixel in-place from linear. Alpha is untouched.
void pixelFromLinear(float *rgba, ColorSpaceIndex cs);

// Get display name for the choice param dropdown.
const char *colorSpaceName(ColorSpaceIndex cs);

} // namespace glasspectrum
