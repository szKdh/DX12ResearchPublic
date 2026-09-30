#pragma once

#include "Core/CpuTimer.h"
#include "Core/IRenderer.h"
#include "Core/RendererConfig.h"
#include "Core/Window.h"

#include <memory>
#include <string>
#include <vector>

namespace MainApp
{
    enum class EExitCode
    {
        Ok = 0,
        InitFailed = 1,
        DebugMessages = 2,
        DeviceRemoved = 3,
    };

    inline constexpr size_t kAdapterFilterLength = 128;

    struct MeasureResult
    {
        Core::EGraphicsApi m_eApi = Core::EGraphicsApi::DX11;
        UINT m_uFrames = 0;
        double m_dTotalMs = 0.0;
        double m_dAverageMs = 0.0;
        double m_dMinMs = 0.0;
        double m_dMaxMs = 0.0;
        double m_dMedianMs = 0.0;
        double m_dP99Ms = 0.0;
        double m_dCpuFrameMs = 0.0;
        double m_dCpuSubmitMs = 0.0;
        UINT m_uDebugMessages = 0;
        bool m_bCancelled = false;
    };

    class App
    {
    public:
        App();
        ~App();
        App(const App&) = delete;
        App& operator=(const App&) = delete;

        int Run();

    private:
        bool InitOverlay();
        void ShutdownOverlay();
        bool CreateRenderer();
        void DestroyRenderer();
        void ApplyPending();
        bool HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);
        void BuildOverlay();
        void DrawStatusLine();
        void DrawRendererSection();
        void DrawMeasureSection();
        void DrawEnvironmentSection();
        void DrawLogSection();
        void BeginMeasure();
        void RecordMeasureFrame(double dFrameMs, const Core::FrameStats& stats);
        void FinishMeasure(bool bCancelled);
        void UpdateTitle();
        UINT TotalDebugMessages() const;
        int ExitCode() const;

        Core::Window m_window;
        std::unique_ptr<Core::IRenderer> m_pRenderer;
        Core::EGraphicsApi m_eApi;
        Core::RendererConfig m_config;
        Core::RendererConfig m_configPending;
        char m_szAdapterFilter[kAdapterFilterLength];
        int m_nMeasureFrames;
        bool m_bOverlayReady;
        bool m_bApplyRequested;
        bool m_bMeasureRequested;
        bool m_bMeasuring;
        UINT m_uMeasureTarget;
        std::vector<double> m_vecMeasureFrameMs;
        double m_dMeasureCpuFrameSum;
        double m_dMeasureCpuSubmitSum;
        UINT m_uMeasureDebugStart;
        MeasureResult m_result;
        bool m_bHasResult;
        std::string m_strStatus;
        std::wstring m_strEnvPath;
        UINT m_uDebugMessagesRetired;
        bool m_bDeviceRemoved;
        bool m_bInitFailed;
        double m_dSmoothedFrameMs;
        Core::FrameTimer m_timer;
    };
}
