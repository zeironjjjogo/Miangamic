#pragma once

#include <stdio.h>

typedef struct parameter {
    char *name;
    char *value;
} PARAMETER;
typedef PARAMETER *PPARAM, *PARAMS;
const PARAMETER NULLPARAM = { NULL, NULL };
inline int IsNullParam(const PARAMETER p)
{
    return (p.name == NULL || p.value == NULL);
}

typedef struct section {
    char *name;
    size_t n_alloc_params;
    size_t n_params;
    struct parameter *params;
} SECTION;
typedef SECTION *PSECTION;
const SECTION NULLSECTION = { NULL, 0, 0, NULL };
inline int IsNullSection(const SECTION s)
{
    return (s.name == NULL && s.n_alloc_params == 0 && s.n_params == 0 && s.params == NULL);
}

typedef struct iniConfigs {
    size_t n_alloc_sections;
    size_t n_sections;
    struct section *sections;
} INICONFIGS;
typedef INICONFIGS *PINICONFIGS;
const INICONFIGS NULLCONFIG = { 0, 0, NULL };
inline int IsNullINIConfigs(const PINICONFIGS pINIConfigs)
{
    return (pINIConfigs->n_alloc_sections == 0 && pINIConfigs->n_sections == 0 && pINIConfigs->sections == NULL);
}

typedef size_t BSIZE_T, NSTR_T, AMOUNT_T;

typedef wchar_t const * const ERRMSG;

typedef struct INIParamLineAttr
{
    const char *lineStart;
    const char *nameEnd;
    const char *valStart;
    const char *lineEnd;
} INIPLA;

typedef struct INISectionLineAttr
{
    const char *lineStart;
    const char *nameStart;
    const char *nameEnd;
    const char *lineEnd;
} INISLA;

#define PLA_NAME_LEN(pPla) (size_t)(((pPla)->nameEnd - (pPla)->lineStart) / sizeof(char))
#define PLA_VAL_LEN(pPla) (size_t)(((pPla)->lineEnd - (pPla)->valStart) / sizeof(char))

#define SLA_NAME_LEN(pSla) (size_t)(((pSla)->nameEnd - (pSla)->nameStart) / sizeof(char))

ERRMSG GetErrMsg();
void ReadINI(FILE *fp, PINICONFIGS pConfigs);
void FreeConfigs(PINICONFIGS pConfigs);
char const * const GetValue(const PINICONFIGS pConfigs, const char *section, const char *key);
