#pragma once

void showLastWINAPIError(const wchar_t const *msgFmt);
void showErrnoError(errno_t err, const wchar_t const *msgFmt);
