#pragma once

#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <string>

namespace RendererDX11
{
    struct DeviceSelection
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> m_pAdapter;
        Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext;
        D3D_FEATURE_LEVEL m_eFeatureLevel = D3D_FEATURE_LEVEL_11_0;
        LUID m_luidAdapter = {};
    };

    bool SelectAdapterAndCreateDevice(IDXGIFactory6* pFactory, const std::wstring& strAdapterFilter, UINT uCreateFlags, DeviceSelection& selection);
}
