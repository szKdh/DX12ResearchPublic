#pragma once

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Core
{
    enum class ELogLevel
    {
        Info,
        Warn,
        Error,
    };

    std::string ToUtf8(std::wstring_view strWide);
    std::wstring ToWide(std::string_view strUtf8);

    namespace Log
    {
        std::wstring MakeDefaultFilePath();
        bool Open(const std::wstring& strFilePath);
        void Close();
        const std::wstring& GetFilePath();
        void Write(ELogLevel eLevel, std::string_view strMessage);
        std::vector<std::string> CopyRecentLines();

        template <typename... Args>
        void Info(std::format_string<Args...> strFormat, Args&&... args)
        {
            Write(ELogLevel::Info, std::format(strFormat, std::forward<Args>(args)...));
        }

        template <typename... Args>
        void Warn(std::format_string<Args...> strFormat, Args&&... args)
        {
            Write(ELogLevel::Warn, std::format(strFormat, std::forward<Args>(args)...));
        }

        template <typename... Args>
        void Error(std::format_string<Args...> strFormat, Args&&... args)
        {
            Write(ELogLevel::Error, std::format(strFormat, std::forward<Args>(args)...));
        }
    }
}
