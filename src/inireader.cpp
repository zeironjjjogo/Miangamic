#include "../include/inireader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define ERR_HANDLED

wchar_t errMsg[1024] = { 0 };

void SetErrMsg(const wchar_t *msg, ...)
{
    va_list va;
    va_start(va, msg);
    _vsnwprintf_s(errMsg, 1024, 1023, msg, va);
    va_end(va);
}

ERRMSG GetErrMsg()
{
    return errMsg;
}

void ClearErrMsg(void)
{
    errMsg[0] = '\0';
}

static void *resizeMem(size_t sizeofElement, size_t *nElements, void *src)
{
    size_t nSrc = *nElements;
    size_t nNew = nSrc + (nSrc >> 2);
    void *tmp = calloc(nNew, sizeofElement);
    if (tmp == NULL)
    {
        return NULL;
    }

    memcpy_s(tmp, nNew * sizeofElement, src, nSrc * sizeofElement);
    free(src);

    *nElements = nNew;
    return tmp;
}

// static size_t resizeStrBuf(BSIZE_T *nBytes, const char **src)
// {
//     size_t nSrc = *nBytes;
//     *nBytes = nSrc + (nSrc >> 2);
//     char *tmp = (char *)malloc((*nBytes) * sizeof(char));
//     if (tmp == NULL)
//     {
//         *nBytes = nSrc;
//         return 0;
//     }
//     strcpy_s(tmp, *nBytes, *src);
//     free(*src);
//     *src = tmp;
//     return *nBytes;
// }

static void scanINIParamLine(const char *const line, INIPLA *pPla)
{
    const char *semicolon = strchr(line, ';');
    const char *equre = strchr(line, '=');
    const char *cr = strchr(line, '\r');
    const char *lf = strchr(line, '\n');
    const char *null = strchr(line, '\0');
    assert(null != NULL);

    pPla->lineStart = line;
    pPla->lineEnd = equre ? (semicolon ? (semicolon < equre ? line : semicolon) : (cr ? cr : (lf ? lf : null))) : line;
    pPla->nameEnd = equre ? (semicolon && semicolon < equre ? line : equre) : line;
    pPla->valStart = equre ? (semicolon && semicolon < equre ? line : equre + 1) : line;
    // if "line" is invalid, all PLA values are set to "line".
}

static void scanINISectionLine(const char *const line, INISLA *pSla)
{
    const char *l_bracket = strchr(line, '[');
    const char *r_bracket = strchr(line, ']');
    const char *cr = strchr(line, '\r');
    const char *lf = strchr(line, '\n');
    const char *null = strchr(line, '\0');
    assert(null != NULL);

    pSla->lineStart = line;
    pSla->nameStart = l_bracket && r_bracket && l_bracket < r_bracket ? l_bracket + 1 : line;
    pSla->nameEnd = l_bracket && r_bracket && l_bracket < r_bracket ? r_bracket : line;
    pSla->lineEnd = l_bracket && r_bracket && l_bracket < r_bracket ? (cr ? cr : (lf ? lf : null)) : line;
}

static int hasNamePLA(const INIPLA * const pPla)
{
    return (pPla->nameEnd - pPla->lineStart) > 0;
}
static int hasValuePLA(const INIPLA * const pPla)
{
    return (pPla->lineEnd - pPla->valStart) > 0;
}

static int hasNameSLA(const INISLA * const pSla)
{
    return (pSla->nameEnd - pSla->nameStart) > 0;
}

static int isValidPLA(const INIPLA * const pPla)
{
    return (pPla->lineEnd - pPla->lineStart) > 0 && hasNamePLA(pPla);
}

static int isValidSLA(const INISLA * const pSla)
{
    return (pSla->lineEnd - pSla->lineStart) > 0 && hasNameSLA(pSla);
}

// // Returns amount of allocated parameters.
// BSIZE_T CreateSection(char *line, PSECTION pDstSection) ERR_HANDLED
// {
//     BSIZE_T nameSize = 64;
//     NSTR_T nameLen = nameSize - 1;
//     char *name = (char *)malloc(nameSize * sizeof(char));
//     if (name == NULL)
//     {
//         SetErrMsg(L"Could not create a section because memory alloction for a name failed.\nLine: %hs", line);
//         *pDstSection = NULLSECTION;
//         return 0;
//     }

//     char const *start = strchr(line, '[') + 1;
//     char const *end = strchr(line, ']');
//     assert(start != 1 && end != NULL && (end - start) > 0);

//     int pos_name = 0;
//     // for (pos_name = 0; line[pos_name + 1] != ']'; pos_name++)
//     for (pos_name = 0; (start + pos_name) < end; pos_name++)
//     {
//         if (nameLen <= pos_name)
//         {
//             if (!resizeStrBuf(&nameSize, &name))
//             {
//                 break;
//             }
//             nameLen = nameSize - 1;
//         }
//         name[pos_name] = line[pos_name + 1];
//     }
//     name[pos_name] = '\0';

//     AMOUNT_T n_alloc_params = 8;

//     SECTION section = {
//         name,
//         n_alloc_params,
//         0,
//         (PARAMS)calloc(n_alloc_params, sizeof(PARAMETER))
//     };

//     if (section.params == NULL)
//     {
//         free(name);
//         SetErrMsg(L"Could not create a section because memory alloction for parameters failed.\nLine: %hs", line);
//         *pDstSection = NULLSECTION;
//         return 0;
//     }

//     ClearErrMsg();
//     *pDstSection = section;
//     return pDstSection->n_alloc_params;
// } 

// PPARAM CreateParameter(char const *line, PPARAM pDstParam) ERR_HANDLED
// {
//     BSIZE_T nameSize = 64, valueSize = 128;
//     NSTR_T nameLen = nameSize - 1, valueLen = valueSize - 1;

//     PARAMETER param = {
//         (char *)malloc(nameSize * sizeof(char)),
//         (char *)malloc(valueSize * sizeof(char))
//     };

//     if (param.name == NULL || param.value == NULL)
//     {
//         SetErrMsg(L"Could not create a section because memory allocation faild.\nLine: %hs", line);
//         *pDstParam = NULLPARAM;
//         return NULL;
//     }

//     size_t iName = 0, iValue = 0;
//     for (iName = 0; line[iName] != '='; iName++)
//     {
//         if (iName >= nameLen)
//         {
//             if (!resizeStrBuf(&nameSize, &(param.name)))
//             {
//                 break;
//             }
//             nameLen = nameSize - 1;
//         }
//         param.name[iName] = line[iName];
//     }
//     param.name[iName] = '\0';

//     char const *valueTop = line + iName + 1;
//     for (iValue = 0; valueTop[iValue] != '\n' && valueTop[iValue] != '\0'; iValue++)
//     {
//         if (iValue >= valueLen)
//         {
//             if (!resizeStrBuf(&valueSize, &(param.value)))
//             {
//                 break;
//             }
//             valueLen = valueSize - 1;
//         }
//         param.value[iValue] = valueTop[iValue];
//     }
//     param.value[iValue] = '\0';

//     ClearErrMsg();
//     *pDstParam = param;
//     return pDstParam;
// }

void CreateSectionFromSLA(const INISLA * const pSla, PSECTION pDstSection) ERR_HANDLED
{
    size_t default_n_alloc_params = 8;
    size_t len_name = SLA_NAME_LEN(pSla);
    SECTION section = {
        (char *)malloc((len_name + 1) * sizeof(char)),
        default_n_alloc_params,
        0,
        (PARAMS)calloc(default_n_alloc_params, sizeof(PARAMETER))
    };

    if (section.name == NULL || section.params == NULL)
    {
        if (section.name) free(section.name);
        if (section.params) free(section.params);

        SetErrMsg(L"Could not create a section because memory alloction failed.\nLine: %hs", pSla->lineStart);
        *pDstSection = NULLSECTION;
        return;
    }

    ptrdiff_t offset;
    for (offset = 0; (pSla->nameStart + offset) < pSla->nameEnd; offset++)
    {
        section.name[offset] = *(pSla->nameStart + offset);
    }
    section.name[offset] = '\0';

    ClearErrMsg();
    *pDstSection = section;
}

void CreateParameterFromPLA(const INIPLA * const pPla, PPARAM pDstParam) ERR_HANDLED
{
    size_t len_name = PLA_NAME_LEN(pPla), len_val = PLA_VAL_LEN(pPla);
    PARAMETER param = {
        (char *)malloc((len_name + 1) * sizeof(char)),
        (char *)malloc((len_val + 1) * sizeof(char))
    };

    if (IsNullParam(param))
    {
        if (param.name) free(param.name);
        if (param.value) free(param.value);

        SetErrMsg(L"Could not create a parameter because memory allocation faild.\nLine: %hs", pPla->lineStart);
        *pDstParam = NULLPARAM;
        return;
    }

    ptrdiff_t offset = 0;

    for (offset = 0; (pPla->lineStart + offset) < pPla->nameEnd; offset++)
    {
        param.name[offset] = pPla->lineStart[offset];
    }
    param.name[offset] = '\0';

    for (offset = 0; (pPla->valStart + offset) < pPla->lineEnd; offset++)
    {
        param.value[offset] = pPla->valStart[offset];
    }
    param.value[offset] = '\0';

    ClearErrMsg();
    *pDstParam = param;
}

void FreeConfigs(PINICONFIGS pConfigs)
{
    for (int i = 0; i < pConfigs->n_sections; i++)
    {
        SECTION s = pConfigs->sections[i];
        free(s.name);
        PARAMS ps = s.params;
        for (int j = 0; j < s.n_params; j++)
        {
            free(ps[j].name);
            free(ps[j].value);
            ps[j] = NULLPARAM;
        }
        free(ps);
        s = NULLSECTION;
    }
    free(pConfigs->sections);
    *pConfigs = NULLCONFIG; 
}

void ReadINI(FILE *fp, PINICONFIGS pConfigs)
{
    char buf[1024];
    
    size_t n_alloc_secs = 8;

    PSECTION mem = (PSECTION)calloc(n_alloc_secs, sizeof(SECTION));
    if (mem == NULL)
    {
        SetErrMsg(L"Could not create a config object because memory allocation for sections failed.");
        *pConfigs = NULLCONFIG;
        return;
    }

    *pConfigs = {
        n_alloc_secs,
        0,
        mem
    };

    PSECTION pCurSection = NULL;
    while (fgets(buf, 1024, fp) != NULL)
    {
        switch (buf[0])
        {
        case ';':
        case ' ':
        case '\t':
        case '\r':
        case '\n':
            // ignore
            break;
        
        case '[':
        {
            INISLA sla = { 0, 0, 0, 0 };
            scanINISectionLine(buf, &sla);
            if (isValidSLA(&sla))
            {
                if (pConfigs->n_sections >= pConfigs->n_alloc_sections)
                {
                    void *tmp = resizeMem(sizeof(SECTION), &(pConfigs->n_alloc_sections), pConfigs->sections);
                    if (tmp == NULL)
                    {
                        SetErrMsg(L"Could not append a section because memory lacking occured.\nLine: %hs", buf);
                        FreeConfigs(pConfigs);
                        return;
                    }
                    pConfigs->sections = (SECTION *)tmp;
                }

                CreateSectionFromSLA(&sla, &(pConfigs->sections[pConfigs->n_sections]));
                if (IsNullSection(pConfigs->sections[pConfigs->n_sections]))
                {
                    // on Failed to allocate memories.
                    FreeConfigs(pConfigs);
                    return;
                }

                pCurSection = &(pConfigs->sections[pConfigs->n_sections]);
                (pConfigs->n_sections)++;
                break;
            }
        }

        default:
        {
            if (pCurSection == NULL)
            {
                break;
            }

            INIPLA pla = { 0, 0, 0, 0 };
            scanINIParamLine(buf, &pla);
            if (!isValidPLA(&pla))
            {
                break;
            }


            if (pCurSection->n_params >= pCurSection->n_alloc_params)
            {
                void *tmp = resizeMem(sizeof(PARAMETER), &(pCurSection->n_alloc_params), pCurSection->params);
                if (tmp == NULL)
                {
                    SetErrMsg(L"Could not append a parameter because memory lacking occurred.\nLine: %hs", buf);
                    FreeConfigs(pConfigs);
                    return;
                }
                pCurSection->params = (PARAMS)tmp;

                // if (!resizeMem(sizeof(PARAMETER), &(pCurSection->n_alloc_params), &(pCurSection->params)))
                // {
                //     // on Failed to allocate memories.
                //     SetErrMsg(L"Could not append a parameter because memory lacking occurred.\nLine: %hs", buf);
                //     FreeConfigs(pConfigs);
                //     return;
                // }
            }

            // if (CreateParameter(buf, &(pCurSection->params[pCurSection->n_params])) == NULL)
            CreateParameterFromPLA(&pla, &(pCurSection->params[pCurSection->n_params]));
            if (IsNullParam(pCurSection->params[pCurSection->n_params]))
            {
                // on Failed to allocate memories.
                FreeConfigs(pConfigs);
                return;
            }

            (pCurSection->n_params)++;
            break;
        }
        }
    }
}

char const * const GetValue(const PINICONFIGS pConfigs, const char *section, const char *name)
{
    for (int i = 0; i < pConfigs->n_sections; i++)
    {
        if (section == NULL || strcmp(pConfigs->sections[i].name, section) == 0)
        {
            for (int j = 0; j < pConfigs->sections[i].n_params; j++)
            {
                if (strcmp(pConfigs->sections[i].params[j].name, name) == 0)
                {
                    return pConfigs->sections[i].params[j].value;
                }
            }
        }
    }
    return NULL;
}
