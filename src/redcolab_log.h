/*
 * REDcoLAB - optional run log
 *
 * Writes to %TEMP%\REDcoLAB.log. Disabled unless REDCOLAB_LOG=1 is set in the
 * environment, so the shipped plugin does not create files behind the user's
 * back. This is the only diagnostic channel we have: a CSP filter runs inside
 * CSP's process and we cannot attach a debugger to a user's machine.
 *
 * Copyright (c) REDcoLAB contributors. Plugin code only; no CELSYS code here.
 */
#ifndef REDCOLAB_LOG_H
#define REDCOLAB_LOG_H

#include <cstdio>
#include <cstdlib>
#include <cstring>

#ifdef _WINDOWS
#include <windows.h>
#endif

namespace redcolab {

/* Returns true when logging is enabled via the REDCOLAB_LOG environment variable. */
inline bool LogEnabled() {
    static int cached = -1;
    if (cached < 0) {
        char buf[8] = { 0 };
        DWORD n = GetEnvironmentVariableA("REDCOLAB_LOG", buf, sizeof(buf));
        cached = (n > 0 && n < sizeof(buf) && buf[0] == '1') ? 1 : 0;
    }
    return cached == 1;
}

inline FILE* LogFile() {
    static FILE* fp = NULL;
    static bool tried = false;
    if (!tried) {
        tried = true;
        if (LogEnabled()) {
            char path[MAX_PATH] = { 0 };
            DWORD n = GetEnvironmentVariableA("TEMP", path, MAX_PATH);
            if (n == 0 || n >= MAX_PATH) {
                std::strcpy(path, "C:\\Windows\\Temp");
            }
            std::strcat(path, "\\REDcoLAB.log");
            fp = std::fopen(path, "ab");
        }
    }
    return fp;
}

} /* namespace redcolab */

#define REDCOLAB_LOG(...)                                            \
    do {                                                             \
        if (redcolab::LogEnabled()) {                                 \
            FILE* redcolab_fp = redcolab::LogFile();                   \
            if (redcolab_fp) {                                        \
                std::fprintf(redcolab_fp, __VA_ARGS__);                \
                std::fflush(redcolab_fp);                              \
            }                                                        \
        }                                                            \
    } while (0)

#endif /* REDCOLAB_LOG_H */
