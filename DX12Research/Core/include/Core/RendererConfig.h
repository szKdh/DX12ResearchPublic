#pragma once

#include <windows.h>

#include <string>

namespace Core
{
    enum class EGraphicsApi
    {
        DX11,
        DX12,
    };

    inline constexpr UINT kDefaultClientWidth = 1920;
    inline constexpr UINT kDefaultClientHeight = 1080;
    inline constexpr UINT kBackBufferCount = 3;
    inline constexpr UINT kDefaultRecordThreads = 1;

#if defined(_DEBUG)
    inline constexpr bool kDebugToolsDefault = true;
#else
    inline constexpr bool kDebugToolsDefault = false;
#endif

    struct RendererConfig
    {
        UINT m_uWidth = kDefaultClientWidth;
        UINT m_uHeight = kDefaultClientHeight;
        bool m_bVsync = false;
        UINT m_uBackBufferCount = kBackBufferCount;

        bool m_bDebugLayer = kDebugToolsDefault;
        bool m_bGpuValidation = kDebugToolsDefault;
        bool m_bDred = kDebugToolsDefault;
        std::wstring m_strAdapterFilter;
        std::wstring m_strShaderDir;

        bool m_bDx11Instancing = false;

        bool m_bFenceNBuffering = false;
        bool m_bUploadRing = false;
        bool m_bDescriptorTable = false;
        bool m_bBindless = false;
        bool m_bPsoPrecompile = false;
        bool m_bBarrierBatching = false;
        UINT m_uRecordThreads = kDefaultRecordThreads;
        bool m_bInstancing = false;
        bool m_bGpuDriven = false;
        bool m_bAsyncCompute = false;
    };
}
