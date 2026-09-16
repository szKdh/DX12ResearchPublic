#pragma once

#include <windows.h>

#include <functional>
#include <string>

namespace Core
{
    class Window
    {
    public:
        using ResizeCallback = std::function<void(UINT uWidth, UINT uHeight)>;
        using MessageHook = std::function<bool(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult)>;

        Window();
        ~Window();
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;

        bool Create(UINT uClientWidth, UINT uClientHeight, const std::wstring& strTitle);
        bool PumpMessages();
        void SetResizeCallback(ResizeCallback fnOnResize);
        void SetMessageHook(MessageHook fnHook);
        void SetTitle(const std::wstring& strTitle);

        HWND GetHwnd() const;
        UINT GetClientWidth() const;
        UINT GetClientHeight() const;
        bool IsClosed() const;

    private:
        static LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        LRESULT HandleMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
        bool ApplyClientSize(UINT uClientWidth, UINT uClientHeight);

        HINSTANCE m_hInstance;
        HWND m_hWnd;
        ATOM m_atomClass;
        UINT m_uClientWidth;
        UINT m_uClientHeight;
        bool m_bClosed;
        ResizeCallback m_fnOnResize;
        MessageHook m_fnMessageHook;
    };
}
