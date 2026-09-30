#include "DX11Renderer.h"

#include "DX11Adapter.h"
#include "DX11ShaderCompiler.h"
#include "RendererDX11/RendererDX11.h"

#include "Core/CpuTimer.h"
#include "Core/HrCheck.h"
#include "Core/Log.h"

#include "backends/imgui_impl_dx11.h"
#include "imgui.h"

#include <string_view>
#include <vector>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

namespace RendererDX11
{
    namespace
    {
        constexpr float kClearColor[4] = { 0.05f, 0.10f, 0.35f, 1.0f };
        constexpr DXGI_FORMAT kBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        constexpr UINT kSampleCount = 1;
        constexpr UINT kRenderTargetCount = 1;
        constexpr UINT kSyncIntervalVsync = 1;
        constexpr UINT kSyncIntervalOff = 0;

        bool IsDeviceRemovedError(HRESULT hrResult)
        {
            return DXGI_ERROR_DEVICE_REMOVED == hrResult || DXGI_ERROR_DEVICE_RESET == hrResult;
        }

        const char* SeverityTag(D3D11_MESSAGE_SEVERITY eSeverity)
        {
            switch (eSeverity)
            {
            case D3D11_MESSAGE_SEVERITY_CORRUPTION:
            {
                return "CORRUPTION";
            }
            case D3D11_MESSAGE_SEVERITY_ERROR:
            {
                return "ERROR";
            }
            case D3D11_MESSAGE_SEVERITY_WARNING:
            {
                return "WARNING";
            }
            default:
            {
                return "INFO";
            }
            }
        }
    }

    DX11Renderer::DX11Renderer()
        : m_hWnd(nullptr)
        , m_config()
        , m_stats()
        , m_pFactory()
        , m_pAdapter()
        , m_pDevice()
        , m_pContext()
        , m_pInfoQueue()
        , m_pSwapChain()
        , m_pBackBufferRtv()
        , m_eFeatureLevel(D3D_FEATURE_LEVEL_11_0)
        , m_uSwapChainFlags(0)
        , m_bTearingSupported(false)
        , m_bOverlayReady(false)
    {
    }

    DX11Renderer::~DX11Renderer()
    {
        if (true == m_bOverlayReady)
        {
            ImGui_ImplDX11_Shutdown();
            m_bOverlayReady = false;
        }
        if (nullptr != m_pContext)
        {
            m_pContext->ClearState();
            m_pContext->Flush();
        }
    }

    bool DX11Renderer::Init(HWND hWnd, const Core::RendererConfig& config)
    {
        m_hWnd = hWnd;
        m_config = config;

        if (false == CreateFactory())
        {
            return false;
        }

        UINT uCreateFlags = 0;
        if (true == m_config.m_bDebugLayer)
        {
            uCreateFlags |= static_cast<UINT>(D3D11_CREATE_DEVICE_DEBUG);
        }
        DeviceSelection selection;
        if (false == SelectAdapterAndCreateDevice(m_pFactory.Get(), m_config.m_strAdapterFilter, uCreateFlags, selection))
        {
            return false;
        }
        m_pAdapter = selection.m_pAdapter;
        m_pDevice = selection.m_pDevice;
        m_pContext = selection.m_pContext;
        m_eFeatureLevel = selection.m_eFeatureLevel;

        if (true == m_config.m_bDebugLayer)
        {
            SetupInfoQueue();
        }
        if (false == CreateSwapChain())
        {
            return false;
        }
        const HRESULT hrView = CreateBackBufferView();
        if (FAILED(hrView))
        {
            Core::Log::Error("dx11 rtv failed {}", Core::HrToString(hrView));
            return false;
        }
        if (false == InitOverlay())
        {
            return false;
        }
        Core::Log::Info("dx11 init {}x{} buffers {} tearing {} debug {}", m_config.m_uWidth, m_config.m_uHeight, m_config.m_uBackBufferCount, m_bTearingSupported, m_config.m_bDebugLayer);
        LogSetupTestCompile(m_config.m_strShaderDir);
        return true;
    }

    bool DX11Renderer::CreateFactory()
    {
        UINT uFlags = 0;
        if (true == m_config.m_bDebugLayer)
        {
            uFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
        const HRESULT hrFactory = CreateDXGIFactory2(uFlags, IID_PPV_ARGS(&m_pFactory));
        if (FAILED(hrFactory))
        {
            Core::Log::Error("CreateDXGIFactory2 failed {}", Core::HrToString(hrFactory));
            return false;
        }
        BOOL bAllowTearing = FALSE;
        const HRESULT hrTearing = m_pFactory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &bAllowTearing, sizeof(bAllowTearing));
        m_bTearingSupported = SUCCEEDED(hrTearing) && FALSE != bAllowTearing;
        Core::Log::Info("tearing {}", m_bTearingSupported);
        return true;
    }

    void DX11Renderer::SetupInfoQueue()
    {
        if (FAILED(m_pDevice.As(&m_pInfoQueue)))
        {
            Core::Log::Warn("ID3D11InfoQueue not available");
            return;
        }
#if defined(_DEBUG)
        if (FALSE != IsDebuggerPresent())
        {
            m_pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            m_pInfoQueue->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
        }
#endif
    }

    bool DX11Renderer::CreateSwapChain()
    {
        RECT rcClient{};
        GetClientRect(m_hWnd, &rcClient);
        const UINT uClientWidth = static_cast<UINT>(rcClient.right - rcClient.left);
        const UINT uClientHeight = static_cast<UINT>(rcClient.bottom - rcClient.top);
        if (0 != uClientWidth && 0 != uClientHeight)
        {
            m_config.m_uWidth = uClientWidth;
            m_config.m_uHeight = uClientHeight;
        }

        m_uSwapChainFlags = 0;
        if (true == m_bTearingSupported)
        {
            m_uSwapChainFlags = static_cast<UINT>(DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING);
        }

        DXGI_SWAP_CHAIN_DESC1 desc{};
        desc.Width = m_config.m_uWidth;
        desc.Height = m_config.m_uHeight;
        desc.Format = kBackBufferFormat;
        desc.SampleDesc.Count = kSampleCount;
        desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        desc.BufferCount = m_config.m_uBackBufferCount;
        desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        desc.Flags = m_uSwapChainFlags;

        const HRESULT hrSwapChain = m_pFactory->CreateSwapChainForHwnd(m_pDevice.Get(), m_hWnd, &desc, nullptr, nullptr, &m_pSwapChain);
        if (FAILED(hrSwapChain))
        {
            Core::Log::Error("CreateSwapChainForHwnd failed {}", Core::HrToString(hrSwapChain));
            return false;
        }
        const HRESULT hrAssociation = m_pFactory->MakeWindowAssociation(m_hWnd, DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(hrAssociation))
        {
            Core::Log::Warn("MakeWindowAssociation failed {}", Core::HrToString(hrAssociation));
        }
        return true;
    }

    HRESULT DX11Renderer::CreateBackBufferView()
    {
        Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
        const HRESULT hrBuffer = m_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
        if (FAILED(hrBuffer))
        {
            return hrBuffer;
        }
        return m_pDevice->CreateRenderTargetView(pBackBuffer.Get(), nullptr, &m_pBackBufferRtv);
    }

    bool DX11Renderer::InitOverlay()
    {
        if (nullptr == ImGui::GetCurrentContext())
        {
            Core::Log::Info("dx11 overlay off");
            return true;
        }
        if (false == ImGui_ImplDX11_Init(m_pDevice.Get(), m_pContext.Get()))
        {
            Core::Log::Error("ImGui_ImplDX11_Init failed");
            return false;
        }
        m_bOverlayReady = true;
        if (false == ImGui_ImplDX11_CreateDeviceObjects())
        {
            Core::Log::Error("ImGui_ImplDX11_CreateDeviceObjects failed");
            return false;
        }
        Core::Log::Info("dx11 overlay imgui {}", IMGUI_VERSION);
        return true;
    }

    void DX11Renderer::DrawOverlay()
    {
        if (false == m_bOverlayReady)
        {
            return;
        }
        ImDrawData* pDrawData = ImGui::GetDrawData();
        if (nullptr == pDrawData)
        {
            return;
        }
        ImGui_ImplDX11_RenderDrawData(pDrawData);
    }

    void DX11Renderer::LoadLevel(const Core::LevelData&)
    {
    }

    void DX11Renderer::RenderFrame(const Core::CameraData&)
    {
        if (nullptr == m_pSwapChain || true == m_stats.m_bDeviceRemoved)
        {
            return;
        }
        Core::CpuTimer timerFrame;

        ID3D11RenderTargetView* arrRtvs[kRenderTargetCount] = { m_pBackBufferRtv.Get() };
        m_pContext->OMSetRenderTargets(kRenderTargetCount, arrRtvs, nullptr);
        m_pContext->ClearRenderTargetView(m_pBackBufferRtv.Get(), kClearColor);
        DrawOverlay();

        UINT uSyncInterval = kSyncIntervalOff;
        UINT uPresentFlags = 0;
        if (true == m_config.m_bVsync)
        {
            uSyncInterval = kSyncIntervalVsync;
        }
        else if (true == m_bTearingSupported)
        {
            uPresentFlags = DXGI_PRESENT_ALLOW_TEARING;
        }

        Core::CpuTimer timerSubmit;
        const HRESULT hrPresent = m_pSwapChain->Present(uSyncInterval, uPresentFlags);
        m_stats.m_dCpuSubmitMs = timerSubmit.ElapsedMs();
        if (true == IsDeviceRemovedError(hrPresent))
        {
            HandleDeviceRemoved(hrPresent);
            return;
        }
        HR_CHECK(hrPresent);

        m_stats.m_dCpuFrameMs = timerFrame.ElapsedMs();
        CollectDebugMessages();
    }

    void DX11Renderer::Resize(UINT uWidth, UINT uHeight)
    {
        if (nullptr == m_pSwapChain || true == m_stats.m_bDeviceRemoved)
        {
            return;
        }
        if (0 == uWidth || 0 == uHeight)
        {
            return;
        }
        if (uWidth == m_config.m_uWidth && uHeight == m_config.m_uHeight)
        {
            return;
        }

        m_pContext->OMSetRenderTargets(0, nullptr, nullptr);
        m_pBackBufferRtv.Reset();
        m_pContext->Flush();

        const HRESULT hrResize = m_pSwapChain->ResizeBuffers(0, uWidth, uHeight, DXGI_FORMAT_UNKNOWN, m_uSwapChainFlags);
        if (true == IsDeviceRemovedError(hrResize))
        {
            HandleDeviceRemoved(hrResize);
            return;
        }
        HR_CHECK(hrResize);
        HR_CHECK(CreateBackBufferView());

        m_config.m_uWidth = uWidth;
        m_config.m_uHeight = uHeight;
        Core::Log::Info("dx11 resize {}x{}", uWidth, uHeight);
    }

    Core::FrameStats DX11Renderer::GetStats() const
    {
        return m_stats;
    }

    void DX11Renderer::CollectDebugMessages()
    {
        if (nullptr == m_pInfoQueue)
        {
            return;
        }
        const UINT64 uCount = m_pInfoQueue->GetNumStoredMessages();
        std::vector<BYTE> vecBuffer;
        for (UINT64 iMessage = 0; uCount > iMessage; ++iMessage)
        {
            SIZE_T uLength = 0;
            if (FAILED(m_pInfoQueue->GetMessage(iMessage, nullptr, &uLength)) || 0 == uLength)
            {
                continue;
            }
            vecBuffer.resize(uLength);
            D3D11_MESSAGE* pMessage = reinterpret_cast<D3D11_MESSAGE*>(vecBuffer.data());
            if (FAILED(m_pInfoQueue->GetMessage(iMessage, pMessage, &uLength)))
            {
                continue;
            }
            if (D3D11_MESSAGE_SEVERITY_WARNING >= pMessage->Severity)
            {
                ++m_stats.m_uDebugMessages;
                Core::Log::Warn("d3d11 {} {} {}", SeverityTag(pMessage->Severity), static_cast<int>(pMessage->ID), std::string_view(pMessage->pDescription));
            }
        }
        m_pInfoQueue->ClearStoredMessages();
    }

    void DX11Renderer::HandleDeviceRemoved(HRESULT hrResult)
    {
        const HRESULT hrReason = m_pDevice->GetDeviceRemovedReason();
        Core::Log::Error("dx11 device removed {} reason {}", Core::HrToString(hrResult), Core::HrToString(hrReason));
        m_stats.m_bDeviceRemoved = true;
    }

    std::unique_ptr<Core::IRenderer> CreateDX11Renderer()
    {
        return std::make_unique<DX11Renderer>();
    }
}
