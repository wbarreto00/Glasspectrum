/*
 * Glasspectrum — Metal Compute Shaders
 * Primary GPU rendering path for macOS.
 *
 * All pipeline stages as Metal compute kernels operating on RGBA float textures.
 * Processing is done in scene-linear color space.
 */

#include <metal_stdlib>
using namespace metal;

// ── Shared parameter buffer ─────────────────────────────────────

struct GlassPipelineParams {
    // Image dimensions
    uint2 imageSize;
    float2 center;
    float maxR;

    // Distortion (Brown–Conrady)
    float dist_k1, dist_k2, dist_k3;
    float dist_p1, dist_p2;
    float distortionAmount;
    int scaleToFrame;

    // Chromatic Aberration
    float caEdgePx;
    float caAmount;
    float caResScale;

    // Coma
    float comaStrength;
    float comaAnisotropy;
    float comaHueBias;
    float comaFalloff;
    float comaAmount;

    // Vignette & Cast
    float vigStops;
    float vigPower;
    float vigAmount;
    float castDeltaX, castDeltaY;

    // Bloom
    float bloomThreshold;
    float bloomGain;
    float bloomFalloffExp;
    float3 bloomTint;
    float bloomAmount;

    // Fringing
    float fringeGain;
    float fringeHueDeg;
    float fringeWidthPx;
    float fringeRadialBias;
    float fringeAmount;

    // Sharpener
    float sharpMicrocontrast;
    float sharpWideOpenGlow;
    float fStopSharpener;
    float aperture;

    // Bokeh / DOF
    float focusDistance;
    float focalLength;
    float sensorWidth;
    float depthGain;
    float bokehAmount;
    float swirlBase;
    float swirlAmount;
    int bladeCount;
    float bladeCurvature;
    float catEyeStrength;
    int maxSteps;

    // Anamorphic
    int anamorphicEnabled;
    float anaSqueeze;
    float anaOval;

    // Master blend
    float masterBlend;

    // Sensor crop
    float cropFactor;
};

// ── Utility functions ───────────────────────────────────────────

inline float2 normalizeCoord(uint2 gid, constant GlassPipelineParams& p) {
    return float2(float(gid.x) - p.center.x, float(gid.y) - p.center.y) / p.maxR;
}

inline float radialDistance(float2 d) {
    return length(d);
}

// Bilinear sample from texture
inline float4 sampleBilinear(texture2d<float, access::read> tex, float2 uv, uint2 size) {
    float2 coord = clamp(uv, float2(0.0), float2(size - 1));
    int2 c0 = int2(floor(coord));
    int2 c1 = min(c0 + 1, int2(size - 1));
    float2 f = fract(coord);

    float4 p00 = tex.read(uint2(c0));
    float4 p10 = tex.read(uint2(c1.x, c0.y));
    float4 p01 = tex.read(uint2(c0.x, c1.y));
    float4 p11 = tex.read(uint2(c1));

    float4 top = mix(p00, p10, f.x);
    float4 bot = mix(p01, p11, f.x);
    return mix(top, bot, f.y);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Distortion Warp (Brown–Conrady)  ════════════════
// ═══════════════════════════════════════════════════════════════

kernel void distortionKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    if (params.distortionAmount < 0.001f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float2 d = normalizeCoord(gid, params);

    // Scale to frame
    float scaleFactor = 1.0f;
    if (params.scaleToFrame) {
        float r2 = 1.0f;
        float radial = 1.0f + params.dist_k1 * r2 + params.dist_k2 * r2 * r2 + params.dist_k3 * r2 * r2 * r2;
        scaleFactor = 1.0f / radial;
    }
    d *= scaleFactor;

    float r2 = dot(d, d);
    float r4 = r2 * r2;
    float r6 = r4 * r2;

    float k1 = params.dist_k1 * params.distortionAmount;
    float k2 = params.dist_k2 * params.distortionAmount;
    float k3 = params.dist_k3 * params.distortionAmount;
    float p1 = params.dist_p1 * params.distortionAmount;
    float p2 = params.dist_p2 * params.distortionAmount;

    float radial = 1.0f + k1 * r2 + k2 * r4 + k3 * r6;
    float2 tangential = float2(
        2.0f * p1 * d.x * d.y + p2 * (r2 + 2.0f * d.x * d.x),
        p1 * (r2 + 2.0f * d.y * d.y) + 2.0f * p2 * d.x * d.y
    );

    float2 srcCoord = (d * radial + tangential) * params.maxR + params.center;
    float4 color = sampleBilinear(inTex, srcCoord, params.imageSize);
    outTex.write(color, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Chromatic Aberration (Lateral)  ═════════════════
// ═══════════════════════════════════════════════════════════════

kernel void chromaticAberrationKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    float totalCA = params.caEdgePx * params.caAmount * params.caResScale / params.cropFactor;
    if (totalCA < 0.01f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float2 d = float2(float(gid.x) - params.center.x, float(gid.y) - params.center.y);
    float r = length(d) / params.maxR;
    float r2 = r * r;

    float rShift = totalCA * r2;
    float bShift = -totalCA * r2 * 0.7f;

    float2 n = float2(0.0f);
    if (r > 0.001f) n = normalize(d);

    float2 rCoord = float2(gid) + n * rShift;
    float2 bCoord = float2(gid) + n * bShift;

    float4 rSample = sampleBilinear(inTex, rCoord, params.imageSize);
    float4 gSample = inTex.read(gid);
    float4 bSample = sampleBilinear(inTex, bCoord, params.imageSize);

    outTex.write(float4(rSample.r, gSample.g, bSample.b, gSample.a), gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Coma  ══════════════════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void comaKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    float strength = params.comaStrength * params.comaAmount;
    if (strength < 0.001f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float2 d = float2(float(gid.x) - params.center.x, float(gid.y) - params.center.y);
    float r = length(d) / params.maxR;

    float edgeScale = 1.0f / params.cropFactor;
    float maxRadius = strength * 8.0f * params.caResScale * edgeScale;
    float falloff = pow(r, params.comaFalloff);
    float radius = maxRadius * falloff;

    if (radius < 0.5f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float2 n = (r > 0.001f) ? normalize(d) : float2(0.0f);
    float2 t = float2(-n.y, n.x); // tangential

    int steps = min(params.maxSteps, max(4, int(radius * 2)));
    float4 accum = float4(0.0f);
    float totalWeight = 0.0f;

    for (int s = 0; s < steps; ++s) {
        float st = (float(s) / max(1, steps - 1)) * 2.0f - 1.0f;
        float radialOff = st * radius;
        float tangentialOff = st * radius * (1.0f - params.comaAnisotropy) * 0.3f;

        float2 sampleCoord = float2(gid) + n * radialOff + t * tangentialOff;
        float4 sample = sampleBilinear(inTex, sampleCoord, params.imageSize);

        float w = exp(-st * st * 2.0f);
        accum += sample * w;
        totalWeight += w;
    }

    outTex.write(accum / totalWeight, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Vignette & Color Cast  ═════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void vignetteKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    float4 color = inTex.read(gid);

    if (params.vigAmount < 0.001f) {
        outTex.write(color, gid);
        return;
    }

    float2 d = float2(float(gid.x) - params.center.x, float(gid.y) - params.center.y);
    float r = length(d) / params.maxR / params.cropFactor;

    float rn = pow(r, params.vigPower);
    float stopLoss = params.vigStops * params.vigAmount * rn;
    float multiplier = pow(2.0f, -stopLoss);

    color.rgb *= multiplier;

    // Color cast
    if (abs(params.castDeltaX) > 0.0001f || abs(params.castDeltaY) > 0.0001f) {
        float castR = rn * params.vigAmount;
        color.r *= (1.0f + params.castDeltaX * castR * 50.0f);
        color.b *= (1.0f - params.castDeltaY * castR * 50.0f);
    }

    outTex.write(color, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Bloom - Highlight Extraction  ══════════════════
// ═══════════════════════════════════════════════════════════════

kernel void bloomExtractKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    if (params.bloomAmount < 0.001f || params.bloomGain < 0.001f) {
        outTex.write(float4(0.0f), gid);
        return;
    }

    float4 color = inTex.read(gid);
    float luma = dot(color.rgb, float3(0.2126f, 0.7152f, 0.0722f));

    if (luma > params.bloomThreshold) {
        float excess = (luma - params.bloomThreshold) / luma;
        outTex.write(float4(color.rgb * excess, 0.0f), gid);
    } else {
        outTex.write(float4(0.0f), gid);
    }
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Bloom - Gaussian Blur (Separable)  ═════════════
// ═══════════════════════════════════════════════════════════════

kernel void bloomBlurHKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant int&                   radius  [[buffer(0)]],
    constant uint2&                 imgSize [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= imgSize.x || gid.y >= imgSize.y) return;

    float4 accum = float4(0.0f);
    float totalWeight = 0.0f;
    float sigma = float(radius) * 0.4f;
    float invSigma2 = 1.0f / (2.0f * sigma * sigma);

    for (int kx = -radius; kx <= radius; ++kx) {
        uint sx = clamp(int(gid.x) + kx, 0, int(imgSize.x) - 1);
        float w = exp(-float(kx * kx) * invSigma2);
        accum += inTex.read(uint2(sx, gid.y)) * w;
        totalWeight += w;
    }

    outTex.write(accum / totalWeight, gid);
}

kernel void bloomBlurVKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant int&                   radius  [[buffer(0)]],
    constant uint2&                 imgSize [[buffer(1)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= imgSize.x || gid.y >= imgSize.y) return;

    float4 accum = float4(0.0f);
    float totalWeight = 0.0f;
    float sigma = float(radius) * 0.4f;
    float invSigma2 = 1.0f / (2.0f * sigma * sigma);

    for (int ky = -radius; ky <= radius; ++ky) {
        uint sy = clamp(int(gid.y) + ky, 0, int(imgSize.y) - 1);
        float w = exp(-float(ky * ky) * invSigma2);
        accum += inTex.read(uint2(gid.x, sy)) * w;
        totalWeight += w;
    }

    outTex.write(accum / totalWeight, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Bloom - Recombine  ═════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void bloomRecombineKernel(
    texture2d<float, access::read>  origTex   [[texture(0)]],
    texture2d<float, access::read>  bloomTex  [[texture(1)]],
    texture2d<float, access::write> outTex    [[texture(2)]],
    constant GlassPipelineParams&   params    [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    float4 orig = origTex.read(gid);
    float4 bloom = bloomTex.read(gid);

    float gain = params.bloomGain * params.bloomAmount;
    orig.r += bloom.r * gain * params.bloomTint.x;
    orig.g += bloom.g * gain * params.bloomTint.y;
    orig.b += bloom.b * gain * params.bloomTint.z;

    outTex.write(orig, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Fringing  ══════════════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void fringingKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;
    if (gid.x < 1 || gid.y < 1 || gid.x >= params.imageSize.x - 1 || gid.y >= params.imageSize.y - 1) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float fGain = params.fringeGain * params.fringeAmount;
    if (fGain < 0.001f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float4 color = inTex.read(gid);
    float4 left  = inTex.read(uint2(gid.x - 1, gid.y));
    float4 right = inTex.read(uint2(gid.x + 1, gid.y));
    float4 up    = inTex.read(uint2(gid.x, gid.y - 1));
    float4 down  = inTex.read(uint2(gid.x, gid.y + 1));

    float3 lumaW = float3(0.2126f, 0.7152f, 0.0722f);
    float lumaL = dot(left.rgb, lumaW);
    float lumaR = dot(right.rgb, lumaW);
    float lumaU = dot(up.rgb, lumaW);
    float lumaD = dot(down.rgb, lumaW);
    float gx = (lumaR - lumaL) * 0.5f;
    float gy = (lumaD - lumaU) * 0.5f;
    float edgeMag = sqrt(gx * gx + gy * gy);

    if (edgeMag < 0.01f) {
        outTex.write(color, gid);
        return;
    }

    float2 d = float2(float(gid.x) - params.center.x, float(gid.y) - params.center.y);
    float r = length(d) / params.maxR;
    float radialMix = mix(1.0f - params.fringeRadialBias, 1.0f, r);

    float hueRad = params.fringeHueDeg * M_PI_F / 180.0f;
    float3 fringeColor = float3(
        0.5f + 0.5f * cos(hueRad),
        0.5f + 0.5f * cos(hueRad - 2.094395f),
        0.5f + 0.5f * cos(hueRad + 2.094395f)
    );

    float fringeStrength = min(edgeMag * fGain * radialMix, 0.5f);
    color.rgb += fringeStrength * fringeColor;

    outTex.write(color, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: f-Stop Sharpener  ══════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void sharpenerKernel(
    texture2d<float, access::read>  inTex   [[texture(0)]],
    texture2d<float, access::write> outTex  [[texture(1)]],
    constant GlassPipelineParams&   params  [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;
    if (gid.x < 1 || gid.y < 1 || gid.x >= params.imageSize.x - 1 || gid.y >= params.imageSize.y - 1) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    if (abs(params.fStopSharpener) < 0.001f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float wideOpenFactor = 1.0f;
    if (params.aperture < 2.0f) {
        wideOpenFactor = mix(0.5f, 1.0f, (params.aperture - 0.7f) / 1.3f);
    }
    float sharpenAmount = params.fStopSharpener * params.sharpMicrocontrast * wideOpenFactor;

    // 3x3 local average
    float4 avg = float4(0.0f);
    for (int ky = -1; ky <= 1; ++ky) {
        for (int kx = -1; kx <= 1; ++kx) {
            avg += inTex.read(uint2(int2(gid) + int2(kx, ky)));
        }
    }
    avg /= 9.0f;

    float4 color = inTex.read(gid);
    float4 detail = color - avg;

    float3 lumaW = float3(0.2126f, 0.7152f, 0.0722f);
    float luma = dot(color.rgb, lumaW);
    float protection = (luma > 1.0f) ? 1.0f / (luma * 0.5f + 0.5f) : 1.0f;

    color.rgb += detail.rgb * sharpenAmount * protection;
    outTex.write(color, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Bokeh / DOF Blur  ══════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void bokehBlurKernel(
    texture2d<float, access::read>  inTex     [[texture(0)]],
    texture2d<float, access::read>  depthTex  [[texture(1)]],
    texture2d<float, access::write> outTex    [[texture(2)]],
    constant GlassPipelineParams&   params    [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    if (params.bokehAmount < 0.001f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    float depth = depthTex.read(gid).r;

    // Map depth to subject distance
    float subjectDist = depth * params.focusDistance * 3.0f + 0.1f;

    // Thin-lens CoC
    float f = params.focalLength * 0.001f; // mm to m
    float S1 = params.focusDistance;
    float N = params.aperture;
    float coc_mm = 0.0f;

    if (S1 > f && subjectDist > 0.0f && N > 0.0f) {
        float m = f / (S1 - f);
        coc_mm = abs(subjectDist - S1) / subjectDist * f * m / N * 1000.0f;
    }

    float blurRadius = coc_mm / params.sensorWidth * float(params.imageSize.x) * params.depthGain * params.bokehAmount;

    if (blurRadius < 0.5f) {
        outTex.write(inTex.read(gid), gid);
        return;
    }

    blurRadius = min(blurRadius, float(params.maxSteps));

    // Swirl angle
    float2 d = float2(float(gid.x) - params.center.x, float(gid.y) - params.center.y);
    float rNorm = length(d) / params.maxR;
    float swirlAngle = params.swirlAmount * params.swirlBase * rNorm * rNorm * M_PI_F;

    int samples = min(params.maxSteps, max(8, int(blurRadius * 4)));

    float4 accum = float4(0.0f);
    float totalWeight = 0.0f;
    float goldenAngle = 2.39996323f;

    for (int s = 0; s < samples; ++s) {
        float sr = sqrt((float(s) + 0.5f) / float(samples)) * blurRadius;
        float sa = float(s) * goldenAngle + swirlAngle;

        float2 offset = float2(cos(sa) * sr, sin(sa) * sr);

        if (params.anamorphicEnabled) {
            offset.y *= params.anaSqueeze * params.anaOval;
        }

        float4 sample = sampleBilinear(inTex, float2(gid) + offset, params.imageSize);
        accum += sample;
        totalWeight += 1.0f;
    }

    outTex.write(accum / totalWeight, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Master Blend  ══════════════════════════════════
// ═══════════════════════════════════════════════════════════════

kernel void masterBlendKernel(
    texture2d<float, access::read>  origTex   [[texture(0)]],
    texture2d<float, access::read>  procTex   [[texture(1)]],
    texture2d<float, access::write> outTex    [[texture(2)]],
    constant GlassPipelineParams&   params    [[buffer(0)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= params.imageSize.x || gid.y >= params.imageSize.y) return;

    float4 orig = origTex.read(gid);
    float4 proc = procTex.read(gid);

    float4 result = mix(orig, proc, params.masterBlend);
    outTex.write(result, gid);
}

// ═══════════════════════════════════════════════════════════════
// ═══  KERNEL: Color Space Conversion  ════════════════════════
// ═══════════════════════════════════════════════════════════════

// sRGB <-> Linear
inline float srgbToLinear(float v) {
    return (v <= 0.04045f) ? v / 12.92f : pow((v + 0.055f) / 1.055f, 2.4f);
}
inline float linearToSrgb(float v) {
    return (v <= 0.0031308f) ? v * 12.92f : 1.055f * pow(v, 1.0f / 2.4f) - 0.055f;
}

// ARRI LogC3 <-> Linear
inline float logcToLinear(float v) {
    const float cut = 0.010591f;
    const float A = 5.555556f, B = 0.052272f, C = 0.247190f, D = 0.385537f, E = 5.367655f, F = 0.092809f;
    if (v > E * cut + F)
        return (pow(10.0f, (v - D) / C) - B) / A;
    return (v - F) / E;
}
inline float linearToLogc(float v) {
    const float cut = 0.010591f;
    const float A = 5.555556f, B = 0.052272f, C = 0.247190f, D = 0.385537f, E = 5.367655f, F = 0.092809f;
    if (v > cut)
        return C * log10(A * v + B) + D;
    return E * v + F;
}

// Generic conversion kernel (direction: 0 = to linear, 1 = from linear)
kernel void colorConvertKernel(
    texture2d<float, access::read>  inTex     [[texture(0)]],
    texture2d<float, access::write> outTex    [[texture(1)]],
    constant int&                   csIndex   [[buffer(0)]],
    constant int&                   direction [[buffer(1)]],
    constant uint2&                 imgSize   [[buffer(2)]],
    uint2 gid [[thread_position_in_grid]])
{
    if (gid.x >= imgSize.x || gid.y >= imgSize.y) return;

    float4 color = inTex.read(gid);

    // csIndex: 0=linear, 1=sRGB, 7=LogC3 (most common in Resolve)
    if (csIndex == 0 || csIndex == 4) {
        // Linear or ACEScg — passthrough
        outTex.write(color, gid);
        return;
    }

    if (direction == 0) { // to linear
        if (csIndex == 1) { // sRGB
            color.r = srgbToLinear(color.r);
            color.g = srgbToLinear(color.g);
            color.b = srgbToLinear(color.b);
        } else if (csIndex == 7) { // LogC3
            color.r = logcToLinear(color.r);
            color.g = logcToLinear(color.g);
            color.b = logcToLinear(color.b);
        }
        // Other color spaces: add as needed
    } else { // from linear
        if (csIndex == 1) {
            color.r = linearToSrgb(color.r);
            color.g = linearToSrgb(color.g);
            color.b = linearToSrgb(color.b);
        } else if (csIndex == 7) {
            color.r = linearToLogc(color.r);
            color.g = linearToLogc(color.g);
            color.b = linearToLogc(color.b);
        }
    }

    outTex.write(color, gid);
}
