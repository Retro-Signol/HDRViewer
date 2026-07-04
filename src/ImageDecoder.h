#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <optional>

struct ImageData {
    std::vector<uint16_t> pixels;  // RGBA f16 每像素 4 × uint16
    int width = 0;
    int height = 0;
    enum ColorSpace { sRGB, scRGB, PQ_ST2084, HLG, SceneLinear } colorSpace = sRGB;
    bool isHDR = false;
    float maxNits = 1000.0f;
};

std::optional<ImageData> DecodeImage(const std::wstring& path);

// 各格式解码器
std::optional<ImageData> DecodeJXL(const std::wstring& path);
std::optional<ImageData> DecodeEXR(const std::wstring& path);
std::optional<ImageData> DecodeJXR(const std::wstring& path);
std::optional<ImageData> DecodeSTB(const std::wstring& path);
