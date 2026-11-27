#pragma once

char *mbsUtf82wcsSjis(const char *mbsUtf8Str);
void verboseAsSjis(const char *lpszStr, void (*log)(wchar_t *));
wchar_t *mbs2wcs(wchar_t *wcsDst, size_t dstSizeInWords, const char const *mbsSrc);
