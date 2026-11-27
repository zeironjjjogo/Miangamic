#include "../include/lgr.h"

#include <stdio.h>
#include <stdarg.h>

const wchar_t const *LGR_LEVEL_STRINGFIES[] = {
    L"DEBUG",
    L"INFO",
    L"WARNING",
    L"ERROR",
    L"CRITICAL"
};

const wchar_t *lgr_format = L"[%s][%s@%d(%s)]: %s";

FILE *logfp = NULL;

static void wrappedFprintf(const wchar_t *msg)
{
    if (logfp == NULL) return;
    _fwprintf_p(logfp, L"%s\n", msg);
}

static void wrappedPrintf(const wchar_t *msg)
{
    wprintf(L"%s\n", msg);
}

int n_outputTargets = 2;
OTRG outputTargets[4] = {
    {
        LGR_DEBUG,
        &wrappedPrintf
    },
    {
        LGR_DEBUG,
        &wrappedFprintf
    }
};

int SetOutputTarget(OTRG trg)
{
    if (n_outputTargets < 4)
    {
        outputTargets[n_outputTargets] = trg;
        n_outputTargets++;
        return n_outputTargets - 1;
    }
    return -1;
}

int GetOutputTarget(int idx, OTRG *pTrg)
{
    if (idx < n_outputTargets)
    {
        *pTrg = outputTargets[idx];
        return 1;
    }
    return 0;
}

void LogMsg(LGR_LEVEL level, const wchar_t *file, int line, const wchar_t *func, const wchar_t *msg)
{
    static wchar_t msgBuf[1024];
    _snwprintf_s(msgBuf, 1024, 1023, lgr_format, LGR_LEVEL_STRINGFIES[level], file, line, func, msg);
    for (int i = 0; i < n_outputTargets; i++)
    {
        if (outputTargets[i].level <= level)
            outputTargets[i].func(msgBuf);
    }
}

void LogMsgF(LGR_LEVEL level, const wchar_t *file, int line, const wchar_t *func, const wchar_t *fmt, ...)
{
    static wchar_t msgBuf[1024];
    va_list va;
    va_start(va, fmt);
    _vsnwprintf_s(msgBuf, 1024, 1023, fmt, va);
    va_end(va);
    LogMsg(level, file, line, func, msgBuf);
}

void InitLGR(const char *logFilePath)
{
    logfp = fopen(logFilePath, "a");
}

void UninitLGR()
{
    if (logfp != NULL)
        fclose(logfp);
}
