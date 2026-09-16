#include "Core/HrCheck.h"

#include "Core/Log.h"

#include <cstdlib>
#include <format>

namespace Core
{
    std::string HrToString(HRESULT hrResult)
    {
        return std::format("0x{:08X}", static_cast<unsigned long>(hrResult));
    }

    void HrFail(HRESULT hrResult, const char* szExpression, const char* szFile, int nLine)
    {
        Log::Error("hresult {} {} {}:{}", HrToString(hrResult), szExpression, szFile, nLine);
#if defined(_DEBUG)
        __debugbreak();
#endif
        std::abort();
    }
}
