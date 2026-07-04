#include <windows.h>
#include <shellapi.h>
#include <shellscalingapi.h>
#include <string>
#include "MainWindow.h"
#include "RegistrySetup.h"

// DebugView trace helper
#define TRACE(fmt, ...) do { wchar_t _b[512]; swprintf_s(_b, L##fmt, ##__VA_ARGS__); OutputDebugStringW(_b); } while(0)

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE /*hPrev*/, LPSTR lpCmdLine, int /*nShowCmd*/) {
    TRACE("[HDRViewer] WinMain start");

    // 1. DPI 感知 (Per-Monitor V2)
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    // 2. 解析命令行 — 直接用 CommandLineToArgvW
    bool noRegistry = false;
    bool startFullscreen = false;
    std::wstring firstImage;

    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argv) {
        for (int i = 1; i < argc; i++) {
            std::wstring arg(argv[i]);
            if (arg == L"--no-registry")
                noRegistry = true;
            else if (arg == L"--fullscreen")
                startFullscreen = true;
            else if (firstImage.empty() && !arg.empty() && arg[0] != L'-')
                firstImage = arg;
        }
        LocalFree(argv);
    }

    TRACE("[HDRViewer] firstImage='%s' noRegistry=%d fullscreen=%d",
        firstImage.c_str(), noRegistry, startFullscreen);

    // 3. 注册表写入 (仅首次)
    if (!noRegistry) RegistrySetup::Ensure();
    TRACE("[HDRViewer] RegistrySetup done");

    // 4. COM 初始化
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    TRACE("[HDRViewer] CoInitializeEx hr=0x%08X", (unsigned)hr);

    // 5. 创建主窗口
    TRACE("[HDRViewer] Creating MainWindow...");
    MainWindow window(hInst);
    bool ok = window.Create(firstImage, startFullscreen);
    TRACE("[HDRViewer] MainWindow::Create returned %d", ok);
    if (!ok) {
        CoUninitialize();
        return 1;
    }

    // 6. 消息泵
    TRACE("[HDRViewer] Entering message loop");
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    TRACE("[HDRViewer] Exiting");
    CoUninitialize();
    return 0;
}
