#ifndef OFX_IMAGE_EFFECT_H
#define OFX_IMAGE_EFFECT_H

/*
 * OpenFX Image Effect API - Vendored header for Glasspectrum
 * Based on the OpenFX 1.4 specification
 */

#include "ofxCore.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Image Effect Actions === */
#define kOfxImageEffectActionDescribeInContext                                 \
  "OfxImageEffectActionDescribeInContext"
#define kOfxImageEffectActionGetRegionOfDefinition                             \
  "OfxImageEffectActionGetRegionOfDefinition"
#define kOfxImageEffectActionGetRegionsOfInterest                              \
  "OfxImageEffectActionGetRegionsOfInterest"
#define kOfxImageEffectActionGetTimeDomain "OfxImageEffectActionGetTimeDomain"
#define kOfxImageEffectActionGetFramesNeeded                                   \
  "OfxImageEffectActionGetFramesNeeded"
#define kOfxImageEffectActionGetClipPreferences                                \
  "OfxImageEffectActionGetClipPreferences"
#define kOfxImageEffectActionIsIdentity "OfxImageEffectActionIsIdentity"
#define kOfxImageEffectActionRender "OfxImageEffectActionRender"
#define kOfxImageEffectActionBeginSequenceRender                               \
  "OfxImageEffectActionBeginSequenceRender"
#define kOfxImageEffectActionEndSequenceRender                                 \
  "OfxImageEffectActionEndSequenceRender"

/* === Contexts === */
#define kOfxImageEffectContextFilter "OfxImageEffectContextFilter"
#define kOfxImageEffectContextGeneral "OfxImageEffectContextGeneral"
#define kOfxImageEffectContextGenerator "OfxImageEffectContextGenerator"

/* === Clip Names === */
#define kOfxImageEffectSimpleSourceClipName "Source"
#define kOfxImageEffectOutputClipName "Output"

/* === Properties === */
#define kOfxImageEffectPropSupportedContexts                                   \
  "OfxImageEffectPropSupportedContexts"
#define kOfxImageEffectPropContext "OfxImageEffectPropContext"
#define kOfxImageEffectPropSupportsMultiResolution                             \
  "OfxImageEffectPropSupportsMultiResolution"
#define kOfxImageEffectPropSupportsTiles "OfxImageEffectPropSupportsTiles"
#define kOfxImageEffectPropSupportsMultipleClipDepths                          \
  "OfxImageEffectPropSupportsMultipleClipDepths"
#define kOfxImageEffectPropRenderScale "OfxImageEffectPropRenderScale"
#define kOfxImageEffectPropProjectSize "OfxImageEffectPropProjectSize"
#define kOfxImageEffectPropProjectOffset "OfxImageEffectPropProjectOffset"
#define kOfxImageEffectPropProjectExtent "OfxImageEffectPropProjectExtent"
#define kOfxImageEffectPropProjectPixelAspectRatio                             \
  "OfxImageEffectPropProjectPixelAspectRatio"
#define kOfxImageEffectPropPluginHandle "OfxImageEffectPropPluginHandle"
#define kOfxImageEffectPropFrameRate "OfxImageEffectPropFrameRate"
#define kOfxImageEffectPropIsBackground "OfxImageEffectPropIsBackground"
#define kOfxImageEffectPropFieldToRender "OfxImageEffectPropFieldToRender"
#define kOfxImageEffectPropRenderWindow "OfxImageEffectPropRenderWindow"
#define kOfxImageEffectPropRenderQualityDraft                                  \
  "OfxImageEffectPropRenderQualityDraft"
#define kOfxImageEffectPropSequentialRenderStatus                              \
  "OfxImageEffectPropSequentialRenderStatus"
#define kOfxImageEffectInstancePropSequentialRender                            \
  "OfxImageEffectInstancePropSequentialRender"

/* GPU Render properties */
#define kOfxImageEffectPropOpenCLRenderSupported                               \
  "OfxImageEffectPropOpenCLRenderSupported"
#define kOfxImageEffectPropCudaRenderSupported                                 \
  "OfxImageEffectPropCudaRenderSupported"
#define kOfxImageEffectPropMetalRenderSupported                                \
  "OfxImageEffectPropMetalRenderSupported"
#define kOfxImageEffectPropOpenCLEnabled "OfxImageEffectPropOpenCLEnabled"
#define kOfxImageEffectPropCudaEnabled "OfxImageEffectPropCudaEnabled"
#define kOfxImageEffectPropMetalEnabled "OfxImageEffectPropMetalEnabled"
#define kOfxImageEffectPropCudaStream "OfxImageEffectPropCudaStream"
#define kOfxImageEffectPropOpenCLCommandQueue                                  \
  "OfxImageEffectPropOpenCLCommandQueue"
#define kOfxImageEffectPropMetalCommandQueue                                   \
  "OfxImageEffectPropMetalCommandQueue"

/* === Clip Properties === */
#define kOfxImageEffectPropSupportedPixelDepths                                \
  "OfxImageEffectPropSupportedPixelDepths"
#define kOfxImageClipPropConnected "OfxImageClipPropConnected"
#define kOfxImageClipPropOptional "OfxImageClipPropOptional"
#define kOfxImageClipPropIsMask "OfxImageClipPropIsMask"
#define kOfxImagePropPixelDepth "OfxImagePropPixelDepth"
#define kOfxImagePropRowBytes "OfxImagePropRowBytes"
#define kOfxImagePropBounds "OfxImagePropBounds"
#define kOfxImagePropData "OfxImagePropData"
#define kOfxImagePropRegionOfDefinition "OfxImagePropRegionOfDefinition"
#define kOfxImageEffectPropPixelDepth "OfxImageEffectPropPixelDepth"
#define kOfxImageEffectPropComponents "OfxImageEffectPropComponents"
#define kOfxImageEffectPropPreMultiplication                                   \
  "OfxImageEffectPropPreMultiplication"

/* === Pixel Depths === */
#define kOfxBitDepthByte "OfxBitDepthByte"
#define kOfxBitDepthShort "OfxBitDepthShort"
#define kOfxBitDepthHalf "OfxBitDepthHalf"
#define kOfxBitDepthFloat "OfxBitDepthFloat"

/* === Components === */
#define kOfxImageComponentRGBA "OfxImageComponentRGBA"
#define kOfxImageComponentRGB "OfxImageComponentRGB"
#define kOfxImageComponentAlpha "OfxImageComponentAlpha"

/* === PreMultiplication === */
#define kOfxImageOpaque "OfxImageOpaque"
#define kOfxImagePreMultiplied "OfxImagePreMultiplied"
#define kOfxImageUnPreMultiplied "OfxImageUnPreMultiplied"

/* === Field Order === */
#define kOfxImageFieldNone "OfxFieldNone"
#define kOfxImageFieldBoth "OfxFieldBoth"
#define kOfxImageFieldLower "OfxFieldLower"
#define kOfxImageFieldUpper "OfxFieldUpper"

/* === Suites === */
#define kOfxImageEffectSuite "OfxImageEffectSuite"
#define kOfxPropertySuite "OfxPropertySuite"
#define kOfxParameterSuite "OfxParameterSuite"
#define kOfxMemorySuite "OfxMemorySuite"
#define kOfxMultiThreadSuite "OfxMultiThreadSuite"
#define kOfxMessageSuite "OfxMessageSuite"

/* === Image Effect Suite V1 === */
typedef struct OfxImageEffectSuiteV1 {
  OfxStatus (*getPropertySet)(OfxImageEffectHandle imageEffect,
                              OfxPropertySetHandle *propHandle);
  OfxStatus (*getParamSet)(OfxImageEffectHandle imageEffect,
                           OfxParamSetHandle *paramSet);
  OfxStatus (*clipDefine)(OfxImageEffectHandle imageEffect, const char *name,
                          OfxPropertySetHandle *propertySet);
  OfxStatus (*clipGetHandle)(OfxImageEffectHandle imageEffect, const char *name,
                             OfxImageClipHandle *clip,
                             OfxPropertySetHandle *propertySet);
  OfxStatus (*clipGetPropertySet)(OfxImageClipHandle clip,
                                  OfxPropertySetHandle *propHandle);
  OfxStatus (*clipGetImage)(OfxImageClipHandle clip, double time,
                            const OfxRectD *region,
                            OfxPropertySetHandle *imageHandle);
  OfxStatus (*clipReleaseImage)(OfxPropertySetHandle imageHandle);
  OfxStatus (*clipGetRegionOfDefinition)(OfxImageClipHandle clip, double time,
                                         OfxRectD *bounds);
  int (*abort)(OfxImageEffectHandle imageEffect);
} OfxImageEffectSuiteV1;

#ifdef __cplusplus
}
#endif

#endif /* OFX_IMAGE_EFFECT_H */
