#include "HDRDisplay.h"
#include <stdio.h>

// DebugView trace helper
#define TRACE(fmt, ...) do { wchar_t _b[512]; swprintf_s(_b, L##fmt, ##__VA_ARGS__); OutputDebugStringW(_b); } while(0)

bool HDRDisplay::Init(HWND hWnd) {
    TRACE("[HDRDisplay] Init enter");

    // 获取客户区尺寸
    RECT rc;
    GetClientRect(hWnd, &rc);
    TRACE("[HDRDisplay] Client rect before Init: %dx%d", rc.right - rc.left, rc.bottom - rc.top);

    // --- 1. 创建 D3D11 设备 ---
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL featLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, flags,
        nullptr, 0, D3D11_SDK_VERSION, &m_d3dDevice, &featLevel, nullptr);
    TRACE("[HDRDisplay] D3D11CreateDevice hr=0x%08X featLevel=0x%X", (unsigned)hr, (unsigned)featLevel);
    if (FAILED(hr)) return false;

    wil::com_ptr<IDXGIDevice> dxgiDevice;
    hr = m_d3dDevice.As(&dxgiDevice);
    TRACE("[HDRDisplay] QI IDXGIDevice hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    wil::com_ptr<IDXGIAdapter> adapter;
    hr = dxgiDevice->GetAdapter(&adapter);
    TRACE("[HDRDisplay] GetAdapter hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    wil::com_ptr<IDXGIFactory2> factory;
    hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    TRACE("[HDRDisplay] GetParent(IDXGIFactory2) hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    // --- 2. 检测 HDR 支持 ---
    wil::com_ptr<IDXGIOutput> output;
    wil::com_ptr<IDXGIOutput6> output6;
    if (SUCCEEDED(adapter->EnumOutputs(0, &output)) &&
        SUCCEEDED(output.As(&output6))) {
        DXGI_OUTPUT_DESC1 desc1 = {};
        if (SUCCEEDED(output6->GetDesc1(&desc1))) {
            m_hdrSupported = (desc1.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            m_colorSpace = desc1.ColorSpace;
        }
    }
    TRACE("[HDRDisplay] HDR detection: supported=%d colorSpace=%d", m_hdrSupported, (int)m_colorSpace);

    // --- 3. 创建交换链 (FP16) ---
    GetClientRect(hWnd, &rc);
    TRACE("[HDRDisplay] Client rect for swap chain: %dx%d", rc.right - rc.left, rc.bottom - rc.top);

    DXGI_SWAP_CHAIN_DESC1 scDesc = {};
    scDesc.Width       = static_cast<UINT>(rc.right - rc.left);
    scDesc.Height      = static_cast<UINT>(rc.bottom - rc.top);
    scDesc.Format      = DXGI_FORMAT_R16G16B16A16_FLOAT;
    scDesc.SampleDesc  = {1, 0};
    scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scDesc.BufferCount = 2;
    scDesc.Scaling     = DXGI_SCALING_NONE;
    scDesc.SwapEffect  = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    scDesc.AlphaMode   = DXGI_ALPHA_MODE_IGNORE;

    TRACE("[HDRDisplay] Creating swap chain %ux%u FP16...", scDesc.Width, scDesc.Height);
    wil::com_ptr<IDXGISwapChain1> sc1;
    hr = factory->CreateSwapChainForHwnd(m_d3dDevice.Get(), hWnd, &scDesc,
        nullptr, nullptr, &sc1);
    TRACE("[HDRDisplay] CreateSwapChainForHwnd hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    hr = sc1.As(&m_swapChain);
    TRACE("[HDRDisplay] QI IDXGISwapChain3 hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    // 设置 HDR 色彩空间
    if (m_hdrSupported) {
        wil::com_ptr<IDXGISwapChain4> sc4;
        if (SUCCEEDED(m_swapChain.As(&sc4))) {
            sc4->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
            TRACE("[HDRDisplay] SetColorSpace1 to HDR10 (PQ/2020)");
        }
    }

    // --- 4. 创建 Direct2D ---
    hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, IID_PPV_ARGS(&m_d2dFactory));
    TRACE("[HDRDisplay] D2D1CreateFactory hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    wil::com_ptr<ID2D1Device> d2dDevice;
    hr = m_d2dFactory->CreateDevice(dxgiDevice.Get(), &d2dDevice);
    TRACE("[HDRDisplay] CreateDevice hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    hr = d2dDevice->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &m_d2dContext);
    TRACE("[HDRDisplay] CreateDeviceContext hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return false;

    TRACE("[HDRDisplay] Calling Resize...");
    Resize();

    TRACE("[HDRDisplay] Init exit OK");
    return true;
}

void HDRDisplay::Resize() {
    TRACE("[HDRDisplay] Resize enter");

    if (!m_d2dContext || !m_swapChain) {
        TRACE("[HDRDisplay] Resize: null context or swapchain, abort");
        return;
    }

    m_d2dContext->SetTarget(nullptr);

    HRESULT hr = m_swapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
    TRACE("[HDRDisplay] ResizeBuffers hr=0x%08X", (unsigned)hr);

    wil::com_ptr<IDXGISurface> backBuffer;
    hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    TRACE("[HDRDisplay] GetBuffer hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return;

    D2D1_BITMAP_PROPERTIES1 bp = {};
    bp.pixelFormat.format       = DXGI_FORMAT_R16G16B16A16_FLOAT;
    bp.pixelFormat.alphaMode    = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bp.bitmapOptions            = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;

    wil::com_ptr<ID2D1Bitmap1> target;
    hr = m_d2dContext->CreateBitmapFromDxgiSurface(backBuffer.Get(), &bp, &target);
    TRACE("[HDRDisplay] CreateBitmapFromDxgiSurface hr=0x%08X", (unsigned)hr);
    if (FAILED(hr)) return;

    m_d2dContext->SetTarget(target.Get());
    TRACE("[HDRDisplay] Resize exit OK");
}
