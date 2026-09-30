#include "EnvironmentInfo.h"

#include "Core/Log.h"
#include "Core/SystemInfo.h"
#include "RendererDX11/RendererDX11.h"

#include <windows.h>

#include <filesystem>
#include <format>
#include <fstream>
#include <vector>

namespace MainApp
{
    namespace
    {
        constexpr const wchar_t kEnvDirectory[] = L"Results\\env";
        constexpr int kLabelWidth = 28;

        void AppendRow(std::string& strOut, const std::string& strLabel, const std::string& strValue)
        {
            strOut += std::format("  {:<{}}{}\n", strLabel, kLabelWidth, strValue);
        }

        const char* PrimaryText(bool bPrimary)
        {
            if (true == bPrimary)
            {
                return " primary";
            }
            return "";
        }

        std::string BuildText(const Core::SystemInfo& info, const std::vector<Core::MonitorInfo>& vecMonitors, const SYSTEMTIME& stNow, const std::wstring& strAdapterFilter)
        {
            std::string strOut = "DX12Research environment\n";
            AppendRow(strOut, "Generated", std::format("{:04}-{:02}-{:02} {:02}:{:02}:{:02}", stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute, stNow.wSecond));
            strOut += "\nSystem\n";
            AppendRow(strOut, "OS", std::format("Windows {}", info.m_strOsVersion));
            AppendRow(strOut, "CPU", info.m_strCpuBrand);
            AppendRow(strOut, "Cores", std::format("{} ({} performance, {} efficiency), {} logical", info.m_uPhysicalCores, info.m_uPerformanceCores, info.m_uEfficiencyCores, info.m_uLogicalProcessors));
            AppendRow(strOut, "RAM", std::format("{} MB", info.m_uTotalMemoryMb));
            UINT iMonitor = 0;
            for (const Core::MonitorInfo& monitor : vecMonitors)
            {
                AppendRow(strOut, std::format("Monitor {}", iMonitor), std::format("{} {}x{} {} Hz dpi {}{}", monitor.m_strDeviceName, monitor.m_uWidth, monitor.m_uHeight, monitor.m_uRefreshHz, monitor.m_uDpi, PrimaryText(monitor.m_bPrimary)));
                ++iMonitor;
            }
            strOut += "\nBuild\n";
            AppendRow(strOut, "Configuration", info.m_strBuildConfig);
            AppendRow(strOut, "Compiler", info.m_strCompiler);
            AppendRow(strOut, "Windows SDK", info.m_strWindowsSdkVersion);
            strOut += "\n";
            strOut += RendererDX11::DumpDX11Features(strAdapterFilter);
            return strOut;
        }
    }

    std::wstring SaveEnvironmentInfo(const std::wstring& strAdapterFilter)
    {
        const Core::SystemInfo info = Core::QuerySystemInfo();
        const std::vector<Core::MonitorInfo> vecMonitors = Core::QueryMonitors();
        SYSTEMTIME stNow{};
        GetLocalTime(&stNow);
        const std::string strText = BuildText(info, vecMonitors, stNow, strAdapterFilter);

        const std::wstring strFileName = std::format(L"env-{:04}{:02}{:02}-{:02}{:02}.txt", stNow.wYear, stNow.wMonth, stNow.wDay, stNow.wHour, stNow.wMinute);
        const std::filesystem::path pathFile = std::filesystem::path(kEnvDirectory) / strFileName;
        std::error_code ec;
        std::filesystem::create_directories(pathFile.parent_path(), ec);
        std::ofstream file(pathFile, std::ios::out | std::ios::trunc);
        if (false == file.is_open())
        {
            Core::Log::Error("env write failed {}", Core::ToUtf8(pathFile.wstring()));
            return std::wstring();
        }
        file.write(strText.data(), static_cast<std::streamsize>(strText.size()));
        file.close();
        Core::Log::Info("env {}", Core::ToUtf8(pathFile.wstring()));
        return pathFile.wstring();
    }
}
