/*
 * 红红子LAB / REDcoLAB - CLIP STUDIO PAINT filter plug-in
 *
 * Built against the CELSYS Triglav Plug-In SDK (filter plug-in kind).
 * Export surface is exactly one symbol: TriglavPluginCall.
 *
 * What this plug-in can do (everything the SDK actually offers):
 *   - module init/terminate, reporting moduleID and moduleKind = Filter
 *   - filter init: category name, filter name, live preview flag,
 *     accepted target layer kinds, and the property (parameter) items
 *   - filter run: read the source/destination offscreen bitmaps, the
 *     selection rectangle, the selection mask, and write pixels back
 *
 * What this plug-in deliberately does NOT do:
 *   - no screen capture of any kind (forbidden by project rules)
 *   - no layer enumeration, no clipping-mask handling, no dockable panel,
 *     no shortcut registration: the SDK exposes none of these
 *
 * Copyright (c) REDcoLAB contributors. Plugin code only; no CELSYS code here.
 */

#ifndef _WINDOWS
#define _WINDOWS
#endif

#include <windows.h>

#include <cstring>
#include <new>
#include <vector>

#include "TriglavPlugInSDK.h"

#include "grayscale.h"
#include "redcolab_log.h"

/* ------------------------------------------------------------------ identity */

static const char* const kModuleID = "B7F3E2A1-9C4D-4E6B-8A15-2D3F7C9B1E40";
static const char* const kVersionString = "1.0.0";

/* UI strings. These are passed to the host as UTF-16, so they display in CSP's
 * own font. Note: U+00B7 (middle dot) is banned in visible UI text project-wide. */
static const char16_t kTextCategory[]   = u"红红子LAB";
static const char16_t kTextFilterName[] = u"一键灰度";

static const char16_t kTextAlgorithm[] = u"灰度算法";
static const char16_t kAlgoRec709[]    = u"Rec.709 加权";
static const char16_t kAlgoRec601[]    = u"Rec.601 加权";
static const char16_t kAlgoAverage[]   = u"平均值";
static const char16_t kAlgoMax[]       = u"明度最大";
static const char16_t kAlgoMin[]       = u"明度最小";
static const char16_t kAlgoRed[]       = u"单通道 红";
static const char16_t kAlgoGreen[]     = u"单通道 绿";
static const char16_t kAlgoBlue[]      = u"单通道 蓝";

static const char16_t kTextStrength[] = u"强度";

/* length helper: must be used where the array is still in scope, because
 * sizeof() decays to a pointer size once the array is passed to a function */
#define U16LEN(a) static_cast<TriglavPlugInInt>((sizeof(a) / sizeof(char16_t)) - 1)

enum {
    kRedcoLogInit = 0  /* keep the enum non-empty for older compilers */
};

/* ------------------------------------------------------------------ state */

struct RedcoState {
    TriglavPlugInInt algorithm;
    TriglavPlugInInt strength;

    /* services are handed to us by the host on each call; we keep the last
     * known pointers only so the property callback can read/write values */
    TriglavPlugInPropertyService* propertyService;
    TriglavPlugInPropertyService2* propertyService2;
};

static RedcoState* NewState() {
    RedcoState* st = new (std::nothrow) RedcoState();
    if (st != NULL) {
        st->algorithm = redcolab::kAlgoRec709;
        st->strength = 100;
        st->propertyService = NULL;
        st->propertyService2 = NULL;
    }
    return st;
}

/* ------------------------------------------------------------------ helpers */

static TriglavPlugInStringObject MakeUnicodeString(TriglavPlugInStringService* svc,
                                                   const char16_t* text,
                                                   TriglavPlugInInt length) {
    if (svc == NULL || svc->createWithUnicodeStringProc == NULL) { return NULL; }
    TriglavPlugInStringObject obj = NULL;
    svc->createWithUnicodeStringProc(&obj,
                                     reinterpret_cast<const TriglavPlugInUniChar*>(text),
                                     length);
    return obj;
}

#define MAKESTR(svc, arr) MakeUnicodeString((svc), (arr), U16LEN(arr))

static void ReleaseString(TriglavPlugInStringService* svc, TriglavPlugInStringObject obj) {
    if (svc != NULL && obj != NULL && svc->releaseProc != NULL) { svc->releaseProc(obj); }
}

/* ------------------------------------------------------------------ host call guards
 *
 * The host fills the record/service suites with function pointers, but not
 * every entry is guaranteed to be present: it depends on the CSP version and
 * on what the host chose to expose. Calling a NULL entry crashes CSP inside
 * the user's process, so every host call goes through one of these guards.
 * When a pointer is missing we fall back to a safe default and log it.
 */

static inline bool HostHasProc(const void* proc) {
    return proc != NULL;
}

static inline void CallGetProperty(TriglavPlugInRecordSuite* rec, TriglavPlugInPropertyObject* out,
                                   TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->getPropertyProc != NULL) {
        TriglavPlugInFilterRunGetProperty(rec, out, host);
    }
}

static inline void CallGetDestination(TriglavPlugInRecordSuite* rec,
                                      TriglavPlugInOffscreenObject* out,
                                      TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->getDestinationOffscreenProc != NULL) {
        TriglavPlugInFilterRunGetDestinationOffscreen(rec, out, host);
    }
}

static inline void CallGetSelectAreaRect(TriglavPlugInRecordSuite* rec, TriglavPlugInRect* out,
                                         TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->getSelectAreaRectProc != NULL) {
        TriglavPlugInFilterRunGetSelectAreaRect(rec, out, host);
    }
}

static inline void CallHasSelectArea(TriglavPlugInRecordSuite* rec, TriglavPlugInBool* out,
                                     TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->hasSelectAreaOffscreenProc != NULL) {
        TriglavPlugInFilterRunHasSelectAreaOffscreen(rec, out, host);
    }
}

static inline void CallGetSelectAreaOffscreen(TriglavPlugInRecordSuite* rec,
                                              TriglavPlugInOffscreenObject* out,
                                              TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->getSelectAreaOffscreenProc != NULL) {
        TriglavPlugInFilterRunGetSelectAreaOffscreen(rec, out, host);
    }
}

static inline void CallIsAlphaLocked(TriglavPlugInRecordSuite* rec, TriglavPlugInBool* out,
                                     TriglavPlugInHostObject host) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->isAlphaLockedProc != NULL) {
        TriglavPlugInFilterRunIsAlphaLocked(rec, out, host);
    }
}

static inline void CallUpdateDestinationRect(TriglavPlugInRecordSuite* rec,
                                             TriglavPlugInHostObject host,
                                             const TriglavPlugInRect* rect) {
    if (rec->filterRunRecord != NULL &&
        rec->filterRunRecord->updateDestinationOffscreenRectProc != NULL) {
        TriglavPlugInFilterRunUpdateDestinationOffscreenRect(rec, host, rect);
    }
}

static inline void CallSetProgressTotal(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                        TriglavPlugInInt total) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->setProgressTotalProc != NULL) {
        TriglavPlugInFilterRunSetProgressTotal(rec, host, total);
    }
}

static inline void CallSetProgressDone(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                       TriglavPlugInInt done) {
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->setProgressDoneProc != NULL) {
        TriglavPlugInFilterRunSetProgressDone(rec, host, done);
    }
}

/* Returns the host's process result; kTriglavPlugInFilterRunProcessResultExit
 * is the safe fallback when the host did not expose processProc at all. */
static inline TriglavPlugInInt CallProcess(TriglavPlugInRecordSuite* rec,
                                           TriglavPlugInHostObject host,
                                           TriglavPlugInInt processState) {
    TriglavPlugInInt processResult = kTriglavPlugInFilterRunProcessResultExit;
    if (rec->filterRunRecord != NULL && rec->filterRunRecord->processProc != NULL) {
        TriglavPlugInFilterRunProcess(rec, &processResult, host, processState);
    }
    return processResult;
}

/* ---------------- property service guards ----------------
 *
 * The property service is how parameters are declared, so its procs are
 * needed rather than optional - but they can still be absent on a host that
 * chose not to expose them, and a call through a NULL pointer takes CSP down
 * with it. Registration therefore fails cleanly instead of crashing, and the
 * caller reports it.
 */

static inline bool PropertyServiceUsable(TriglavPlugInPropertyService* ps) {
    return ps != NULL &&
           ps->createProc != NULL &&
           ps->addItemProc != NULL &&
           ps->setIntegerValueProc != NULL &&
           ps->setIntegerDefaultValueProc != NULL &&
           ps->setIntegerMinValueProc != NULL &&
           ps->setIntegerMaxValueProc != NULL;
}

static inline void CallPropertyAddItem(TriglavPlugInPropertyService* ps,
                                       TriglavPlugInPropertyObject prop, TriglavPlugInInt key,
                                       TriglavPlugInInt valueType, TriglavPlugInInt valueKind,
                                       TriglavPlugInInt inputKind, TriglavPlugInStringObject caption,
                                       TriglavPlugInChar accessKey) {
    if (ps != NULL && ps->addItemProc != NULL) {
        ps->addItemProc(prop, key, valueType, valueKind, inputKind, caption, accessKey);
    }
}

static inline void CallPropertySetInteger(TriglavPlugInPropertyService* ps,
                                          TriglavPlugInPropertyObject prop,
                                          TriglavPlugInInt key, TriglavPlugInInt value) {
    if (ps != NULL && ps->setIntegerValueProc != NULL) {
        ps->setIntegerValueProc(prop, key, value);
    }
}

static inline void CallPropertySetIntegerDefault(TriglavPlugInPropertyService* ps,
                                                 TriglavPlugInPropertyObject prop,
                                                 TriglavPlugInInt key, TriglavPlugInInt value) {
    if (ps != NULL && ps->setIntegerDefaultValueProc != NULL) {
        ps->setIntegerDefaultValueProc(prop, key, value);
    }
}

static inline void CallPropertySetIntegerRange(TriglavPlugInPropertyService* ps,
                                               TriglavPlugInPropertyObject prop,
                                               TriglavPlugInInt key,
                                               TriglavPlugInInt minValue, TriglavPlugInInt maxValue) {
    if (ps != NULL && ps->setIntegerMinValueProc != NULL) {
        ps->setIntegerMinValueProc(prop, key, minValue);
    }
    if (ps != NULL && ps->setIntegerMaxValueProc != NULL) {
        ps->setIntegerMaxValueProc(prop, key, maxValue);
    }
}

static inline void CallPropertyRelease(TriglavPlugInPropertyService* ps,
                                       TriglavPlugInPropertyObject prop) {
    if (ps != NULL && ps->releaseProc != NULL) {
        ps->releaseProc(prop);
    }
}

static inline void CallPropertyAddEnumerationItem(TriglavPlugInPropertyService2* ps2,
                                                  TriglavPlugInPropertyObject prop,
                                                  TriglavPlugInInt key, TriglavPlugInInt value,
                                                  TriglavPlugInStringObject caption,
                                                  TriglavPlugInChar accessKey) {
    if (ps2 != NULL && ps2->addEnumerationItemProc != NULL) {
        ps2->addEnumerationItemProc(prop, key, value, caption, accessKey);
    }
}

static inline void CallPropertySetEnumeration(TriglavPlugInPropertyService2* ps2,
                                              TriglavPlugInPropertyObject prop,
                                              TriglavPlugInInt key, TriglavPlugInInt value) {
    if (ps2 != NULL && ps2->setEnumerationValueProc != NULL) {
        ps2->setEnumerationValueProc(prop, key, value);
    }
}

static inline void CallPropertySetEnumerationDefault(TriglavPlugInPropertyService2* ps2,
                                                     TriglavPlugInPropertyObject prop,
                                                     TriglavPlugInInt key, TriglavPlugInInt value) {
    if (ps2 != NULL && ps2->setEnumerationDefaultValueProc != NULL) {
        ps2->setEnumerationDefaultValueProc(prop, key, value);
    }
}

static inline void CallPropertyStoreValue(TriglavPlugInPropertyService2* ps2,
                                          TriglavPlugInPropertyObject prop,
                                          TriglavPlugInInt key, TriglavPlugInBool store) {
    if (ps2 != NULL && ps2->setItemStoreValueProc != NULL) {
        ps2->setItemStoreValueProc(prop, key, store);
    }
}

/* ---------------- filter initialize record guards ---------------- */

static inline void CallSetFilterCategory(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                         TriglavPlugInStringObject name, TriglavPlugInChar key) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setFilterCategoryNameProc != NULL) {
        TriglavPlugInFilterInitializeSetFilterCategoryName(rec, host, name, key);
    }
}

static inline void CallSetFilterName(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                     TriglavPlugInStringObject name, TriglavPlugInChar key) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setFilterNameProc != NULL) {
        TriglavPlugInFilterInitializeSetFilterName(rec, host, name, key);
    }
}

static inline void CallSetCanPreview(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                     TriglavPlugInBool canPreview) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setCanPreviewProc != NULL) {
        TriglavPlugInFilterInitializeSetCanPreview(rec, host, canPreview);
    }
}

static inline void CallSetTargetKinds(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                      const TriglavPlugInInt* kinds, TriglavPlugInInt count) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setTargetKindsProc != NULL) {
        TriglavPlugInFilterInitializeSetTargetKinds(rec, host, kinds, count);
    }
}

static inline void CallSetProperty(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                   TriglavPlugInPropertyObject prop) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setPropertyProc != NULL) {
        TriglavPlugInFilterInitializeSetProperty(rec, host, prop);
    }
}

static inline void CallSetPropertyCallback(TriglavPlugInRecordSuite* rec, TriglavPlugInHostObject host,
                                           TriglavPlugInPropertyCallBackProc callback,
                                           TriglavPlugInPtr data) {
    TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
    if (fi != NULL && fi->setPropertyCallBackProc != NULL) {
        TriglavPlugInFilterInitializeSetPropertyCallBack(rec, host, callback, data);
    }
}

/* ------------------------------------------------------------------ property callback */

static void TRIGLAV_PLUGIN_CALLBACK RedcoPropertyCallback(TriglavPlugInInt* result,
                                                          TriglavPlugInPropertyObject propertyObject,
                                                          const TriglavPlugInInt itemKey,
                                                          const TriglavPlugInInt notify,
                                                          TriglavPlugInPtr data) {
    if (result != NULL) { *result = kTriglavPlugInPropertyCallBackResultNoModify; }

    RedcoState* st = static_cast<RedcoState*>(data);
    if (st == NULL || st->propertyService == NULL) { return; }

    TriglavPlugInPropertyService* ps = st->propertyService;
    TriglavPlugInPropertyService2* ps2 = st->propertyService2;

    if (notify != kTriglavPlugInPropertyCallBackNotifyValueChanged) { return; }

    /* Mirror the host's current values into our state so the next run uses them. */
    if (ps2 != NULL && ps2->getEnumerationValueProc != NULL) {
        TriglavPlugInInt alg = redcolab::kAlgoRec709;
        ps2->getEnumerationValueProc(&alg, propertyObject, redcolab::kItemAlgorithm);
        st->algorithm = alg;
    }
    if (ps->getIntegerValueProc != NULL) {
        TriglavPlugInInt strength = 100;
        ps->getIntegerValueProc(&strength, propertyObject, redcolab::kItemStrength);
        st->strength = strength;
    }

    REDCOLAB_LOG("[property] changed itemKey=%d algorithm=%d strength=%d\n",
                 (int)itemKey, (int)st->algorithm, (int)st->strength);
}

/* ------------------------------------------------------------------ entry point */

TRIGLAV_PLUGIN_DLL_EXTERN void TRIGLAV_PLUGIN_CALLBACK TriglavPluginCall(
    TriglavPlugInInt* result,
    TriglavPlugInPtr* data,
    const TriglavPlugInInt selector,
    TriglavPlugInServer* pluginServer,
    TriglavPlugInPtr reserved) {

    (void)reserved;

    if (result == NULL || pluginServer == NULL) { return; }
    *result = kTriglavPlugInCallResultFailed;

    TriglavPlugInHostObject host = pluginServer->hostObject;
    TriglavPlugInRecordSuite* rec = &pluginServer->recordSuite;
    TriglavPlugInStringService* strSvc = pluginServer->serviceSuite.stringService;
    TriglavPlugInPropertyService* propSvc = pluginServer->serviceSuite.propertyService;
    TriglavPlugInPropertyService2* propSvc2 = pluginServer->serviceSuite.propertyService2;
    TriglavPlugInOffscreenService* offSvc = pluginServer->serviceSuite.offscreenService;

    /* ---------------------------------------------------------- module init */
    if (selector == kTriglavPlugInSelectorModuleInitialize) {
        TriglavPlugInModuleInitializeRecord* mi = rec->moduleInitializeRecord;
        if (mi == NULL || strSvc == NULL) {
            REDCOLAB_LOG("[init] FAILED: moduleInitializeRecord=%p stringService=%p\n",
                         (void*)mi, (void*)strSvc);
            return;
        }

        TriglavPlugInInt hostVersion = 0;
        TriglavPlugInInt versionResult = kTriglavPlugInAPIResultFailed;
        if (mi->getHostVersionProc != NULL) {
            versionResult = mi->getHostVersionProc(&hostVersion, host);
        }
        REDCOLAB_LOG("[init] hostVersion=%d (apiResult=%d) required=%d\n",
                     (int)hostVersion, (int)versionResult, (int)kTriglavPlugInNeedHostVersion);

        if (versionResult != kTriglavPlugInAPIResultSuccess ||
            hostVersion < kTriglavPlugInNeedHostVersion) {
            REDCOLAB_LOG("[init] host version unusable or too old, refusing to load\n");
            return;
        }

        TriglavPlugInStringObject moduleID = NULL;
        if (strSvc->createWithAsciiStringProc != NULL) {
            strSvc->createWithAsciiStringProc(&moduleID, kModuleID,
                                              static_cast<TriglavPlugInInt>(std::strlen(kModuleID)));
        }
        if (mi->setModuleIDProc != NULL) { mi->setModuleIDProc(host, moduleID); }
        if (mi->setModuleKindProc != NULL) {
            mi->setModuleKindProc(host, kTriglavPlugInModuleSwitchKindFilter);
        }
        ReleaseString(strSvc, moduleID);

        RedcoState* st = NewState();
        if (st == NULL) { return; }
        *data = st;
        *result = kTriglavPlugInCallResultSuccess;
        REDCOLAB_LOG("[init] module registered, version=%s, moduleKind=0x%X\n",
                     kVersionString, (unsigned)kTriglavPlugInModuleSwitchKindFilter);
        return;
    }

    /* ------------------------------------------------------- module terminate */
    if (selector == kTriglavPlugInSelectorModuleTerminate) {
        RedcoState* st = static_cast<RedcoState*>(*data);
        delete st;
        *data = NULL;
        *result = kTriglavPlugInCallResultSuccess;
        REDCOLAB_LOG("[terminate] module released\n");
        return;
    }

    /* ---------------------------------------------------------- filter init */
    if (selector == kTriglavPlugInSelectorFilterInitialize) {
        if (TriglavPlugInGetFilterInitializeRecord(rec) == NULL || strSvc == NULL ||
            !PropertyServiceUsable(propSvc)) {
            REDCOLAB_LOG("[filterInit] FAILED: record=%p stringService=%p propertyService=%p "
                         "(property service must expose create/addItem/setInteger*)\n",
                         (void*)TriglavPlugInGetFilterInitializeRecord(rec), (void*)strSvc, (void*)propSvc);
            return;
        }

        /* menu entry: Filter > 红红子LAB > 一键灰度 */
        TriglavPlugInStringObject category = MAKESTR(strSvc, kTextCategory);
        TriglavPlugInStringObject filterName = MAKESTR(strSvc, kTextFilterName);
        CallSetFilterCategory(rec, host, category, 'r');
        CallSetFilterName(rec, host, filterName, 'g');
        ReleaseString(strSvc, category);
        ReleaseString(strSvc, filterName);

        /* allow CSP's live preview while the user drags a slider */
        CallSetCanPreview(rec, host, kTriglavPlugInBoolTrue);

        /* Only raster layer kinds are accepted. Anything else (vector, text,
         * folder, 3D, frame border, ...) is filtered out by the host, which is
         * the honest way to express "not supported" - we cannot show our own
         * error dialog, and we must not silently do nothing. */
        const TriglavPlugInInt targets[] = {
            kTriglavPlugInFilterTargetKindRasterLayerRGBAlpha,
            kTriglavPlugInFilterTargetKindRasterLayerGrayAlpha,
            kTriglavPlugInFilterTargetKindRasterLayerBinarizationGrayAlpha
        };
        CallSetTargetKinds(rec, host, targets,
                           static_cast<TriglavPlugInInt>(sizeof(targets) / sizeof(targets[0])));

        /* ---- parameter items ---- */
        TriglavPlugInPropertyObject prop = NULL;
        if (propSvc->createProc(&prop) != kTriglavPlugInAPIResultSuccess || prop == NULL) {
            REDCOLAB_LOG("[filterInit] FAILED: could not create property object\n");
            return;
        }

        /* algorithm (enumeration) */
        TriglavPlugInStringObject capAlgorithm = MAKESTR(strSvc, kTextAlgorithm);
        CallPropertyAddItem(propSvc, prop, redcolab::kItemAlgorithm,
                            kTriglavPlugInPropertyValueTypeEnumeration,
                            kTriglavPlugInPropertyValueKindDefault,
                            kTriglavPlugInPropertyInputKindDefault,
                            capAlgorithm, 'a');
        ReleaseString(strSvc, capAlgorithm);

        struct AlgorithmItem {
            TriglavPlugInInt value;
            const char16_t* caption;
            TriglavPlugInInt length;
            TriglavPlugInChar accessKey;
        };
        static const AlgorithmItem kAlgorithmItems[] = {
            { redcolab::kAlgoRec709,  kAlgoRec709,  U16LEN(kAlgoRec709),  '7' },
            { redcolab::kAlgoRec601,  kAlgoRec601,  U16LEN(kAlgoRec601),  '6' },
            { redcolab::kAlgoAverage, kAlgoAverage, U16LEN(kAlgoAverage), 'v' },
            { redcolab::kAlgoMax,     kAlgoMax,     U16LEN(kAlgoMax),     'x' },
            { redcolab::kAlgoMin,     kAlgoMin,     U16LEN(kAlgoMin),     'n' },
            { redcolab::kAlgoRed,     kAlgoRed,     U16LEN(kAlgoRed),     'r' },
            { redcolab::kAlgoGreen,   kAlgoGreen,   U16LEN(kAlgoGreen),   'g' },
            { redcolab::kAlgoBlue,    kAlgoBlue,    U16LEN(kAlgoBlue),    'b' }
        };
        for (size_t i = 0; i < sizeof(kAlgorithmItems) / sizeof(kAlgorithmItems[0]); ++i) {
            const AlgorithmItem& it = kAlgorithmItems[i];
            TriglavPlugInStringObject cap = MakeUnicodeString(strSvc, it.caption, it.length);
            CallPropertyAddEnumerationItem(propSvc2, prop, redcolab::kItemAlgorithm,
                                           it.value, cap, it.accessKey);
            ReleaseString(strSvc, cap);
        }
        CallPropertySetEnumeration(propSvc2, prop, redcolab::kItemAlgorithm, redcolab::kAlgoRec709);
        CallPropertySetEnumerationDefault(propSvc2, prop, redcolab::kItemAlgorithm, redcolab::kAlgoRec709);

        /* strength (integer slider) */
        TriglavPlugInStringObject capStrength = MAKESTR(strSvc, kTextStrength);
        CallPropertyAddItem(propSvc, prop, redcolab::kItemStrength,
                            kTriglavPlugInPropertyValueTypeInteger,
                            kTriglavPlugInPropertyValueKindDefault,
                            kTriglavPlugInPropertyInputKindDefault,
                            capStrength, 's');
        ReleaseString(strSvc, capStrength);
        CallPropertySetInteger(propSvc, prop, redcolab::kItemStrength, 100);
        CallPropertySetIntegerDefault(propSvc, prop, redcolab::kItemStrength, 100);
        CallPropertySetIntegerRange(propSvc, prop, redcolab::kItemStrength, 0, 100);

        /* remember the user's last settings across invocations */
        CallPropertyStoreValue(propSvc2, prop, redcolab::kItemAlgorithm, kTriglavPlugInBoolTrue);
        CallPropertyStoreValue(propSvc2, prop, redcolab::kItemStrength, kTriglavPlugInBoolTrue);

        CallSetProperty(rec, host, prop);
        CallSetPropertyCallback(rec, host, RedcoPropertyCallback, *data);
        CallPropertyRelease(propSvc, prop);

        RedcoState* st = static_cast<RedcoState*>(*data);
        if (st != NULL) {
            st->propertyService = propSvc;
            st->propertyService2 = propSvc2;
        }

        *result = kTriglavPlugInCallResultSuccess;
        REDCOLAB_LOG("[filterInit] registered filter, targets=RGB/Gray/Binarization, version=%s\n",
                     kVersionString);
        return;
    }

    /* ----------------------------------------------------- filter terminate */
    if (selector == kTriglavPlugInSelectorFilterTerminate) {
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    /* ------------------------------------------------------------ filter run */
    if (selector == kTriglavPlugInSelectorFilterRun) {
        if (TriglavPlugInGetFilterRunRecord(rec) == NULL || offSvc == NULL || propSvc == NULL) {
            REDCOLAB_LOG("[run] FAILED: runRecord=%p offscreenService=%p propertyService=%p\n",
                         (void*)TriglavPlugInGetFilterRunRecord(rec), (void*)offSvc, (void*)propSvc);
            return;
        }

        RedcoState* st = static_cast<RedcoState*>(*data);
        if (st == NULL) {
            st = NewState();
            if (st == NULL) { return; }
            *data = st;
            REDCOLAB_LOG("[run] state was NULL, created a fresh one\n");
        }
        st->propertyService = propSvc;
        st->propertyService2 = propSvc2;

        /* ---- read the parameters the host is currently showing ---- */
        TriglavPlugInPropertyObject prop = NULL;
        TriglavPlugInFilterRunGetProperty(rec, &prop, host);

        TriglavPlugInInt algorithm = redcolab::kAlgoRec709;
        TriglavPlugInInt strength = 100;
        if (prop != NULL) {
            if (propSvc2 != NULL && propSvc2->getEnumerationValueProc != NULL) {
                propSvc2->getEnumerationValueProc(&algorithm, prop, redcolab::kItemAlgorithm);
            }
            if (propSvc->getIntegerValueProc != NULL) {
                propSvc->getIntegerValueProc(&strength, prop, redcolab::kItemStrength);
            }
        }
        if (algorithm < 0 || algorithm >= redcolab::kAlgoCount) { algorithm = redcolab::kAlgoRec709; }
        if (strength < 0) { strength = 0; }
        if (strength > 100) { strength = 100; }

        REDCOLAB_LOG("[run] algorithm=%d strength=%d\n", (int)algorithm, (int)strength);

        /* ---- offscreen buffers ---- */
        TriglavPlugInOffscreenObject destination = NULL;
        TriglavPlugInFilterRunGetDestinationOffscreen(rec, &destination, host);
        REDCOLAB_LOG("[run] destination offscreen=%p\n", (void*)destination);
        if (destination == NULL) {
            REDCOLAB_LOG("[run] FAILED: destination offscreen is NULL\n");
            return;
        }

        TriglavPlugInInt channelOrder = 0;
        if (!HostHasProc((const void*)offSvc->getChannelOrderProc)) {
            REDCOLAB_LOG("[run] FAILED: host exposes no getChannelOrderProc\n");
            return;
        }
        offSvc->getChannelOrderProc(&channelOrder, destination);
        REDCOLAB_LOG("[run] channelOrder=0x%X\n", (int)channelOrder);

        TriglavPlugInInt channelIndex[3] = { 0, 1, 2 };
        TriglavPlugInInt colorChannels = 0;
        if (channelOrder == kTriglavPlugInOffscreenChannelOrderRGBAlpha) {
            if (HostHasProc((const void*)offSvc->getRGBChannelIndexProc)) {
                TriglavPlugInInt r = 0, g = 1, b = 2;
                offSvc->getRGBChannelIndexProc(&r, &g, &b, destination);
                channelIndex[0] = r; channelIndex[1] = g; channelIndex[2] = b;
            }
            colorChannels = 3;
        }
        else if (channelOrder == kTriglavPlugInOffscreenChannelOrderGrayAlpha) {
            /* already a single gray channel: nothing meaningful to convert */
            REDCOLAB_LOG("[run] source is already grayscale (channelOrder=0x%X), nothing to do\n",
                         (int)channelOrder);
            *result = kTriglavPlugInCallResultSuccess;
            return;
        }
        else if (channelOrder == kTriglavPlugInOffscreenChannelOrderBinarizationGrayAlpha) {
            REDCOLAB_LOG("[run] source is binarized (channelOrder=0x%X), nothing to do\n",
                         (int)channelOrder);
            *result = kTriglavPlugInCallResultSuccess;
            return;
        }
        else {
            /* CMYK, alpha-only, plane order, ... : report honestly and change nothing */
            REDCOLAB_LOG("[run] UNSUPPORTED channelOrder=0x%X, leaving the layer untouched\n",
                         (int)channelOrder);
            *result = kTriglavPlugInCallResultSuccess;
            return;
        }

        /* ---- region to process ---- */
        TriglavPlugInRect bounds;
        std::memset(&bounds, 0, sizeof(bounds));
        CallGetSelectAreaRect(rec, &bounds, host);

        TriglavPlugInRect destRect;
        std::memset(&destRect, 0, sizeof(destRect));
        if (HostHasProc((const void*)offSvc->getRectProc)) { offSvc->getRectProc(&destRect, destination); }

        if (bounds.right <= bounds.left || bounds.bottom <= bounds.top) {
            bounds = destRect;
            REDCOLAB_LOG("[run] no selection, using full offscreen rect (%d,%d)-(%d,%d)\n",
                         (int)bounds.left, (int)bounds.top, (int)bounds.right, (int)bounds.bottom);
        }

        TriglavPlugInBool hasSelection = kTriglavPlugInBoolFalse;
        CallHasSelectArea(rec, &hasSelection, host);
        TriglavPlugInOffscreenObject selection = NULL;
        if (hasSelection) {
            CallGetSelectAreaOffscreen(rec, &selection, host);
        }

        /* Informational only: we never touch alpha, so an alpha-locked layer is
         * still safe to convert. Some hosts do not expose this proc at all. */
        TriglavPlugInBool alphaLocked = kTriglavPlugInBoolFalse;
        CallIsAlphaLocked(rec, &alphaLocked, host);
        REDCOLAB_LOG("[run] hasSelection=%d selectionOffscreen=%p alphaLocked=%d\n",
                     (int)hasSelection, (void*)selection, (int)alphaLocked);

        /* ---- block list ---- */
        if (!HostHasProc((const void*)offSvc->getBlockRectCountProc) ||
            !HostHasProc((const void*)offSvc->getBlockRectProc) ||
            !HostHasProc((const void*)offSvc->getBlockImageProc)) {
            REDCOLAB_LOG("[run] FAILED: host exposes no offscreen block API\n");
            return;
        }

        TriglavPlugInInt blockCount = 0;
        offSvc->getBlockRectCountProc(&blockCount, destination, &bounds);
        REDCOLAB_LOG("[run] blockRectCount=%d bounds=(%d,%d)-(%d,%d)\n",
                     (int)blockCount, (int)bounds.left, (int)bounds.top,
                     (int)bounds.right, (int)bounds.bottom);

        CallSetProgressTotal(rec, host, blockCount > 0 ? blockCount : 1);

        std::vector<TriglavPlugInRect> blocks;
        if (blockCount > 0) {
            blocks.resize(static_cast<size_t>(blockCount));
            for (TriglavPlugInInt i = 0; i < blockCount; ++i) {
                std::memset(&blocks[static_cast<size_t>(i)], 0, sizeof(TriglavPlugInRect));
                offSvc->getBlockRectProc(&blocks[static_cast<size_t>(i)], i, destination, &bounds);
            }
        }

        /* ---- process loop (shape follows the official sample) ---- */
        static const unsigned char kOpaqueSelection = 0xFF;
        TriglavPlugInInt blockIndex = 0;
        bool restart = true;
        unsigned long long pixelsTouched = 0;

        while (true) {
            if (restart) {
                restart = false;
                blockIndex = 0;
                if (CallProcess(rec, host, kTriglavPlugInFilterRunProcessStateStart) ==
                    kTriglavPlugInFilterRunProcessResultExit) {
                    break;
                }
            }

            if (blockIndex < blockCount) {
                CallSetProgressDone(rec, host, blockIndex);

                const TriglavPlugInRect blockRect = blocks[static_cast<size_t>(blockIndex)];

                TriglavPlugInPoint position;
                position.x = blockRect.left;
                position.y = blockRect.top;

                /* the host may rewrite the rect we pass in, so keep the loop
                 * bounds in blockRect and hand out a disposable copy */
                TriglavPlugInRect outRect = blockRect;

                TriglavPlugInPtr imageAddress = NULL;
                TriglavPlugInPtr alphaAddress = NULL;
                TriglavPlugInPtr selectionAddress = NULL;
                TriglavPlugInInt imageRowBytes = 0, imagePixelBytes = 0;
                TriglavPlugInInt alphaRowBytes = 0, alphaPixelBytes = 0;
                TriglavPlugInInt selectionRowBytes = 0, selectionPixelBytes = 0;

                offSvc->getBlockAlphaProc(&alphaAddress, &alphaRowBytes, &alphaPixelBytes,
                                          &outRect, destination, &position);
                offSvc->getBlockImageProc(&imageAddress, &imageRowBytes, &imagePixelBytes,
                                          &outRect, destination, &position);
                if (selection != NULL && HostHasProc((const void*)offSvc->getBlockSelectAreaProc)) {
                    offSvc->getBlockSelectAreaProc(&selectionAddress, &selectionRowBytes,
                                                   &selectionPixelBytes, &outRect, selection, &position);
                }
                if (selectionAddress == NULL) {
                    selectionAddress = const_cast<unsigned char*>(&kOpaqueSelection);
                    selectionRowBytes = 0;
                    selectionPixelBytes = 0;
                }

                if (imageAddress != NULL) {
                    unsigned char* image = static_cast<unsigned char*>(imageAddress);
                    const unsigned char* alpha = static_cast<const unsigned char*>(alphaAddress);
                    const unsigned char* mask = static_cast<const unsigned char*>(selectionAddress);

                    const int rowSpan = blockRect.bottom - blockRect.top;
                    const int columnSpan = blockRect.right - blockRect.left;

                    for (int row = 0; row < rowSpan; ++row) {
                        unsigned char* imageRow = image + (imageRowBytes * row);
                        const unsigned char* maskRow = mask + (selectionRowBytes * row);
                        const unsigned char* alphaRow = (alpha != NULL)
                            ? (alpha + (alphaRowBytes * row)) : NULL;

                        for (int column = 0; column < columnSpan; ++column) {
                            const int maskValue = maskRow[selectionPixelBytes * column];
                            if (maskValue == 0) { continue; }

                            /* skip fully transparent pixels, as the official sample does */
                            unsigned char* pixel = imageRow + (imagePixelBytes * column);
                            if (alphaRow != NULL && alphaRow[alphaPixelBytes * column] == 0) { continue; }

                            unsigned char rgb[3];
                            rgb[0] = pixel[channelIndex[0]];
                            rgb[1] = pixel[channelIndex[1]];
                            rgb[2] = pixel[channelIndex[2]];

                            redcolab::ApplyPixel(rgb, algorithm, strength, false);

                            /* blend back through a soft selection edge if present */
                            if (maskValue != 0xFF) {
                                rgb[0] = static_cast<unsigned char>(
                                    redcolab::LerpU8(pixel[channelIndex[0]], rgb[0], maskValue, 255));
                                rgb[1] = static_cast<unsigned char>(
                                    redcolab::LerpU8(pixel[channelIndex[1]], rgb[1], maskValue, 255));
                                rgb[2] = static_cast<unsigned char>(
                                    redcolab::LerpU8(pixel[channelIndex[2]], rgb[2], maskValue, 255));
                            }

                            pixel[channelIndex[0]] = rgb[0];
                            pixel[channelIndex[1]] = rgb[1];
                            pixel[channelIndex[2]] = rgb[2];
                            ++pixelsTouched;
                        }
                    }
                }

                CallUpdateDestinationRect(rec, host, &blockRect);
                ++blockIndex;
            }

            TriglavPlugInInt processResult = 0;
            if (blockIndex < blockCount) {
                processResult = CallProcess(rec, host, kTriglavPlugInFilterRunProcessStateContinue);
            }
            else {
                CallSetProgressDone(rec, host, blockIndex);
                processResult = CallProcess(rec, host, kTriglavPlugInFilterRunProcessStateEnd);
            }

            if (processResult == kTriglavPlugInFilterRunProcessResultRestart) {
                restart = true;
            }
            else if (processResult == kTriglavPlugInFilterRunProcessResultExit) {
                break;
            }
        }

        REDCOLAB_LOG("[run] done, pixels processed=%llu, colorChannels=%d\n",
                     pixelsTouched, (int)colorChannels);

        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    REDCOLAB_LOG("[call] unhandled selector=0x%X\n", (int)selector);
}

/* ------------------------------------------------------------------ DLL entry */

extern "C" BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(module);
    }
    return TRUE;
}
