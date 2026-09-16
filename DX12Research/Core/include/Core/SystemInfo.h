#pragma once

#include <windows.h>

#include <string>
#include <vector>

namespace Core
{
    struct SystemInfo
    {
        std::string m_strOsVersion;
        std::string m_strCpuBrand;
        UINT m_uPhysicalCores = 0;
        UINT m_uPerformanceCores = 0;
        UINT m_uEfficiencyCores = 0;
        UINT m_uLogicalProcessors = 0;
        UINT64 m_uTotalMemoryMb = 0;
        std::string m_strCompiler;
        std::string m_strBuildConfig;
        std::string m_strWindowsSdkVersion;
    };

    struct MonitorInfo
    {
        std::string m_strDeviceName;
        std::string m_strAdapterName;
        UINT m_uWidth = 0;
        UINT m_uHeight = 0;
        UINT m_uRefreshHz = 0;
        UINT m_uDpi = 0;
        bool m_bPrimary = false;
    };

    SystemInfo QuerySystemInfo();
    MonitorInfo QueryMonitorInfo(HWND hWnd);
    std::vector<MonitorInfo> QueryMonitors();
    void LogSystemInfo(const SystemInfo& info);
    void LogMonitorInfo(const MonitorInfo& info);
}
