// ColorPipeline implementation is header-inline.
// This file exists to satisfy the CMake source list.
#include "ColorPipeline.h"

void ColorPipeline::ToScRGB(const ImageData& src,
                             std::vector<uint16_t>& dst,
                             float sdrWhiteNits) {
    dst.resize(static_cast<size_t>(src.width) * src.height * 4);
    const uint16_t* in = src.pixels.data();
    uint16_t* out = dst.data();

    for (int i = 0; i < src.width * src.height * 4; i += 4) {
        float r = HalfToFloat(in[i + 0]);
        float g = HalfToFloat(in[i + 1]);
        float b = HalfToFloat(in[i + 2]);
        float a = HalfToFloat(in[i + 3]);

        switch (src.colorSpace) {
        case ImageData::sRGB:
            out[i + 0] = FloatToHalf(SRGBToLinear(r));
            out[i + 1] = FloatToHalf(SRGBToLinear(g));
            out[i + 2] = FloatToHalf(SRGBToLinear(b));
            out[i + 3] = in[i + 3];
            break;

        case ImageData::PQ_ST2084: {
            float nits = src.maxNits;
            out[i + 0] = FloatToHalf(PQToLinear(r) * nits / sdrWhiteNits);
            out[i + 1] = FloatToHalf(PQToLinear(g) * nits / sdrWhiteNits);
            out[i + 2] = FloatToHalf(PQToLinear(b) * nits / sdrWhiteNits);
            out[i + 3] = in[i + 3];
            break;
        }

        case ImageData::SceneLinear:
            out[i + 0] = FloatToHalf(r);
            out[i + 1] = FloatToHalf(g);
            out[i + 2] = FloatToHalf(b);
            out[i + 3] = in[i + 3];
            break;

        default:
            std::memcpy(out + i, in + i, 4 * sizeof(uint16_t));
            break;
        }
    }
}
