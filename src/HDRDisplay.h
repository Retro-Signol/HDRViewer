#pragma once
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <d2d1_3.h>
#include <wrl/client.h>

namespace wil {
    template<typename T>
    using com_ptr = Microsoft::WRL::ComPtr<T>;
}

class HDRDisplay {
    wil::com_ptr<ID3D11Device>        m_d3dDevice;
    wil::com_ptr<IDXGISwapChain3>     m_swapChain;
    wil::com_ptr<ID2D1DeviceContext>  m_d2dContext;
    wil::com_ptr<ID2D1Factory3>       m_d2dFactory;

    bool m_hdrSupported = false;
    DXGI_COLOR_SPACE_TYPE m_colorSpace = DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709;

public:
    bool Init(HWND hWnd);
    bool IsHDR() const { return m_hdrSupported; }
    void Resize();
    ID2D1DeviceContext* GetD2DContext() const { return m_d2dContext.Get(); }
    IDXGISwapChain3* GetSwapChain() const { return m_swapChain.Get(); }
    ID3D11Device* GetD3DDevice() const { return m_d3dDevice.Get(); }
};
