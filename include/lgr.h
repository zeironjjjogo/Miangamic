#pragma once

typedef enum {
    LGR_DEBUG,
    LGR_INFO,
    LGR_WARNING,
    LGR_ERROR,
    LGR_CRITICAL
} LGR_LEVEL;

typedef void (*OTRG_FUNC)(const wchar_t *msg);

typedef struct {
    LGR_LEVEL level;
    OTRG_FUNC func;
} OTRG;

int SetOutputTarget(OTRG trg);
int GetOutputTarget(int idx, OTRG *pTrg);

void LogMsg(LGR_LEVEL level, const wchar_t *file, int line, const wchar_t *func, const wchar_t *msg);
void LogMsgF(LGR_LEVEL level, const wchar_t *file, int line, const wchar_t *func, const wchar_t *fmt, ...);

#define LGR_LOG(level, msg) LogMsg(level, __FILEW__, __LINE__, __FUNCTIONW__, msg)
#define LGR_LOGF(level, fmt, ...) LogMsgF(level, __FILEW__, __LINE__, __FUNCTIONW__, fmt, __VA_ARGS__)

void InitLGR(const char *logFilePath);
void UninitLGR();
