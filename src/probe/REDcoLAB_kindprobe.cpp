/*
 * REDcoLAB - module-kind probe
 *
 * Purpose: find out which module kind CLIP STUDIO PAINT 5.0.4 accepts.
 *
 * What we learned from the previous probe (real CSP, host version 6):
 *   - CSP DOES load a .dll placed in  ...\CLIP STUDIO PAINT\PlugIn\PAINT\
 *   - it calls TriglavPluginCall with kTriglavPlugInSelectorModuleInitialize
 *   - it supplies moduleInitializeRecord and the full service suite
 *   - but filterInitializeRecord and filterRunRecord are NULL, and CSP goes
 *     straight to moduleTerminate: it never asks us to register a filter
 *
 * So the plug-in is rejected at module-init time. The most likely cause is the
 * declared module kind. This build decides the kind AT RUNTIME by trying one
 * of three values per process, chosen from an environment variable, so that a
 * single CSP start can tell us which values CSP tolerates:
 *
 *   REDCOLAB_KIND = 0x4380  -> kTriglavPlugInModuleKindFilter
 *   REDCOLAB_KIND = 0x5530  -> kTriglavPlugInModuleKindFilterActivation
 *   REDCOLAB_KIND = 0       -> do not call setModuleKindProc at all
 *
 * Because env vars are inherited, one CSP run only tests one value. To test all
 * three in one start, this build instead registers under ALL values it can:
 * it calls setModuleKindProc once with Filter, and logs what the host answered.
 * CSP only calls ModuleInitialize a limited number of times, so the practical
 * approach is: run with one value, observe whether filterInitializeRecord
 * becomes non-NULL, then switch values.
 *
 * Log: %TEMP%\REDcoLAB_kindprobe.log  (always written)
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

static FILE* g_log = NULL;

static FILE* KindLog() {
    if (g_log != NULL) { return g_log; }
    char path[MAX_PATH] = { 0 };
    DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) { std::strcpy(path, "C:\\Windows\\Temp"); }
    std::strcat(path, "\\REDcoLAB_kindprobe.log");
    g_log = std::fopen(path, "ab");
    return g_log;
}

#define KLOG(...)                                     \
    do {                                              \
        FILE* fp = KindLog();                          \
        if (fp) { std::fprintf(fp, __VA_ARGS__); std::fflush(fp); } \
    } while (0)

static const char* KindName(TriglavPlugInInt kind) {
    if (kind == kTriglavPlugInModuleKindFilter) { return "Filter(0x4380)"; }
    if (kind == kTriglavPlugInModuleKindFilterActivation) { return "FilterActivation(0x5530)"; }
    return "other";
}

/* Returns the kind this process should declare.
 * Default: 0x4380. Override with REDCOLAB_KIND=5530 or REDCOLAB_KIND=0. */
static TriglavPlugInInt KindFromEnvironment() {
    char buf[32] = { 0 };
    DWORD n = GetEnvironmentVariableA("REDCOLAB_KIND", buf, sizeof(buf));
    if (n == 0) { return kTriglavPlugInModuleKindFilter; }

    if (std::strcmp(buf, "0") == 0) { return 0; }
    long v = std::strtol(buf, NULL, 16);
    if (v <= 0) { return kTriglavPlugInModuleKindFilter; }
    return static_cast<TriglavPlugInInt>(v);
}

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

    if (selector == kTriglavPlugInSelectorModuleInitialize) {
        TriglavPlugInModuleInitializeRecord* mi = rec->moduleInitializeRecord;
        TriglavPlugInInt wantKind = KindFromEnvironment();

        KLOG("\n===== KIND PROBE: this process declares %s =====\n", KindName(wantKind));
        KLOG("  moduleInitializeRecord = %p\n", (void*)mi);
        KLOG("  filterInitializeRecord = %p\n", (void*)rec->filterInitializeRecord);
        KLOG("  filterRunRecord        = %p\n", (void*)rec->filterRunRecord);
        KLOG("  filterActivationInitializeRecord = %p\n",
             (void*)rec->filterActivationInitializeRecord);
        KLOG("  filterActivationRunRecord        = %p\n",
             (void*)rec->filterActivationRunRecord);

        if (mi == NULL) {
            KLOG("  ABORT: no moduleInitializeRecord\n");
            return;
        }

        TriglavPlugInInt hostVersion = -1;
        if (mi->getHostVersionProc != NULL) {
            TriglavPlugInInt r = mi->getHostVersionProc(&hostVersion, host);
            KLOG("  hostVersion=%d apiResult=%d\n", (int)hostVersion, (int)r);
        }
        else {
            KLOG("  getHostVersionProc is NULL\n");
        }

        /* Always set an ID so the host has something to key on. */
        TriglavPlugInStringService* ss = pluginServer->serviceSuite.stringService;
        if (mi->setModuleIDProc != NULL && ss != NULL &&
            ss->createWithAsciiStringProc != NULL) {
            TriglavPlugInStringObject id = NULL;
            ss->createWithAsciiStringProc(&id, "REDcoLAB-KINDPROBE", 18);
            TriglavPlugInInt r = mi->setModuleIDProc(host, id);
            KLOG("  setModuleID -> %d\n", (int)r);
            if (id != NULL && ss->releaseProc != NULL) { ss->releaseProc(id); }
        }

        if (wantKind != 0) {
            if (mi->setModuleKindProc != NULL) {
                TriglavPlugInInt r = mi->setModuleKindProc(host, wantKind);
                KLOG("  setModuleKind(%s) -> %d\n", KindName(wantKind), (int)r);
            }
            else {
                KLOG("  setModuleKindProc is NULL - cannot declare a kind\n");
            }
        }
        else {
            KLOG("  REDCOLAB_KIND=0 -> not calling setModuleKind at all\n");
        }

        if (data != NULL) { *data = NULL; }
        *result = kTriglavPlugInCallResultSuccess;
        KLOG("  moduleInitialize done (result=success)\n");
        return;
    }

    if (selector == kTriglavPlugInSelectorModuleTerminate) {
        KLOG("  moduleTerminate\n");
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    if (selector == kTriglavPlugInSelectorFilterInitialize) {
        /* If we ever get here, the declared kind was ACCEPTED. */
        KLOG("  *** filterInitialize reached - the declared kind was accepted! ***\n");
        KLOG("      filterInitializeRecord = %p\n", (void*)TriglavPlugInGetFilterInitializeRecord(rec));

        TriglavPlugInFilterInitializeRecord* fi = TriglavPlugInGetFilterInitializeRecord(rec);
        if (fi != NULL && pluginServer->serviceSuite.stringService != NULL) {
            TriglavPlugInStringService* ss = pluginServer->serviceSuite.stringService;
            static const char16_t kCat[] = u"REDcoLAB KINDPROBE";
            static const char16_t kName[] = u"kind accepted";
            TriglavPlugInStringObject cat = NULL, name = NULL;
            ss->createWithUnicodeStringProc(&cat,
                reinterpret_cast<const TriglavPlugInUniChar*>(kCat),
                static_cast<TriglavPlugInInt>((sizeof(kCat) / sizeof(char16_t)) - 1));
            ss->createWithUnicodeStringProc(&name,
                reinterpret_cast<const TriglavPlugInUniChar*>(kName),
                static_cast<TriglavPlugInInt>((sizeof(kName) / sizeof(char16_t)) - 1));
            if (fi->setFilterCategoryNameProc != NULL) { fi->setFilterCategoryNameProc(host, cat, 'r'); }
            if (fi->setFilterNameProc != NULL) { fi->setFilterNameProc(host, name, 'k'); }
            if (ss->releaseProc != NULL) {
                if (cat != NULL) { ss->releaseProc(cat); }
                if (name != NULL) { ss->releaseProc(name); }
            }
            KLOG("      category/name reported\n");
        }
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    if (selector == kTriglavPlugInSelectorFilterTerminate) {
        KLOG("  filterTerminate\n");
        *result = kTriglavPlugInCallResultSuccess;
        return;
    }

    KLOG("  selector 0x%X\n", (int)selector);
}

extern "C" BOOL APIENTRY DllMain(HMODULE module, DWORD reason, LPVOID reserved) {
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH) {
        ::DisableThreadLibraryCalls(module);
        KLOG("\n--- DLL_PROCESS_ATTACH (kind probe build) ---\n");
    }
    return TRUE;
}
