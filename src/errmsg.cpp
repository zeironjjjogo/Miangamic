#include <Windows.h>

#include "../include/lgr.h"

void showLastWINAPIError(const wchar_t const *msgFmt)
{
    wchar_t *msgBuf = NULL;
    DWORD err = GetLastError();
    if (FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM | 
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        err,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        msgBuf,
        0,
        NULL
    ) == 0)
    {
        LGR_LOGF(LGR_ERROR, L"Failed to format WINAPI error (code: %d). << \"%s\"", err, msgFmt);
        return;
    }

    LGR_LOGF(LGR_ERROR, msgFmt, msgBuf);
    LocalFree(msgBuf);
}

void showErrnoError(errno_t err, const wchar_t const *msgFmt)
{
    LGR_LOGF(LGR_ERROR, msgFmt, _wcserror(err));
}
