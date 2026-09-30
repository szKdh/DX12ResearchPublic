#include "RendererDX11/RendererDX11.h"

#include "DX11Adapter.h"

#include "Core/HrCheck.h"
#include "Core/Log.h"

#include <d3d11_4.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <format>
#include <string>

namespace RendererDX11
{
    namespace
    {
        constexpr UINT kQueryPollLimit = 1000;
        constexpr DWORD kQueryPollSleepMs = 1;
        constexpr int kLabelWidth = 28;

        std::string FeatureLevelName(D3D_FEATURE_LEVEL eLevel)
        {
            switch (eLevel)
            {
            case D3D_FEATURE_LEVEL_11_0:
            {
                return "11_0";
            }
            case D3D_FEATURE_LEVEL_11_1:
            {
                return "11_1";
            }
            case D3D_FEATURE_LEVEL_12_0:
            {
                return "12_0";
            }
            case D3D_FEATURE_LEVEL_12_1:
            {
                return "12_1";
            }
            case D3D_FEATURE_LEVEL_12_2:
            {
                return "12_2";
            }
            default:
            {
                return std::format("0x{:X}", static_cast<unsigned int>(eLevel));
            }
            }
        }

        std::string LuidText(const LUID& luid)
        {
            return std::format("{:08X}-{:08X}", static_cast<unsigned long>(luid.HighPart), luid.LowPart);
        }

        const char* YesNo(BOOL bValue)
        {
            if (FALSE == bValue)
            {
                return "no";
            }
            return "yes";
        }

        void AppendRow(std::string& strOut, const char* szLabel, const std::string& strValue)
        {
            strOut += std::format("  {:<{}}{}\n", szLabel, kLabelWidth, strValue);
        }

        std::string TimestampFrequencyText(ID3D11Device* pDevice, ID3D11DeviceContext* pContext)
        {
            D3D11_QUERY_DESC queryDesc{};
            queryDesc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
            Microsoft::WRL::ComPtr<ID3D11Query> pQuery;
            const HRESULT hrQuery = pDevice->CreateQuery(&queryDesc, &pQuery);
            if (FAILED(hrQuery))
            {
                return Core::HrToString(hrQuery);
            }
            pContext->Begin(pQuery.Get());
            pContext->End(pQuery.Get());
            pContext->Flush();
            D3D11_QUERY_DATA_TIMESTAMP_DISJOINT data{};
            for (UINT iPoll = 0; kQueryPollLimit > iPoll; ++iPoll)
            {
                const HRESULT hrData = pContext->GetData(pQuery.Get(), &data, sizeof(data), 0);
                if (S_OK == hrData)
                {
                    return std::format("{} Hz (disjoint {})", data.Frequency, YesNo(data.Disjoint));
                }
                if (FAILED(hrData))
                {
                    return Core::HrToString(hrData);
                }
                Sleep(kQueryPollSleepMs);
            }
            return "timeout";
        }
    }

    std::string DumpDX11Features(const std::wstring& strAdapterFilter)
    {
        std::string strOut = "DX11\n";
        Microsoft::WRL::ComPtr<IDXGIFactory6> pFactory;
        const HRESULT hrFactory = CreateDXGIFactory2(0, IID_PPV_ARGS(&pFactory));
        if (FAILED(hrFactory))
        {
            AppendRow(strOut, "CreateDXGIFactory2", Core::HrToString(hrFactory));
            strOut += "\n";
            return strOut;
        }
        DeviceSelection selection;
        if (false == SelectAdapterAndCreateDevice(pFactory.Get(), strAdapterFilter, 0, selection))
        {
            AppendRow(strOut, "Adapter", "none");
            strOut += "\n";
            return strOut;
        }
        ID3D11Device* pDevice = selection.m_pDevice.Get();
        DXGI_ADAPTER_DESC1 desc{};
        selection.m_pAdapter->GetDesc1(&desc);
        AppendRow(strOut, "Adapter", std::format("{} (LUID {})", Core::ToUtf8(desc.Description), LuidText(selection.m_luidAdapter)));
        AppendRow(strOut, "Feature level", FeatureLevelName(selection.m_eFeatureLevel));

        D3D11_FEATURE_DATA_THREADING threading{};
        if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_THREADING, &threading, sizeof(threading))))
        {
            AppendRow(strOut, "Driver command lists", YesNo(threading.DriverCommandLists));
            AppendRow(strOut, "Driver concurrent creates", YesNo(threading.DriverConcurrentCreates));
        }
        else
        {
            AppendRow(strOut, "Threading query", "failed");
        }

        D3D11_FEATURE_DATA_D3D11_OPTIONS2 options2{};
        if (SUCCEEDED(pDevice->CheckFeatureSupport(D3D11_FEATURE_D3D11_OPTIONS2, &options2, sizeof(options2))))
        {
            AppendRow(strOut, "Unified memory", YesNo(options2.UnifiedMemoryArchitecture));
        }

        AppendRow(strOut, "Timestamp frequency", TimestampFrequencyText(pDevice, selection.m_pContext.Get()));
        strOut += "\n";
        return strOut;
    }
}
