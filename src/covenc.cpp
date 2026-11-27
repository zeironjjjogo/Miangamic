#include "../include/covenc.h"

#include <Windows.h>
#include <stdlib.h>

#include "../include/lgr.h"
#include "../include/errmsg.h"

char *mbsUtf82wcsSjis(const char *mbsUtf8Str) {
    const size_t len_mbsUtf8Str = strlen(mbsUtf8Str) + 1;

    int len_wcsUtf16str = MultiByteToWideChar(CP_UTF8, 0, mbsUtf8Str, len_mbsUtf8Str, NULL, 0) + 1;
    wchar_t *wcsUtf16str = (wchar_t *)calloc(len_wcsUtf16str, sizeof(wchar_t));
    if (wcsUtf16str == NULL) {
        return NULL;
    }

    MultiByteToWideChar(CP_UTF8, 0, mbsUtf8Str, len_mbsUtf8Str, wcsUtf16str, len_wcsUtf16str);

    int len_mbsSjisStr = WideCharToMultiByte(CP_THREAD_ACP, 0, wcsUtf16str, len_mbsUtf8Str, NULL, 0, 0, 0) + 1;
    char *mbsSjisStr = (char *)calloc(len_mbsSjisStr, sizeof(char));
    if (mbsSjisStr == NULL) {
        free(wcsUtf16str);
        return NULL;
    }

    WideCharToMultiByte(CP_THREAD_ACP, 0, wcsUtf16str, len_mbsUtf8Str, mbsSjisStr, len_mbsSjisStr, NULL, NULL);

    free(wcsUtf16str);

    return mbsSjisStr;
}

void verboseAsSjis(const char *lpszStr, void (*log)(wchar_t *)) {
    char *sjisStr = mbsUtf82wcsSjis(lpszStr);
    if (sjisStr == NULL) return;
    size_t len = strlen(sjisStr), iRet;
    wchar_t *wcsBuf = (wchar_t *)calloc(len + 1, sizeof(wchar_t));
    if (wcsBuf == NULL) {
        free(sjisStr);
        return;
    }
    mbstowcs_s(&iRet, wcsBuf, len + 1, sjisStr, len + 1);
    free(sjisStr);
    log(wcsBuf);
    free(wcsBuf);
}

wchar_t *mbs2wcs(wchar_t *wcsDst, size_t dstSizeInWords, const char const *mbsSrc)
{
    if (mbsSrc == NULL)
    {
        LGR_LOG(LGR_ERROR, L"The passed source string is null.");
        return NULL;
    }
    int cwStrLen = MultiByteToWideChar(CP_UTF8, 0, mbsSrc, -1, NULL, 0);
    if (cwStrLen <= 0) {
        showLastWINAPIError(L"Failed to get memory size for convert string from utf-8 to utf-16. Reason: %s");
        return NULL;
    }
    wchar_t *cnvBuf = (wchar_t *)malloc(cwStrLen * sizeof(wchar_t));
    if (cnvBuf == NULL)
    {
        LGR_LOGF(LGR_ERROR, L"Failed to allocate memories for conversion.");
        return NULL;
    }
    cwStrLen = MultiByteToWideChar(CP_UTF8, 0, mbsSrc, -1, cnvBuf, cwStrLen);
    if (cwStrLen <= 0)
    {
        free(cnvBuf);
        showLastWINAPIError(L"Failed to convert string from utf-8 to utf-16. Reason: %s");
        return NULL;
    }
    int cwNormedLen = NormalizeString(NormalizationC, cnvBuf, -1, NULL, 0);
    if (cwNormedLen <= 0)
    {
        free(cnvBuf);
        showLastWINAPIError(L"Failed to get memory size for normalize string. Reason: %s");
        return NULL;
    }
    else if (cwNormedLen > dstSizeInWords)
    {
        free(cnvBuf);
        LGR_LOGF(LGR_ERROR, L"The passed buffer is not enough to align text: \"%hs\".", mbsSrc);
        return NULL;
    }
    cwNormedLen = NormalizeString(NormalizationC, cnvBuf, -1, wcsDst, dstSizeInWords);
    free(cnvBuf);
    if (cwNormedLen <= 0) {
        showLastWINAPIError(L"Failed to normalize string. Reason: %s");
        return NULL;
    }
    return wcsDst;
}
