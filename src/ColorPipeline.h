#pragma once
#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include "ImageDecoder.h"

class ColorPipeline {
public:
    // 将 ImageData 转换为 scRGB 线性空间
    // 输出: RGBA f16, scRGB 线性 (1.0 = 80 nits SDR white)
    static void ToScRGB(const ImageData& src,
                        std::vector<uint16_t>& dst,
                        float sdrWhiteNits = 80.0f);

    // PQ EOTF (SMPTE ST.2084)
    static float PQToLinear(float pq) {
        const float m1 = 0.1593017578125f;
        const float m2 = 78.84375f;
        const float c1 = 0.8359375f;
        const float c2 = 18.8515625f;
        const float c3 = 18.6875f;

        float v = std::pow(pq, 1.0f / m2);
        float num = std::max(v - c1, 0.0f);
        float den = std::max(c2 - c3 * v, 1e-10f);
        return std::pow(num / den, 1.0f / m1);
    }

    // sRGB gamma decode
    static float SRGBToLinear(float srgb) {
        if (srgb <= 0.04045f)
            return srgb / 12.92f;
        return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
    }

    // half-float 转换 (声明为 static inline 以共享)
    static inline float HalfToFloat(uint16_t h) {
        uint32_t s = (h >> 15) & 0x01;
        uint32_t e = (h >> 10) & 0x1F;
        uint32_t m = h & 0x03FF;
        if (e == 0) { uint32_t f = (s << 31) | (m << 13); float r; memcpy(&r, &f, 4); return r; }
        if (e == 31) { uint32_t f = (s << 31) | 0x7F800000 | (m << 13); float r; memcpy(&r, &f, 4); return r; }
        uint32_t f = (s << 31) | ((e + 112) << 23) | (m << 13);
        float r; memcpy(&r, &f, 4); return r;
    }

    static inline uint16_t FloatToHalf(float f) {
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
};
