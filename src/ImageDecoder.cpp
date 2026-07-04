#include "ImageDecoder.h"
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <cstring>
#include "stb_image.h"

// half-float <-> float 转换
static float HalfToFloat(uint16_t h) {
    uint32_t s = (h >> 15) & 0x01;
    uint32_t e = (h >> 10) & 0x1F;
    uint32_t m = h & 0x03FF;
    if (e == 0) { uint32_t f = (s << 31) | (m << 13); float r; memcpy(&r, &f, 4); return r; }
    if (e == 31) { uint32_t f = (s << 31) | 0x7F800000 | (m << 13); float r; memcpy(&r, &f, 4); return r; }
    uint32_t f = (s << 31) | ((e + 112) << 23) | (m << 13);
    float r; memcpy(&r, &f, 4); return r;
}

static uint16_t FloatToHalf(float f) {
    uint32_t x; memcpy(&x, &f, 4);
    uint32_t s = (x >> 16) & 0x8000;
    uint32_t e = (x >> 23) & 0xFF;
    uint32_t m = x & 0x007FFFFF;
    if (e == 0) return static_cast<uint16_t>(s);
    int ne = static_cast<int>(e) - 127 + 15;
    if (ne <= 0) return static_cast<uint16_t>(s);
    if (ne >= 31) return static_cast<uint16_t>(s | 0x7C00);
    return static_cast<uint16_t>(s | (ne << 10) | (m >> 13));
}

// -----------------------------------------------------------------------
// 主调度
// -----------------------------------------------------------------------
std::optional<ImageData> DecodeImage(const std::wstring& path) {
    auto dot = path.rfind(L'.');
    if (dot == std::wstring::npos) return std::nullopt;

    std::wstring ext = path.substr(dot + 1);
    for (auto& c : ext) c = static_cast<wchar_t>(std::towlower(c));

    if (ext == L"jxl") return DecodeJXL(path);
    if (ext == L"exr") return DecodeEXR(path);
    if (ext == L"jxr" || ext == L"wdp" || ext == L"hdp") return DecodeJXR(path);
    if (ext == L"png" || ext == L"jpg" || ext == L"jpeg" ||
        ext == L"bmp" || ext == L"tga" || ext == L"gif")
        return DecodeSTB(path);
    return std::nullopt;
}

// -----------------------------------------------------------------------
// JXL 解码 (libjxl)
// -----------------------------------------------------------------------
#ifdef HAS_JXL
#include <jxl/decode.h>
#include <jxl/thread_parallel_runner.h>

std::optional<ImageData> DecodeJXL(const std::wstring& path) {
    FILE* f = _wfopen(path.c_str(), L"rb");
    if (!f) return std::nullopt;
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> compressed(fileSize);
    if (fread(compressed.data(), 1, fileSize, f) != fileSize) {
        fclose(f);
        return std::nullopt;
    }
    fclose(f);

    auto* dec = JxlDecoderCreate(nullptr);
    if (!dec) return std::nullopt;

    void* runner = JxlThreadParallelRunnerCreate(nullptr, 4);
    JxlDecoderSetParallelRunner(dec, JxlThreadParallelRunner, runner);

    JxlDecoderSetInput(dec, compressed.data(), compressed.size());
    JxlDecoderCloseInput(dec);

    JxlBasicInfo info = {};
    JxlPixelFormat format = {4, JXL_TYPE_FLOAT16, JXL_NATIVE_ENDIAN, 0};

    ImageData result;
    bool gotInfo = false, gotImage = false;

    for (;;) {
        auto status = JxlDecoderProcessInput(dec);
        if (status == JXL_DEC_BASIC_INFO) {
            if (JxlDecoderGetBasicInfo(dec, &info) == JXL_DEC_SUCCESS) {
                result.width  = static_cast<int>(info.xsize);
                result.height = static_cast<int>(info.ysize);
                result.maxNits = info.intensity_target;
                result.pixels.resize(result.width * result.height * 4);
                gotInfo = true;
            }
        } else if (status == JXL_DEC_NEED_IMAGE_OUT_BUFFER) {
            size_t bufSize = 0;
            JxlDecoderImageOutBufferSize(dec, &format, &bufSize);
            if (bufSize == result.pixels.size() * sizeof(uint16_t)) {
                JxlDecoderSetImageOutBuffer(dec, &format,
                    result.pixels.data(), bufSize);
            }
        } else if (status == JXL_DEC_FULL_IMAGE) {
            gotImage = true;
            break;
        } else if (status == JXL_DEC_SUCCESS || status == JXL_DEC_FRAME) {
            continue;
        } else {
            break;
        }
    }

    if (gotInfo) {
        // 通过 JxlDecoderGetColorAsEncodedProfile 获取色彩编码
        JxlColorEncoding colorEncoding;
        if (JxlDecoderGetColorAsEncodedProfile(dec,
            JXL_COLOR_PROFILE_TARGET_ORIGINAL, &colorEncoding) == JXL_DEC_SUCCESS) {
            if (colorEncoding.transfer_function == JXL_TRANSFER_FUNCTION_PQ) {
                result.colorSpace = ImageData::PQ_ST2084;
                result.isHDR = true;
            } else if (colorEncoding.transfer_function == JXL_TRANSFER_FUNCTION_HLG) {
                result.colorSpace = ImageData::HLG;
                result.isHDR = true;
            } else {
                result.colorSpace = ImageData::sRGB;
                result.isHDR = (info.bits_per_sample > 8);
            }
        } else {
            result.colorSpace = ImageData::sRGB;
            result.isHDR = (info.bits_per_sample > 8);
        }
        result.maxNits = info.intensity_target;
        if (result.maxNits <= 0) result.maxNits = 1000.0f;
    }

    JxlThreadParallelRunnerDestroy(runner);
    JxlDecoderDestroy(dec);

    if (!gotImage) return std::nullopt;
    return result;
}

#else
std::optional<ImageData> DecodeJXL(const std::wstring&) {
    return std::nullopt;
}
#endif

// -----------------------------------------------------------------------
// EXR 解码 (OpenEXR)
// -----------------------------------------------------------------------
#ifdef HAS_OPENEXR
#include <OpenEXR/ImfRgbaFile.h>
#include <OpenEXR/ImfArray.h>

std::optional<ImageData> DecodeEXR(const std::wstring& path) {
    char mbPath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, mbPath, MAX_PATH, nullptr, nullptr);

    try {
        Imf::RgbaInputFile file(mbPath);
        Imath::Box2i dw = file.dataWindow();
        int w = dw.max.x - dw.min.x + 1;
        int h = dw.max.y - dw.min.y + 1;

        Imf::Array2D<Imf::Rgba> pixels(h, w);
        file.setFrameBuffer(&pixels[0][0] - dw.min.x - dw.min.y * w, 1, w);
        file.readPixels(dw.min.y, dw.max.y);

        ImageData result;
        result.width  = w;
        result.height = h;
        result.pixels.resize(static_cast<size_t>(w) * h * 4);
        memcpy(result.pixels.data(), &pixels[0][0],
               static_cast<size_t>(w) * h * 4 * sizeof(uint16_t));
        result.colorSpace = ImageData::SceneLinear;
        result.isHDR = true;
        result.maxNits = 1000.0f;
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

#else
std::optional<ImageData> DecodeEXR(const std::wstring&) {
    return std::nullopt;
}
#endif

// -----------------------------------------------------------------------
// JXR 解码 (WIC — HD Photo / JPEG XR)
// -----------------------------------------------------------------------
#include <wincodec.h>

std::optional<ImageData> DecodeJXR(const std::wstring& path) {
    // WIC 工厂 (COM 已初始化)
    IWICImagingFactory* factory = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr,
        CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))))
        return std::nullopt;

    IWICBitmapDecoder* decoder = nullptr;
    HRESULT hr = factory->CreateDecoderFromFilename(
        path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) { factory->Release(); return std::nullopt; }

    IWICBitmapFrameDecode* frame = nullptr;
    if (FAILED(decoder->GetFrame(0, &frame))) {
        decoder->Release(); factory->Release(); return std::nullopt;
    }

    UINT w = 0, h = 0;
    frame->GetSize(&w, &h);

    ImageData result;
    result.width  = static_cast<int>(w);
    result.height = static_cast<int>(h);
    result.pixels.resize(static_cast<size_t>(w) * h * 4);

    // 转换为 FP16 以保留 HDR
    IWICFormatConverter* converter = nullptr;
    hr = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame,
            GUID_WICPixelFormat64bppRGBAHalf,
            WICBitmapDitherTypeNone, nullptr, 0.0,
            WICBitmapPaletteTypeMedianCut);
        if (SUCCEEDED(hr)) {
            UINT stride = w * 8;  // 4 * uint16 = 8 bytes
            converter->CopyPixels(nullptr, stride,
                static_cast<UINT>(result.pixels.size() * sizeof(uint16_t)),
                reinterpret_cast<BYTE*>(result.pixels.data()));
        }
        converter->Release();
    }

    // 判断是否为 HDR: JXR 通常是 scRGB
    result.colorSpace = ImageData::scRGB;
    result.isHDR = true;
    result.maxNits = 1000.0f;

    frame->Release();
    decoder->Release();
    factory->Release();
    return result;
}

// -----------------------------------------------------------------------
// SDR 解码 (stb_image)
// -----------------------------------------------------------------------
std::optional<ImageData> DecodeSTB(const std::wstring& path) {
    char mbPath[MAX_PATH];
    WideCharToMultiByte(CP_UTF8, 0, path.c_str(), -1, mbPath, MAX_PATH, nullptr, nullptr);

    int w, h, comp;
    uint8_t* data = stbi_load(mbPath, &w, &h, &comp, 4);
    if (!data) return std::nullopt;

    ImageData result;
    result.width  = w;
    result.height = h;
    result.pixels.resize(static_cast<size_t>(w) * h * 4);

    for (int i = 0; i < w * h * 4; i++) {
        result.pixels[i] = FloatToHalf(data[i] / 255.0f);
    }
    result.colorSpace = ImageData::sRGB;
    result.isHDR = false;

    stbi_image_free(data);
    return result;
}
