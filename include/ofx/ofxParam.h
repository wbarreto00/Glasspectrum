#ifndef OFX_PARAM_H
#define OFX_PARAM_H

/*
 * OpenFX Parameter Suite - Vendored header for Glasspectum
 */

#include "ofxCore.h"

#ifdef __cplusplus
extern "C" {
#endif

/* === Parameter Types === */
#define kOfxParamTypeInteger "OfxParamTypeInteger"
#define kOfxParamTypeDouble "OfxParamTypeDouble"
#define kOfxParamTypeBoolean "OfxParamTypeBoolean"
#define kOfxParamTypeChoice "OfxParamTypeChoice"
#define kOfxParamTypeRGBA "OfxParamTypeRGBA"
#define kOfxParamTypeRGB "OfxParamTypeRGB"
#define kOfxParamTypeDouble2D "OfxParamTypeDouble2D"
#define kOfxParamTypeInteger2D "OfxParamTypeInteger2D"
#define kOfxParamTypeDouble3D "OfxParamTypeDouble3D"
#define kOfxParamTypeInteger3D "OfxParamTypeInteger3D"
#define kOfxParamTypeString "OfxParamTypeString"
#define kOfxParamTypeCustom "OfxParamTypeCustom"
#define kOfxParamTypePushButton "OfxParamTypePushButton"
#define kOfxParamTypeGroup "OfxParamTypeGroup"
#define kOfxParamTypePage "OfxParamTypePage"

/* === Parameter Properties === */
#define kOfxParamPropDefault "OfxParamPropDefault"
#define kOfxParamPropMin "OfxParamPropMin"
#define kOfxParamPropMax "OfxParamPropMax"
#define kOfxParamPropDisplayMin "OfxParamPropDisplayMin"
#define kOfxParamPropDisplayMax "OfxParamPropDisplayMax"
#define kOfxParamPropAnimates "OfxParamPropAnimates"
#define kOfxParamPropIsAnimating "OfxParamPropIsAnimating"
#define kOfxParamPropHint "OfxParamPropHint"
#define kOfxParamPropScriptName "OfxParamPropScriptName"
#define kOfxParamPropParent "OfxParamPropParent"
#define kOfxParamPropEnabled "OfxParamPropEnabled"
#define kOfxParamPropSecret "OfxParamPropSecret"
#define kOfxParamPropChoiceOption "OfxParamPropChoiceOption"
#define kOfxParamPropDoubleType "OfxParamPropDoubleType"
#define kOfxParamPropIncrement "OfxParamPropIncrement"
#define kOfxParamPropDigits "OfxParamPropDigits"
#define kOfxParamPropGroupOpen "OfxParamPropGroupOpen"
#define kOfxParamPropPageChild "OfxParamPropPageChild"
#define kOfxParamPropPluginMayWrite "OfxParamPropPluginMayWrite"
#define kOfxParamPropCacheInvalidation "OfxParamPropCacheInvalidation"
#define kOfxParamPropCanUndo "OfxParamPropCanUndo"
#define kOfxParamPropDataPtr "OfxParamPropDataPtr"

/* === Double Types === */
#define kOfxParamDoubleTypePlain "OfxParamDoubleTypePlain"
#define kOfxParamDoubleTypeAngle "OfxParamDoubleTypeAngle"
#define kOfxParamDoubleTypeScale "OfxParamDoubleTypeScale"
#define kOfxParamDoubleTypeTime "OfxParamDoubleTypeTime"
#define kOfxParamDoubleTypeAbsoluteTime "OfxParamDoubleTypeAbsoluteTime"
#define kOfxParamDoubleTypeX "OfxParamDoubleTypeX"
#define kOfxParamDoubleTypeXAbsolute "OfxParamDoubleTypeXAbsolute"
#define kOfxParamDoubleTypeY "OfxParamDoubleTypeY"
#define kOfxParamDoubleTypeYAbsolute "OfxParamDoubleTypeYAbsolute"
#define kOfxParamDoubleTypeXY "OfxParamDoubleTypeXY"
#define kOfxParamDoubleTypeXYAbsolute "OfxParamDoubleTypeXYAbsolute"
#define kOfxParamDoubleTypeNormalisedX "OfxParamDoubleTypeNormalisedX"
#define kOfxParamDoubleTypeNormalisedY "OfxParamDoubleTypeNormalisedY"
#define kOfxParamDoubleTypeNormalisedXAbsolute                                 \
  "OfxParamDoubleTypeNormalisedXAbsolute"
#define kOfxParamDoubleTypeNormalisedYAbsolute                                 \
  "OfxParamDoubleTypeNormalisedYAbsolute"
#define kOfxParamDoubleTypeNormalisedXY "OfxParamDoubleTypeNormalisedXY"
#define kOfxParamDoubleTypeNormalisedXYAbsolute                                \
  "OfxParamDoubleTypeNormalisedXYAbsolute"

/* === Cache Invalidation === */
#define kOfxParamInvalidateValueChange "OfxParamInvalidateValueChange"
#define kOfxParamInvalidateAll "OfxParamInvalidateAll"

/* === Parameter Suite V1 === */
typedef struct OfxParameterSuiteV1 {
  OfxStatus (*paramDefine)(OfxParamSetHandle paramSet, const char *paramType,
                           const char *name, OfxPropertySetHandle *propertySet);
  OfxStatus (*paramGetHandle)(OfxParamSetHandle paramSet, const char *name,
                              OfxParamHandle *param,
                              OfxPropertySetHandle *propertySet);
  OfxStatus (*paramSetGetPropertySet)(OfxParamSetHandle paramSet,
                                      OfxPropertySetHandle *propHandle);
  OfxStatus (*paramGetPropertySet)(OfxParamHandle param,
                                   OfxPropertySetHandle *propHandle);
  OfxStatus (*paramGetValue)(OfxParamHandle param, ...);
  OfxStatus (*paramGetValueAtTime)(OfxParamHandle param, double time, ...);
  OfxStatus (*paramGetDerivative)(OfxParamHandle param, double time, ...);
  OfxStatus (*paramGetIntegral)(OfxParamHandle param, double time1,
                                double time2, ...);
  OfxStatus (*paramSetValue)(OfxParamHandle param, ...);
  OfxStatus (*paramSetValueAtTime)(OfxParamHandle param, double time, ...);
  OfxStatus (*paramGetNumKeys)(OfxParamHandle param,
                               unsigned int *numberOfKeys);
  OfxStatus (*paramGetKeyTime)(OfxParamHandle param, unsigned int nthKey,
                               double *time);
  OfxStatus (*paramGetKeyIndex)(OfxParamHandle param, double time,
                                int direction, int *index);
  OfxStatus (*paramDeleteKey)(OfxParamHandle param, double time);
  OfxStatus (*paramDeleteAllKeys)(OfxParamHandle param);
  OfxStatus (*paramCopy)(OfxParamHandle paramTo, OfxParamHandle paramFrom,
                         double dstOffset, const OfxRangeD *frameRange);
  OfxStatus (*paramEditBegin)(OfxParamSetHandle paramSet, const char *name);
  OfxStatus (*paramEditEnd)(OfxParamSetHandle paramSet);
} OfxParameterSuiteV1;

#ifdef __cplusplus
}
#endif

#endif /* OFX_PARAM_H */
