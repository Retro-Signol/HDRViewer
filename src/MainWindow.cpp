#include "MainWindow.h"
#include "HDRDisplay.h"
#include "Renderer.h"
#include "SlideShow.h"
#include "ColorPipeline.h"
#include <windowsx.h>
#include <dwmapi.h>
#include <shellscalingapi.h>
#include <commdlg.h>

// DebugView trace helper
#define TRACE(fmt, ...) do { wchar_t _b[512]; swprintf_s(_b, L##fmt, ##__VA_ARGS__); OutputDebugStringW(_b); } while(0)

#pragma comment(lib, "dwmapi.lib")

MainWindow::MainWindow(HINSTANCE hInst)
    : m_hInst(hInst)
{
}

MainWindow::~MainWindow() = default;

bool MainWindow::Create(const std::wstring& imagePath, bool startFullscreen) {
    TRACE("[MainWindow] Create enter");
    // 注册窗口类
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = m_hInst;
    wc.hCursor       = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"HDRViewerClass";
    wc.hIcon         = LoadIcon(nullptr, IDI_APPLICATION);
    if (!RegisterClassExW(&wc)) {
        TRACE("[MainWindow] RegisterClassExW FAILED");
        return false;
    }
    TRACE("[MainWindow] RegisterClass OK");

    // 获取屏幕尺寸
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    m_maxWinW = screenW;
    m_maxWinH = screenH;

    // 尝试加载图片（仅解码获取尺寸，渲染器尚未就绪）
    if (!imagePath.empty()) {
        OpenImage(imagePath);
    }

    // 自动决定启动模式: 图片宽或高 ≥ 屏幕 × 0.85 则全屏
    if (!startFullscreen && m_imgWidth > 0 && m_imgHeight > 0) {
        startFullscreen = (m_imgWidth >= screenW * 0.85f ||
                           m_imgHeight >= screenH * 0.85f);
    }

    // 计算最小窗口尺寸 (原图 1/10)
    m_minWinW = std::max(200, static_cast<int>(m_imgWidth * 0.1f));
    m_minWinH = std::max(150, static_cast<int>(m_imgHeight * 0.1f));

    DWORD style;
    int winW, winH;
    int winX = CW_USEDEFAULT, winY = CW_USEDEFAULT;

    if (startFullscreen) {
        m_fullscreen = true;
        style = WS_POPUP;
        winW = screenW;
        winH = screenH;
        winX = 0;
        winY = 0;
    } else {
        style = WS_OVERLAPPEDWINDOW;
        m_zoom = 1.0f;
        if (m_imgWidth > 0 && m_imgHeight > 0) {
            winW = m_imgWidth;
            winH = m_imgHeight + 24;  // +24 状态栏
        } else {
            winW = 800;
            winH = 600;
        }
    }

    m_hWnd = CreateWindowExW(
        m_fullscreen ? WS_EX_APPWINDOW : 0,
        L"HDRViewerClass",
        L"HDRViewer",
        style,
        winX, winY, winW, winH,
        nullptr, nullptr, m_hInst, this);

    if (!m_hWnd) {
        TRACE("[MainWindow] CreateWindowExW FAILED, style=0x%X w=%d h=%d", style, winW, winH);
        return false;
    }
    TRACE("[MainWindow] CreateWindowExW hWnd=0x%p style=0x%X w=%d h=%d", (void*)m_hWnd, style, winW, winH);

    // 注册拖放
    DragAcceptFiles(m_hWnd, TRUE);
    TRACE("[MainWindow] DragAcceptFiles set");

    // 创建 D3D/D2D 设备
    TRACE("[MainWindow] Calling HDRDisplay::Init...");
    m_display = std::make_unique<HDRDisplay>();
    bool displayOk = m_display->Init(m_hWnd);
    TRACE("[MainWindow] HDRDisplay::Init returned %d", displayOk);

    m_renderer = std::make_unique<Renderer>();
    m_renderer->Init(m_display.get());
    TRACE("[MainWindow] Renderer::Init done");

    // 渲染器就绪后重新打开图片（上传位图）
    if (!imagePath.empty() && m_imgWidth > 0) {
        OpenImage(imagePath);
    }

    // 重新评估全屏（此时 m_imgWidth/m_imgHeight 有效）
    if (!m_fullscreen && m_imgWidth > 0 && m_imgHeight > 0) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        if (m_imgWidth >= sw * 0.85f || m_imgHeight >= sh * 0.85f) {
            m_fullscreen = true;
        }
    }

    // 状态栏（必须在全屏调整之前创建，以便隐藏）
    m_hStatusBar = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        0, 0, 0, 24,
        m_hWnd, nullptr, m_hInst, nullptr);

    // 全屏模式调整窗口
    if (m_fullscreen) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowLongPtrW(m_hWnd, GWL_STYLE, WS_POPUP);
        SetWindowPos(m_hWnd, HWND_TOP, 0, 0, sw, sh,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        if (m_imgWidth > 0 && m_imgHeight > 0) {
            float scaleX = (float)sw / m_imgWidth;
            float scaleY = (float)sh / m_imgHeight;
            m_zoom = std::min(scaleX, scaleY);
            m_panX = (sw - m_imgWidth * m_zoom) * 0.5f;
            m_panY = (sh - m_imgHeight * m_zoom) * 0.5f;
        }
        ShowWindow(m_hStatusBar, SW_HIDE);
        m_display->Resize();
    }

    UpdateTitleAndStatus();

    ShowWindow(m_hWnd, SW_SHOW);
    TRACE("[MainWindow] ShowWindow done, fullscreen=%d imgW=%d imgH=%d m_renderer=%p",
        m_fullscreen, m_imgWidth, m_imgHeight, (void*)m_renderer.get());
    UpdateWindowSize();
    TRACE("[MainWindow] Create exit OK");
    return true;
}

void MainWindow::UpdateWindowSize() {
    if (m_fullscreen || m_imgWidth <= 0 || m_imgHeight <= 0) return;

    int newW = std::clamp(static_cast<int>(m_imgWidth * m_zoom), m_minWinW, m_maxWinW);
    int newH = std::clamp(static_cast<int>(m_imgHeight * m_zoom), m_minWinH, m_maxWinH);
    int totalH = newH + 24;

    RECT wr;
    GetWindowRect(m_hWnd, &wr);
    int oldCx = wr.left + (wr.right - wr.left) / 2;
    int oldCy = wr.top  + (wr.bottom - wr.top) / 2;

    SetWindowPos(m_hWnd, nullptr,
        oldCx - newW / 2, oldCy - totalH / 2,
        newW, totalH,
        SWP_NOZORDER | SWP_NOACTIVATE);
}

void MainWindow::ToggleFullscreen() {
    if (m_imgWidth <= 0) return;

    m_fullscreen = !m_fullscreen;

    DWORD style = m_fullscreen ? WS_POPUP : WS_OVERLAPPEDWINDOW;
    SetWindowLongPtrW(m_hWnd, GWL_STYLE, style);

    if (m_fullscreen) {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        SetWindowPos(m_hWnd, HWND_TOP, 0, 0, sw, sh,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        float scaleX = (float)sw / m_imgWidth;
        float scaleY = (float)sh / m_imgHeight;
        m_zoom = std::min(scaleX, scaleY);
        m_panX = (sw - m_imgWidth * m_zoom) * 0.5f;
        m_panY = (sh - m_imgHeight * m_zoom) * 0.5f;
    } else {
        m_zoom = 1.0f;
        int winW = std::clamp(m_imgWidth, m_minWinW, m_maxWinW);
        int winH = std::clamp(m_imgHeight, m_minWinH, m_maxWinH);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int wx = (screenW - winW) / 2;
        int wy = (screenH - (winH + 24)) / 2;
        SetWindowPos(m_hWnd, nullptr, wx, wy, winW, winH + 24,
            SWP_FRAMECHANGED | SWP_SHOWWINDOW);
        RECT client;
        GetClientRect(m_hWnd, &client);
        RecenterImage(client.right, client.bottom - 24);
    }

    // 显示/隐藏状态栏
    ShowWindow(m_hStatusBar, m_fullscreen ? SW_HIDE : SW_SHOW);

    m_display->Resize();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void MainWindow::OpenImage(const std::wstring& path) {
    TRACE("[MainWindow] OpenImage '%s'", path.c_str());
    if (path.empty()) {
        TRACE("[MainWindow] OpenImage: path empty");
        return;
    }

    // 初始化 SlideShow (如果还没初始化)
    if (!m_slideshow) {
        m_slideshow = std::make_unique<SlideShow>();
        m_slideshow->LoadDirectory(path);
    }

    auto decoded = DecodeImage(path);
    if (!decoded) {
        TRACE("[MainWindow] OpenImage: DecodeImage FAILED");
        return;
    }

    m_currentImage = std::move(*decoded);
    m_imgWidth  = m_currentImage.width;
    m_imgHeight = m_currentImage.height;
    TRACE("[MainWindow] OpenImage: decoded %dx%d colorSpace=%d isHDR=%d",
        m_imgWidth, m_imgHeight, (int)m_currentImage.colorSpace, m_currentImage.isHDR);

    // 渲染器未就绪时，只记录尺寸，跳过绘制
    if (!m_renderer) {
        TRACE("[MainWindow] OpenImage: m_renderer null, skipping upload/paint");
        return;
    }

    // 转换为 scRGB
    std::vector<uint16_t> scRGBData;
    ColorPipeline::ToScRGB(m_currentImage, scRGBData, 80.0f);

    // 上传到渲染器
    ImageData scRGBImage;
    scRGBImage.pixels = std::move(scRGBData);
    scRGBImage.width  = m_imgWidth;
    scRGBImage.height = m_imgHeight;
    TRACE("[MainWindow] OpenImage: calling UploadImage");
    m_renderer->UploadImage(scRGBImage);
    TRACE("[MainWindow] OpenImage: UploadImage done");

    // 更新窗口
    if (!m_fullscreen) {
        m_zoom = 1.0f;
        int winW = std::clamp(m_imgWidth, m_minWinW, m_maxWinW);
        int winH = std::clamp(m_imgHeight, m_minWinH, m_maxWinH);
        int screenW = GetSystemMetrics(SM_CXSCREEN);
        int screenH = GetSystemMetrics(SM_CYSCREEN);
        int wx = (screenW - winW) / 2;
        int wy = (screenH - (winH + 24)) / 2;
        SetWindowPos(m_hWnd, nullptr, wx, wy, winW, winH + 24,
            SWP_NOZORDER | SWP_NOACTIVATE);
        RECT client;
        GetClientRect(m_hWnd, &client);
        RecenterImage(client.right, client.bottom - 24);
    } else {
        int sw = GetSystemMetrics(SM_CXSCREEN);
        int sh = GetSystemMetrics(SM_CYSCREEN);
        float scaleX = static_cast<float>(sw) / m_imgWidth;
        float scaleY = static_cast<float>(sh) / m_imgHeight;
        m_zoom = std::min(scaleX, scaleY);
        m_panX = (sw - m_imgWidth  * m_zoom) * 0.5f;
        m_panY = (sh - m_imgHeight * m_zoom) * 0.5f;
    }

    UpdateTitleAndStatus();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void MainWindow::RecenterImage(int renderW, int renderH) {
    if (renderW <= 0 || renderH <= 0) return;
    m_panX = (renderW - m_imgWidth * m_zoom) * 0.5f;
    m_panY = (renderH - m_imgHeight * m_zoom) * 0.5f;
}

void MainWindow::UpdateTitleAndStatus() {
    if (!m_hWnd) return;

    std::wstring title = L"HDRViewer";
    if (m_imgWidth > 0 && m_imgHeight > 0) {
        title += L" - " + std::to_wstring(m_imgWidth) + L"x" + std::to_wstring(m_imgHeight);
        if (m_currentImage.isHDR) title += L" HDR";
    }

    SetWindowTextW(m_hWnd, title.c_str());

    if (m_hStatusBar && m_imgWidth > 0 && m_imgHeight > 0) {
        std::wstring status = std::to_wstring(m_imgWidth) + L"x" + std::to_wstring(m_imgHeight);
        status += L"  Zoom: " + std::to_wstring(static_cast<int>(m_zoom * 100)) + L"%";
        if (m_currentImage.isHDR) status += L"  HDR";
        SetWindowTextW(m_hStatusBar, status.c_str());
    }
}

// ---- 消息处理 ----

LRESULT CALLBACK MainWindow::WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(hWnd, GWLP_USERDATA));

    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCT*>(lp);
        self = static_cast<MainWindow*>(cs->lpCreateParams);
        SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        self->m_hWnd = hWnd;
        return 0;
    }

    if (!self) return DefWindowProcW(hWnd, msg, wp, lp);

    switch (msg) {
        case WM_PAINT:
            TRACE("[MainWindow] WM_PAINT");
            self->OnPaint();
            return 0;
        case WM_SIZE:
            TRACE("[MainWindow] WM_SIZE wp=%llu", (unsigned long long)wp);
            self->OnSize();
            return 0;
        case WM_ERASEBKGND: return 1;  // 避免闪烁
        case WM_MOUSEWHEEL:
            self->OnMouseWheel(GET_WHEEL_DELTA_WPARAM(wp),
                               GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_LBUTTONDOWN:
            self->m_panStartX = GET_X_LPARAM(lp);
            self->m_panStartY = GET_Y_LPARAM(lp);
            self->m_panBaseX = self->m_panX;
            self->m_panBaseY = self->m_panY;
            self->m_panning = true;
            SetCapture(self->m_hWnd);
            return 0;
        case WM_MOUSEMOVE:
            if (self->m_panning) {
                int dx = GET_X_LPARAM(lp) - self->m_panStartX;
                int dy = GET_Y_LPARAM(lp) - self->m_panStartY;
                self->m_panX = self->m_panBaseX + static_cast<float>(dx);
                self->m_panY = self->m_panBaseY + static_cast<float>(dy);
                InvalidateRect(self->m_hWnd, nullptr, FALSE);
            }
            return 0;
        case WM_LBUTTONUP:
            self->m_panning = false;
            ReleaseCapture();
            return 0;
        case WM_LBUTTONDBLCLK: self->ToggleFullscreen(); return 0;
        case WM_KEYDOWN:    self->OnKeyDown(wp);       return 0;
        case WM_DROPFILES:
            self->OnDropFiles(reinterpret_cast<HDROP>(wp));
            return 0;
        case WM_RBUTTONUP:
            self->OnContextMenu(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
            return 0;
        case WM_DESTROY:    PostQuitMessage(0);        return 0;
    }
    return DefWindowProcW(hWnd, msg, wp, lp);
}

void MainWindow::OnPaint() {
    TRACE("[MainWindow] OnPaint enter");
    PAINTSTRUCT ps;
    BeginPaint(m_hWnd, &ps);

    if (m_renderer) {
        RECT rc;
        GetClientRect(m_hWnd, &rc);
        int clientH = rc.bottom - rc.top;
        // 减去状态栏高度
        int renderH = clientH - (m_hStatusBar && !m_fullscreen ? 24 : 0);
        TRACE("[MainWindow] OnPaint clientRect=%dx%d renderH=%d", rc.right, clientH, renderH);

        m_renderer->Render(m_zoom, m_panX, m_panY,
                           rc.right, renderH, m_fullscreen);
    } else {
        TRACE("[MainWindow] OnPaint: m_renderer is NULL!");
    }

    EndPaint(m_hWnd, &ps);
}

void MainWindow::OnSize() {
    TRACE("[MainWindow] OnSize enter");
    if (!m_hStatusBar) {
        TRACE("[MainWindow] OnSize: m_hStatusBar null");
        return;
    }

    // 重建交换链适配新窗口尺寸
    if (m_display) {
        TRACE("[MainWindow] OnSize: calling m_display->Resize()");
        m_display->Resize();
    }

    RECT rc;
    GetClientRect(m_hWnd, &rc);
    int statusH = m_fullscreen ? 0 : 24;
    SetWindowPos(m_hStatusBar, nullptr, 0, rc.bottom - statusH,
                 rc.right, statusH, SWP_NOZORDER);
    TRACE("[MainWindow] OnSize exit, client=%dx%d", rc.right, rc.bottom);
}

void MainWindow::OnMouseWheel(short delta, int /*screenX*/, int /*screenY*/) {
    if (m_imgWidth <= 0) return;
    float factor = (delta > 0) ? 1.1f : 1.0f / 1.1f;
    float newZoom = std::clamp(m_zoom * factor, 0.1f, 10.0f);
    if (newZoom == m_zoom) return;

    // 1) 缩放前：记录当前视口中心对应图像哪一点
    RECT rc; GetClientRect(m_hWnd, &rc);
    int renderH = (rc.bottom - rc.top) - (m_fullscreen ? 0 : 24);
    float vpCx = rc.right * 0.5f;
    float vpCy = renderH * 0.5f;
    float imgX = (vpCx - m_panX) / m_zoom;
    float imgY = (vpCy - m_panY) / m_zoom;

    // 2) 更新 zoom，窗口化时同步窗口大小
    m_zoom = newZoom;
    if (!m_fullscreen) UpdateWindowSize();

    // 3) 缩放后：用新视口中心，让同一图像点回到视口中心
    GetClientRect(m_hWnd, &rc);
    renderH = (rc.bottom - rc.top) - (m_fullscreen ? 0 : 24);
    m_panX = rc.right * 0.5f - imgX * m_zoom;
    m_panY = renderH * 0.5f - imgY * m_zoom;

    UpdateTitleAndStatus();
    InvalidateRect(m_hWnd, nullptr, FALSE);
}

void MainWindow::OnKeyDown(WPARAM wParam) {
    switch (wParam) {
        case VK_ESCAPE:
            DestroyWindow(m_hWnd);
            break;
        case VK_LEFT:
        case VK_UP:
            if (m_slideshow) OpenImage(m_slideshow->Prev());
            break;
        case VK_RIGHT:
        case VK_DOWN:
            if (m_slideshow) OpenImage(m_slideshow->Next());
            break;
        case VK_F11:
            ToggleFullscreen();
            break;
        case VK_F10:
            ToggleFullscreen();
            break;
        case VK_RETURN:
            if (GetKeyState(VK_MENU) < 0) ToggleFullscreen();
            break;
        case '0':
            m_zoom = 1.0f;
            {
                RECT rc; GetClientRect(m_hWnd, &rc);
                int rh = (rc.bottom - rc.top) - (m_fullscreen ? 0 : 24);
                RecenterImage(rc.right, rh);
            }
            if (!m_fullscreen) UpdateWindowSize();
            InvalidateRect(m_hWnd, nullptr, FALSE);
            break;
        case 'O':
            if (GetKeyState(VK_CONTROL) < 0) OpenFileDialog();
            break;
    }
}

void MainWindow::OnDropFiles(HDROP hDrop) {
    wchar_t path[MAX_PATH];
    UINT n = DragQueryFileW(hDrop, 0, path, MAX_PATH);
    TRACE("[MainWindow] OnDropFiles count=%u path='%s'", (unsigned)n, path);
    if (n > 0) OpenImage(path);
    DragFinish(hDrop);
}

void MainWindow::OpenFileDialog() {
    wchar_t path[MAX_PATH] = {};
    OPENFILENAMEW ofn = { sizeof(ofn) };
    ofn.hwndOwner = m_hWnd;
    ofn.lpstrFilter = L"所有图像\0*.jxl;*.exr;*.jxr;*.wdp;*.hdp;*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.gif\0"
                      L"JPEG XL (*.jxl)\0*.jxl\0"
                      L"OpenEXR (*.exr)\0*.exr\0"
                      L"JPEG XR (*.jxr)\0*.jxr;*.wdp;*.hdp\0"
                      L"PNG (*.png)\0*.png\0"
                      L"JPEG (*.jpg)\0*.jpg;*.jpeg\0"
                      L"所有文件 (*.*)\0*.*\0";
    ofn.lpstrFile = path;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (GetOpenFileNameW(&ofn)) {
        OpenImage(path);
    }
}

void MainWindow::OnContextMenu(int x, int y) {
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;

    AppendMenuW(hMenu, MF_STRING, 1, L"打开文件...\tCtrl+O");
    AppendMenuW(hMenu, MF_STRING, 2, L"全屏\tF11");
    if (m_imgWidth > 0) {
        AppendMenuW(hMenu, MF_STRING, 3, L"原始大小\t0");
    }

    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY,
                             x, y, 0, m_hWnd, nullptr);
    if (cmd == 1) OpenFileDialog();
    else if (cmd == 2) ToggleFullscreen();
    else if (cmd == 3) {
        m_zoom = 1.0f;
        OnKeyDown('0');
    }
    DestroyMenu(hMenu);
}
