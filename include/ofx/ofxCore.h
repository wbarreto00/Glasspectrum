#ifndef OFX_CORE_H
#define OFX_CORE_H

/*
 * OpenFX Core API - Minimal vendored header for Glasspectrum
 * Based on the OpenFX 1.4 specification (BSD-3-Clause licensed)
 * Only types and constants needed for a DaVinci Resolve OFX plugin.
 */

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* === Basic Types === */
typedef int OfxStatus;
typedef struct OfxPropertySetStruct* OfxPropertySetHandle;
typedef struct OfxParamSetStruct* OfxParamSetHandle;
typedef struct OfxParamStruct* OfxParamHandle;
typedef struct OfxImageEffectStruct* OfxImageEffectHandle;
typedef struct OfxImageClipStruct* OfxImageClipHandle;
typedef void* OfxPropertySuiteV1Handle;

typedef struct OfxRangeD {
    double min, max;
} OfxRangeD;

typedef struct OfxRangeI {
    int min, max;
} OfxRangeI;

typedef struct OfxRectI {
    int x1, y1, x2, y2;
} OfxRectI;

typedef struct OfxRectD {
    double x1, y1, x2, y2;
} OfxRectD;

typedef struct OfxPointD {
    double x, y;
} OfxPointD;

typedef struct OfxPointI {
    int x, y;
} OfxPointI;

/* === Status Codes === */
#define kOfxStatOK                  0
#define kOfxStatFailed              1
#define kOfxStatErrFatal            2
#define kOfxStatErrUnknown          3
#define kOfxStatErrMissingHostFeature 4
#define kOfxStatErrUnsupported      5
#define kOfxStatErrExists           6
#define kOfxStatErrFormat           7
#define kOfxStatErrMemory           8
#define kOfxStatErrBadHandle        9
#define kOfxStatErrBadIndex         10
#define kOfxStatErrValue            11
#define kOfxStatReplyDefault        14
#define kOfxStatReplyYes            15
#define kOfxStatReplyNo             16

/* === Action Strings === */
#define kOfxActionLoad                "OfxActionLoad"
#define kOfxActionUnload              "OfxActionUnload"
#define kOfxActionDescribe            "OfxActionDescribe"
#define kOfxActionCreateInstance       "OfxActionCreateInstance"
#define kOfxActionDestroyInstance      "OfxActionDestroyInstance"
#define kOfxActionBeginInstanceChanged "OfxActionBeginInstanceChanged"
#define kOfxActionInstanceChanged     "OfxActionInstanceChanged"
#define kOfxActionEndInstanceChanged  "OfxActionEndInstanceChanged"
#define kOfxActionPurgeCaches         "OfxActionPurgeCaches"
#define kOfxActionSyncPrivateData     "OfxActionSyncPrivateData"
#define kOfxActionBeginInstanceEdit   "OfxActionBeginInstanceEdit"
#define kOfxActionEndInstanceEdit     "OfxActionEndInstanceEdit"

/* === Property Names === */
#define kOfxPropType                  "OfxPropType"
#define kOfxPropName                  "OfxPropName"
#define kOfxPropLabel                 "OfxPropLabel"
#define kOfxPropShortLabel            "OfxPropShortLabel"
#define kOfxPropLongLabel             "OfxPropLongLabel"
#define kOfxPropVersion               "OfxPropVersion"
#define kOfxPropVersionLabel          "OfxPropVersionLabel"
#define kOfxPropAPIVersion            "OfxPropAPIVersion"
#define kOfxPropTime                  "OfxPropTime"
#define kOfxPropIsInteractive         "OfxPropIsInteractive"
#define kOfxPropChangeReason          "OfxPropChangeReason"
#define kOfxPropPluginDescription     "OfxPropPluginDescription"

/* === Host Descriptor === */
#define kOfxPropHostOSHandle          "OfxPropHostOSHandle"

/* === Suite Fetching === */
typedef const void* (*OfxFetchSuiteFuncV1)(OfxPropertySetHandle host, const char* suiteName, int suiteVersion);

typedef struct OfxHost {
    OfxPropertySetHandle host;
    OfxFetchSuiteFuncV1  fetchSuite;
} OfxHost;

/* === Plugin Struct === */
typedef struct OfxPlugin {
    const char*    pluginApi;
    int            apiVersion;
    const char*    pluginIdentifier;
    unsigned int   pluginVersionMajor;
    unsigned int   pluginVersionMinor;
    void           (*setHost)(OfxHost* host);
    OfxStatus      (*mainEntry)(const char* action, const void* handle,
                                OfxPropertySetHandle inArgs,
                                OfxPropertySetHandle outArgs);
} OfxPlugin;

/* === Plugin API identifier === */
#define kOfxImageEffectPluginApi       "OfxImageEffectPluginAPI"
#define kOfxImageEffectPluginApiVersion 1

/* === Standard function exported by plugin === */
#ifdef _WIN32
  #define OfxExport extern __declspec(dllexport)
#else
  #define OfxExport extern __attribute__((visibility("default")))
#endif

/* These must be implemented by the plugin */
/* OfxExport int OfxGetNumberOfPlugins(void); */
/* OfxExport OfxPlugin* OfxGetPlugin(int nth); */

#ifdef __cplusplus
}
#endif

#endif /* OFX_CORE_H */
