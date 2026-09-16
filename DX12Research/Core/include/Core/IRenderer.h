#pragma once

#include "Core/RendererConfig.h"
#include "Core/LevelData.h"

#include <windows.h>
#include <DirectXMath.h>

namespace Core
{
    inline constexpr DirectX::XMFLOAT4X4 kIdentityMatrix(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f);

    struct CameraData
    {
        DirectX::XMFLOAT4X4 m_mView = kIdentityMatrix;
        DirectX::XMFLOAT4X4 m_mProj = kIdentityMatrix;
        DirectX::XMFLOAT3 m_vPosition = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
    };

    struct FrameStats
    {
        double m_dCpuFrameMs = 0.0;
        double m_dCpuSubmitMs = 0.0;
        double m_dGpuFrameMs = 0.0;
        UINT m_uDrawCalls = 0;
        UINT m_uInstances = 0;
        UINT m_uDebugMessages = 0;
        bool m_bDeviceRemoved = false;
    };

    struct IRenderer
    {
        virtual ~IRenderer() = default;
        virtual bool       Init(HWND hWnd, const RendererConfig& config) = 0;
        virtual void       LoadLevel(const LevelData& level) = 0;
        virtual void       RenderFrame(const CameraData& camera) = 0;
        virtual void       Resize(UINT uWidth, UINT uHeight) = 0;
        virtual FrameStats GetStats() const = 0;
    };
}
