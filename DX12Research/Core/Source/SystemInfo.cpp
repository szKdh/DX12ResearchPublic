#include "Core/SystemInfo.h"

#include "Core/Log.h"

#include <intrin.h>
#include <shellscalingapi.h>

#include <cstring>
#include <cwchar>
#include <format>
#include <map>
#include <vector>

#pragma comment(lib, "shcore.lib")

namespace Core
{
    namespace
    {
        constexpr int kCpuidBrandLeafFirst = static_cast<int>(0x80000002u);
        constexpr int kCpuidBrandLeafCount = 3;
        constexpr int kCpuidRegisterCount = 4;
        constexpr size_t kCpuBrandLength = static_cast<size_t>(kCpuidBrandLeafCount) * static_cast<size_t>(kCpuidRegisterCount) * sizeof(int);
        constexpr UINT64 kBytesPerMb = 1024ull * 1024ull;
        constexpr int kMscVerMajorDivisor = 100;
        constexpr int kMscFullVerBuildModulus = 100000;
        constexpr const char kUnknown[] = "unknown";

#define DX12R_STRINGIZE_IMPL(x) #x
#define DX12R_STRINGIZE(x) DX12R_STRINGIZE_IMPL(x)

#if defined(DX12R_WINDOWS_SDK_VERSION)
        constexpr const char kWindowsSdkVersion[] = DX12R_STRINGIZE(DX12R_WINDOWS_SDK_VERSION);
#else
        constexpr const char kWindowsSdkVersion[] = "unknown";
#endif

        typedef LONG (WINAPI* PfnRtlGetVersion)(PRTL_OSVERSIONINFOW);

        struct CoreTopology
        {
            UINT m_uCores = 0;
            UINT m_uPerformanceCores = 0;
            UINT m_uEfficiencyCores = 0;
        };

        std::string QueryOsVersion()
        {
            const HMODULE hNtdll = GetModuleHandleW(L"ntdll.dll");
            if (nullptr == hNtdll)
            {
                return kUnknown;
            }
            const PfnRtlGetVersion pfnRtlGetVersion = reinterpret_cast<PfnRtlGetVersion>(GetProcAddress(hNtdll, "RtlGetVersion"));
            if (nullptr == pfnRtlGetVersion)
            {
                return kUnknown;
            }
            RTL_OSVERSIONINFOW osvi{};
            osvi.dwOSVersionInfoSize = sizeof(osvi);
            if (0 != pfnRtlGetVersion(&osvi))
            {
                return kUnknown;
            }
            return std::format("{}.{}.{}", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.dwBuildNumber);
        }

        std::string QueryCpuBrand()
        {
            char szBrand[kCpuBrandLength + 1] = {};
            for (int iLeaf = 0; kCpuidBrandLeafCount > iLeaf; ++iLeaf)
            {
                int arrRegisters[kCpuidRegisterCount] = {};
                __cpuid(arrRegisters, kCpuidBrandLeafFirst + iLeaf);
                std::memcpy(szBrand + static_cast<size_t>(iLeaf) * sizeof(arrRegisters), arrRegisters, sizeof(arrRegisters));
            }
            const std::string strRaw(szBrand);
            const size_t uFirst = strRaw.find_first_not_of(' ');
            if (std::string::npos == uFirst)
            {
                return kUnknown;
            }
            const size_t uLast = strRaw.find_last_not_of(' ');
            return strRaw.substr(uFirst, uLast - uFirst + 1);
        }

        CoreTopology QueryCoreTopology()
        {
            CoreTopology topology;
            DWORD dwLength = 0;
            GetLogicalProcessorInformationEx(RelationProcessorCore, nullptr, &dwLength);
            if (0 == dwLength)
            {
                return topology;
            }
            std::vector<BYTE> vecBuffer(dwLength);
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX pBuffer = reinterpret_cast<PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX>(vecBuffer.data());
            if (FALSE == GetLogicalProcessorInformationEx(RelationProcessorCore, pBuffer, &dwLength))
            {
                return topology;
            }
            std::map<BYTE, UINT> mapCoresByClass;
            DWORD dwOffset = 0;
            while (dwLength > dwOffset)
            {
                const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX* pInfo = reinterpret_cast<const SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX*>(vecBuffer.data() + dwOffset);
                if (RelationProcessorCore == pInfo->Relationship)
                {
                    ++topology.m_uCores;
                    ++mapCoresByClass[pInfo->Processor.EfficiencyClass];
                }
                dwOffset += pInfo->Size;
            }
            if (true == mapCoresByClass.empty())
            {
                return topology;
            }
            const UINT uHighestClassCores = mapCoresByClass.rbegin()->second;
            if (1 < mapCoresByClass.size())
            {
                topology.m_uPerformanceCores = uHighestClassCores;
                topology.m_uEfficiencyCores = topology.m_uCores - uHighestClassCores;
            }
            else
            {
                topology.m_uPerformanceCores = topology.m_uCores;
                topology.m_uEfficiencyCores = 0;
            }
            return topology;
        }

        UINT64 QueryTotalMemoryMb()
        {
            MEMORYSTATUSEX msex{};
            msex.dwLength = sizeof(msex);
            if (FALSE == GlobalMemoryStatusEx(&msex))
            {
                return 0;
            }
            return msex.ullTotalPhys / kBytesPerMb;
        }

        std::string QueryCompiler()
        {
            return std::format("MSVC {}.{}.{}", _MSC_VER / kMscVerMajorDivisor, _MSC_VER % kMscVerMajorDivisor, _MSC_FULL_VER % kMscFullVerBuildModulus);
        }

        std::string QueryBuildConfig()
        {
#if defined(_DEBUG)
            return "Debug";
#else
            return "Release";
#endif
        }

        MonitorInfo QueryMonitorFromHandle(HMONITOR hMonitor)
        {
            MonitorInfo info;
            MONITORINFOEXW mi{};
            mi.cbSize = sizeof(mi);
            if (FALSE == GetMonitorInfoW(hMonitor, &mi))
            {
                return info;
            }
            info.m_strDeviceName = ToUtf8(mi.szDevice);
            info.m_bPrimary = 0 != (mi.dwFlags & MONITORINFOF_PRIMARY);
            DEVMODEW dm{};
            dm.dmSize = sizeof(dm);
            if (FALSE != EnumDisplaySettingsW(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm))
            {
                info.m_uWidth = dm.dmPelsWidth;
                info.m_uHeight = dm.dmPelsHeight;
                info.m_uRefreshHz = dm.dmDisplayFrequency;
            }
            DISPLAY_DEVICEW dd{};
            dd.cb = sizeof(dd);
            for (DWORD iDevice = 0; FALSE != EnumDisplayDevicesW(nullptr, iDevice, &dd, 0); ++iDevice)
            {
                if (0 == std::wcscmp(dd.DeviceName, mi.szDevice))
                {
                    info.m_strAdapterName = ToUtf8(dd.DeviceString);
                    break;
                }
            }
            UINT uDpiX = 0;
            UINT uDpiY = 0;
            if (SUCCEEDED(GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &uDpiX, &uDpiY)))
            {
                info.m_uDpi = uDpiY;
            }
            return info;
        }

        BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam)
        {
            std::vector<MonitorInfo>* pVecMonitors = reinterpret_cast<std::vector<MonitorInfo>*>(lParam);
            pVecMonitors->push_back(QueryMonitorFromHandle(hMonitor));
            return TRUE;
        }
    }

    SystemInfo QuerySystemInfo()
    {
        SystemInfo info;
        const CoreTopology topology = QueryCoreTopology();
        info.m_strOsVersion = QueryOsVersion();
        info.m_strCpuBrand = QueryCpuBrand();
        info.m_uPhysicalCores = topology.m_uCores;
        info.m_uPerformanceCores = topology.m_uPerformanceCores;
        info.m_uEfficiencyCores = topology.m_uEfficiencyCores;
        info.m_uLogicalProcessors = GetActiveProcessorCount(ALL_PROCESSOR_GROUPS);
        info.m_uTotalMemoryMb = QueryTotalMemoryMb();
        info.m_strCompiler = QueryCompiler();
        info.m_strBuildConfig = QueryBuildConfig();
        info.m_strWindowsSdkVersion = kWindowsSdkVersion;
        return info;
    }

    MonitorInfo QueryMonitorInfo(HWND hWnd)
    {
        return QueryMonitorFromHandle(MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY));
    }

    std::vector<MonitorInfo> QueryMonitors()
    {
        std::vector<MonitorInfo> vecMonitors;
        EnumDisplayMonitors(nullptr, nullptr, &MonitorEnumProc, reinterpret_cast<LPARAM>(&vecMonitors));
        return vecMonitors;
    }

    void LogSystemInfo(const SystemInfo& info)
    {
        Log::Info("os windows {}", info.m_strOsVersion);
        Log::Info("cpu {} cores {} p {} e {} threads {}", info.m_strCpuBrand, info.m_uPhysicalCores, info.m_uPerformanceCores, info.m_uEfficiencyCores, info.m_uLogicalProcessors);
        Log::Info("ram {} MB", info.m_uTotalMemoryMb);
        Log::Info("compiler {} {} sdk {}", info.m_strCompiler, info.m_strBuildConfig, info.m_strWindowsSdkVersion);
    }

    void LogMonitorInfo(const MonitorInfo& info)
    {
        Log::Info("monitor {} {} {}x{} {}Hz dpi {} primary {}", info.m_strDeviceName, info.m_strAdapterName, info.m_uWidth, info.m_uHeight, info.m_uRefreshHz, info.m_uDpi, info.m_bPrimary);
    }
}
