#include "DX11Adapter.h"

#include "Core/HrCheck.h"
#include "Core/Log.h"

#include <cwctype>
#include <format>
#include <iterator>

namespace RendererDX11
{
    namespace
    {
        constexpr D3D_FEATURE_LEVEL kFeatureLevels[] = { D3D_FEATURE_LEVEL_11_1, D3D_FEATURE_LEVEL_11_0 };
        constexpr UINT64 kBytesPerMb = 1024ull * 1024ull;

        std::wstring ToLower(std::wstring strText)
        {
            for (wchar_t& chValue : strText)
            {
                chValue = static_cast<wchar_t>(std::towlower(chValue));
            }
            return strText;
        }

        bool ContainsNoCase(const std::wstring& strText, const std::wstring& strPattern)
        {
            return std::wstring::npos != ToLower(strText).find(ToLower(strPattern));
        }

        std::string FormatDriverVersion(const LARGE_INTEGER& liVersion)
        {
            return std::format("{}.{}.{}.{}", HIWORD(liVersion.HighPart), LOWORD(liVersion.HighPart), HIWORD(liVersion.LowPart), LOWORD(liVersion.LowPart));
        }
    }

    bool SelectAdapterAndCreateDevice(IDXGIFactory6* pFactory, const std::wstring& strAdapterFilter, UINT uCreateFlags, DeviceSelection& selection)
    {
        for (UINT iAdapter = 0; ; ++iAdapter)
        {
            Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
            const HRESULT hrEnum = pFactory->EnumAdapterByGpuPreference(iAdapter, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter));
            if (DXGI_ERROR_NOT_FOUND == hrEnum)
            {
                break;
            }
            HR_CHECK(hrEnum);

            DXGI_ADAPTER_DESC1 desc{};
            HR_CHECK(pAdapter->GetDesc1(&desc));
            const std::string strName = Core::ToUtf8(desc.Description);
            if (0 != (desc.Flags & static_cast<UINT>(DXGI_ADAPTER_FLAG_SOFTWARE)))
            {
                Core::Log::Info("adapter {} {} skip software", iAdapter, strName);
                continue;
            }
            if (false == strAdapterFilter.empty() && false == ContainsNoCase(desc.Description, strAdapterFilter))
            {
                Core::Log::Info("adapter {} {} skip filter", iAdapter, strName);
                continue;
            }

            Microsoft::WRL::ComPtr<ID3D11Device> pDevice;
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext;
            D3D_FEATURE_LEVEL eFeatureLevel = D3D_FEATURE_LEVEL_11_0;
            const HRESULT hrCreate = D3D11CreateDevice(pAdapter.Get(), D3D_DRIVER_TYPE_UNKNOWN, nullptr, uCreateFlags, kFeatureLevels, static_cast<UINT>(std::size(kFeatureLevels)), D3D11_SDK_VERSION, &pDevice, &eFeatureLevel, &pContext);
            if (FAILED(hrCreate))
            {
                Core::Log::Warn("adapter {} {} skip D3D11CreateDevice {}", iAdapter, strName, Core::HrToString(hrCreate));
                continue;
            }

            LARGE_INTEGER liDriver{};
            std::string strDriver = "unknown";
            if (SUCCEEDED(pAdapter->CheckInterfaceSupport(__uuidof(IDXGIDevice), &liDriver)))
            {
                strDriver = FormatDriverVersion(liDriver);
            }
            Core::Log::Info("adapter {} {} luid {:08X}-{:08X} vram {} MB driver {} fl 0x{:X}", iAdapter, strName, static_cast<unsigned long>(desc.AdapterLuid.HighPart), desc.AdapterLuid.LowPart, desc.DedicatedVideoMemory / kBytesPerMb, strDriver, static_cast<unsigned int>(eFeatureLevel));

            selection.m_pAdapter = pAdapter;
            selection.m_pDevice = pDevice;
            selection.m_pContext = pContext;
            selection.m_eFeatureLevel = eFeatureLevel;
            selection.m_luidAdapter = desc.AdapterLuid;
            return true;
        }
        Core::Log::Error("adapter not found");
        return false;
    }
}
