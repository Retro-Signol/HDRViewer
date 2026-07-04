#pragma once
#include <windows.h>
#include <d2d1_3.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <vector>
#include "ImageDecoder.h"

namespace wil {
    template<typename T>
    using com_ptr = Microsoft::WRL::ComPtr<T>;
}

class HDRDisplay;

class Renderer {
    wil::com_ptr<ID2D1Bitmap1>   m_imageBitmap;
    wil::com_ptr<IDWriteFactory> m_dwriteFactory;
    wil::com_ptr<IDWriteTextFormat> m_textFormat;
    HDRDisplay* m_display = nullptr;

    void DrawEmptyHint(ID2D1DeviceContext* ctx, int windowW, int windowH);

public:
    void Init(HDRDisplay* display);
    void UploadImage(const ImageData& scRGBData);
    void Render(float zoom, float panX, float panY,
                int windowW, int windowH, bool fullscreen);
};
