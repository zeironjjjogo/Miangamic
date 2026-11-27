#pragma once
#include "inireader.h"

#ifdef _DEBUG
#define DBG_STATIC
int getPreffered(const wchar_t * const fileLangTag, const PCZZWSTR langsPreffered, size_t nLangs);
#else
#define DBG_STATIC static
#endif

ERRMSG InitializeI18n(void);
void UninitializeI18n(void);
const wchar_t * const GetI18nText(wchar_t *buf, size_t n_strBuf, const char * const section, const char * const key);


