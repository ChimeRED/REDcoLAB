/*
 * REDcoLAB - diagnostic probe build
 *
 * A deliberately separate build from the shipping plug-in. It answers the
 * question we cannot answer from the outside:
 *
 *   "Did CLIP STUDIO PAINT actually load this file, and if so, what does the
 *    host it loaded it into actually provide?"
 *
 * Why this is needed: the shipping plug-in only writes a log when
 * REDCOLAB_LOG=1, and its log lines are indistinguishable from the mock-host
 * test run. The probe always logs, opens with an unmistakable banner, and
 * dumps the record/service suite so a failed real-machine test still produces
 * actionable evidence instead of silence.
 *
 * Build:  tools\build.ps1 -Probe   ->  dist\REDcoLAB_probe.dll
 * Install: tools\install_plugin.ps1 -Probe ...
 * Log:     %TEMP%\REDcoLAB_probe.log
 *
 * Behaviour: it registers as a filter and does NOT modify any pixel. It is a
 * measurement instrument, not a filter. Do not ship it as the product.
 *
 * Copyright (c) REDcoLAB contributors. Plugin code only; no CELSYS code here.
 */

#ifndef _WINDOWS
#define _WINDOWS
#endif

#include <windows.h>

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <new>

#include "TriglavPlugInSDK.h"

/* ------------------------------------------------------------------ logging */
/* The probe always logs. Never gate this behind an environment variable: the
 * whole point is to produce evidence on a machine we cannot debug directly. */

static FILE* g_probeLog = NULL;

static FILE* ProbeLog() {
    if (g_probeLog != NULL) { return g_probeLog; }

    char path[MAX_PATH] = { 0 };
    DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { std::strcpy(path, "C:\\Windows\\Temp"); }
    std::strcat(path, "\\REDcoLAB_probe.log");

    g_probeLog = std::fopen(path, "ab");
    return g_probeLog;
}

#define PROBE(...)                                                    \
    do {                                                              \
        FILE* redcolab_probe_fp = ProbeLog();                          \
        if (redcolab_probe_fp) {                                       \
            std::fprintf(redcolab_probe_fp, __VA_ARGS__);              \
            std::fflush(redcolab_probe_fp);                            \
        }                                                             \
    } while (0)

/* ------------------------------------------------------------------ host dump */

static void DumpProc(const char* label, const void* proc) {
    PROBE("    %-38s %s\n", label, (proc != NULL) ? "present" : "NULL");
}

static void DumpHost(TriglavPlugInServer* server) {
    PROBE("  hostObject = %p\n", (void*)server->hostObject);

    TriglavPlugInRecordSuite* r = &server->recordSuite;
    PROBE("  recordSuite:\n");
    DumpProc("moduleInitializeRecord", r->moduleInitializeRecord);
    DumpProc("filterInitializeRecord", r->filterInitializeRecord);
    DumpProc("filterRunRecord", r->filterRunRecord);
    DumpProc("filterActivationInitializeRecord", r->filterActivationInitializeRecord);
    DumpProc("filterActivationRunRecord", r->filterActivationRunRecord);

    TriglavPlugInServiceSuite* s = &server->serviceSuite;
    PROBE("  serviceSuite:\n");
    DumpProc("stringService", s->stringService);
    DumpProc("bitmapService", s->bitmapService);
    DumpProc("offscreenService", s->offscreenService);
    DumpProc("propertyService", s->propertyService);
    DumpProc("offscreenService2", s->offscreenService2);
    DumpProc("propertyService2", s->propertyService2);

    if (s->stringService != NULL) {
        PROBE("  stringService procs:\n");
        DumpProc("createWithAsciiStringProc", (const void*)s->stringService->createWithAsciiStringProc);
        DumpProc("createWithUnicodeStringProc", (const void*)s->stringService->createWithUnicodeStringProc);
        DumpProc("releaseProc", (const void*)s->stringService->releaseProc);
    }
    if (s->offscreenService != NULL) {
        PROBE("  offscreenService procs:\n");
        DumpProc("getRectProc", (const void*)s->offscreenService->getRectProc);
        DumpProc("getChannelOrderProc", (const void*)s->offscreenService->getChannelOrderProc);
        DumpProc("getRGBChannelIndexProc", (const void*)s->offscreenService->getRGBChannelIndexProc);
        DumpProc("getBlockRectCountProc", (const void*)s->offscreenService->getBlockRectCountProc);
        DumpProc("getBlockRectProc", (const void*)s->offscreenService->getBlockRectProc);
        DumpProc("getBlockImageProc", (const void*)s->offscreenService->getBlockImageProc);
        DumpProc("getBlockAlphaProc", (const void*)s->offscreenService->getBlockAlphaProc);
        DumpProc("getBlockSelectAreaProc", (const void*)s->offscreenService->getBlockSelectAreaProc);
    }
    if (s->propertyService != NULL) {
        PROBE("  propertyService procs:\n");
        DumpProc("createProc", (const void*)s->propertyService->createProc);
        DumpProc("addItemProc", (const void*)s->propertyService->addItemProc);
        DumpProc("setIntegerValueProc", (const void*)s->propertyService->setIntegerValueProc);
        DumpProc("releaseProc", (const void*)s->propertyService->releaseProc);
    }
    if (s->propertyService2 != NULL) {
        PROBE("  propertyService2 procs:\n");
        DumpProc("addEnumerationItemProc", (const void*)s->propertyService2->addEnumerationItemProc);
        DumpProc("setItemStoreValueProc", (const void*)s->propertyService2->setItemStoreValueProc);
    }

    if (r->filterRunRecord != NULL) {
        PROBE("  filterRunRecord procs:\n");
        DumpProc("getPropertyProc", (const void*)r->filterRunRecord->getPropertyProc);
        DumpProc("getDestinationOffscreenProc", (const void*)r->filterRunRecord->getDestinationOffscreenProc);
        DumpProc("getSourceOffscreenProc", (const void*)r->filterRunRecord->getSourceOffscreenProc);
        DumpProc("getSelectAreaRectProc", (const void*)r->filterRunRecord->getSelectAreaRectProc);
        DumpProc("hasSelectAreaOffscreenProc", (const void*)r->filterRunRecord->hasSelectAreaOffscreenProc);
        DumpProc("isAlphaLockedProc", (const void*)r->filterRunRecord->isAlphaLockedProc);
        DumpProc("processProc", (const void*)r->filterRunRecord->processProc);
        DumpProc("setProgressTotalProc", (const void*)r->filterRunRecord->setProgressTotalProc);
        DumpProc("setProgressDoneProc", (const void*)r->filterRunRecord->setProgressDoneProc);
    }
}

/* ------------------------------------------------------------------ state */

struct ProbeState {
    int filterInitCount;
};

/* ------------------------------------------------------------------ entry */

TRIGLAV_PLUGIN_DLL_EXTERN void TRIGLAV_PLUGIN_CALLBACK TriglavPluginCall(
    TriglavPlugInInt* result,
    TriglavPlugInPtr* data,
    const TriglavPlugInInt selector,
    TriglavPlugInServer* pluginServer,
    TriglavPlugInPtr reserved) {

    (void)reserved;

    if (result == NULL || pluginServer == NULL) { return; }
    *result = kTriglavPlugInCallResultFailed;

    PROBE("PROBE selector=0x%X data=%p server=%p\n",
          (int)selector, (void*)data, (void*)pluginServer);

    TriglavPlugInHostObject host = pluginServer->hostObject;
    TriglavPlugInRecordSuite* rec = &pluginServer->recordSuite;
    TriglavPlugInStringService* strSvc = pluginServer->serviceSuite.stringService;

    /* ---------------------------------------------------------- module init */
    if (selector == kTriglavPlugInSelectorModuleInitialize) {
        PROBE("PROBE === moduleInitialize ===\n");
        PROBE("  *** CSP LOADED THE FILE *** (this line only appears in a real host)\n");
        DumpHost(pluginServer);

        TriglavPlugInModuleInitializeRecord* mi = rec->moduleInitializeRecord;
        if (mi == NULL) {
            PROBE("  ABORT: moduleInitializeRecord is NULL\n");
            return;
        }

        if (mi->getHostVersionProc != NULL) {
            TriglavPlugInInt hostVersion = -1;
            TriglavPlugInInt apiResult = mi->getHostVersionProc(&hostVersion, host);
            PROBE("  hostVersion=%d apiResult=%d (we require %d)\n",
                  (int)hostVersion, (int)apiResult, (int)kTriglavPlugInNeedHostVersion);
            if (apiResult != kTriglavPlugInAPIResultSuccess ||
                hostVersion < kTriglavPlugInNeedHostVersion) {
                PROBE("  ABORT: host version unusable or too old\n");
                return;
            }
        }
        else {
            PROBE("  getHostVersionProc is NULL - proceeding anyway (probe only)\n");
        }

        if (strSvc != NULL && strSvc->createWithAsciiStringProc != NULL &&
            mi->setModuleIDProc != NULL) {
            TriglavPlugInStringObject id = NULL;
            strSvc->createWithAsciiStringProc(&id, "REDcoLAB-PROBE", 14);
            mi->setModuleIDProc(host, id);
            if (strSvc->releaseProc != NULL && id != NULL) { strSvc->releaseProc(id); }
            PROBE("  moduleID reported\n");
        }

        if (mi->setModuleKindProc != NULL) {
            mi->setModuleKindProc(host, kTriglavPlugInModuleSwitchKindFilter);
            PROBE("  moduleKind reported = 0x%X\n", (unsigned)kTriglavPlugInModuleSwitchKindFilter);
        }

        ProbeState* st = new (std::nothrow) ProbeState();
        if (st != NULL) { st->filterInitCount = 0; }
        *data = st;
        *result = kTriglavPlugInCallResultSuccess;
        PROBE("PROBE moduleInitialize done\n");
        return;
    }

    /* ------------------------------------------------------- module terminate */
    if (selector == kTriglavPlugInSelectorModuleTerminate) {
        PROBE("PROBE === moduleTerminate ===\n");
        ProbeState* st = static_cast<ProbeState*>(*data);
        delete st;
        *data = NULL;
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    /* ---------------------------------------------------------- filter init */
    if (selector == kTriglavPlugInSelectorFilterInitialize) {
        ProbeState* st = static_cast<ProbeState*>(*data);
        int attempt = (st != NULL) ? ++st->filterInitCount : 1;
        PROBE("PROBE === filterInitialize (attempt %d) ===\n", attempt);
        PROBE("  -> the host is asking us to describe a filter\n");

        TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
        if (fi == NULL) {
            PROBE("  ABORT: filter initialize record is NULL\n");
            return;
        }

        PROBE("  filterInitializeRecord procs:\n");
        DumpProc("setFilterCategoryNameProc", (const void*)fi->setFilterCategoryNameProc);
        DumpProc("setFilterNameProc", (const void*)fi->setFilterNameProc);
        DumpProc("setCanPreviewProc", (const void*)fi->setCanPreviewProc);
        DumpProc("setUseBlankImageProc", (const void*)fi->setUseBlankImageProc);
        DumpProc("setTargetKindsProc", (const void*)fi->setTargetKindsProc);
        DumpProc("setPropertyProc", (const void*)fi->setPropertyProc);
        DumpProc("setPropertyCallBackProc", (const void*)fi->setPropertyCallBackProc);

        if (strSvc == NULL || fi->setFilterCategoryNameProc == NULL ||
            fi->setFilterNameProc == NULL) {
            PROBE("  ABORT: string service or name procs missing\n");
            return;
        }

        static const char16_t kCategory[] = u"REDcoLAB PROBE";
        static const char16_t kName[] = u"probe only - no pixel changes";

        TriglavPlugInStringObject category = NULL;
        TriglavPlugInStringObject name = NULL;
        strSvc->createWithUnicodeStringProc(
            &category, reinterpret_cast<const TriglavPlugInUniChar*>(kCategory),
            static_cast<TriglavPlugInInt>((sizeof(kCategory) / sizeof(char16_t)) - 1));
        strSvc->createWithUnicodeStringProc(
            &name, reinterpret_cast<const TriglavPlugInUniChar*>(kName),
            static_cast<TriglavPlugInInt>((sizeof(kName) / sizeof(char16_t)) - 1));

        fi->setFilterCategoryNameProc(host, category, 'r');
        fi->setFilterNameProc(host, name, 'p');
        if (strSvc->releaseProc != NULL) {
            if (category != NULL) { strSvc->releaseProc(category); }
            if (name != NULL) { strSvc->releaseProc(name); }
        }
        PROBE("  category/name reported (look for 'REDcoLAB PROBE' in the Filter menu)\n");

        if (fi->setCanPreviewProc != NULL) {
            fi->setCanPreviewProc(host, kTriglavPlugInBoolTrue);
            PROBE("  canPreview reported = true\n");
        }

        if (fi->setTargetKindsProc != NULL) {
            const TriglavPlugInInt targets[] = {
                kTriglavPlugInFilterTargetKindRasterLayerRGBAlpha,
                kTriglavPlugInFilterTargetKindRasterLayerGrayAlpha,
                kTriglavPlugInFilterTargetKindRasterLayerBinarizationGrayAlpha
            };
            fi->setTargetKindsProc(host, targets,
                                   static_cast<TriglavPlugInInt>(sizeof(targets) / sizeof(targets[0])));
            PROBE("  targetKinds reported (RGB / Gray / Binarization)\n");
        }

        /* No property items on purpose: the probe must be trivially simple so
         * that a failure cannot be blamed on parameter registration. Record
         * whether a usable property service exists anyway - it is useful
         * evidence for the real product, which does need it. */
        {
            TriglavPlugInPropertyService* ps = pluginServer->serviceSuite.propertyService;
            bool usable = (ps != NULL) && (ps->createProc != NULL) &&
                          (ps->addItemProc != NULL) &&
                          (ps->setIntegerValueProc != NULL) &&
                          (ps->setIntegerDefaultValueProc != NULL) &&
                          (ps->setIntegerMinValueProc != NULL) &&
                          (ps->setIntegerMaxValueProc != NULL);
            PROBE("  property service usable for the product build: %s\n",
                  usable ? "yes" : "NO - the product build would refuse to register");
        }

        *result = kTriglavPlugInCallResultSuccess;
        PROBE("PROBE filterInitialize done\n");
        return;
    }

    /* ----------------------------------------------------- filter terminate */
    if (selector == kTriglavPlugInSelectorFilterTerminate) {
        PROBE("PROBE === filterTerminate ===\n");
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    /* ------------------------------------------------------------ filter run */
    if (selector == kTriglavPlugInSelectorFilterRun) {
        PROBE("PROBE === filterRun ===\n");
        PROBE("  *** the host actually invoked the filter ***\n");

        TriglavPlugInRecordSuite* r = rec;
        if (r->filterRunRecord != NULL) {
            DumpHost(pluginServer);

            if (r->filterRunRecord->getDestinationOffscreenProc != NULL) {
                TriglavPlugInOffscreenObject destination = NULL;
                r->filterRunRecord->getDestinationOffscreenProc(&destination, host);
                PROBE("  destination offscreen = %p\n", (void*)destination);

                TriglavPlugInOffscreenService* off =
                    pluginServer->serviceSuite.offscreenService;
                if (destination != NULL && off != NULL && off->getRectProc != NULL) {
                    TriglavPlugInRect rect;
                    std::memset(&rect, 0, sizeof(rect));
                    off->getRectProc(&rect, destination);
                    PROBE("  offscreen rect = (%d,%d)-(%d,%d)  size %dx%d\n",
                          (int)rect.left, (int)rect.top, (int)rect.right, (int)rect.bottom,
                          (int)(rect.right - rect.left), (int)(rect.bottom - rect.top));
                }
                if (destination != NULL && off != NULL && off->getChannelOrderProc != NULL) {
                    TriglavPlugInInt order = 0;
                    off->getChannelOrderProc(&order, destination);
                    PROBE("  channel order = 0x%X\n", (int)order);
                }
            }
        }

        PROBE("  leaving every pixel untouched (this build is a probe)\n");
        PROBE("PROBE filterRun done - no pixels were modified\n");
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    PROBE("PROBE unhandled selector 0x%X\n", (int)selector);
}

/* ------------------------------------------------------------------ DLL entry */

extern "C" BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(module);
        /* Log immediately on load: this line proves the DLL was mapped into a
         * process, even if the host never calls the entry point. */
        PROBE("\n=============================\n");
        PROBE("PROBE DLL_PROCESS_ATTACH at %s\n", "REDcoLAB_probe");
        PROBE("=============================\n");
    }
    return TRUE;
}
