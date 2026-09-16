#include "Core/Window.h"

#include "Core/Log.h"

#include <utility>

namespace Core
{
    namespace
    {
        constexpr const wchar_t kWindowClassName[] = L"DX12ResearchWindow";
        constexpr DWORD kWindowStyle = WS_OVERLAPPEDWINDOW;
        constexpr DWORD kWindowExStyle = 0;

        void EnableDpiAwareness()
        {
            static bool s_bEnabled = false;
            if (true == s_bEnabled)
            {
                return;
            }
            s_bEnabled = true;
            if (FALSE == SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2))
            {
                Log::Warn("SetProcessDpiAwarenessContext failed {}", GetLastError());
            }
        }
    }

    Window::Window()
        : m_hInstance(GetModuleHandleW(nullptr))
        , m_hWnd(nullptr)
        , m_atomClass(0)
        , m_uClientWidth(0)
        , m_uClientHeight(0)
        , m_bClosed(false)
        , m_fnOnResize()
        , m_fnMessageHook()
    {
    }

    Window::~Window()
    {
        if (nullptr != m_hWnd)
        {
            DestroyWindow(m_hWnd);
            m_hWnd = nullptr;
        }
        if (0 != m_atomClass)
        {
            UnregisterClassW(kWindowClassName, m_hInstance);
            m_atomClass = 0;
        }
    }

    bool Window::Create(UINT uClientWidth, UINT uClientHeight, const std::wstring& strTitle)
    {
        EnableDpiAwareness();

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = &Window::WndProc;
        wc.hInstance = m_hInstance;
        wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        wc.lpszClassName = kWindowClassName;
        m_atomClass = RegisterClassExW(&wc);
        if (0 == m_atomClass)
        {
            Log::Error("RegisterClassExW failed {}", GetLastError());
            return false;
        }

        m_hWnd = CreateWindowExW(kWindowExStyle, kWindowClassName, strTitle.c_str(), kWindowStyle, CW_USEDEFAULT, CW_USEDEFAULT, static_cast<int>(uClientWidth), static_cast<int>(uClientHeight), nullptr, nullptr, m_hInstance, this);
        if (nullptr == m_hWnd)
        {
            Log::Error("CreateWindowExW failed {}", GetLastError());
            return false;
        }
        if (false == ApplyClientSize(uClientWidth, uClientHeight))
        {
            return false;
        }
        ShowWindow(m_hWnd, SW_SHOW);
        return true;
    }

    bool Window::ApplyClientSize(UINT uClientWidth, UINT uClientHeight)
    {
        const UINT uDpi = GetDpiForWindow(m_hWnd);
        RECT rcWindow{};
        rcWindow.right = static_cast<LONG>(uClientWidth);
        rcWindow.bottom = static_cast<LONG>(uClientHeight);
        if (FALSE == AdjustWindowRectExForDpi(&rcWindow, kWindowStyle, FALSE, kWindowExStyle, uDpi))
        {
            Log::Error("AdjustWindowRectExForDpi failed {}", GetLastError());
            return false;
        }
        const int nWindowWidth = static_cast<int>(rcWindow.right - rcWindow.left);
        const int nWindowHeight = static_cast<int>(rcWindow.bottom - rcWindow.top);
        if (FALSE == SetWindowPos(m_hWnd, nullptr, 0, 0, nWindowWidth, nWindowHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE))
        {
            Log::Error("SetWindowPos failed {}", GetLastError());
            return false;
        }
        RECT rcClient{};
        GetClientRect(m_hWnd, &rcClient);
        m_uClientWidth = static_cast<UINT>(rcClient.right - rcClient.left);
        m_uClientHeight = static_cast<UINT>(rcClient.bottom - rcClient.top);
        Log::Info("client {}x{} dpi {}", m_uClientWidth, m_uClientHeight, uDpi);
        return true;
    }

    bool Window::PumpMessages()
    {
        MSG msg{};
        while (FALSE != PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
        {
            if (WM_QUIT == msg.message)
            {
                m_bClosed = true;
                return false;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        return false == m_bClosed;
    }

    void Window::SetResizeCallback(ResizeCallback fnOnResize)
    {
        m_fnOnResize = std::move(fnOnResize);
    }

    void Window::SetMessageHook(MessageHook fnHook)
    {
        m_fnMessageHook = std::move(fnHook);
    }

    void Window::SetTitle(const std::wstring& strTitle)
    {
        if (nullptr != m_hWnd)
        {
            SetWindowTextW(m_hWnd, strTitle.c_str());
        }
    }

    HWND Window::GetHwnd() const
    {
        return m_hWnd;
    }

    UINT Window::GetClientWidth() const
    {
        return m_uClientWidth;
    }

    UINT Window::GetClientHeight() const
    {
        return m_uClientHeight;
    }

    bool Window::IsClosed() const
    {
        return m_bClosed;
    }

    LRESULT CALLBACK Window::WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (WM_NCCREATE == uMsg)
        {
            const CREATESTRUCTW* pCreate = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreate->lpCreateParams));
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        Window* pWindow = reinterpret_cast<Window*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (nullptr == pWindow)
        {
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        return pWindow->HandleMessage(hWnd, uMsg, wParam, lParam);
    }

    LRESULT Window::HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (nullptr != m_fnMessageHook)
        {
            LRESULT lResult = 0;
            if (true == m_fnMessageHook(hWnd, uMsg, wParam, lParam, lResult))
            {
                return lResult;
            }
        }

        switch (uMsg)
        {
        case WM_SIZE:
        {
            const UINT uSizeType = static_cast<UINT>(wParam);
            if (SIZE_MINIMIZED != uSizeType)
            {
                m_uClientWidth = LOWORD(lParam);
                m_uClientHeight = HIWORD(lParam);
                if (nullptr != m_fnOnResize && 0 != m_uClientWidth && 0 != m_uClientHeight)
                {
                    m_fnOnResize(m_uClientWidth, m_uClientHeight);
                }
            }
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(hWnd);
            return 0;
        }
        case WM_DESTROY:
        {
            m_hWnd = nullptr;
            m_bClosed = true;
            PostQuitMessage(0);
            return 0;
        }
        default:
        {
            return DefWindowProcW(hWnd, uMsg, wParam, lParam);
        }
        }
    }
}
