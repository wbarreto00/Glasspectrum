/*
 * Glasspectrum — Color Pipeline Implementation
 * All transfer functions preserve out-of-range values.
 */

#include "color_pipeline.h"
#include <algorithm>
#include <cmath>

namespace glasspectrum {

// ── sRGB OETF / EOTF ─────────────────────────────────────────────

static float srgbToLinear(float v) {
  if (v <= 0.04045f)
    return v / 12.92f;
  return std::pow((v + 0.055f) / 1.055f, 2.4f);
}

static float linearToSrgb(float v) {
  if (v <= 0.0031308f)
    return v * 12.92f;
  return 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
}

// ── Rec.709 (BT.1886 EOTF with Rec.709 OETF) ────────────────────

static float rec709ToLinear(float v) {
  if (v < 0.081f)
    return v / 4.5f;
  return std::pow((v + 0.099f) / 1.099f, 1.0f / 0.45f);
}

static float linearToRec709(float v) {
  if (v < 0.018f)
    return v * 4.5f;
  return 1.099f * std::pow(v, 0.45f) - 0.099f;
}

// ── Rec.2020 HLG (ARIB STD-B67 OETF/EOTF) ──────────────────────

static const float HLG_A = 0.17883277f;
static const float HLG_B = 0.28466892f; // 1 - 4*a
static const float HLG_C = 0.55991073f; // 0.5 - a*ln(4*a)

static float hlgOetfInv(float v) {
  // E' -> linear scene light (relative)
  if (v <= 0.5f)
    return (v * v) / 3.0f;
  return (std::exp((v - HLG_C) / HLG_A) + HLG_B) / 12.0f;
}

static float hlgOetf(float v) {
  if (v <= 1.0f / 12.0f)
    return std::sqrt(3.0f * v);
  return HLG_A * std::log(12.0f * v - HLG_B) + HLG_C;
}

// ── ACEScc ────────────────────────────────────────────────────────

static float acesccToLinear(float v) {
  if (v < -0.3013698630f)
    return (std::pow(2.0f, v * 17.52f - 9.72f) - 0.000030517578125f) * 2.0f;
  if (v < (std::log2(65504.0f) + 9.72f) / 17.52f)
    return std::pow(2.0f, v * 17.52f - 9.72f);
  return 65504.0f;
}

static float linearToAcescc(float v) {
  if (v <= 0.0f)
    return -0.3584474886f;    // (log2(pow(2,-15)*0.5) + 9.72) / 17.52
  if (v < 0.000030517578125f) // 2^-15
    return (std::log2(0.000030517578125f + v * 0.5f) + 9.72f) / 17.52f;
  return (std::log2(v) + 9.72f) / 17.52f;
}

// ── ACEScct ───────────────────────────────────────────────────────

static const float ACESCCT_CUT = 0.0078125f; // 2^-7
static const float ACESCCT_CUT_OUT = 0.155251141552511f;

static float acescctToLinear(float v) {
  if (v <= ACESCCT_CUT_OUT)
    return (v - 0.0729055341958355f) / 10.5402377416545f;
  return std::pow(2.0f, v * 17.52f - 9.72f);
}

static float linearToAcescct(float v) {
  if (v <= ACESCCT_CUT)
    return 10.5402377416545f * v + 0.0729055341958355f;
  return (std::log2(v) + 9.72f) / 17.52f;
}

// ── ARRI LogC3 (EI 800, SUP 3.x) ─────────────────────────────────

static const float LOGC_CUT = 0.010591f;
static const float LOGC_A = 5.555556f;
static const float LOGC_B = 0.052272f;
static const float LOGC_C = 0.247190f;
static const float LOGC_D = 0.385537f;
static const float LOGC_E = 5.367655f;
static const float LOGC_F = 0.092809f;

static float logcToLinear(float v) {
  if (v > LOGC_E * LOGC_CUT + LOGC_F)
    return (std::pow(10.0f, (v - LOGC_D) / LOGC_C) - LOGC_B) / LOGC_A;
  return (v - LOGC_F) / LOGC_E;
}

static float linearToLogc(float v) {
  if (v > LOGC_CUT)
    return LOGC_C * std::log10(LOGC_A * v + LOGC_B) + LOGC_D;
  return LOGC_E * v + LOGC_F;
}

// ── Sony S-Log3 ───────────────────────────────────────────────────

static float slog3ToLinear(float v) {
  if (v >= 171.2102946929f / 1023.0f)
    return std::pow(10.0f, (v * 1023.0f - 420.0f) / 261.5f) * (0.18f + 0.01f) -
           0.01f;
  return (v * 1023.0f - 95.0f) * 0.01125000f / (171.2102946929f - 95.0f);
}

static float linearToSlog3(float v) {
  if (v >= 0.01125000f)
    return (420.0f + std::log10((v + 0.01f) / (0.18f + 0.01f)) * 261.5f) /
           1023.0f;
  return (v * (171.2102946929f - 95.0f) / 0.01125000f + 95.0f) / 1023.0f;
}

// ── Panasonic V-Log ───────────────────────────────────────────────

static const float VLOG_CUT_F = 0.01f;
static const float VLOG_B = 0.00873f;
static const float VLOG_C = 0.241514f;
static const float VLOG_D = 0.598206f;

static float vlogToLinear(float v) {
  if (v < 0.181f)
    return (v - 0.125f) / 5.6f;
  return std::pow(10.0f, (v - VLOG_D) / VLOG_C) - VLOG_B;
}

static float linearToVlog(float v) {
  if (v < VLOG_CUT_F)
    return 5.6f * v + 0.125f;
  return VLOG_C * std::log10(v + VLOG_B) + VLOG_D;
}

// ── Canon Log ─────────────────────────────────────────────────────

static float canonLogToLinear(float v) {
  if (v < 0.0730597f)
    return -(std::pow(10.0f, (0.0730597f - v) / 0.529136f) - 1.0f) / 10.1596f;
  return (std::pow(10.0f, (v - 0.0730597f) / 0.529136f) - 1.0f) / 10.1596f;
}

static float linearToCanonLog(float v) {
  if (v < 0.0f)
    return -0.529136f * std::log10(1.0f - 10.1596f * v) + 0.0730597f;
  return 0.529136f * std::log10(1.0f + 10.1596f * v) + 0.0730597f;
}

// ── Public API ────────────────────────────────────────────────────

float toLinear(float v, ColorSpaceIndex cs) {
  switch (cs) {
  case CS_LINEAR:
    return v;
  case CS_SRGB:
    return srgbToLinear(v);
  case CS_REC709:
    return rec709ToLinear(v);
  case CS_REC2020_HLG:
    return hlgOetfInv(v);
  case CS_ACESCG:
    return v; // already linear
  case CS_ACESCC:
    return acesccToLinear(v);
  case CS_ACESCCT:
    return acescctToLinear(v);
  case CS_LOG_C:
    return logcToLinear(v);
  case CS_SLOG3:
    return slog3ToLinear(v);
  case CS_VLOG:
    return vlogToLinear(v);
  case CS_CANON_LOG:
    return canonLogToLinear(v);
  default:
    return v;
  }
}

float fromLinear(float v, ColorSpaceIndex cs) {
  switch (cs) {
  case CS_LINEAR:
    return v;
  case CS_SRGB:
    return linearToSrgb(v);
  case CS_REC709:
    return linearToRec709(v);
  case CS_REC2020_HLG:
    return hlgOetf(v);
  case CS_ACESCG:
    return v;
  case CS_ACESCC:
    return linearToAcescc(v);
  case CS_ACESCCT:
    return linearToAcescct(v);
  case CS_LOG_C:
    return linearToLogc(v);
  case CS_SLOG3:
    return linearToSlog3(v);
  case CS_VLOG:
    return linearToVlog(v);
  case CS_CANON_LOG:
    return linearToCanonLog(v);
  default:
    return v;
  }
}

void pixelToLinear(float *rgba, ColorSpaceIndex cs) {
  if (cs == CS_LINEAR || cs == CS_ACESCG)
    return;
  rgba[0] = toLinear(rgba[0], cs);
  rgba[1] = toLinear(rgba[1], cs);
  rgba[2] = toLinear(rgba[2], cs);
  // alpha untouched
}

void pixelFromLinear(float *rgba, ColorSpaceIndex cs) {
  if (cs == CS_LINEAR || cs == CS_ACESCG)
    return;
  rgba[0] = fromLinear(rgba[0], cs);
  rgba[1] = fromLinear(rgba[1], cs);
  rgba[2] = fromLinear(rgba[2], cs);
}

static const char *s_colorSpaceNames[CS_COUNT] = {"Linear (Scene Referred)",
                                                  "sRGB",
                                                  "Rec.709",
                                                  "Rec.2020 / HLG",
                                                  "ACEScg (Linear)",
                                                  "ACEScc",
                                                  "ACEScct",
                                                  "ARRI LogC3",
                                                  "Sony S-Log3",
                                                  "Panasonic V-Log",
                                                  "Canon Log"};

const char *colorSpaceName(ColorSpaceIndex cs) {
  if (cs < 0 || cs >= CS_COUNT)
    return "Unknown";
  return s_colorSpaceNames[cs];
}

} // namespace glasspectrum
