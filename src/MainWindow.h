#pragma once
#include <windows.h>
#include <shellapi.h>
#include <commdlg.h>
#include <string>
#include <memory>
#include "ImageDecoder.h"

class HDRDisplay;
class Renderer;
class SlideShow;

class MainWindow {
    HINSTANCE m_hInst = nullptr;
    HWND m_hWnd = nullptr;
    HWND m_hStatusBar = nullptr;

    // 渲染
    std::unique_ptr<HDRDisplay> m_display;
    std::unique_ptr<Renderer> m_renderer;

    // 图片状态
    std::unique_ptr<SlideShow> m_slideshow;
    ImageData m_currentImage;
    float m_zoom = 1.0f;
    float m_panX = 0.0f, m_panY = 0.0f;
    int m_imgWidth = 0, m_imgHeight = 0;

    // 交互状态
    bool m_fullscreen = false;
    bool m_panning = false;
    int m_panStartX = 0, m_panStartY = 0;
    float m_panBaseX = 0.0f, m_panBaseY = 0.0f;

    // 窗口尺寸限制
    int m_minWinW = 0, m_minWinH = 0;
    int m_maxWinW = 0, m_maxWinH = 0;

    void UpdateWindowSize();
    void OnPaint();
    void OnSize();
    void OnMouseWheel(short delta, int screenX, int screenY);
    void OnKeyDown(WPARAM wParam);
    void ToggleFullscreen();
    void OpenImage(const std::wstring& path);
    void UpdateTitleAndStatus();
    void RecenterImage(int renderW, int renderH);

    void OnDropFiles(HDROP hDrop);
    void OpenFileDialog();
    void OnContextMenu(int x, int y);

    static LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wp, LPARAM lp);

public:
    explicit MainWindow(HINSTANCE hInst);
    ~MainWindow();

    bool Create(const std::wstring& imagePath, bool startFullscreen);
    HWND GetHWND() const { return m_hWnd; }
};
