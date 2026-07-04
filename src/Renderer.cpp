#include "Renderer.h"
#include "HDRDisplay.h"
#include <stdio.h>

// DebugView trace helper
#define TRACE(fmt, ...) do { wchar_t _b[512]; swprintf_s(_b, L##fmt, ##__VA_ARGS__); OutputDebugStringW(_b); } while(0)

void Renderer::Init(HDRDisplay* display) {
    TRACE("[Renderer] Init enter, display=%p", (void*)display);
    m_display = display;

    // DWrite 工厂
    HRESULT hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
        __uuidof(IDWriteFactory),
        reinterpret_cast<IUnknown**>(m_dwriteFactory.GetAddressOf()));
    TRACE("[Renderer] DWriteCreateFactory hr=0x%08X", (unsigned)hr);

    if (m_dwriteFactory) {
        hr = m_dwriteFactory->CreateTextFormat(
            L"Segoe UI", nullptr,
            DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            22.0f,
            L"",
            &m_textFormat);
        TRACE("[Renderer] CreateTextFormat hr=0x%08X", (unsigned)hr);
        if (m_textFormat) {
            m_textFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            m_textFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            TRACE("[Renderer] TextFormat created and aligned");
        }
    }
    TRACE("[Renderer] Init exit");
}

void Renderer::UploadImage(const ImageData& scRGBData) {
    TRACE("[Renderer] UploadImage enter: %dx%d pixels.size=%zu",
        scRGBData.width, scRGBData.height, scRGBData.pixels.size());
    if (!m_display) {
        TRACE("[Renderer] UploadImage: m_display null, abort");
        return;
    }

    auto* ctx = m_display->GetD2DContext();
    if (!ctx) {
        TRACE("[Renderer] UploadImage: ctx null, abort");
        return;
    }

    m_imageBitmap.Reset();

    D2D1_SIZE_U size = { static_cast<UINT32>(scRGBData.width),
                         static_cast<UINT32>(scRGBData.height) };

    D2D1_BITMAP_PROPERTIES1 bp = {};
    bp.pixelFormat.format       = DXGI_FORMAT_R16G16B16A16_FLOAT;
    bp.pixelFormat.alphaMode    = D2D1_ALPHA_MODE_PREMULTIPLIED;
    bp.bitmapOptions            = D2D1_BITMAP_OPTIONS_NONE;
    bp.dpiX                     = 96.0f;
    bp.dpiY                     = 96.0f;

    UINT32 stride = static_cast<UINT32>(scRGBData.width) * 8;  // 4 * uint16 = 8 bytes
    UINT32 bufSize = static_cast<UINT32>(scRGBData.pixels.size()) *
                     static_cast<UINT32>(sizeof(uint16_t));

    TRACE("[Renderer] CreateBitmap: size=%dx%d stride=%u bufSize=%u", size.width, size.height, stride, bufSize);
    wil::com_ptr<ID2D1Bitmap1> bitmap;
    HRESULT hr = ctx->CreateBitmap(size, scRGBData.pixels.data(), stride, bp, &bitmap);
    TRACE("[Renderer] CreateBitmap hr=0x%08X", (unsigned)hr);
    if (SUCCEEDED(hr)) {
        m_imageBitmap = std::move(bitmap);
        TRACE("[Renderer] UploadImage: bitmap stored successfully");
    } else {
        TRACE("[Renderer] UploadImage: CreateBitmap FAILED!");
    }
    TRACE("[Renderer] UploadImage exit");
}

void Renderer::Render(float zoom, float panX, float panY,
                       int windowW, int windowH, bool fullscreen) {
    auto* ctx = m_display->GetD2DContext();
    if (!ctx) {
        TRACE("[Renderer] Render: NULL D2D context, abort");
        return;
    }

    ctx->BeginDraw();
    TRACE("[Renderer] BeginDraw done, window=%dx%d, zoom=%.2f, pan=(%.0f,%.0f), fs=%d",
        windowW, windowH, zoom, panX, panY, fullscreen);

    // 纯黑背景
    ctx->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 1.0f));

    if (m_imageBitmap) {
        auto bmpSize = m_imageBitmap->GetSize();
        TRACE("[Renderer] Drawing bitmap size=%.0fx%.0f", bmpSize.width, bmpSize.height);

        D2D1::Matrix3x2F transform = D2D1::Matrix3x2F::Identity();
        if (bmpSize.width > 0 && bmpSize.height > 0) {
            transform = D2D1::Matrix3x2F::Scale(zoom, zoom)
                      * D2D1::Matrix3x2F::Translation(panX, panY);
            TRACE("[Renderer] Transform: zoom=%.2f pan=(%.0f,%.0f)", zoom, panX, panY);
        }

        ctx->SetTransform(transform);
        ctx->DrawBitmap(m_imageBitmap.Get(), nullptr, 1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, nullptr);
        TRACE("[Renderer] DrawBitmap done");
    } else {
        TRACE("[Renderer] No image bitmap, drawing empty hint");
        DrawEmptyHint(ctx, windowW, windowH);
    }

    HRESULT hr = ctx->EndDraw();
    TRACE("[Renderer] EndDraw hr=0x%08X", (unsigned)hr);
    if (hr == D2DERR_RECREATE_TARGET) {
        TRACE("[Renderer] RECREATE_TARGET, calling Resize");
        m_imageBitmap.Reset();
        m_display->Resize();
    } else if (FAILED(hr)) {
        TRACE("[Renderer] EndDraw FAILED with error!");
    }

    if (auto* sc = m_display->GetSwapChain()) {
        HRESULT hrPresent = sc->Present(1, 0);
        TRACE("[Renderer] Present hr=0x%08X", (unsigned)hrPresent);
    } else {
        TRACE("[Renderer] No swap chain for Present!");
    }
}

void Renderer::DrawEmptyHint(ID2D1DeviceContext* ctx, int windowW, int windowH) {
    TRACE("[Renderer] DrawEmptyHint: %dx%d", windowW, windowH);
    if (!m_textFormat) {
        TRACE("[Renderer] DrawEmptyHint: m_textFormat null");
        return;
    }

    // 计算一个暗色半透明背景条
    float barH = 100.0f;
    float barY = (static_cast<float>(windowH) - barH) * 0.5f;

    D2D1_RECT_F bgRect = { 0, barY, static_cast<float>(windowW), barY + barH };
    wil::com_ptr<ID2D1SolidColorBrush> bgBrush;
    ctx->CreateSolidColorBrush(
        D2D1::ColorF(0.08f, 0.08f, 0.08f, 0.75f), &bgBrush);
    if (bgBrush) {
        ctx->FillRectangle(bgRect, bgBrush.Get());
    }

    // 白色提示文字
    wil::com_ptr<ID2D1SolidColorBrush> textBrush;
    ctx->CreateSolidColorBrush(
        D2D1::ColorF(1.0f, 1.0f, 1.0f, 0.85f), &textBrush);
    if (textBrush) {
        D2D1_RECT_F textRect = {
            20.0f, barY,
            static_cast<float>(windowW) - 20.0f, barY + barH
        };
        const wchar_t* hintText = L"拖放图片到此处 | Ctrl+O 打开文件";
        ctx->DrawText(
            hintText,
            static_cast<UINT32>(wcslen(hintText)), m_textFormat.Get(), &textRect, textBrush.Get());
        TRACE("[Renderer] DrawText done");
    }
}
