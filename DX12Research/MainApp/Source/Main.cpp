#include "App.h"

#include <windows.h>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    MainApp::App app;
    return app.Run();
}
