#include "../include/i18n.h"
#include <Windows.h>
#include <crtdbg.h>

#include <stdio.h>
#include <stdlib.h>

#include "../include/inireader.h"
#include "../include/lgr.h"
#include "../include/covenc.h"

#define ERR_HANDLED [[err_handled]]             // don't need error-handling on caller. When returned invalid values, please return to your caller.
#define ERR_NOHANDLEABLE [[err_nohandleable]]   // can't catch any errors in this function, please check some return value.
#define ERR_PLZHANDLE [[err_plzhandle]]         // This function might returns unified value, please check some return values because it is more than efficient.

INICONFIGS ini = { 0, 0, 0 };

typedef struct {
    size_t len;
    const wchar_t const *top;
} WSTRWL;  // wide string with length

typedef struct {
    WSTRWL script;
    WSTRWL region;
    WSTRWL language;
} LANGTAGINFO;

#define INIT_WSTRWL(pWstrwl) ((pWstrwl)->len = 0); ((pWstrwl)->top = NULL);
#define IS_SAME_WSTRWL(x, y) ( (x).len == (y).len && wcsncmp((x).top, (y).top, (x).len) == 0 )
#define IS_EMPTY_WSTRWL(x) ( (x).len == 0 || (x).top == NULL )

wchar_t INIFilePath[MAX_PATH] = { 0 };

static wchar_t *conv2cwstr(const WSTRWL wswl)
{
    if (IS_EMPTY_WSTRWL(wswl)) return NULL;
    wchar_t *cwstr = (wchar_t *)malloc((wswl.len + 1) * sizeof(wchar_t));
    if (cwstr == NULL) return NULL;
    errno_t err = wcsncpy_s(cwstr, wswl.len + 1, wswl.top, wswl.len);
    if (err != 0) {
        free(cwstr);
        LGR_LOGF(LGR_ERROR, L"Failed to convert from WSTRWL to wchar_t*: %s", wswl.top);
        return NULL;
    }
    cwstr[wswl.len] = L'\0';
    return cwstr;
}

static void showErrnoErr(errno_t err)
{
    LGR_LOG(LGR_ERROR, _wcserror(err));
}

static void showLastWinAPIErr()
{
    LPVOID lpMsgBuf;
    DWORD dw = GetLastError();

    if (FormatMessageW(
        FORMAT_MESSAGE_ALLOCATE_BUFFER |
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        dw,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (LPWSTR)&lpMsgBuf,
        0,
        NULL
    ) == 0)
    {
        MessageBoxW(NULL, L"Failed to format error message on initializing Miangamic.i18n.", L"ERROR", MB_OK);
        return;
    }
    MessageBoxW(NULL, (LPWSTR)lpMsgBuf, L"ERROR", MB_OK);
    LocalFree(lpMsgBuf);
}

// Returns n_str written without '\0'
ERR_HANDLED static size_t getDllFile(wchar_t *dllFilePath, size_t sizeInWords)
{
    HMODULE hMod = GetModuleHandleW(L"Miangamic.aux2");
    if (hMod == NULL)
    {
        showLastWinAPIErr();
        return 0;
    }

    DWORD len = GetModuleFileNameW(hMod, dllFilePath, sizeInWords);
    if (len == 0)
    {
        showLastWinAPIErr();
    }
    return len;
}

// Returns n_str written without '\0'
ERR_HANDLED static size_t getDllDir(wchar_t *lpwFilePath, size_t sizeInWords)
{
    if (getDllFile(lpwFilePath, sizeInWords) == 0)
    {
        return 0;
    }

    wchar_t *dirPos = wcsrchr(lpwFilePath, '\\');
    if (dirPos == NULL)
    {
        
        LGR_LOGF(LGR_ERROR, L"Could not find backslash from this dll path: %s", lpwFilePath);
        return 0;
    }
    else if (dirPos - lpwFilePath >= sizeInWords - 1)
    {
        LGR_LOGF(LGR_ERROR, L"Could find backslash from this dll path but buffer size is lacking.");
        return 0;
    }
    dirPos[1] = '\0';
    
    return (dirPos + 1 - lpwFilePath);
}

ERR_HANDLED static size_t getI18nDir(wchar_t *lpwszI18nPath, size_t sizeInWords)
{
    if (getDllDir(lpwszI18nPath, sizeInWords) == 0)
    {
        return 0;
    }

    errno_t err = wcscat_s(lpwszI18nPath, sizeInWords, L"i18n\\");
    if (err != 0)
    {
        LGR_LOG(LGR_ERROR, _wcserror(err));
        return 0;
    }

    return wcslen(lpwszI18nPath);
}

ERR_HANDLED static PZZWSTR getLanguages(PULONG pNLang, PULONG pBufSize)
{
    *pBufSize = 0;
    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, pNLang, NULL, pBufSize))
    {
        showLastWinAPIErr();
        return NULL;
    }

    PZZWSTR languages = (PZZWSTR)malloc(*pBufSize * sizeof(wchar_t));
    if (languages == NULL)
    {
        LGR_LOG(LGR_DEBUG, L"Failed to allocate memories for preffered languages.");
        return NULL;
    }

    if (!GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, pNLang, languages, pBufSize))
    {
        free(languages);
        showLastWinAPIErr();
        return NULL;
    }
    return languages;
}

ERR_NOHANDLEABLE static const wchar_t *getNextPZZWSTRElement(const wchar_t *element)
{
    while (*element != '\0') element++;
    element++;
    return element;
}

ERR_NOHANDLEABLE static int isCapital(wchar_t c)
{
    return 'A' <= c && c <= 'Z';
}

ERR_NOHANDLEABLE static int isSmall(wchar_t c)
{
    return 'a' <= c && c <= 'z';
}

ERR_NOHANDLEABLE static int isLanguage(const wchar_t * const top)
{
    return top != NULL && isSmall(top[0]) && isSmall(top[1]);
}

ERR_NOHANDLEABLE static int isScript(const wchar_t * const top)
{
    if (top == NULL) return 0;
    int ret = isCapital(top[0]);
    for (int i = 1; i < 4; i++)
    {
        ret = ret && isSmall(top[i]);
    }
    return ret;
}

ERR_NOHANDLEABLE static int isRegion(const wchar_t * const top)
{
    return top != NULL && isCapital(top[0]) && isCapital(top[1]);
}

ERR_NOHANDLEABLE static int isSameWS(const wchar_t * const x, const wchar_t * const y, size_t n)
{
    return (x != NULL && y != NULL && wcsncmp(x, y, n) == 0);
}

/*
    Expected: {
        lang
        lang-REGION
        lang-Script-REGION
    }
*/
ERR_NOHANDLEABLE static int scanLocaleStr(wchar_t* localeStr, wchar_t** language, wchar_t** script, wchar_t** region)
{
    *language = NULL;
    *script = NULL;
    *region = NULL;

    size_t len = wcslen(localeStr);
    switch (len)
    {
    case 2:
        if (isLanguage(localeStr))
        {
            *language = localeStr;
        }
        break;
    case 5:
        if (isLanguage(localeStr))
        {
            *language = localeStr;
        }
        if (isRegion(localeStr + 3))
        {
            *region = localeStr + 3;
        }
        break;
    case 10:
        if (isLanguage(localeStr))
        {
            *language = localeStr;
        }
        if (isScript(localeStr + 3))
        {
            *script = localeStr + 3;
        }
        if (isRegion(localeStr + 8))
        {
            *region = localeStr + 8;
        }
        break;
    
    default:
        break;
    }

    return (*language != NULL || *script != NULL || *region != NULL);
}

static int isLanguageN(const wchar_t * const top)
{
    int ret = top != NULL;
    if (!ret) return 0;
    
    size_t len = wcslen(top);
    if (ret = (ret && (len == 2 || len == 3)))
    {
        for (int i = 0; i < len; i++)
        {
            ret = ret && isSmall(top[i]);
        }
    }
    return ret ? len : 0;
}

static int isScriptN(const wchar_t * const top)
{
    int ret = top != NULL;
    if (!ret) return 0;

    size_t len = wcslen(top);
    if (ret = (ret && (len == 4) && isCapital(*top)))
    {
        for (int i = 1; i < len; i++)
        {
            ret = ret && isSmall(top[i]);
        }
    }
    return ret ? len : 0;
}

static int isNumber(wchar_t c)
{
    return L'0' <= c && c <= L'9';
}

static int isRegionN(const wchar_t * const top)
{
    int ret = top != NULL;
    if (!ret) return 0;

    size_t len = wcslen(top);
    if (ret = (ret && (len == 2 || len == 3)))
    {
        switch (len)
        {
        case 2:
            ret = ret && isCapital(top[0]) && isCapital(top[1]);
            break;
        case 3:
            for (int i = 0; i < len; i++)
            {
                ret = ret && isNumber(top[i]);
            }
            break;
        default:
            break;
        }
    }
    return ret ? len : 0;
}

static int scanLocaleStrNew(const wchar_t * const localeStr, LANGTAGINFO *lti)
{
    wchar_t tagBuf[16] = { 0 };

    INIT_WSTRWL(&lti->language);
    INIT_WSTRWL(&lti->script);
    INIT_WSTRWL(&lti->region);

    // first token
    const wchar_t *curTop = localeStr;
    const wchar_t *hyphen = wcschr(curTop, '-');
    size_t tokLen = hyphen ? hyphen - curTop : wcslen(curTop);

    wcsncpy_s(tagBuf, _countof(tagBuf), curTop, tokLen);
    tagBuf[tokLen] = L'\0';

    size_t len = isLanguageN(tagBuf);
    if (len == 0) return 0;

    lti->language.len = len;
    lti->language.top = curTop;

    if (hyphen == NULL) return 1;


    // second token
    curTop = hyphen + 1;
    hyphen = wcschr(curTop, '-');
    tokLen = hyphen ? hyphen - curTop : wcslen(curTop);

    tagBuf[0] = L'\0';
    wcsncpy_s(tagBuf, _countof(tagBuf), curTop, tokLen);
    tagBuf[tokLen] = L'\0';

    if ((len = isScriptN(tagBuf)) != 0)
    {
        lti->script.len = len;
        lti->script.top = curTop;
    }
    else if ((len = isRegionN(tagBuf)) != 0)
    {
        lti->region.len = len;
        lti->region.top = curTop;
        return 2;
    }

    if (hyphen == NULL) return 2;


    // third token
    curTop = hyphen + 1;
    hyphen = wcschr(curTop, '-');
    tokLen = hyphen ? hyphen - curTop : wcslen(curTop);

    tagBuf[0] = L'\0';
    wcsncpy_s(tagBuf, _countof(tagBuf), curTop, tokLen);
    tagBuf[tokLen] = L'\0';

    if (len = isRegionN(tagBuf))
    {
        lti->region.len = len;
        lti->region.top = curTop;
        return 3;
    }

    return -1;
}

/*
    This funtion is able to scan some next formats,
    n(lang) = 2 or 3
    n(Script) = 4
    n(REGION) = 2 or 3
    Expect:
        lang
        lang-Script
        lang-REGION
        lang-Script-REGION
*/
static int getLangTagInfo(const wchar_t * const localeStr, LANGTAGINFO *lti)
{
    wchar_t tagbuf[32] = { 0 };
    const wchar_t *curTop = NULL;
    const wchar_t *hyphen = NULL;
    size_t tokLen = 0, len = 0;
    errno_t err = 0;

    INIT_WSTRWL(&lti->language);
    INIT_WSTRWL(&lti->script);
    INIT_WSTRWL(&lti->region);

    for (int i = 0; i < 3; i++)
    {
        curTop = curTop == NULL ? localeStr : hyphen + 1;
        hyphen = wcschr(curTop, '-');
        tokLen = hyphen ? hyphen - curTop : wcslen(curTop);

        tagbuf[0] = L'\0';
        err = wcsncpy_s(tagbuf, _countof(tagbuf), curTop, tokLen);
        if (err != 0)
        {
            LGR_LOGF(LGR_ERROR, L"Failed to copy a tag token: \"%s\". reason: %s", curTop, _wcserror(err));
            return 0;
        }
        tagbuf[tokLen] = L'\0';

        switch (i)
        {
        case 0:
            if ((len = isLanguageN(tagbuf)) != 0)
            {
                lti->language.len = len;
                lti->language.top = curTop;
            }
            else
            {
                return 0;
            }
            if (hyphen == NULL) return 1;
            break;
        case 1:
            if ((len = isScriptN(tagbuf)) != 0)
            {
                lti->script.len = len;
                lti->script.top = curTop;
            }
            else if ((len = isRegionN(tagbuf)) != 0)
            {
                lti->region.len = len;
                lti->region.top = curTop;
                return 2;
            }
            else
            {
                return 1;
            }
            if (hyphen == NULL) return 2;
            break;
        case 2:
            if ((len = isRegionN(tagbuf)) != 0)
            {
                lti->region.len = len;
                lti->region.top = curTop;
                return 3;
            }
            return 2;
        }
    }
    return 0;
}

static int getScore(int prefferdLevel, WSTRWL *xObjectsArr, WSTRWL *yObjectsArr)
{
    static const int gains[] = { 8, 1, 2 };
    int score = 0;
    for (int i = 0; i < 3; i++)
    {
        if (xObjectsArr[i].len == yObjectsArr[i].len)
        {
            int matched = wcsncmp(xObjectsArr[i].top, yObjectsArr[i].top, xObjectsArr[i].len) == 0 ? 1 : 0;
            score += matched * gains[i] * prefferdLevel;
        }
    }
    return score;
}

ERR_NOHANDLEABLE static int calcScore(LANGTAGINFO xlti, LANGTAGINFO ylti)
{
    const size_t nObjArr = sizeof(LANGTAGINFO) / sizeof(WSTRWL);
    WSTRWL *xObjArr = (WSTRWL *)&xlti, *yObjArr = (WSTRWL *)&ylti;
    if (!IS_SAME_WSTRWL(xlti.language, ylti.language)) return 0;
    int score = 0, gain = 1;
    for (int i = 0; i < nObjArr; i++)
    {
        if (IS_SAME_WSTRWL(xObjArr[i], yObjArr[i]) && !IS_EMPTY_WSTRWL(xObjArr[i]))
        {
            score += gain;
        }
        gain = gain << 1;
    }
    return score;
}

ERR_HANDLED static wchar_t *copyLangTag(wchar_t *dstLangTag, size_t dstSizeInWords, const wchar_t * const srcFileName)
{
    const wchar_t * const firstDot = wcschr(srcFileName, '.');
    if (firstDot == NULL)
    {   
        LGR_LOGF(LGR_ERROR, L"Could not find the dot from \"%s\"", srcFileName);
        return NULL;
    }

    size_t langTagLen = firstDot - srcFileName;
    if (langTagLen > dstSizeInWords - 1)
    {
        LGR_LOGF(LGR_ERROR, L"The passed buffer size is not enough to copy a language tag.");
        return NULL;
    }

    errno_t err = wcsncpy_s(dstLangTag, dstSizeInWords, srcFileName, langTagLen);
    if (err != 0)
    {
        LGR_LOGF(LGR_ERROR, L"Failed to copy a language tag.: %hs", _wcserror(err));
        return NULL;
    }
    dstLangTag[langTagLen] = L'\0';

    return dstLangTag;
}

DBG_STATIC int getPreffered(const wchar_t * const fileLangTag, const PCZZWSTR langsPreffered, size_t nLangs)
{
    int prefLevel = 0;
    const wchar_t * lang = langsPreffered;
    LANGTAGINFO filelti = {};
    // if (scanLocaleStrNew(fileLangTag, &filelti) <= 0)
    if (getLangTagInfo(fileLangTag, &filelti) == 0)
    {
        LGR_LOGF(LGR_WARNING, L"Failed to scan a language tag from directory: %s", fileLangTag);
        return -1;
    }

    {
        wchar_t *l = conv2cwstr(filelti.language);
        wchar_t *s = conv2cwstr(filelti.script);
        wchar_t *r = conv2cwstr(filelti.region);
        LGR_LOGF(LGR_INFO, L"scaned: language: \"%s\", script: \"%s\", region: \"%s\"", l, s, r);
        if (l) free(l);
        if (s) free(s);
        if (r) free(r);
    }

    for (int i = 0; i < nLangs; i++)
    {
        int score = 0;
        LANGTAGINFO preflti = {};
        // if (scanLocaleStrNew(lang, &preflti) <= 0)
        if (getLangTagInfo(lang, &preflti) == 0)
        {
            LGR_LOGF(LGR_CRITICAL, L"Failed to scan a language tag from WINAPI: %s", lang);
            if (*(lang = getNextPZZWSTRElement(lang)) == L'\0') break;
            continue;
        }

        score = calcScore(filelti, preflti) * (nLangs - i);
        LGR_LOGF(LGR_INFO, L"FileTag: %s, UserTag: %s, Earned score: %d", fileLangTag, lang, score);
        prefLevel = score > prefLevel ? score : prefLevel;

        if (*(lang = getNextPZZWSTRElement(lang)) == L'\0') break;
    }

    return prefLevel;
}

ERR_PLZHANDLE errno_t setINIPath(const wchar_t* const lpwszDllDirPath, const wchar_t* const lpwszINIFileName)
{
    errno_t err = wcscpy_s(INIFilePath, lpwszDllDirPath);
    if (lpwszINIFileName == NULL || err != 0) return err;
    err = wcscat_s(INIFilePath, lpwszINIFileName);
    return err;
}

ERR_HANDLED static errno_t findLangFile()
{
    wchar_t lpwDirPath[MAX_PATH] = { 0 };
    wchar_t lpwQueryPath[MAX_PATH] = { 0 };
    errno_t err = 0;

    size_t nStr = getI18nDir(lpwDirPath, _countof(lpwDirPath));
    if (nStr == 0)
    {
        return 256;
    }

    err = wcscpy_s(lpwQueryPath, lpwDirPath);
    if (err != 0)
    {
        LGR_LOG(LGR_ERROR, _wcserror(err));
        return err;
    }

    err = setINIPath(lpwDirPath, L"ja-JP.Miangamic.ini");
    if (err != 0)
    {
        LGR_LOG(LGR_ERROR, _wcserror(err));
        return err;
    }

    err = wcscat_s(lpwQueryPath, L"*.Miangamic.ini");
    if (err != 0)
    {
        LGR_LOG(LGR_ERROR, _wcserror(err));
        return err;
    }

    ULONG nLangs, retSize;
    PZZWSTR languages = getLanguages(&nLangs, &retSize);    // need to deallocate
    if (languages == NULL)
    {
        return 257;
    }

    wchar_t foundFileName[MAX_PATH] = { 0 };
    int preffered = 0;
    WIN32_FIND_DATAW ffd = { 0 };
    HANDLE hFind = FindFirstFileW(lpwQueryPath, &ffd);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        free(languages);
        showLastWinAPIErr();
        return 258;
    }

    do {
        LGR_LOGF(LGR_DEBUG, L"File found.: %s", ffd.cFileName);
        if (wcscmp(ffd.cFileName, L".") == 0 || wcscmp(ffd.cFileName, L"..") == 0) continue;

        wchar_t langTagBuf[32] = { 0 };
        if (copyLangTag(langTagBuf, _countof(langTagBuf), ffd.cFileName) == NULL)
        {
            continue;
        }

        int score = getPreffered(langTagBuf, languages, nLangs);
        if (score != -1 && score > preffered)
        {
            preffered = score;
            err = wcscpy_s(foundFileName, _countof(foundFileName), ffd.cFileName);
            if (err != 0)
            {
                LGR_LOGF(LGR_ERROR, L"Failed to copy a file name: \"%s\". Reason: %s", ffd.cFileName, _wcserror(err));
                break;
            }
        }

        // wchar_t *firstDot = wcschr(ffd.cFileName, '.');
        // if (firstDot == NULL)
        // {
        //     LGR_LOGF(LGR_DEBUG, L"File found but not including dots.: %s", ffd.cFileName);
        //     continue;
        // }

        // wchar_t langTagBuf[11] = { 0 };
        // size_t lenLangTag = firstDot - ffd.cFileName;
        // if (lenLangTag > 10)
        // {
        //     LGR_LOGF(LGR_DEBUG, L"File found but not following format.: %s", ffd.cFileName);
        //     continue;
        // }
        // wcsncpy_s(langTagBuf, ffd.cFileName, lenLangTag);

        // WSTRWL tagAttrs[3] = {
        // };

        // if (scanLocaleStrNew(langTagBuf, tagAttrs, tagAttrs + 1, tagAttrs + 2) < 1)
        // {
            
        //     LGR_LOGF(LGR_DEBUG, L"Unmatched format: %s", ffd.cFileName);
        //     continue;
        // }

        // wchar_t *lang = languages;

        // for (int i = 0; i < nLangs; i++)
        // {
        //     WSTRWL attrs[3] = {};
        //     if (scanLocaleStrNew(lang, attrs, attrs + 1, attrs + 2) < 1)
        //     {
                
        //         LGR_LOGF(LGR_WARNING, L"Unknown format code found from os preffered languages: %s", lang);
        //         continue;
        //     }
        //     int score = getScore(nLangs - i, tagAttrs, attrs);
        //     LGR_LOGF(LGR_DEBUG, L"score between \"%s\" and \"%s\": %d", langTagBuf, lang, score);
        //     if (score > preffered)
        //     {
        //         preffered = score;
        //         err = wcscpy_s(foundFileName, ffd.cFileName);
        //         if (err != 0)
        //         {
        //             LGR_LOG(LGR_ERROR, _wcserror(err));
        //             break;
        //         }
        //     }
        //     lang = getNextPZZWSTRElement(lang);
        //     if (*lang == '\0') break;
        // }
        // wchar_t *fLang = NULL;
        // wchar_t *fScript = NULL;
        // wchar_t *fRegion = NULL;

        // if (!scanLocaleStr(langTagBuf, &fLang, &fScript, &fRegion))
        // {
        //     LGR_LOGF(LGR_DEBUG, L"Unmatched format: %s", ffd.cFileName);
        //     continue;
        // }

        // wchar_t *lang = languages;

        // // to get preffered level
        // for (int i = 0; i < nLangs; i++)
        // {
        //     wchar_t *l = NULL;
        //     wchar_t *s = NULL;
        //     wchar_t *r = NULL;
        //     WSTRWL larr, sarr, rarr;

        //     // if (!scanLocaleStr(lang, &l, &s, &r))
        //     if (!scanLocaleStrNew(lang, &larr, &sarr, &rarr))
        //     {
        //         LGR_LOGF(LGR_WARNING, L"Unknown format code found from os preffered languages: %s", lang);
        //         continue;
        //     }

        //     if (isSameWS(l, fLang, 2) || isSameWS(s, fScript, 4) || isSameWS(r, fRegion, 2))
        //     {
        //         if (i < preffered)
        //         {
        //             preffered = i;
        //             err = wcscpy_s(foundFileName, ffd.cFileName);
        //             if (err != 0)
        //             {
        //                 LGR_LOG(LGR_ERROR, _wcserror(err));
        //             }
        //         }
        //         break;
        //     }
        //     lang = getNextPZZWSTRElement(lang);
        //     if (*lang == '\0') break;
        // }
        
        // if (err != 0)
        // {
        //     break;
        // }
        // if (preffered == 0)
        // {
        //     break;
        // }

    } while (FindNextFileW(hFind, &ffd));

    FindClose(hFind);
    free(languages);

    if (err != 0)
    {
        return err;
    }

    if (foundFileName[0] != '\0')
    {
        err = setINIPath(lpwDirPath, foundFileName);
        if (err != 0)
        {
            LGR_LOG(LGR_ERROR, _wcserror(err));
            return err;
        }
    }
    return 0;
}

ERRMSG InitializeI18n(void)
{
    if (!IsNullINIConfigs(&ini))
    {
        return L"Already initialized.";
    }

    errno_t err = findLangFile();
    if (err != 0)
    {
        return L"Failed to find i18n files.";
    }

    FILE *fp = _wfopen(INIFilePath, L"r");
    if (fp == NULL)
    {
        return L"Failed to open the i18n file.";
    }

    ReadINI(fp, &ini);
    fclose(fp);

    if (IsNullINIConfigs(&ini))
    {
        return GetErrMsg();
    }
    return NULL;
}

void UninitializeI18n(void)
{
    FreeConfigs(&ini);
}

const wchar_t * const GetI18nText(wchar_t *buf, size_t sizeInWords, const char * const section, const char * const name)
{
    _ASSERT(ini.sections != NULL);
    if (sizeInWords >= 2)
    {
        buf[0] = L'\?';
        buf[1] = L'\0';
    }

    if (IsNullINIConfigs(&ini)) {
        LGR_LOG(LGR_ERROR, L"INI file is not initialized or failed to get.");
        return buf;
    }

    const char *val = GetValue(&ini, section, name);
    if (val == NULL) {
        LGR_LOGF(LGR_ERROR, L"\"%s\"@[%s] is not found from INI configs.", name, section);
        return buf;
    }

    if (mbs2wcs(buf, sizeInWords, val) == NULL)
    {
        LGR_LOGF(LGR_ERROR, L"Failed to convert string: %hs", val);
        return buf;
    }

    return buf;
}
