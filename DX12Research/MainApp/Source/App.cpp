#include "App.h"

#include "EnvironmentInfo.h"

#include "Core/Log.h"
#include "Core/SystemInfo.h"
#include "RendererDX11/RendererDX11.h"

#include "backends/imgui_impl_win32.h"
#include "imgui.h"

#include <algorithm>
#include <filesystem>
#include <format>
#include <numeric>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

namespace MainApp
{
    namespace
    {
        constexpr const wchar_t kWindowTitle[] = L"DX12Research";
        constexpr const wchar_t kShaderDirectoryName[] = L"Shaders";
        constexpr const char kPanelTitle[] = "DX12Research";
        constexpr int kDefaultMeasureFrames = 300;
        constexpr int kMinMeasureFrames = 1;
        constexpr int kMaxMeasureFrames = 100000;
        constexpr double kFrameSmoothing = 0.05;
        constexpr double kMsPerSecond = 1000.0;
        constexpr double kP99Ratio = 0.99;
        constexpr float kPanelPosX = 10.0f;
        constexpr float kPanelPosY = 10.0f;
        constexpr float kLogWidth = 640.0f;
        constexpr float kLogHeight = 200.0f;

        const wchar_t* ApiName(Core::EGraphicsApi eApi)
        {
            if (Core::EGraphicsApi::DX11 == eApi)
            {
                return L"dx11";
            }
            return L"dx12";
        }

        const char* ApiNameUtf8(Core::EGraphicsApi eApi)
        {
            if (Core::EGraphicsApi::DX11 == eApi)
            {
                return "dx11";
            }
            return "dx12";
        }

        std::wstring DefaultShaderDirectory()
        {
            wchar_t szPath[MAX_PATH] = {};
            if (0 == GetModuleFileNameW(nullptr, szPath, MAX_PATH))
            {
                return std::wstring(kShaderDirectoryName);
            }
            return (std::filesystem::path(szPath).parent_path() / kShaderDirectoryName).wstring();
        }
    }

    App::App()
        : m_window()
        , m_pRenderer()
        , m_eApi(Core::EGraphicsApi::DX11)
        , m_config()
        , m_configPending()
        , m_szAdapterFilter()
        , m_nMeasureFrames(kDefaultMeasureFrames)
        , m_bOverlayReady(false)
        , m_bApplyRequested(false)
        , m_bMeasureRequested(false)
        , m_bMeasuring(false)
        , m_uMeasureTarget(0)
        , m_vecMeasureFrameMs()
        , m_dMeasureCpuFrameSum(0.0)
        , m_dMeasureCpuSubmitSum(0.0)
        , m_uMeasureDebugStart(0)
        , m_result()
        , m_bHasResult(false)
        , m_strStatus()
        , m_strEnvPath()
        , m_uDebugMessagesRetired(0)
        , m_bDeviceRemoved(false)
        , m_bInitFailed(false)
        , m_dSmoothedFrameMs(0.0)
        , m_timer()
    {
    }

    App::~App()
    {
        DestroyRenderer();
        ShutdownOverlay();
    }

    int App::Run()
    {
        const std::wstring strLogPath = Core::Log::MakeDefaultFilePath();
        if (false == Core::Log::Open(strLogPath))
        {
            Core::Log::Warn("log open failed {}", Core::ToUtf8(strLogPath));
        }
        else
        {
            Core::Log::Info("log {}", Core::ToUtf8(Core::Log::GetFilePath()));
        }
        Core::LogSystemInfo(Core::QuerySystemInfo());

        if (false == m_window.Create(m_config.m_uWidth, m_config.m_uHeight, kWindowTitle))
        {
            Core::Log::Error("window failed");
            Core::Log::Close();
            return static_cast<int>(EExitCode::InitFailed);
        }
        Core::LogMonitorInfo(Core::QueryMonitorInfo(m_window.GetHwnd()));

        m_config.m_strShaderDir = DefaultShaderDirectory();
        m_configPending = m_config;
        Core::Log::Info("shaders {}", Core::ToUtf8(m_config.m_strShaderDir));

        if (false == InitOverlay())
        {
            Core::Log::Close();
            return static_cast<int>(EExitCode::InitFailed);
        }
        m_window.SetMessageHook([this](HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
        {
            return HandleMessage(hWnd, uMsg, wParam, lParam, lResult);
        });
        m_window.SetResizeCallback([this](UINT uWidth, UINT uHeight)
        {
            Core::Log::Info("resize {}x{}", uWidth, uHeight);
            if (nullptr != m_pRenderer)
            {
                m_pRenderer->Resize(uWidth, uHeight);
            }
        });

        if (false == CreateRenderer())
        {
            m_bInitFailed = true;
        }

        Core::CameraData camera;
        while (false == m_bInitFailed && true == m_window.PumpMessages())
        {
            if (false == m_bMeasuring)
            {
                ImGui_ImplWin32_NewFrame();
                ImGui::NewFrame();
                BuildOverlay();
                ImGui::Render();
            }
            m_pRenderer->RenderFrame(camera);
            m_timer.Tick();
            const Core::FrameStats stats = m_pRenderer->GetStats();
            if (true == stats.m_bDeviceRemoved)
            {
                m_bDeviceRemoved = true;
                break;
            }
            const double dFrameMs = m_timer.GetDeltaMs();
            m_dSmoothedFrameMs += (dFrameMs - m_dSmoothedFrameMs) * kFrameSmoothing;
            if (true == m_bMeasuring)
            {
                RecordMeasureFrame(dFrameMs, stats);
            }
            else if (true == m_bMeasureRequested)
            {
                BeginMeasure();
            }
            else if (true == m_bApplyRequested)
            {
                ApplyPending();
            }
        }

        double dAverageMs = 0.0;
        if (0 != m_timer.GetFrameCount())
        {
            dAverageMs = m_timer.GetTotalMs() / static_cast<double>(m_timer.GetFrameCount());
        }
        Core::Log::Info("frames {} total {:.3f}ms avg {:.3f}ms", m_timer.GetFrameCount(), m_timer.GetTotalMs(), dAverageMs);

        m_window.SetResizeCallback(nullptr);
        m_window.SetMessageHook(nullptr);
        DestroyRenderer();
        ShutdownOverlay();

        const int nExitCode = ExitCode();
        if (true == m_bDeviceRemoved)
        {
            Core::Log::Error("device removed");
        }
        if (0 != m_uDebugMessagesRetired)
        {
            Core::Log::Error("debug messages {}", m_uDebugMessagesRetired);
        }
        Core::Log::Info("exit {}", nExitCode);
        Core::Log::Close();
        return nExitCode;
    }

    bool App::InitOverlay()
    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.LogFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        ImGui::StyleColorsDark();
        if (false == ImGui_ImplWin32_Init(m_window.GetHwnd()))
        {
            Core::Log::Error("ImGui_ImplWin32_Init failed");
            ImGui::DestroyContext();
            return false;
        }
        const float fScale = ImGui_ImplWin32_GetDpiScaleForHwnd(m_window.GetHwnd());
        ImGui::GetStyle().ScaleAllSizes(fScale);
        ImGui::GetStyle().FontScaleDpi = fScale;
        m_bOverlayReady = true;
        Core::Log::Info("overlay imgui {} scale {:.2f}", IMGUI_VERSION, fScale);
        return true;
    }

    void App::ShutdownOverlay()
    {
        if (false == m_bOverlayReady)
        {
            return;
        }
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        m_bOverlayReady = false;
    }

    bool App::CreateRenderer()
    {
        m_pRenderer = RendererDX11::CreateDX11Renderer();
        Core::Log::Info("renderer {}", ApiNameUtf8(m_eApi));
        if (false == m_pRenderer->Init(m_window.GetHwnd(), m_config))
        {
            Core::Log::Error("init failed");
            m_pRenderer.reset();
            return false;
        }
        UpdateTitle();
        return true;
    }

    void App::DestroyRenderer()
    {
        if (nullptr == m_pRenderer)
        {
            return;
        }
        m_uDebugMessagesRetired += m_pRenderer->GetStats().m_uDebugMessages;
        m_pRenderer.reset();
    }

    void App::ApplyPending()
    {
        m_bApplyRequested = false;
        m_configPending.m_strAdapterFilter = Core::ToWide(m_szAdapterFilter);

        const Core::RendererConfig configPrevious = m_config;
        DestroyRenderer();
        m_config = m_configPending;
        if (true == CreateRenderer())
        {
            m_strStatus = std::format("applied {}", ApiNameUtf8(m_eApi));
            return;
        }

        Core::Log::Error("apply failed, restoring previous renderer");
        m_config = configPrevious;
        m_configPending = configPrevious;
        if (false == CreateRenderer())
        {
            m_bInitFailed = true;
            return;
        }
        m_strStatus = "apply failed, previous settings restored (see log)";
    }

    bool App::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)
    {
        if (WM_KEYDOWN == uMsg && VK_ESCAPE == wParam && true == m_bMeasuring)
        {
            FinishMeasure(true);
            lResult = 0;
            return true;
        }
        if (false == m_bOverlayReady)
        {
            return false;
        }
        lResult = ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam);
        return 0 != lResult;
    }

    void App::BuildOverlay()
    {
        ImGui::SetNextWindowPos(ImVec2(kPanelPosX, kPanelPosY), ImGuiCond_FirstUseEver);
        ImGui::Begin(kPanelTitle, nullptr, ImGuiWindowFlags_AlwaysAutoResize);
        DrawStatusLine();
        ImGui::SeparatorText("Renderer");
        DrawRendererSection();
        if (true == ImGui::Button("Apply (restart renderer)"))
        {
            m_bApplyRequested = true;
        }
        if (false == m_strStatus.empty())
        {
            ImGui::SameLine();
            ImGui::TextUnformatted(m_strStatus.c_str());
        }
        ImGui::SeparatorText("Measure");
        DrawMeasureSection();
        ImGui::SeparatorText("Environment");
        DrawEnvironmentSection();
        if (true == ImGui::CollapsingHeader("Log"))
        {
            DrawLogSection();
        }
        ImGui::End();
    }

    void App::DrawStatusLine()
    {
        double dFps = 0.0;
        if (0.0 < m_dSmoothedFrameMs)
        {
            dFps = kMsPerSecond / m_dSmoothedFrameMs;
        }
        ImGui::Text("active %s   frame %.3f ms (%.0f fps)   debug messages %u", ApiNameUtf8(m_eApi), m_dSmoothedFrameMs, dFps, TotalDebugMessages());
    }

    void App::DrawRendererSection()
    {
        ImGui::InputText("Adapter filter", m_szAdapterFilter, sizeof(m_szAdapterFilter));
        ImGui::Checkbox("VSync", &m_configPending.m_bVsync);
        ImGui::Checkbox("Debug layer", &m_configPending.m_bDebugLayer);
        ImGui::Checkbox("DX11 instancing", &m_configPending.m_bDx11Instancing);
    }

    void App::DrawMeasureSection()
    {
        ImGui::InputInt("Frames", &m_nMeasureFrames);
        m_nMeasureFrames = std::clamp(m_nMeasureFrames, kMinMeasureFrames, kMaxMeasureFrames);
        ImGui::SameLine();
        if (true == ImGui::Button("Start"))
        {
            m_bMeasureRequested = true;
        }
        ImGui::SameLine();
        ImGui::TextUnformatted("overlay hidden while measuring, Esc cancels");
        if (false == m_bHasResult)
        {
            return;
        }
        const char* szCancelled = "";
        if (true == m_result.m_bCancelled)
        {
            szCancelled = " (cancelled)";
        }
        ImGui::Text("%s %u frames%s   total %.3f ms", ApiNameUtf8(m_result.m_eApi), m_result.m_uFrames, szCancelled, m_result.m_dTotalMs);
        ImGui::Text("frame ms   avg %.3f   min %.3f   median %.3f   p99 %.3f   max %.3f", m_result.m_dAverageMs, m_result.m_dMinMs, m_result.m_dMedianMs, m_result.m_dP99Ms, m_result.m_dMaxMs);
        ImGui::Text("renderer   cpu %.3f ms   submit %.3f ms   debug messages %u", m_result.m_dCpuFrameMs, m_result.m_dCpuSubmitMs, m_result.m_uDebugMessages);
    }

    void App::DrawEnvironmentSection()
    {
        if (true == ImGui::Button("Save environment info"))
        {
            m_strEnvPath = SaveEnvironmentInfo(Core::ToWide(m_szAdapterFilter));
        }
        if (false == m_strEnvPath.empty())
        {
            ImGui::SameLine();
            ImGui::TextUnformatted(Core::ToUtf8(m_strEnvPath).c_str());
        }
    }

    void App::DrawLogSection()
    {
        ImGui::BeginChild("log", ImVec2(kLogWidth, kLogHeight), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
        const std::vector<std::string> vecLines = Core::Log::CopyRecentLines();
        for (const std::string& strLine : vecLines)
        {
            ImGui::TextUnformatted(strLine.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        {
            ImGui::SetScrollHereY(1.0f);
        }
        ImGui::EndChild();
    }

    void App::BeginMeasure()
    {
        m_bMeasureRequested = false;
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        ImGui::EndFrame();
        m_bMeasuring = true;
        m_uMeasureTarget = static_cast<UINT>(m_nMeasureFrames);
        m_vecMeasureFrameMs.clear();
        m_vecMeasureFrameMs.reserve(m_uMeasureTarget);
        m_dMeasureCpuFrameSum = 0.0;
        m_dMeasureCpuSubmitSum = 0.0;
        m_uMeasureDebugStart = m_pRenderer->GetStats().m_uDebugMessages;
        UpdateTitle();
        Core::Log::Info("measure start {} frames {}", ApiNameUtf8(m_eApi), m_uMeasureTarget);
    }

    void App::RecordMeasureFrame(double dFrameMs, const Core::FrameStats& stats)
    {
        m_vecMeasureFrameMs.push_back(dFrameMs);
        m_dMeasureCpuFrameSum += stats.m_dCpuFrameMs;
        m_dMeasureCpuSubmitSum += stats.m_dCpuSubmitMs;
        if (m_uMeasureTarget <= m_vecMeasureFrameMs.size())
        {
            FinishMeasure(false);
        }
    }

    void App::FinishMeasure(bool bCancelled)
    {
        m_bMeasuring = false;
        UpdateTitle();

        MeasureResult result;
        result.m_eApi = m_eApi;
        result.m_uFrames = static_cast<UINT>(m_vecMeasureFrameMs.size());
        result.m_bCancelled = bCancelled;
        if (0 != result.m_uFrames)
        {
            std::vector<double> vecSorted = m_vecMeasureFrameMs;
            std::sort(vecSorted.begin(), vecSorted.end());
            const double dFrames = static_cast<double>(result.m_uFrames);
            const size_t iP99 = std::min(vecSorted.size() - 1, static_cast<size_t>(dFrames * kP99Ratio));
            result.m_dTotalMs = std::accumulate(vecSorted.begin(), vecSorted.end(), 0.0);
            result.m_dAverageMs = result.m_dTotalMs / dFrames;
            result.m_dMinMs = vecSorted.front();
            result.m_dMaxMs = vecSorted.back();
            result.m_dMedianMs = vecSorted[vecSorted.size() / 2];
            result.m_dP99Ms = vecSorted[iP99];
            result.m_dCpuFrameMs = m_dMeasureCpuFrameSum / dFrames;
            result.m_dCpuSubmitMs = m_dMeasureCpuSubmitSum / dFrames;
        }
        result.m_uDebugMessages = m_pRenderer->GetStats().m_uDebugMessages - m_uMeasureDebugStart;
        m_result = result;
        m_bHasResult = true;
        Core::Log::Info("measure {} frames {} cancelled {} total {:.3f}ms avg {:.3f}ms min {:.3f}ms median {:.3f}ms p99 {:.3f}ms max {:.3f}ms cpu {:.3f}ms submit {:.3f}ms debug {}", ApiNameUtf8(result.m_eApi), result.m_uFrames, result.m_bCancelled, result.m_dTotalMs, result.m_dAverageMs, result.m_dMinMs, result.m_dMedianMs, result.m_dP99Ms, result.m_dMaxMs, result.m_dCpuFrameMs, result.m_dCpuSubmitMs, result.m_uDebugMessages);
    }

    void App::UpdateTitle()
    {
        std::wstring strTitle = std::format(L"{} {}", kWindowTitle, ApiName(m_eApi));
        if (true == m_bMeasuring)
        {
            strTitle += L" measuring";
        }
        m_window.SetTitle(strTitle);
    }

    UINT App::TotalDebugMessages() const
    {
        UINT uTotal = m_uDebugMessagesRetired;
        if (nullptr != m_pRenderer)
        {
            uTotal += m_pRenderer->GetStats().m_uDebugMessages;
        }
        return uTotal;
    }

    int App::ExitCode() const
    {
        if (true == m_bInitFailed)
        {
            return static_cast<int>(EExitCode::InitFailed);
        }
        if (true == m_bDeviceRemoved)
        {
            return static_cast<int>(EExitCode::DeviceRemoved);
        }
        if (0 != m_uDebugMessagesRetired)
        {
            return static_cast<int>(EExitCode::DebugMessages);
        }
        return static_cast<int>(EExitCode::Ok);
    }
}
