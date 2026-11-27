#include <Windows.h>
#include <crtdbg.h>

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <clocale>

#include <aviutl2_sdk/plugin2.h>
#include <aviutl2_sdk/logger2.h>
#include <aviutl2_sdk/config2.h>

#include "../include/covenc.h"
#include "../include/i18n.h"
#include "../include/lgr.h"

#define EXPORT extern "C" __declspec(dllexport)

#define PROJECT_NAME "Miangamamic"
#define PRE_PROJNAMEW(x, y) x ## y
#define MID_PROJNAMEW(x, y) PRE_PROJNAMEW(x, y)
#define PROJECT_NAMEW MID_PROJNAMEW(L, PROJECT_NAME)

#define MSGBOXINFO(msg) MessageBoxW(NULL, msg, PROJECT_NAMEW L": Info", MB_OK)
#define MSGBOXERR(msg) MessageBoxW(NULL, msg, PROJECT_NAMEW L": Error", MB_OK)

typedef char * LPADLPSZ;

LOG_HANDLE* logger = NULL;
CONFIG_HANDLE *config = NULL;

wchar_t wcsTableInfoBuf[128] = { 0 };
wchar_t wcsMenuTextBuf[4][256] = { 0 };

EXPORT uint32_t RequiredVersion(void)
{
    return 2010301;
}

static void wrappedInfo(const wchar_t *msg)
{
    logger->info(logger, msg);
}

static void wrappedMSGBOX(const wchar_t *msg)
{
    MSGBOXERR(msg);
}

EXPORT void InitializeLogger(LOG_HANDLE *handle)
{
    logger = handle;
    logger->info(logger, L"InitializeLogger");
    InitLGR("./" PROJECT_NAME ".log");
    SetOutputTarget({
        LGR_DEBUG,
        &wrappedInfo
    });
    SetOutputTarget({
        LGR_ERROR,
        &wrappedMSGBOX
    });
}

EXPORT void InitializeConfig(CONFIG_HANDLE *config_handle)
{
    config = config_handle;
    logger->info(logger, L"InitializeConfig");
}

EXPORT bool InitializePlugin(DWORD version)
{
#ifdef _DEBUG
    logger->info(logger, L"Waiting for attacing debugger...");
    while (!IsDebuggerPresent())
    {
        Sleep(500);
    }
#endif

    ERRMSG err = InitializeI18n();
    if (err != NULL)
    {
        logger->error(logger, err);
        return false;
    }
    logger->info(logger, L"InitializePlugin");
    return true;
}

EXPORT COMMON_PLUGIN_TABLE * GetCommonPluginTable(void)
{
    if (GetI18nText(wcsTableInfoBuf, 128, "TABLE", "info") == NULL)
    {
        _snwprintf_s(wcsTableInfoBuf, _TRUNCATE, L"Insert text objects at once.");
    }
    static COMMON_PLUGIN_TABLE table = {
        PROJECT_NAMEW,
        wcsTableInfoBuf ? wcsTableInfoBuf : L"Insert text objects at once."
    };
    if (logger)
        logger->info(logger, L"GetCommonPluginTable");
    return &table;
}

EXPORT void UninitializePlugin(void)
{
    UninitializeI18n();
    UninitLGR();
    logger->info(logger, L"UninitializePlugin");
}

static wchar_t * AskFileName(wchar_t * buffer, size_t n_str)
{
    OPENFILENAMEW ofn = { 0 };
    ofn.lStructSize = sizeof(OPENFILENAMEW);
    ofn.lpstrFilter = L"テキストファイル (*.txt)\0*.txt\0すべてのファイル (*.*)\0*.*\0";
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = n_str;
    return GetOpenFileNameW(&ofn) ? buffer : NULL;
}

static bool ReadHeader(FILE * fp, int * length, wchar_t alias[256])
{
    if (fp == NULL) {
        logger->error(logger, L"fp was NULL.");
        return false;
    }

    constexpr size_t MAX_LINE_LEN = 256;
    wchar_t line[MAX_LINE_LEN];

    if (fgetws(line, MAX_LINE_LEN - 1, fp) == NULL) {
        logger->error(logger, L"Failed to get header line.");
        return false;
    }
    logger->info(logger, L"Successfully get the header line.");

    if (_snwscanf_s(line, 256 * sizeof(wchar_t), L"%d", length) == EOF) {
        logger->error(logger, L"Failed to get length.");
        return false;
    }
    logger->info(logger, L"Successfully get length.");

    wchar_t *pos;
    if ((pos = wcsrchr(line, L' ')) == NULL) {
        logger->error(logger, L"A spece is not found in the header line.");
        return false;
    }
    logger->info(logger, L"Successfully get first position of alias name.");

    errno_t result;
    if ((result = wcsncpy_s(alias, 256, pos + 1, _TRUNCATE)) != 0 && result != STRUNCATE) {
        wchar_t bf[128] = { 0 };
        _snwprintf_s(bf, 128, L"Error: %d; Failed to copy alias name.\n%s", result, alias);
        logger->error(logger, bf);
        return false;
    }
    logger->info(logger, L"Successfully aligned alias name to variable.");

    return true;
}

static bool CheckEmpty(EDIT_SECTION * edit_section, int *layer, int *frame, int *length)
{
    OBJECT_HANDLE hObj = edit_section->find_object(*layer, *frame);
    if (hObj == nullptr) return true;
    wchar_t logBuf[128] = { 0 };
    int end_frame = *frame + *length;
    OBJECT_LAYER_FRAME olf = edit_section->get_object_layer_frame(hObj);
    _snwprintf_s(logBuf, 128, L"choise:%d:%d-%d/found:%d:%d-%d", *layer, *frame, end_frame, olf.layer, olf.start, olf.end);
    logger->verbose(logger, logBuf);
    if (end_frame < olf.start) return true;
    // [~~~~~~~]
    //     [~~]
    if (olf.start < end_frame) {
        *frame = olf.end + 1;
        return CheckEmpty(edit_section, layer, frame, length);
    }
    MessageBoxW(NULL, L"Unknown range detected...", L"CheckEmpty", MB_OK);
    return false;
}

static LPCSTR GetAliasFromCurrentObject(EDIT_SECTION * edit_section)
{
    // OBJECT_HANDLE hObj = edit_section->get_selected_object(0);
    // if (hObj == nullptr) {
    //     logger->error(logger, L"Failed to get selected object.");
    //     return NULL;
    // }
    OBJECT_HANDLE hObj = edit_section->get_focus_object();
    if (hObj == nullptr) {
        logger->error(logger, L"Failed to get selected object.");
        return NULL;
    }
    return edit_section->get_object_alias(hObj);
}

static LPCSTR GetAliasFromDefaultObject(EDIT_SECTION * edit_section, int layer, int frame, int length)
{
    if (!CheckEmpty(edit_section, &layer, &frame, &length)) return NULL;
    OBJECT_HANDLE hObj = edit_section->create_object(L"テキスト", layer, frame, length);
    if (hObj == nullptr) return NULL;
    LPCSTR alias = edit_section->get_object_alias(hObj);
    edit_section->delete_object(hObj);
    return alias;
}

static void RemoveLastN(char * str)
{
    int len = strlen(str);
    if (str[len - 1] == '\n') str[len - 1] = '\0';
}

static int CreateTexts(EDIT_SECTION * edit_section, FILE * fp, LPCSTR alias, int layer, int frame, int length)
{
    int i = 0, n_result = 0, next_frame = frame;
    char buffer[1024] = { 0 };
    wchar_t logBuf[256] = { 0 };
    while (fgets(buffer, 1024, fp) != NULL) {
        i++;
        _snwprintf_s(logBuf, _TRUNCATE, L"%d %d %d", layer, next_frame, length);
        logger->verbose(logger, logBuf);
        if (!CheckEmpty(edit_section, &layer, &next_frame, &length)) continue;
        OBJECT_HANDLE hObj = edit_section->create_object_from_alias(alias, layer, next_frame, length);
        if (hObj == nullptr) continue;
        RemoveLastN(buffer);
        if (edit_section->set_object_item_value(hObj, L"テキスト", L"テキスト", buffer)) {
            n_result++;
        }
        next_frame += length;
    }
    return i - n_result;
}

static bool StartsWith(const char *str, const char *pattern) {
    if (str == NULL || pattern == NULL) {
        logger->error(logger, L"Parameter includes NULL.");
        return false;
    }
    if (strlen(str) < strlen(pattern)) {
        logger->error(logger, L"An argument \'pattern\' is longer than \'str\'.");
        return false;
    }
    bool isMatched = true;
    for (int i = 0; pattern[i] != NULL && str[i] != NULL; i++) {
        if (str[i] != pattern[i]) {
            isMatched = false;
            break;
        }
    }
    return isMatched;
}

static char *StandardizeAlias(LPCSTR alias)
{
    _ASSERTE(alias != NULL);
    size_t len = strlen(alias);
    char *buf = (char *)malloc((len + 1) * sizeof(char));
    if (buf == NULL) {
        return NULL;
    }

    int dsti = 0, srci = 0;
    bool isNextNewLine = false;
    bool doesCopyNextChar = true;
    constexpr char const *PATTERN = "frame=";
    size_t LEN_PATTERN = strlen(PATTERN);

    for (srci = 0; alias[srci] != NULL; srci++) {
        if (isNextNewLine && (len - srci) >= LEN_PATTERN && StartsWith((alias + srci), PATTERN)) {
            doesCopyNextChar = false;
            logger->verbose(logger, L"key=frame detected");
        }

        if (doesCopyNextChar) {
            buf[dsti] = alias[srci];
            dsti++;
        }

        if (alias[srci] == '\n') {
            isNextNewLine = true;
            doesCopyNextChar = true;
        }
    }
    buf[dsti] = '\0';

    return buf;
}

static LPADLPSZ GetTexts(FILE * fp, size_t *n_lines) {
    if (fp == NULL) {
        logger->error(logger, L"An argument \"fp\" is NULL.");
        return NULL;
    }
    *n_lines = 0;

    size_t bufferSize = 32;
    size_t currentPos = 0;

    wchar_t logBuf[256] = { 0 };

    LPADLPSZ buffer = (LPADLPSZ)calloc(bufferSize, sizeof(char));
    if (buffer == NULL) {
        logger->error(logger, L"Failed to allocate memories.");
        return buffer;
    }

    LPADLPSZ lineTop = buffer, beforeLineTop = NULL;
    size_t memRemain = bufferSize;
    bool severed = false;

    while (fgets(lineTop, memRemain, fp) != NULL)
    {
        if (severed) {
            severed = false;
        } else {
            (*n_lines)++;
        }

        size_t lineLen = strlen(lineTop); // amount of characters (without '\0') equals size of after removed '\n'.

        _snwprintf_s(logBuf, 256, L"Line: %zu\tn_str: %zu", *n_lines, lineLen);
        logger->verbose(logger, logBuf);

        currentPos += lineLen;
        memRemain -= lineLen;

        _ASSERT_EXPR((currentPos + memRemain) == bufferSize, "sumcheck failed");

        if (currentPos > bufferSize * 0.8)
        {
            size_t newSize = bufferSize + bufferSize * 0.5;
            _ASSERT_EXPR(((newSize > bufferSize)), "re-allocate size bug");
            LPADLPSZ tmp = (LPADLPSZ)calloc(newSize, sizeof(char));
            if (tmp == NULL) {
                free(buffer);
                logger->error(logger, L"Failed to re-allocate memories.");
                return NULL;
            }

            _snwprintf_s(logBuf, 256, L"Using: %zuB(%.1f%%), To alloc: %zuB", currentPos, 100.f * currentPos / bufferSize, newSize);
            logger->verbose(logger, logBuf);
            
            memcpy_s(tmp, newSize, buffer, currentPos);
            free(buffer);
            buffer = tmp;
            bufferSize = newSize;
            lineTop = buffer + currentPos - lineLen;
            memRemain = bufferSize - currentPos;

            _snwprintf_s(logBuf, 256, L"Re-alloceted using: %zuB(%.1f%%)", currentPos, 100.f * currentPos / bufferSize);
            logger->verbose(logger, logBuf);
        }

        LPADLPSZ nullPos = lineTop + lineLen;
        if (*(nullPos - 1) != '\n')
        {
            if (feof(fp)) {
                verboseAsSjis(beforeLineTop ? beforeLineTop : lineTop, [](wchar_t *wcs) { logger->verbose(logger, wcs); });
                break;
            }
            logger->info(logger, L"severed");
            severed = true;
            beforeLineTop = lineTop;
            lineTop = nullPos;
            continue;
        }

        *(nullPos - 1) = '\0';
        nullPos--;

        verboseAsSjis(beforeLineTop ? beforeLineTop : lineTop, [](wchar_t *wcs) { logger->verbose(logger, wcs); });

        _ASSERT_EXPR(nullPos + 1 == buffer + currentPos, "nullpos missed");
        beforeLineTop = NULL;
        lineTop = nullPos + 1;
    }
    return buffer;
}

static bool CreateTextsFromArr(EDIT_SECTION *edit, size_t n_line, LPADLPSZ arr, LPCSTR alias, int layer, int frame, int length) {
    if (arr == NULL) return false;

    bool result = true;
    int lengthPerLine = length / n_line;
    LPADLPSZ line = arr;
    OBJECT_HANDLE *handles = (OBJECT_HANDLE *)calloc(n_line, sizeof(OBJECT_HANDLE));
    if (handles == NULL) {
        return false;
    }

    for (int i = 0; i < n_line; i++) {
        size_t lineLen = strlen(line);
        int start = frame + i * lengthPerLine;
        auto hObj = (alias == NULL) ? edit->create_object(L"テキスト", layer, start, lengthPerLine) : edit->create_object_from_alias(alias, layer, start, lengthPerLine);
        if (hObj != NULL) {
            edit->set_object_item_value(hObj, L"テキスト", L"テキスト", line);
            logger->verbose(logger, L"object created");
            handles[i] = hObj;
        } else {
            result = false;
            for (int j = 0; j < i; j++) {
                edit->delete_object(handles[j]);
            }
            break;
        }
        line += lineLen + 1;
    }
    free(handles);
    return result;
}

// static void main_routine(EDIT_SECTION * edit_section, bool srcIsObj)
// {
//     constexpr size_t MAX_PATH_LEN = 256;
//     wchar_t filepath[MAX_PATH_LEN] = { 0 };

//     if (AskFileName(filepath, MAX_PATH_LEN) == NULL) {
//         MessageBoxW(NULL, L"Failed to get file path.", L"Miangamic:ERROR", MB_OK);
//         return;
//     }
//     logger->info(logger, L"Successfully get file name.");

//     FILE * fp = _wfopen(filepath, L"r");
//     if (fp == NULL) {
//         MessageBoxW(NULL, L"Failed to open the file.", L"Miangamic:ERROR", MB_OK);
//         return;
//     }
//     logger->info(logger, L"Successfully open the file.");

//     int length = 60;
//     bool withHeader = !srcIsObj && strcmp(getConfig("options", "wHeader"), "true") == 0;

//     wchar_t alias_name[256] = { 0 };
//     if (withHeader)
//     {
//         if (!ReadHeader(fp, &length, alias_name)) {
//             fclose(fp);
//             MessageBoxW(NULL, L"Failed to read header.", L"Miangamic:ERROR", MB_OK);
//             return;
//         }
//         logger->info(logger, L"Successfully read the header line.");
//     }

//     int cur_layer = edit_section->info->layer, cur_frame = edit_section->info->frame;
//     wchar_t curpos[128] = { 0 };
//     _snwprintf_s(curpos, 128, L"Layer: %d, Frame: %d.", cur_layer, cur_frame);
//     logger->verbose(logger, curpos);

//     LPCSTR alias = srcIsObj ? GetAliasFromCurrentObject(edit_section) : GetAliasFromDefaultObject(edit_section, cur_layer, cur_frame, length);
//     if (alias == NULL) {
//         fclose(fp);
//         MessageBoxW(NULL, L"Failed to get alias.", L"Miangamic:ERROR", MB_OK);
//         return;
//     }
//     logger->info(logger, L"Successfully get the alias script.");

//     int n_failed = CreateTexts(edit_section, fp, alias, cur_layer, cur_frame, length);
    
//     fclose(fp);

//     wchar_t message[128] = { 0 };
//     _snwprintf_s(message, _TRUNCATE, L"Failed to create %d objects.", n_failed);

//     MessageBoxW(NULL, message, L"Miangamic", MB_OK);
// }

static void InsertAtPlaceholder(EDIT_SECTION * edit, bool extendsAlias)
{
    OBJECT_HANDLE hPlaceholder = edit->get_focus_object();
    if (hPlaceholder == NULL) {
        // MSGBOXERR(L"オブジェクトが選択されていません。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"オブジェクトが選択されていません。"));
        return;
    }

    if (extendsAlias && edit->find_effect(hPlaceholder, L"テキスト") == NULL) {
        // MSGBOXERR(L"テキスト以外のオブジェクトを継承することはできません。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"テキスト以外のオブジェクトを継承することはできません。"));
        return;
    }

    const size_t SIZE_FILEPATH = 256;
    wchar_t wszFilepath[SIZE_FILEPATH] = { 0 };
    if (AskFileName(wszFilepath, SIZE_FILEPATH) == NULL)
    {
        return;
    }

    FILE *fp = _wfopen(wszFilepath, L"r");
    if (fp == NULL) {
        // MSGBOXERR(L"ファイルを開けませんでした。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"ファイルを開けませんでした。"));

        return;
    }
    logger->info(logger, L"Successfully open the file.");

    size_t n_lines = 0;
    LPADLPSZ arr = GetTexts(fp, &n_lines);

    fclose(fp);

    if (arr == NULL) {
        // MSGBOXERR(L"ファイルを読み込めませんでした。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"ファイルを読み込めませんでした。"));
        return;
    }
    // From this line, Require to free "arr"
    logger->info(logger, L"Successfully read the file.");

    if (n_lines <= 0) {
        // MSGBOXERR(L"ファイルの中身が空でした。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"ファイルの中身が空でした。"));
        free(arr);
        return;
    }

    OBJECT_LAYER_FRAME olf = edit->get_object_layer_frame(hPlaceholder);
    LPCSTR alias = edit->get_object_alias(hPlaceholder);
    char *stdAlias = StandardizeAlias(alias);

    size_t length = olf.end - olf.start;

    if (length < n_lines) {
        // MSGBOXERR(L"選択されたオブジェクトが短く、置き換えに足りません。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"選択されたオブジェクトが短く、置き換えに足りません。"));
        free(arr);
        return;
    }

    edit->delete_object(hPlaceholder);

    bool result = CreateTextsFromArr(edit, n_lines, arr, (extendsAlias) ? (stdAlias == NULL) ? alias : stdAlias : NULL, olf.layer, olf.start, length);
    if (!result) {
        edit->create_object_from_alias(alias, olf.layer, olf.start, length);
        // MSGBOXERR(L"オブジェクトの作成に失敗しました。");
        MSGBOXERR(config->get_language_text(config, PROJECT_NAMEW, L"オブジェクトの作成に失敗しました。"));
    }
    free(arr);
    if (stdAlias != NULL) free(stdAlias);
}

EXPORT void RegisterPlugin(HOST_APP_TABLE* host)
{
    setlocale(LC_ALL, "");
    // host->register_layer_menu(GetI18nText(wcsMenuTextBuf[0], 256, "MENU", withHeader == 0 ? "menu0a" : "menu0"), [](EDIT_SECTION *edit_section) { main_routine(edit_section, false); });
    // host->register_object_menu(GetI18nText(wcsMenuTextBuf[1], 256, "MENU", "menu1"), [](EDIT_SECTION *edit_section) { main_routine(edit_section, true); });
    host->register_object_menu(GetI18nText(wcsMenuTextBuf[2], 256, "MENU", "menu2"), [](EDIT_SECTION *edit) { InsertAtPlaceholder(edit, false); });
    host->register_object_menu(GetI18nText(wcsMenuTextBuf[3], 256, "MENU", "menu3"), [](EDIT_SECTION *edit) { InsertAtPlaceholder(edit, true); });

    logger->info(logger, L"RegisterPlugin");
}
