#pragma once

#include <windows.h>

#include <string>

namespace Core
{
    std::string HrToString(HRESULT hrResult);
    [[noreturn]] void HrFail(HRESULT hrResult, const char* szExpression, const char* szFile, int nLine);
}

#define HR_CHECK(expression) \
    do \
    { \
        const HRESULT hrCheckResult = (expression); \
        if (FAILED(hrCheckResult)) \
        { \
            ::Core::HrFail(hrCheckResult, #expression, __FILE__, __LINE__); \
        } \
    } while (false)
