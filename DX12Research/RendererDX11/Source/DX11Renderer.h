#pragma once

#include "Core/IRenderer.h"

#include <d3d11.h>
#include <d3d11sdklayers.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

namespace RendererDX11
{
    class DX11Renderer final : public Core::IRenderer
    {
    public:
        DX11Renderer();
        ~DX11Renderer() override;

        bool Init(HWND hWnd, const Core::RendererConfig& config) override;
        void LoadLevel(const Core::LevelData& level) override;
        void RenderFrame(const Core::CameraData& camera) override;
        void Resize(UINT uWidth, UINT uHeight) override;
        Core::FrameStats GetStats() const override;

    private:
        bool CreateFactory();
        void SetupInfoQueue();
        bool CreateSwapChain();
        HRESULT CreateBackBufferView();
        bool InitOverlay();
        void DrawOverlay();
        void CollectDebugMessages();
        void HandleDeviceRemoved(HRESULT hrResult);

        HWND m_hWnd;
        Core::RendererConfig m_config;
        Core::FrameStats m_stats;
        Microsoft::WRL::ComPtr<IDXGIFactory6> m_pFactory;
        Microsoft::WRL::ComPtr<IDXGIAdapter1> m_pAdapter;
        Microsoft::WRL::ComPtr<ID3D11Device> m_pDevice;
        Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_pContext;
        Microsoft::WRL::ComPtr<ID3D11InfoQueue> m_pInfoQueue;
        Microsoft::WRL::ComPtr<IDXGISwapChain1> m_pSwapChain;
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pBackBufferRtv;
        D3D_FEATURE_LEVEL m_eFeatureLevel;
        UINT m_uSwapChainFlags;
        bool m_bTearingSupported;
        bool m_bOverlayReady;
    };
}
