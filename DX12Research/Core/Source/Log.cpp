#include "Core/Log.h"

#include <windows.h>

#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace Core
{
    namespace
    {
        constexpr const wchar_t kDefaultLogDirectory[] = L"Results\\tmp";
        constexpr size_t kRecentLineCapacity = 200;

        std::ofstream s_file;
        std::mutex s_mutex;
        std::wstring s_strFilePath;
        std::deque<std::string> s_dequeRecent;

        const char* LevelTag(ELogLevel eLevel)
        {
            switch (eLevel)
            {
            case ELogLevel::Warn:
            {
                return "WARN";
            }
            case ELogLevel::Error:
            {
                return "ERROR";
            }
            case ELogLevel::Info:
            default:
            {
                return "INFO";
            }
            }
        }
    }

    std::string ToUtf8(std::wstring_view strWide)
    {
        if (true == strWide.empty())
        {
            return std::string();
        }
        const int nLength = static_cast<int>(strWide.size());
        const int nRequired = WideCharToMultiByte(CP_UTF8, 0, strWide.data(), nLength, nullptr, 0, nullptr, nullptr);
        if (0 >= nRequired)
        {
            return std::string();
        }
        std::string strResult(static_cast<size_t>(nRequired), '\0');
        WideCharToMultiByte(CP_UTF8, 0, strWide.data(), nLength, strResult.data(), nRequired, nullptr, nullptr);
        return strResult;
    }

    std::wstring ToWide(std::string_view strUtf8)
    {
        if (true == strUtf8.empty())
        {
            return std::wstring();
        }
        const int nLength = static_cast<int>(strUtf8.size());
        const int nRequired = MultiByteToWideChar(CP_UTF8, 0, strUtf8.data(), nLength, nullptr, 0);
        if (0 >= nRequired)
        {
            return std::wstring();
        }
        std::wstring strResult(static_cast<size_t>(nRequired), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, strUtf8.data(), nLength, strResult.data(), nRequired);
        return strResult;
    }

    namespace Log
    {
        std::wstring MakeDefaultFilePath()
        {
            SYSTEMTIME stNow{};
            GetLocalTime(&stNow);
            const std::wstring strFileName = std::format(L"run-{:04}{:02}{:02}-{:02}{:02}{:02}.log", stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute, stNow.wSecond);
            const std::filesystem::path pathFile = std::filesystem::path(kDefaultLogDirectory) / strFileName;
            return pathFile.wstring();
        }

        bool Open(const std::wstring& strFilePath)
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (true == s_file.is_open())
            {
                s_file.close();
            }
            s_strFilePath.clear();
            const std::filesystem::path pathFile(strFilePath);
            const std::filesystem::path pathDirectory = pathFile.parent_path();
            if (false == pathDirectory.empty())
            {
                std::error_code ec;
                std::filesystem::create_directories(pathDirectory, ec);
            }
            s_file.open(pathFile, std::ios::out | std::ios::trunc);
            if (false == s_file.is_open())
            {
                return false;
            }
            s_strFilePath = pathFile.wstring();
            return true;
        }

        void Close()
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            if (true == s_file.is_open())
            {
                s_file.close();
            }
            s_strFilePath.clear();
        }

        const std::wstring& GetFilePath()
        {
            return s_strFilePath;
        }

        void Write(ELogLevel eLevel, std::string_view strMessage)
        {
            SYSTEMTIME stNow{};
            GetLocalTime(&stNow);
            std::string strLine = std::format("{:02}:{:02}:{:02}.{:03} {} {}\n", stNow.wHour, stNow.wMinute, stNow.wSecond, stNow.wMilliseconds, LevelTag(eLevel), strMessage);
            std::lock_guard<std::mutex> lock(s_mutex);
            OutputDebugStringA(strLine.c_str());
            if (true == s_file.is_open())
            {
                s_file.write(strLine.data(), static_cast<std::streamsize>(strLine.size()));
                s_file.flush();
            }
            strLine.pop_back();
            s_dequeRecent.push_back(std::move(strLine));
            if (kRecentLineCapacity < s_dequeRecent.size())
            {
                s_dequeRecent.pop_front();
            }
        }

        std::vector<std::string> CopyRecentLines()
        {
            std::lock_guard<std::mutex> lock(s_mutex);
            return std::vector<std::string>(s_dequeRecent.begin(), s_dequeRecent.end());
        }
    }
}
