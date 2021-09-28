/*
 * Copyright (C) 2018 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl32.h>

#include <array>
#include <cmath>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <android/hardware_buffer.h>
#include <android/log.h>
#include <gtest/gtest.h>

#define NO_ERROR 0
#define LOG_TAG "AHBGLTest"
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

namespace android {
namespace {

// The 'stride' field is ignored by AHardwareBuffer_allocate, so we can use it
// to pass these flags.
enum TestFlags {
    kGlFormat = 0x1,            // The 'format' field specifies a GL format.
    kUseSrgb = 0x2,             // Whether to use the sRGB transfer function.
    kExplicitYuvSampling = 0x4, // Whether to do explicit YUV conversion sampling,
                                // if false, GL will do conversions implicitly.
};

// Conversion from YUV to RGB used by GPU. This assumes BT.601 (partial) format.
// The matrix M is multiplied in (Y,U,V) = M * (R, G, B, 1) to obtain the final YUV value s.
// TODO(b/123041714): Assumes ITU_601 color space is used. Can we count on this? Spec is unclear for
//                    glReadPixels YUV -> RGB conversion.
const double kYuvToRgb[] = {
    0.25678823529411765,  0.50412941176470580,  0.09790588235294118, 16.00,
   -0.14822352941176470, -0.29099215686274510,  0.43921568627450980, 128.0,
    0.43921568627450980, -0.36778823529411764, -0.07142745098039215, 128.0
};

#define FORMAT_CASE(x) case AHARDWAREBUFFER_FORMAT_##x: return #x; break
#define GL_FORMAT_CASE(x) case x: return #x; break;
const char* AHBFormatAsString(int32_t format) {
    switch (format) {
        FORMAT_CASE(R8G8B8A8_UNORM);
        FORMAT_CASE(R8G8B8X8_UNORM);
        FORMAT_CASE(R8G8B8_UNORM);
        FORMAT_CASE(R5G6B5_UNORM);
        FORMAT_CASE(R16G16B16A16_FLOAT);
        FORMAT_CASE(R10G10B10A2_UNORM);
        FORMAT_CASE(BLOB);
        FORMAT_CASE(D16_UNORM);
        FORMAT_CASE(D24_UNORM);
        FORMAT_CASE(D24_UNORM_S8_UINT);
        FORMAT_CASE(D32_FLOAT);
        FORMAT_CASE(D32_FLOAT_S8_UINT);
        FORMAT_CASE(S8_UINT);
        FORMAT_CASE(Y8Cb8Cr8_420);
        GL_FORMAT_CASE(GL_RGB8);
        GL_FORMAT_CASE(GL_RGBA8);
        GL_FORMAT_CASE(GL_RGB565);
        GL_FORMAT_CASE(GL_SRGB8_ALPHA8);
        GL_FORMAT_CASE(GL_RGBA16F);
        GL_FORMAT_CASE(GL_RGB10_A2);
        GL_FORMAT_CASE(GL_DEPTH_COMPONENT16);
        GL_FORMAT_CASE(GL_DEPTH24_STENCIL8);
        GL_FORMAT_CASE(GL_STENCIL_INDEX8);
    }
    return "";
}

std::string GetTestName(const ::testing::TestParamInfo<AHardwareBuffer_Desc>& info) {
    std::ostringstream name;
    const char* format_string = AHBFormatAsString(info.param.format);
    if (strlen(format_string) == 0) {
        name << info.index;
    } else {
        name << format_string;
        if (info.param.stride & kUseSrgb) {
            name << "_sRGB";
        }
        if (info.param.stride & kExplicitYuvSampling) {
            name << "_explicitYuvSampling";
        }
    }
    return name.str();
}

union IntFloat {
    uint32_t i;
    float f;
};

// Copied from android.util.Half
// Used for reading directly from half-float buffers
float FloatFromHalf(uint16_t bits) {
    uint32_t s = bits & 0x8000;
    uint32_t e = (bits & 0x7C00) >> 10;
    uint32_t m = bits & 0x3FF;
    uint32_t outE = 0;
    uint32_t outM = 0;
    if (e == 0) { // Denormal or 0
        if (m != 0) {
            // Convert denorm fp16 into normalized fp32
            IntFloat uif;
            uif.i = (126 << 23);
            float denormal = uif.f;
            uif.i += m;
            float o = uif.f - denormal;
            return s == 0 ? o : -o;
        }
    } else {
        outM = m << 13;
        if (e == 0x1f) { // Infinite or NaN
            outE = 0xff;
        } else {
            outE = e - 15 + 127;
        }
    }
    IntFloat result;
    result.i = (s << 16) | (outE << 23) | outM;
    return result.f;
}

// Copied from android.util.Half
// Used for writing directly into half-float buffers.
uint16_t HalfFromFloat(float value) {
    uint32_t bits = *reinterpret_cast<uint32_t*>(&value); // convert to int bits
    int32_t s = (bits >> 31);
    int32_t e = (bits >> 23) & 0xFF;
    int32_t m = bits & 0x7FFFFF;

    int32_t outE = 0;
    int32_t outM = 0;

    if (e == 0xff) { // Infinite or NaN
        outE = 0x1f;
        outM = m != 0 ? 0x200 : 0;
    } else {
        e = e - 127 + 15;
        if (e >= 0x1f) { // Overflow
            outE = 0x31;
        } else if (e <= 0) { // Underflow
            if (e < -10) {
                // The absolute fp32 value is less than MIN_VALUE, flush to +/-0
            } else {
                // The fp32 value is a normalized float less than MIN_NORMAL,
                // we convert to a denorm fp16
                m = (m | 0x800000) >> (1 - e);
                if ((m & 0x1000) != 0) m += 0x2000;
                outM = m >> 13;
            }
        } else {
            outE = e;
            outM = m >> 13;
            if ((m & 0x1000) != 0) {
                // Round to nearest "0.5" up
                int out = (outE << 10) | outM;
                out++;
                return static_cast<uint16_t>(out | (s << 15));
            }
        }
    }
    return static_cast<uint16_t>((s << 15) | (outE << 10) | outM);
}

// Utility function to ensure converted values are clamped to [0...255].
uint8_t ClampToUInt8(float value) {
    return static_cast<uint8_t>(value <= 0.0 ? 0 : (value >= 255.0 ? 255 : value));
}

int MipLevelCount(uint32_t width, uint32_t height) {
    return 1 + static_cast<int>(std::floor(std::log2(static_cast<float>(std::max(width, height)))));
}

uint32_t RoundUpToPowerOf2(uint32_t value) {
    return value == 0u ? value : 1u << (32 - __builtin_clz(value - 1));
}

// Returns true only if the format has a dedicated alpha channel
bool FormatHasAlpha(uint32_t format) {
    switch (format) {
        case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
        case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
        case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
        // This may look scary, but fortunately AHardwareBuffer formats and GL pixel formats
        // do not overlap.
        case GL_RGBA8:
        case GL_RGB10_A2:
        case GL_RGBA16F:
        case GL_SRGB8_ALPHA8:
            return true;
        default: return false;
    }
}

// Returns true only if the format has its components specified in some floating point format.
bool FormatIsFloat(uint32_t format) {
    return format == AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT || format == GL_RGBA16F;
}

// Returns true only if the format is a YUV format.
bool FormatIsYuv(uint32_t format) {
    // Update with more YUV cases here if more formats become available
    return format == AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420;
}

void UploadData(const AHardwareBuffer_Desc& desc, GLenum format, GLenum type, const void* data) {
    if (desc.layers <= 1) {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, desc.width, desc.height, format, type, data);
        ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "glTexSubImage2D failed";
    } else {
        for (uint32_t layer = 0; layer < desc.layers; ++layer) {
            glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, desc.width, desc.height, 1,
                            format, type, data);
            ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "glTexSubImage3D failed";
        }
    }
}

// Uploads opaque red to the currently bound texture.
void UploadRedPixels(const AHardwareBuffer_Desc& desc) {
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
    const bool use_srgb = desc.stride & kUseSrgb;
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    switch (desc.format) {
        case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM:
        case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM:
        case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM:
        case GL_RGB565:
        case GL_RGB8: {
            // GL_RGB565 supports uploading GL_UNSIGNED_BYTE data.
            const int size = desc.width * desc.height * 3;
            std::unique_ptr<uint8_t[]> pixels(new uint8_t[size]);
            for (int i = 0; i < size; i += 3) {
                pixels[i] = use_srgb ? 188 : 255;
                pixels[i + 1] = 0;
                pixels[i + 2] = 0;
            }
            UploadData(desc, GL_RGB, GL_UNSIGNED_BYTE, pixels.get());
            break;
        }
        case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
        case GL_RGBA8:
        case GL_SRGB8_ALPHA8: {
            const int size = desc.width * desc.height * 4;
            std::unique_ptr<uint8_t[]> pixels(new uint8_t[size]);
            for (int i = 0; i < size; i += 4) {
                pixels[i] = use_srgb ? 188 : 255;
                pixels[i + 1] = 0;
                pixels[i + 2] = 0;
                pixels[i + 3] = use_srgb ? 128 : 255;
            }
            UploadData(desc, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
            break;
        }
        case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT:
        case GL_RGBA16F: {
            const int size = desc.width * desc.height * 4;
            std::unique_ptr<float[]> pixels(new float[size]);
            for (int i = 0; i < size; i += 4) {
                pixels[i] = 1.f;
                pixels[i + 1] = 0.f;
                pixels[i + 2] = 0.f;
                pixels[i + 3] = 1.f;
            }
            UploadData(desc, GL_RGBA, GL_FLOAT, pixels.get());
            break;
        }
        case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM:
        case GL_RGB10_A2: {
            const int size = desc.width * desc.height;
            std::unique_ptr<uint32_t[]> pixels(new uint32_t[size]);
            for (int i = 0; i < size; ++i) {
                // Opaque red is top 2 bits and bottom 10 bits set.
                pixels[i] = 0xc00003ff;
            }
            UploadData(desc, GL_RGBA, GL_UNSIGNED_INT_2_10_10_10_REV_EXT, pixels.get());
            break;
        }
        default: FAIL() << "Unrecognized AHardwareBuffer format"; break;
    }
}

// A pre-defined list of colors that will be used for testing
enum GoldenColor {
    kZero,  // all zero, i.e., transparent black
    kBlack,  // opaque black
    kWhite,  // opaque white
    kRed,  // opaque red
    kGreen,  // opaque green
    kBlue,  // opaque blue
    kRed50,  // 50% premultiplied red, i.e., (0.5, 0, 0, 0.5)
    kRed50Srgb,  // 50% premultiplied red under sRGB transfer function
    kRed50Alpha100,  // Opaque 50% red
};

// A golden color at a specified position (given in pixel coordinates)
struct GoldenPixel {
    int x;
    int y;
    GoldenColor color;
};

// Compares a golden pixel against an actual pixel given the 4 color values. The values must
// match exactly.
template <typename T>
void CheckGoldenPixel(int x, int y, const std::array<T, 4>& golden,
                      const std::array<T, 4>& actual) {
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
    EXPECT_EQ(golden, actual) << "Pixel doesn't match at X=" << x << ", Y=" << y;
}

// Compares an actual pixel against a range of pixel values specified by a minimum and maximum
// 4-component pixel value.
template <typename T>
void CheckGoldenPixel(int x, int y, const std::array<T, 4>& minimum,
                      const std::array<T, 4>& maximum, const std::array<T, 4>& actual) {
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
    bool in_range = true;
    for (int i = 0; i < 4; ++i) {
        if (actual[i] < minimum[i] || actual[i] > maximum[i]) {
            in_range = false;
            break;
        }
    }
    // Prefix with '+' so that uint8_t values are printed out as integers
    EXPECT_TRUE(in_range) << "Pixel out of acceptable range at X=" << x << ", Y=" << y
        << "; actual value: {" << +actual[0] << ", " << +actual[1] << ", "
                               << +actual[2] << ", " << +actual[3]
        << "}, minimum: {" << +minimum[0] << ", " << +minimum[1] << ", "
                           << +minimum[2] << ", " << +minimum[3]
        << "}, maximum: {" << +maximum[0] << ", " << +maximum[1] << ", "
                           << +maximum[2] << ", " << +maximum[3]
        << "}";
}

// Given a golden color, format, and maximum allowed error, returns a 4-component pixel (as well as
// a maximum where it makes sense). Returns true, if the golden_max_out parameter was set, and
// the return values indicate a range.
bool GetGoldenColor(const GoldenColor& golden, uint32_t format, int32_t max_err,
                    std::array<uint8_t, 4>* golden_pixel_out,
                    std::array<uint8_t, 4>* golden_max_out) {
    bool use_range = false;
    // Adjust color values.
    std::array<uint8_t, 4>& golden_pixel = *golden_pixel_out;
    std::array<uint8_t, 4>& golden_max = *golden_max_out;
    golden_pixel[0] = golden_pixel[1] = golden_pixel[2] = 0;
    golden_max[0] = golden_max[1] = golden_max[2] = 0;
    golden_pixel[3] = 255;
    golden_max[3] = 255;
    switch (golden) {
        case kRed: golden_pixel[0] = 255; break;
        case kRed50:
        case kRed50Alpha100:
            use_range = true;
            golden_pixel[0] = 127;
            golden_max[0] = 128;
            break;
        case kRed50Srgb:
            use_range = true;
            golden_pixel[0] = 187;
            golden_max[0] = 188;
            break;
        case kGreen: golden_pixel[1] = 255; break;
        case kBlue: golden_pixel[2] = 255; break;
        case kZero: if (FormatHasAlpha(format)) golden_pixel[3] = 0; break;
        case kWhite: golden_pixel[0] = 255; golden_pixel[1] = 255; golden_pixel[2] = 255; break;
        case kBlack: break;
        default:
            ADD_FAILURE() << "Unrecognized golden pixel color";
            return false;
    }
    // Adjust alpha.
    if (golden == kRed50 || golden == kRed50Srgb) {
        if (format == GL_RGB10_A2 || format == AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM) {
            golden_pixel[3] = 85;
            golden_max[3] = 170;
        } else if (FormatHasAlpha(format)) {
            golden_pixel[3] = 127;
            golden_max[3] = 128;
        }
    }
    // Adjust color range for RGB565.
    if ((golden == kRed50 || golden == kRed50Alpha100) &&
        (format == GL_RGB565 || format == AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM)) {
        golden_pixel[0] = 123;
        golden_max[0] = 132;
    }
    // Convert to YUV if this is a YUV format
    if (FormatIsYuv(format)) {
        const uint8_t r = golden_pixel[0];
        const uint8_t g = golden_pixel[1];
        const uint8_t b = golden_pixel[2];
        const float y = kYuvToRgb[0] * r + kYuvToRgb[1] * g + kYuvToRgb[2]   * b + kYuvToRgb[3];
        const float u = kYuvToRgb[4] * r + kYuvToRgb[5] * g + kYuvToRgb[6]   * b + kYuvToRgb[7];
        const float v = kYuvToRgb[8] * r + kYuvToRgb[9] * g + kYuvToRgb[10]  * b + kYuvToRgb[11];
        golden_pixel[0] = ClampToUInt8(y);
        golden_pixel[1] = ClampToUInt8(u);
        golden_pixel[2] = ClampToUInt8(v);
    }
    // Apply error bounds
    if (max_err != 0) {
        for (int i = 0; i < 4; ++i) {
            golden_pixel[i] = ClampToUInt8(golden_pixel[i] - max_err);
            golden_max[i] = ClampToUInt8(golden_pixel[i] + max_err);
        }
        use_range = true;
    }
    return use_range;
}

// Get a golden color for range-less values.
void GetGoldenColor(const GoldenColor& golden, uint32_t format,
                    std::array<uint8_t, 4>* golden_pixel_out) {
    std::array<uint8_t, 4> ignore;
    GetGoldenColor(golden, format, 0, golden_pixel_out, &ignore);
}

// Get a golden color for floating point values.
void GetGoldenColor(const GoldenColor& golden, std::array<float, 4>* golden_pixel_out) {
    std::array<float, 4>& golden_pixel = *golden_pixel_out;
    golden_pixel[0] = golden_pixel[1] = golden_pixel[2] = 0.f;
    golden_pixel[3] = 1.f;
    switch (golden) {
        case kRed: golden_pixel[0] = 1.f; break;
        case kRed50: golden_pixel[0] = 0.5f; golden_pixel[3] = 0.5f; break;
        case kGreen: golden_pixel[1] = 1.f; break;
        case kBlue: golden_pixel[2] = 1.f; break;
        case kZero: golden_pixel[3] = 0.f; break;
        case kWhite: golden_pixel[0] = 1.f; golden_pixel[1] = 1.f; golden_pixel[2] = 1.f; break;
        case kBlack: break;
        default: FAIL() << "Unrecognized golden pixel color";
    }
}

// Checks a pixel against a golden pixel of the specified format with the given error bounds.
void CheckGoldenPixel(const GoldenPixel& golden, const std::array<uint8_t, 4>& pixel,
                      uint32_t format, int32_t max_err) {
    std::array<uint8_t, 4> golden_pixel;
    std::array<uint8_t, 4> golden_max;
    if (GetGoldenColor(golden.color, format, max_err, &golden_pixel, &golden_max)) {
        CheckGoldenPixel(golden.x, golden.y, golden_pixel, golden_max, pixel);
    } else {
        CheckGoldenPixel(golden.x, golden.y, golden_pixel, pixel);
    }
}

// Checks a pixel against a golden pixel of the specified format with no room for error.
void CheckGoldenPixel(const GoldenPixel& golden, const std::array<uint8_t, 4>& pixel,
                      uint32_t format) {
    CheckGoldenPixel(golden, pixel, format, 0);
}

// Checks a floating point pixel against a golden pixel.
void CheckGoldenPixel(const GoldenPixel& golden, const std::array<float, 4>& pixel) {
    std::array<float, 4> golden_pixel;
    GetGoldenColor(golden.color, &golden_pixel);
    CheckGoldenPixel(golden.x, golden.y, golden_pixel, pixel);
}

// Using glReadPixels, reads out the individual pixel values of each golden pixel location, and
// compares each against the golden color.
void CheckGoldenPixels(const std::vector<GoldenPixel>& goldens,
                       uint32_t format,
                       int16_t max_err = 0) {
    // We currently do not test any float formats that don't have alpha.
    EXPECT_TRUE(FormatIsFloat(format) ? FormatHasAlpha(format) : true);
    if (FormatIsYuv(format)) {
        format = GL_RGB8;   // YUV formats are read out as RGB for glReadPixels
        max_err = 255;      // Conversion method is unknown, so we cannot assume
                            // anything about the actual colors
    }
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    // In OpenGL, Y axis grows up, so bottom = minimum Y coordinate.
    int bottom = INT_MAX, left = INT_MAX, right = 0, top = 0;
    for (const GoldenPixel& golden : goldens) {
        left = std::min(left, golden.x);
        right = std::max(right, golden.x);
        bottom = std::min(bottom, golden.y);
        top = std::max(top, golden.y);
        if (FormatIsFloat(format)) {
            std::array<float, 4> pixel = {0.5f, 0.5f, 0.5f, 0.5f};
            glReadPixels(golden.x, golden.y, 1, 1, GL_RGBA, GL_FLOAT, pixel.data());
            ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "Could not read pixel at " << golden.x << "," << golden.y;
            CheckGoldenPixel(golden, pixel);
        } else {
            std::array<uint8_t, 4> pixel = {127, 127, 127, 127};
            glReadPixels(golden.x, golden.y, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel.data());
            CheckGoldenPixel(golden, pixel, format, max_err);
        }
    }
    // Repeat the test, but read back all the necessary pixels in a single glReadPixels call.
    const int width = right - left + 1;
    const int height = top - bottom + 1;
    if (FormatIsFloat(format)) {
        std::unique_ptr<float[]> pixels(new float[width * height * 4]);
        glReadPixels(left, bottom, width, height, GL_RGBA, GL_FLOAT, pixels.get());
        ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
        for (const GoldenPixel& golden : goldens) {
            float* pixel = pixels.get() + ((golden.y - bottom) * width + golden.x - left) * 4;
            std::array<float, 4> pixel_array;
            memcpy(pixel_array.data(), pixel, 4 * sizeof(float));
            CheckGoldenPixel(golden, pixel_array);
        }
    } else {
        std::unique_ptr<uint8_t[]> pixels(new uint8_t[width * height * 4]);
        glReadPixels(left, bottom, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.get());
        ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
        for (const GoldenPixel& golden : goldens) {
            uint8_t* pixel = pixels.get() + ((golden.y - bottom) * width + golden.x - left) * 4;
            std::array<uint8_t, 4> pixel_array;
            memcpy(pixel_array.data(), pixel, 4);
            CheckGoldenPixel(golden, pixel_array, format, max_err);
        }
    }
}

// Using direct memory access by locking the buffer, accesses the individual pixel values of each
// golden pixel location, and compares each against the golden color. This variant works for RGBA
// layouts only.
void CheckCpuGoldenPixelsRgba(const std::vector<GoldenPixel>& goldens,
                              AHardwareBuffer* buffer,
                              const AHardwareBuffer_Desc& desc) {
    void* data = nullptr;
    int result = AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1, nullptr,
                                      &data);
    ASSERT_EQ(NO_ERROR, result) << "AHardwareBuffer_lock failed with error " << result;
    for (const GoldenPixel& golden : goldens) {
        ptrdiff_t row_offset = golden.y * desc.stride;
        switch (desc.format) {
            case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
            case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM: {
                uint8_t* pixel = reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 4;
                std::array<uint8_t, 4> pixel_to_check;
                memcpy(pixel_to_check.data(), pixel, 4);
                if (desc.format == AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM) {
                    pixel_to_check[3] = 255;
                }
                CheckGoldenPixel(golden, pixel_to_check, desc.format);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM: {
                uint8_t* pixel = reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 3;
                std::array<uint8_t, 4> pixel_to_check;
                memcpy(pixel_to_check.data(), pixel, 3);
                pixel_to_check[3] = 255;
                CheckGoldenPixel(golden, pixel_to_check, desc.format);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM: {
                uint16_t* pixel = reinterpret_cast<uint16_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 2);
                std::array<uint8_t, 4> pixel_to_check = {
                    static_cast<uint8_t>(((*pixel & 0xF800) >> 11) * (255./31.)),
                    static_cast<uint8_t>(((*pixel & 0x07E0) >> 5) * (255./63.)),
                    static_cast<uint8_t>((*pixel & 0x001F) * (255./31.)),
                    255,
                };
                CheckGoldenPixel(golden, pixel_to_check, desc.format);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT: {
                uint16_t* pixel = reinterpret_cast<uint16_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 8);
                std::array<float, 4> pixel_to_check;
                for (int i = 0; i < 4; ++i) {
                    pixel_to_check[i] = FloatFromHalf(pixel[i]);
                }
                CheckGoldenPixel(golden, pixel_to_check);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM: {
                uint32_t* pixel = reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 4);
                std::array<uint8_t, 4> pixel_to_check = {
                    static_cast<uint8_t>((*pixel & 0x000003FF) * (255./1023.)),
                    static_cast<uint8_t>(((*pixel & 0x000FFC00) >> 10) * (255./1023.)),
                    static_cast<uint8_t>(((*pixel & 0x3FF00000) >> 20) * (255./1023.)),
                    static_cast<uint8_t>(((*pixel & 0xC0000000) >> 30) * (255./3.)),
                };
                CheckGoldenPixel(golden, pixel_to_check, desc.format);
                break;
            }
            default: FAIL() << "Unrecognized AHardwareBuffer format"; break;
        }
    }
    AHardwareBuffer_unlock(buffer, nullptr);
}

// Using direct memory access by locking the buffer, accesses the individual pixel values of each
// golden pixel location, and compares each against the golden color. This variant works for YUV
// layouts only.
void CheckCpuGoldenPixelsYuv(const std::vector<GoldenPixel>& goldens,
                             AHardwareBuffer* buffer,
                             const AHardwareBuffer_Desc& desc) {
    AHardwareBuffer_Planes planes_info;
    int result = AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1,
                                            nullptr, &planes_info);
    ASSERT_EQ(NO_ERROR, result) << "AHardwareBuffer_lock failed with error " << result;
    for (const GoldenPixel& golden : goldens) {
        switch (desc.format) {
            case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420: {
                ASSERT_EQ(3U, planes_info.planeCount) << "Unexpected number of planes in YUV data: "
                                                      << planes_info.planeCount;
                AHardwareBuffer_Plane* planes = planes_info.planes;
                ptrdiff_t y_offset = golden.y * planes[0].rowStride
                                   + golden.x * planes[0].pixelStride;
                ptrdiff_t u_offset = (golden.y / 2) * planes[1].rowStride
                                   + (golden.x / 2) * planes[1].pixelStride;
                ptrdiff_t v_offset = (golden.y / 2) * planes[2].rowStride
                                   + (golden.x / 2) * planes[2].pixelStride;
                // Check colors in YUV space (which desc.format is)
                std::array<uint8_t, 4> pixel_to_check = {
                    *(reinterpret_cast<uint8_t*>(planes[0].data) + y_offset),
                    *(reinterpret_cast<uint8_t*>(planes[1].data) + u_offset),
                    *(reinterpret_cast<uint8_t*>(planes[2].data) + v_offset),
                    255
                };
                CheckGoldenPixel(golden, pixel_to_check, desc.format);
            }
            break;
            default: FAIL() << "Unrecognized AHardwareBuffer format"; break;
        }
    }
    AHardwareBuffer_unlock(buffer, nullptr);
}

// Using direct memory access by locking the buffer, accesses the individual pixel values of each
// golden pixel location, and compares each against the golden color. This variant forwards to the
// appropriate RGBA or YUV variants.
void CheckCpuGoldenPixels(const std::vector<GoldenPixel>& goldens,
                          AHardwareBuffer* buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);
    if (FormatIsYuv(desc.format)) {
        CheckCpuGoldenPixelsYuv(goldens, buffer, desc);
    } else {
        CheckCpuGoldenPixelsRgba(goldens, buffer, desc);
    }
}

// Draws the following checkerboard pattern using glScissor and glClear.
// The number after the color is the stencil value and the floating point number is the depth value.
// The pattern is asymmetric to detect coordinate system mixups.
//        +-----+-----+ (W, H)
//        | OR1 | OB2 |
//        | 0.5 | 0.0 |
//        +-----+-----+  Tb = transparent black
//        | Tb0 | OG3 |  OR = opaque red
//        | 1.0 | 1.0 |  OG = opaque green
// (0, 0) +-----+-----+  OB = opaque blue
//
void DrawCheckerboard(int width, int height, uint32_t format) {
    glEnable(GL_SCISSOR_TEST);
    const GLbitfield all_bits = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    std::array<uint8_t, 4> color;

    GetGoldenColor(kZero, format, &color);
    glClearColor(color[0] / 255.f, color[1] / 255.f, color[2] / 255.f, color[3] / 255.f);
    glClearDepthf(1.0f);
    glClearStencil(0);
    glScissor(0, 0, width, height);
    glClear(all_bits);

    GetGoldenColor(kRed, format, &color);
    glClearColor(color[0] / 255.f, color[1] / 255.f, color[2] / 255.f, color[3] / 255.f);
    glClearDepthf(0.5f);
    glClearStencil(1);
    glScissor(0, height / 2, width / 2, height / 2);
    glClear(all_bits);

    GetGoldenColor(kGreen, format, &color);
    glClearColor(color[0] / 255.f, color[1] / 255.f, color[2] / 255.f, color[3] / 255.f);
    glClearDepthf(1.0f);
    glClearStencil(3);
    glScissor(width / 2, 0, width / 2, height / 2);
    glClear(all_bits);

    GetGoldenColor(kBlue, format, &color);
    glClearColor(color[0] / 255.f, color[1] / 255.f, color[2] / 255.f, color[3] / 255.f);
    glClearDepthf(0.f);
    glClearStencil(2);
    glScissor(width / 2, height / 2, width / 2, height / 2);
    glClear(all_bits);

    glDisable(GL_SCISSOR_TEST);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
}

// Using direct memory access, writes each specified golden pixel to the correct memory address
// inside the given buffer. This variant is compatible with RGBA color buffers only.
void WriteGoldenPixelsRgba(AHardwareBuffer* buffer,
                           const AHardwareBuffer_Desc& desc,
                           const std::vector<GoldenPixel>& goldens) {
    void* data = nullptr;
    int result = AHardwareBuffer_lock(buffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY, -1, nullptr,
                                      &data);
    ASSERT_EQ(NO_ERROR, result) << "AHardwareBuffer_lock failed with error " << result;
    std::array<uint8_t, 4> golden_color;
    std::array<float, 4> golden_float;
    for (const GoldenPixel& golden : goldens) {
        ptrdiff_t row_offset = golden.y * desc.stride;
        switch (desc.format) {
            case AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM:
            case AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM: {
                uint8_t* pixel = reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 4;
                GetGoldenColor(golden.color, desc.format, &golden_color);
                memcpy(pixel, golden_color.data(), 4);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM: {
                uint8_t* pixel = reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 3;
                GetGoldenColor(golden.color, desc.format, &golden_color);
                memcpy(pixel, golden_color.data(), 3);
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM: {
                uint16_t* pixel = reinterpret_cast<uint16_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 2);
                GetGoldenColor(golden.color, desc.format, &golden_color);
                uint16_t golden_565 =
                    static_cast<uint8_t>(golden_color[0] * (31./255.)) << 11
                    | static_cast<uint8_t>(golden_color[1] * (63./255.)) << 5
                    | static_cast<uint8_t>(golden_color[2] * (31./255.));
                *pixel = golden_565;
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT: {
                uint16_t* pixel = reinterpret_cast<uint16_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 8);
                GetGoldenColor(golden.color, &golden_float);
                for (int i = 0; i < 4; ++i) {
                    pixel[i] = HalfFromFloat(golden_float[i]);
                }
                break;
            }
            case AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM: {
                uint32_t* pixel = reinterpret_cast<uint32_t*>(
                    reinterpret_cast<uint8_t*>(data) + (row_offset + golden.x) * 4);
                GetGoldenColor(golden.color, desc.format, &golden_color);
                uint32_t golden_10102 =
                    static_cast<uint16_t>(golden_color[0] * (1023./255.))
                    | static_cast<uint16_t>(golden_color[1] * (1023./255.)) << 10
                    | static_cast<uint16_t>(golden_color[2] * (1023./255.)) << 20
                    | static_cast<uint16_t>(golden_color[3] * (3./255.)) << 30;
                *pixel = golden_10102;
                break;
            }
            default: FAIL() << "Unrecognized AHardwareBuffer format"; break;
        }
    }
    AHardwareBuffer_unlock(buffer, nullptr);
}

// Using direct memory access, writes each specified golden pixel to the correct memory address
// inside the given buffer. This variant is compatible with YUV color buffers only.
void WriteGoldenPixelsYuv(AHardwareBuffer* buffer,
                          const AHardwareBuffer_Desc& desc,
                          const std::vector<GoldenPixel>& goldens) {
    AHardwareBuffer_Planes planes_info;
    int result = AHardwareBuffer_lockPlanes(buffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY, -1,
                                            nullptr, &planes_info);
    ASSERT_EQ(NO_ERROR, result) << "AHardwareBuffer_lock failed with error " << result;
    std::array<uint8_t, 4> golden_color;
    for (const GoldenPixel& golden : goldens) {
        switch (desc.format) {
            case AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420: {
                ASSERT_EQ(3U, planes_info.planeCount) << "Unexpected number of planes in YUV data: "
                                                      << planes_info.planeCount;
                AHardwareBuffer_Plane* planes = planes_info.planes;

                ptrdiff_t y_offset = golden.y * planes[0].rowStride
                                   + golden.x * planes[0].pixelStride;
                ptrdiff_t u_offset = (golden.y / 2) * planes[1].rowStride
                                   + (golden.x / 2) * planes[1].pixelStride;
                ptrdiff_t v_offset = (golden.y / 2) * planes[2].rowStride
                                   + (golden.x / 2) * planes[2].pixelStride;

                GetGoldenColor(golden.color, desc.format, &golden_color);
                uint8_t* const y_ptr = reinterpret_cast<uint8_t*>(planes[0].data) + y_offset;
                uint8_t* const u_ptr = reinterpret_cast<uint8_t*>(planes[1].data) + u_offset;
                uint8_t* const v_ptr = reinterpret_cast<uint8_t*>(planes[2].data) + v_offset;
                *y_ptr = golden_color[0];
                *u_ptr = golden_color[1];
                *v_ptr = golden_color[2];
            }
            break;
            default: FAIL() << "Unrecognized AHardwareBuffer format"; break;
        }
    }
    AHardwareBuffer_unlock(buffer, nullptr);
}

// Writes the following checkerboard pattern directly to a buffer.
// The pattern is asymmetric to detect coordinate system mixups.
//        +-----+-----+ (W, H)
//        | OR  | OB  |
//        |     |     |
//        +-----+-----+  Tb = transparent black
//        | Tb  | OG  |  OR = opaque red
//        |     |     |  OG = opaque green
// (0, 0) +-----+-----+  OB = opaque blue
//
void WriteCheckerBoard(AHardwareBuffer* buffer) {
    AHardwareBuffer_Desc desc;
    AHardwareBuffer_describe(buffer, &desc);

    // Write golden values in same manner as checkerboard on GPU
    std::vector<GoldenPixel> goldens(desc.width * desc.height);
    const uint32_t h2 = desc.height / 2;
    const uint32_t w2 = desc.width / 2;
    for (uint32_t y = h2; y < desc.height; ++y) {
        for (uint32_t x = 0; x < w2; ++x) {
            const uint32_t offset = y * desc.width + x;
            goldens[offset].x = x;
            goldens[offset].y = y;
            goldens[offset].color = kRed;
        }
    }
    for (uint32_t y = h2; y < desc.height; ++y) {
        for (uint32_t x = w2; x < desc.width; ++x) {
            const uint32_t offset = y * desc.width + x;
            goldens[offset].x = x;
            goldens[offset].y = y;
            goldens[offset].color = kBlue;
        }
    }
    for (uint32_t y = 0; y < h2; ++y) {
        for (uint32_t x = 0; x < w2; ++x) {
            const uint32_t offset = y * desc.width + x;
            goldens[offset].x = x;
            goldens[offset].y = y;
            goldens[offset].color = kZero;
        }
    }
    for (uint32_t y = 0; y < h2; ++y) {
        for (uint32_t x = w2; x < desc.width; ++x) {
            const uint32_t offset = y * desc.width + x;
            goldens[offset].x = x;
            goldens[offset].y = y;
            goldens[offset].color = kGreen;
        }
    }

    if (FormatIsYuv(desc.format)) {
        WriteGoldenPixelsYuv(buffer, desc, goldens);
    } else {
        WriteGoldenPixelsRgba(buffer, desc, goldens);
    }
}

const char* kVertexShader = R"glsl(#version 100
    attribute vec2 aPosition;
    attribute float aDepth;
    uniform mediump float uScale;
    varying mediump vec2 vTexCoords;
    void main() {
        vTexCoords = (vec2(1.0) + aPosition) * 0.5;
        gl_Position.xy = aPosition * uScale;
        gl_Position.z = aDepth;
        gl_Position.w = 1.0;
    }
)glsl";

const char* kTextureFragmentShader = R"glsl(#version 100
    precision mediump float;
    varying mediump vec2 vTexCoords;
    uniform lowp sampler2D uTexture;
    void main() {
        gl_FragColor = texture2D(uTexture, vTexCoords);
    }
)glsl";

const char* kExternalTextureFragmentShader = R"glsl(#version 100
    #extension GL_OES_EGL_image_external : require
    precision mediump float;
    varying mediump vec2 vTexCoords;
    uniform samplerExternalOES uTexture;
    void main() {
        gl_FragColor = texture2D(uTexture, vTexCoords);
    }
)glsl";

const char* kYuvTextureFragmentShader = R"glsl(#version 300 es
    #extension GL_EXT_YUV_target : require
    precision mediump float;
    uniform __samplerExternal2DY2YEXT uTexture;
    in vec2 vTexCoords;
    out vec4 outColor;
    void main() {
        vec3 srcYuv = texture(uTexture, vTexCoords).xyz;
        outColor = vec4(yuv_2_rgb(srcYuv, itu_601), 1.0);
    }
)glsl";

const char* kCubeMapFragmentShader = R"glsl(#version 100
    precision mediump float;
    varying mediump vec2 vTexCoords;
    uniform lowp samplerCube uTexture;
    uniform mediump vec3 uFaceVector;
    void main() {
        vec2 scaledTexCoords = (2.0 * vTexCoords) - vec2(1.0);
        vec3 coords = uFaceVector;
        if (uFaceVector.x > 0.0) {
            coords.z = -scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.x < 0.0) {
            coords.z = scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.y > 0.0) {
            coords.x = scaledTexCoords.x;
            coords.z = scaledTexCoords.y;
        }
        if (uFaceVector.y < 0.0) {
            coords.x = scaledTexCoords.x;
            coords.z = -scaledTexCoords.y;
        }
        if (uFaceVector.z > 0.0) {
            coords.x = scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.z < 0.0) {
            coords.x = -scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        gl_FragColor = textureCube(uTexture, coords);
    }
)glsl";

const char* kColorFragmentShader = R"glsl(#version 100
    precision mediump float;
    uniform lowp vec4 uColor;
    void main() {
        gl_FragColor = uColor;
    }
)glsl";

const char* kVertexShaderEs3x = R"glsl(
    in vec2 aPosition;
    in float aDepth;
    uniform mediump float uScale;
    out mediump vec2 vTexCoords;
    void main() {
        vTexCoords = (vec2(1.0) + aPosition) * 0.5;
        gl_Position.xy = aPosition * uScale;
        gl_Position.z = aDepth;
        gl_Position.w = 1.0;
    }
)glsl";

const char* kSsboComputeShaderEs31 = R"glsl(#version 310 es
    layout(local_size_x = 1) in;
    layout(std430, binding=0) buffer Output {
        uint data[];
    } bOutput;
    void main() {
        bOutput.data[gl_GlobalInvocationID.x] =
            gl_GlobalInvocationID.x * 3u;
    }
)glsl";

const char* kArrayFragmentShaderEs30 = R"glsl(#version 300 es
    precision mediump float;
    in mediump vec2 vTexCoords;
    uniform lowp sampler2DArray uTexture;
    uniform mediump float uLayer;
    out mediump vec4 color;
    void main() {
        color = texture(uTexture, vec3(vTexCoords, uLayer));
    }
)glsl";

const char* kCubeMapArrayFragmentShaderEs32 = R"glsl(#version 320 es
    precision mediump float;
    in mediump vec2 vTexCoords;
    uniform lowp samplerCubeArray uTexture;
    uniform mediump float uLayer;
    uniform mediump vec3 uFaceVector;
    out mediump vec4 color;
    void main() {
        vec2 scaledTexCoords = (2.0 * vTexCoords) - vec2(1.0);
        vec4 coords = vec4(uFaceVector, uLayer);
        if (uFaceVector.x > 0.0) {
            coords.z = -scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.x < 0.0) {
            coords.z = scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.y > 0.0) {
            coords.x = scaledTexCoords.x;
            coords.z = scaledTexCoords.y;
        }
        if (uFaceVector.y < 0.0) {
            coords.x = scaledTexCoords.x;
            coords.z = -scaledTexCoords.y;
        }
        if (uFaceVector.z > 0.0) {
            coords.x = scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        if (uFaceVector.z < 0.0) {
            coords.x = -scaledTexCoords.x;
            coords.y = -scaledTexCoords.y;
        }
        color = texture(uTexture, coords);
    }
)glsl";

const char* kStencilFragmentShaderEs30 = R"glsl(#version 300 es
    precision mediump float;
    in mediump vec2 vTexCoords;
    uniform lowp usampler2D uTexture;
    out mediump vec4 color;
    void main() {
        uvec4 stencil = texture(uTexture, vTexCoords);
        color.r = stencil.x == 1u ? 1.0 : 0.0;
        color.g = stencil.x == 3u ? 1.0 : 0.0;
        color.b = stencil.x == 2u ? 1.0 : 0.0;
        color.a = stencil.x == 0u ? 0.0 : 1.0;
    }
)glsl";

const char* kStencilArrayFragmentShaderEs30 = R"glsl(#version 300 es
    precision mediump float;
    in mediump vec2 vTexCoords;
    uniform lowp usampler2DArray uTexture;
    uniform mediump float uLayer;
    out mediump vec4 color;
    void main() {
        uvec4 stencil = texture(uTexture, vec3(vTexCoords, uLayer));
        color.r = stencil.x == 1u ? 1.0 : 0.0;
        color.g = stencil.x == 3u ? 1.0 : 0.0;
        color.b = stencil.x == 2u ? 1.0 : 0.0;
        color.a = stencil.x == 0u ? 0.0 : 1.0;
    }
)glsl";

std::string GetTextureVertexShader(uint32_t format, uint32_t flags) {
    return FormatIsYuv(format) && (flags & kExplicitYuvSampling)
        ? std::string("#version 300 es") + kVertexShaderEs3x
        : kVertexShader;
}

std::string GetTextureFragmentShader(uint32_t format, uint32_t flags) {
    return FormatIsYuv(format)
        ? ((flags & kExplicitYuvSampling)
            ? kYuvTextureFragmentShader
            : kExternalTextureFragmentShader)
        : kTextureFragmentShader;
}

uint32_t GetMaxExpectedColorError(uint32_t format, uint32_t flags) {
    // If format is YUV, and we have no explicit sampling, then we do not
    // know how the color will be converted (spec is not specific), and the
    // maximum error allows for any value. We do not want to abort the test
    // as we still want to ensure rendering and read-outs succeed.
    // If we use explicit sampling, then we know the conversion method
    // (BT.601), but account for some imprecision (2).
    // Otherwise, we do not allow any deviation from the expected value.
    return FormatIsYuv(format)
        ? ((flags & kExplicitYuvSampling) ? 2 : 255)
        : 0;
}

// Interleaved X and Y coordinates for 2 triangles forming a quad with CCW
// orientation.
const float kQuadPositions[] = {
    -1.f, -1.f, 1.f, 1.f, -1.f, 1.f,
    -1.f, -1.f, 1.f, -1.f, 1.f, 1.f,
};
const GLsizei kQuadVertexCount = 6;

// Interleaved X, Y and Z coordinates for 4 triangles forming a "pyramid" as
// seen from above. The center vertex has Z=1, while the edge vertices have Z=-1.
// It looks like this:
//
//        +---+ 1, 1
//        |\ /|
//        | x |
//        |/ \|
// -1, -1 +---+
const float kPyramidPositions[] = {
    -1.f, -1.f, -1.f, 0.f, 0.f, 1.f, -1.f, 1.f, -1.f,
    -1.f, 1.f, -1.f, 0.f, 0.f, 1.f, 1.f, 1.f, -1.f,
    1.f, 1.f, -1.f, 0.f, 0.f, 1.f, 1.f, -1.f, -1.f,
    1.f, -1.f, -1.f, 0.f, 0.f, 1.f, -1.f, -1.f, -1.f,
};
const GLsizei kPyramidVertexCount = 12;

}  // namespace

class AHardwareBufferGLTest : public ::testing::TestWithParam<AHardwareBuffer_Desc> {
public:
    enum AttachmentType {
        kNone,
        kBufferAsTexture,
        kBufferAsRenderbuffer,
        kRenderbuffer,
    };

    void SetUp() override;
    virtual bool SetUpBuffer(const AHardwareBuffer_Desc& desc);
    void SetUpProgram(const std::string& vertex_source, const std::string& fragment_source,
                      const float* mesh, float scale, int texture_unit = 0);
    void SetUpTexture(const AHardwareBuffer_Desc& desc, int unit);
    void SetUpBufferObject(uint32_t size, GLenum target, GLbitfield flags);
    void SetUpFramebuffer(int width, int height, int layer, AttachmentType color,
                          AttachmentType depth = kNone, AttachmentType stencil = kNone,
                          AttachmentType depth_stencil = kNone, int level = 0);
    void TearDown() override;

    void MakeCurrent(int which) {
        if (GetParam().stride & kGlFormat) return;
        mWhich = which;
        eglMakeCurrent(mDisplay, mSurface, mSurface, mContext[mWhich]);
    }
    void MakeCurrentNone() {
        eglMakeCurrent(mDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    bool HasEGLExtension(const std::string& s) {
        return mEGLExtensions.find(s) != mEGLExtensions.end();
    }
    bool HasGLExtension(const std::string& s) {
        return mGLExtensions.find(s) != mGLExtensions.end();
    }
    bool IsFormatColorRenderable(uint32_t format, bool use_srgb) {
        if (use_srgb) {
            // According to the spec, GL_SRGB8 is not color-renderable.
            return format == AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM || format == GL_SRGB8_ALPHA8;
        } else {
            if (format == GL_RGBA16F || format == AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT) {
                return mGLVersion >= 32 || HasGLExtension("GL_EXT_color_buffer_float");
            }
            return true;
        }
    }

protected:
    std::set<std::string> mEGLExtensions;
    std::set<std::string> mGLExtensions;
    EGLDisplay mDisplay = EGL_NO_DISPLAY;
    EGLSurface mSurface = EGL_NO_SURFACE;
    EGLContext mContext[2] = { EGL_NO_CONTEXT, EGL_NO_CONTEXT };
    int mWhich = 0;  // Which of the two EGL contexts is current.
    int mContextCount = 2;  // Will be 2 in AHB test cases and 1 in pure GL test cases.
    int mGLVersion = 0;  // major_version * 10 + minor_version

    AHardwareBuffer* mBuffer = nullptr;
    EGLImageKHR mEGLImage = EGL_NO_IMAGE_KHR;
    GLenum mTexTarget = GL_NONE;
    GLuint mProgram = 0;
    GLint mColorLocation = -1;
    GLint mFaceVectorLocation = -1;
    GLuint mTextures[2] = { 0, 0 };
    GLuint mBufferObjects[2] = { 0, 0 };
    GLuint mFramebuffers[2] = { 0, 0 };
    GLint mMaxTextureUnits = 0;
};

void AHardwareBufferGLTest::SetUp() {
    mDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(mDisplay, NULL, NULL);

    // Try creating an OpenGL ES 3.x context and fall back to 2.x if that fails.
    // Create two contexts for cross-context image sharing tests.
    EGLConfig first_config;
    EGLint config_attrib_list[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
        EGL_NONE
    };
    EGLint num_config = 0;
    eglChooseConfig(mDisplay, config_attrib_list, &first_config, 1, &num_config);
    if (num_config == 0) {
        // There are no configs with the ES 3.0 bit, fall back to ES 2.0.
        config_attrib_list[8] = EGL_NONE;
        config_attrib_list[9] = EGL_NONE;
        eglChooseConfig(mDisplay, config_attrib_list, &first_config, 1, &num_config);
    }
    ASSERT_GT(num_config, 0);

    EGLint context_attrib_list[] = {
        EGL_CONTEXT_CLIENT_VERSION, 3,
        EGL_NONE
    };
    // Try creating an ES 3.0 context, but don't bother if there were no ES 3.0 compatible configs.
    if (config_attrib_list[9] != EGL_NONE) {
        mContext[0] = eglCreateContext(mDisplay, first_config, EGL_NO_CONTEXT, context_attrib_list);
    }
    // If we don't have a context yet, fall back to ES 2.0.
    if (mContext[0] == EGL_NO_CONTEXT) {
        context_attrib_list[1] = 2;
        mContext[0] = eglCreateContext(mDisplay, first_config, EGL_NO_CONTEXT, context_attrib_list);
    }
    mContext[1] = eglCreateContext(mDisplay, first_config, EGL_NO_CONTEXT, context_attrib_list);
    ASSERT_NE(EGL_NO_CONTEXT, mContext[0]);
    ASSERT_NE(EGL_NO_CONTEXT, mContext[1]);

    // Parse EGL extension strings into a set for easier processing.
    std::istringstream eglext_stream(eglQueryString(mDisplay, EGL_EXTENSIONS));
    mEGLExtensions = std::set<std::string>{
        std::istream_iterator<std::string>{eglext_stream},
        std::istream_iterator<std::string>{}
    };
    // Create a 1x1 pbuffer surface if surfaceless contexts are not supported.
    if (!HasEGLExtension("EGL_KHR_surfaceless_context")) {
        EGLint const surface_attrib_list[] = {
            EGL_WIDTH, 1,
            EGL_HEIGHT, 1,
            EGL_NONE
        };
        mSurface = eglCreatePbufferSurface(mDisplay, first_config, surface_attrib_list);
    }
    EGLBoolean result = eglMakeCurrent(mDisplay, mSurface, mSurface, mContext[0]);
    ASSERT_EQ(EGLBoolean{EGL_TRUE}, result);

    // Parse GL extension strings into a set for easier processing.
    std::istringstream glext_stream(reinterpret_cast<const char*>(glGetString(GL_EXTENSIONS)));
    mGLExtensions = std::set<std::string>{
        std::istream_iterator<std::string>{glext_stream},
        std::istream_iterator<std::string>{}
    };
    // Parse GL version. Find the first dot, then treat the digit before it as the major version
    // and the digit after it as the minor version.
    std::string version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::size_t dot_pos = version.find('.');
    ASSERT_TRUE(dot_pos > 0 && dot_pos < version.size() - 1);
    mGLVersion = (version[dot_pos - 1] - '0') * 10 + (version[dot_pos + 1] - '0');
    ASSERT_GE(mGLVersion, 20);
    glGetIntegerv(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, &mMaxTextureUnits);
}

bool AHardwareBufferGLTest::SetUpBuffer(const AHardwareBuffer_Desc& desc) {
    const bool use_srgb = desc.stride & kUseSrgb;
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) {
        if (desc.layers > 6) {
            if (mGLVersion < 32) {
                ALOGI("Test skipped: cube map arrays require GL ES 3.2, found %d.%d",
                      mGLVersion / 10, mGLVersion % 10);
                return false;
            }
            mTexTarget = GL_TEXTURE_CUBE_MAP_ARRAY;
        } else {
            mTexTarget = GL_TEXTURE_CUBE_MAP;
        }
    } else {
        if (desc.layers > 1) {
            if (mGLVersion < 30) {
                ALOGI("Test skipped: texture arrays require GL ES 3.0, found %d.%d",
                      mGLVersion / 10, mGLVersion % 10);
                return false;
            }
            mTexTarget = GL_TEXTURE_2D_ARRAY;
        } else {
            if (FormatIsYuv(desc.format)) {
                mTexTarget = GL_TEXTURE_EXTERNAL_OES;
            } else {
                mTexTarget = GL_TEXTURE_2D;
            }
        }
    }
    if ((desc.format == GL_RGB8 || desc.format == GL_RGBA8) &&
        (desc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) &&
        mGLVersion < 30 && !HasGLExtension("GL_OES_rgb8_rgba8")) {
        ALOGI("Test skipped: GL_RGB8/GL_RGBA8 renderbuffers require GL ES 3.0 or "
              "GL_OES_rgb8_rgba8, but neither were found.");
        return false;
    }
    if (desc.format == GL_SRGB8_ALPHA8 && mGLVersion < 30 &&
        !HasGLExtension("GL_EXT_sRGB")) {
        ALOGI("Test skipped: GL_SRGB8_ALPHA8 requires GL ES 3.0 or GL_EXT_sRGB, "
              "but neither were found.");
        return false;
    }
    if (desc.format == GL_RGB10_A2 && mGLVersion < 30) {
        ALOGI("Test skipped: GL_RGB10_A2 requires GL ES 3.0, found %d.%d",
              mGLVersion / 10, mGLVersion % 10);
        return false;
    }
    if (desc.format == GL_RGBA16F && mGLVersion < 30) {
        ALOGI("Test skipped: GL_RGBA16F requires GL ES 3.0, found %d.%d",
              mGLVersion / 10, mGLVersion % 10);
        return false;
    }
    if (desc.format == GL_DEPTH_COMPONENT16 &&
        (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) &&
        mGLVersion < 30 && !HasGLExtension("GL_OES_depth_texture")) {
        ALOGI("Test skipped: depth textures require GL ES 3.0 or "
              "GL_OES_depth_texture, but neither were found.");
        return false;
    }
    if (desc.format == GL_DEPTH24_STENCIL8 &&
        (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) &&
        mGLVersion < 30 && !HasGLExtension("GL_OES_packed_depth_stencil")) {
        ALOGI("Test skipped: depth-stencil textures require GL ES 3.0 or "
              "GL_OES_packed_depth_stencil, but neither were found.");
        return false;
    }
    if (mTexTarget == GL_TEXTURE_EXTERNAL_OES &&
        (desc.usage & AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE) &&
        !HasGLExtension("GL_OES_EGL_image_external")) {
        ALOGI("Test skipped: External textures are not supported but required "
              "by this test.");
        return false;
    }
    if (FormatIsYuv(desc.format) && !HasGLExtension("GL_EXT_YUV_target")) {
        ALOGI("Test skipped: The GL_EXT_YUV_target extension is required for "
              "operations in the YUV color space.");
        return false;
    }
    // For control cases using GL formats, the test should be run in a single
    // context, without using AHardwareBuffer. This simplifies verifying that
    // the test behaves as expected even if the AHardwareBuffer format under
    // test is not supported.
    if (desc.stride & kGlFormat) {
        mContextCount = 1;
        return true;
    }

    // The code below will only execute if we are allocating a real AHardwareBuffer.
    if (use_srgb && !HasEGLExtension("EGL_EXT_image_gl_colorspace")) {
        ALOGI("Test skipped: sRGB hardware buffers require EGL_EXT_image_gl_colorspace");
        return false;
    }
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP &&
        !HasGLExtension("GL_EXT_EGL_image_storage")) {
        ALOGI("Test skipped: cube map array hardware buffers require "
              "GL_EXT_EGL_image_storage");
        return false;
    }
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE &&
        !HasGLExtension("GL_EXT_EGL_image_storage")) {
        ALOGI("Test skipped: mipmapped hardware buffers require "
              "GL_EXT_EGL_image_storage");
        return false;
    }

    int result = AHardwareBuffer_allocate(&desc, &mBuffer);

    ALOGI("Attempting to allocate format=%s width=%d height=%d layers=%d result=%d",
        AHBFormatAsString(desc.format), desc.width, desc.height, desc.layers, result);

    // Skip if this format cannot be allocated.
    if (result != NO_ERROR) {
        EXPECT_FALSE(AHardwareBuffer_isSupported(&desc)) <<
            "AHardwareBuffer_isSupported returned true, but buffer allocation failed. "
            "Potential gralloc bug or resource exhaustion.";
        ALOGI("Test skipped: format %s could not be allocated",
              AHBFormatAsString(desc.format));
        return false;
    }
    EXPECT_TRUE(AHardwareBuffer_isSupported(&desc)) <<
        "AHardwareBuffer_isSupported returned false, but buffer allocation succeeded. "
        "This is most likely a bug in the gralloc implementation.";

    // The code below will only execute if allocating an AHardwareBuffer succeeded.
    // Fail early if the buffer is mipmapped or a cube map, but the GL extension required
    // to actually access it from GL is not present.
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP &&
        !HasGLExtension("GL_EXT_EGL_image_storage")) {
        ADD_FAILURE() << "Cube map AHardwareBuffer allocation succeeded, but the extension "
            "GL_EXT_EGL_image_storage is not present";
        return false;
    }
    if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE &&
            !HasGLExtension("GL_EXT_EGL_image_storage")) {
        ADD_FAILURE() << "Mipmapped AHardwareBuffer allocation succeeded, but the extension "
            "GL_EXT_EGL_image_storage is not present";
        return false;
    }

    // Do not create the EGLImage if this is a blob format.
    if (desc.format == AHARDWAREBUFFER_FORMAT_BLOB) return true;

    EGLint attrib_list[3] = { EGL_NONE, EGL_NONE, EGL_NONE };
    if (use_srgb) {
        attrib_list[0] = EGL_GL_COLORSPACE_KHR;
        attrib_list[1] = EGL_GL_COLORSPACE_SRGB_KHR;
    }
    mEGLImage = eglCreateImageKHR(
        mDisplay, EGL_NO_CONTEXT, EGL_NATIVE_BUFFER_ANDROID,
        eglGetNativeClientBufferANDROID(mBuffer), attrib_list);
    EXPECT_NE(EGL_NO_IMAGE_KHR, mEGLImage) <<
        "AHardwareBuffer allocation succeeded, but binding it to an EGLImage failed. "
        "This is usually caused by a version mismatch between the gralloc implementation and "
        "the OpenGL/EGL driver. Please contact your GPU vendor to resolve this problem.";
    return mEGLImage != EGL_NO_IMAGE_KHR;
}

void AHardwareBufferGLTest::SetUpProgram(const std::string& vertex_source,
                                         const std::string& fragment_source,
                                         const float* mesh, float scale, int texture_unit) {
    ASSERT_EQ(0U, mProgram);
    GLint status = GL_FALSE;
    mProgram = glCreateProgram();
    GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    const char* vertex_source_cstr = vertex_source.c_str();
    glShaderSource(vertex_shader, 1, &vertex_source_cstr, nullptr);
    glCompileShader(vertex_shader);
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &status);
    ASSERT_EQ(GL_TRUE, status) << "Vertex shader compilation failed";
    GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    const char* fragment_source_cstr = fragment_source.c_str();
    glShaderSource(fragment_shader, 1, &fragment_source_cstr, nullptr);
    glCompileShader(fragment_shader);
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &status);
    ASSERT_EQ(GL_TRUE, status) << "Fragment shader compilation failed";
    glAttachShader(mProgram, vertex_shader);
    glAttachShader(mProgram, fragment_shader);
    glLinkProgram(mProgram);
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
    ASSERT_EQ(GL_TRUE, status) << "Shader program linking failed";
    glDetachShader(mProgram, vertex_shader);
    glDetachShader(mProgram, fragment_shader);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glUseProgram(mProgram);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during shader program setup";

    GLint a_position_location = glGetAttribLocation(mProgram, "aPosition");
    GLint a_depth_location = glGetAttribLocation(mProgram, "aDepth");
    if (mesh == kQuadPositions) {
        glVertexAttribPointer(a_position_location, 2, GL_FLOAT, GL_TRUE, 0, kQuadPositions);
        glVertexAttrib1f(a_depth_location, 0.f);
        glEnableVertexAttribArray(a_position_location);
    } else if (mesh == kPyramidPositions) {
        glVertexAttribPointer(a_position_location, 2, GL_FLOAT, GL_TRUE, 3 * sizeof(float),
                              kPyramidPositions);
        glVertexAttribPointer(a_depth_location, 1, GL_FLOAT, GL_TRUE, 3 * sizeof(float),
                              kPyramidPositions + 2);
        glEnableVertexAttribArray(a_position_location);
        glEnableVertexAttribArray(a_depth_location);
    } else {
        FAIL() << "Unknown mesh";
    }
    glUniform1f(glGetUniformLocation(mProgram, "uScale"), scale);
    mColorLocation = glGetUniformLocation(mProgram, "uColor");
    if (mColorLocation >= 0) {
        glUniform4f(mColorLocation, 1.f, 0.f, 0.f, 1.f);
    }
    GLint u_texture_location = glGetUniformLocation(mProgram, "uTexture");
    if (u_texture_location >= 0) {
        glUniform1i(u_texture_location, texture_unit);
    }
    GLint u_layer_location = glGetUniformLocation(mProgram, "uLayer");
    if (u_layer_location >= 0) {
        glUniform1f(u_layer_location, static_cast<float>(GetParam().layers - 1));
    }
    mFaceVectorLocation = glGetUniformLocation(mProgram, "uFaceVector");
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during shader uniform setup";
}

void AHardwareBufferGLTest::SetUpTexture(const AHardwareBuffer_Desc& desc, int unit) {
    GLuint& texture = mTextures[mWhich];
    glGenTextures(1, &texture);
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(mTexTarget, texture);
    // If the texture does not have mipmaps, set a filter that does not require them.
    if (!(desc.usage & AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE)) {
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    }
    if (desc.stride & kGlFormat) {
        int levels = 1;
        if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE) {
            levels = MipLevelCount(desc.width, desc.height);
        }
        // kGlFormat is set in the stride field, so interpret desc.format as a GL format.
        if ((desc.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) ? desc.layers > 6 : desc.layers > 1) {
            glTexStorage3D(mTexTarget, levels, desc.format, desc.width, desc.height, desc.layers);
        } else if (mGLVersion >= 30) {
            glTexStorage2D(mTexTarget, levels, desc.format, desc.width, desc.height);
        } else {
            // Compatibility code for ES 2.0 goes here.
            GLenum internal_format = 0, format = 0, type = 0;
            switch (desc.format) {
                case GL_RGB8:
                    internal_format = GL_RGB;
                    format = GL_RGB;
                    type = GL_UNSIGNED_BYTE;
                    break;
                case GL_RGBA8:
                    internal_format = GL_RGBA;
                    format = GL_RGBA;
                    type = GL_UNSIGNED_BYTE;
                    break;
                case GL_SRGB8_ALPHA8:
                    // Available through GL_EXT_sRGB.
                    internal_format = GL_SRGB_ALPHA_EXT;
                    format = GL_RGBA;
                    type = GL_UNSIGNED_BYTE;
                    break;
                case GL_DEPTH_COMPONENT16:
                    // Available through GL_OES_depth_texture.
                    // Note that these are treated as luminance textures, not as red textures.
                    internal_format = GL_DEPTH_COMPONENT;
                    format = GL_DEPTH_COMPONENT;
                    type = GL_UNSIGNED_SHORT;
                    break;
                case GL_DEPTH24_STENCIL8:
                    // Available through GL_OES_packed_depth_stencil.
                    internal_format = GL_DEPTH_STENCIL_OES;
                    format = GL_DEPTH_STENCIL;
                    type = GL_UNSIGNED_INT_24_8;
                    break;
                default:
                    FAIL() << "Unrecognized GL format"; break;
            }
            if (desc.usage & AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP) {
                for (int face = 0; face < 6; ++face) {
                    uint32_t width = desc.width;
                    uint32_t height = desc.height;
                    for (int level = 0; level < levels; ++level) {
                        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + face, level, internal_format,
                                     width, height, 0, format, type, nullptr);
                        width /= 2;
                        height /= 2;
                    }
                }
            } else {
                uint32_t width = desc.width;
                uint32_t height = desc.height;
                for (int level = 0; level < levels; ++level) {
                    glTexImage2D(mTexTarget, level, internal_format, width, height, 0, format,
                                 type, nullptr);
                    width /= 2;
                    height /= 2;
                }
            }
        }
    } else {
        if (HasGLExtension("GL_EXT_EGL_image_storage")) {
            glEGLImageTargetTexStorageEXT(mTexTarget, static_cast<GLeglImageOES>(mEGLImage),
                                          nullptr);
        } else {
            glEGLImageTargetTexture2DOES(mTexTarget, static_cast<GLeglImageOES>(mEGLImage));
        }
    }
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during texture setup";
}

void AHardwareBufferGLTest::SetUpBufferObject(uint32_t size, GLenum target, GLbitfield flags) {
    glGenBuffers(1, &mBufferObjects[mWhich]);
    glBindBuffer(target, mBufferObjects[mWhich]);
    glBufferStorageExternalEXT(target, 0, size,
                               eglGetNativeClientBufferANDROID(mBuffer), flags);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during buffer object setup";
}

void AHardwareBufferGLTest::SetUpFramebuffer(int width, int height, int layer,
                                             AttachmentType color,
                                             AttachmentType depth,
                                             AttachmentType stencil,
                                             AttachmentType depth_stencil,
                                             int level) {
    AttachmentType attachment_types[] = { color, depth, stencil, depth_stencil };
    GLenum attachment_points[] = {
        GL_COLOR_ATTACHMENT0, GL_DEPTH_ATTACHMENT, GL_STENCIL_ATTACHMENT,
        GL_DEPTH_STENCIL_ATTACHMENT
    };
    GLenum default_formats[] = {
        GL_RGBA8, GL_DEPTH_COMPONENT16, GL_STENCIL_INDEX8, GL_DEPTH24_STENCIL8
    };
    GLuint& fbo = mFramebuffers[mWhich];
    GLbitfield clear_bits[] = {
        GL_COLOR_BUFFER_BIT, GL_DEPTH_BUFFER_BIT, GL_STENCIL_BUFFER_BIT,
        GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT
    };

    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClearDepthf(1.0f);
    glClearStencil(0);
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    for (int i = 0; i < 4; ++i) {
        switch (attachment_types[i]) {
            case kNone:
                break;
            case kBufferAsTexture:
                ASSERT_NE(0U, mTextures[mWhich]);
                if (mTexTarget == GL_TEXTURE_2D || mTexTarget == GL_TEXTURE_EXTERNAL_OES) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_points[i], mTexTarget,
                                           mTextures[mWhich], level);
                } else if (mTexTarget == GL_TEXTURE_CUBE_MAP) {
                    glFramebufferTexture2D(GL_FRAMEBUFFER, attachment_points[i],
                                           GL_TEXTURE_CUBE_MAP_POSITIVE_X + layer,
                                           mTextures[mWhich], level);
                } else {
                    glFramebufferTextureLayer(GL_FRAMEBUFFER, attachment_points[i],
                                              mTextures[mWhich], level, layer);
                }
                break;
            case kBufferAsRenderbuffer: {
                ASSERT_EQ(0, layer);
                GLuint renderbuffer = 0;
                glGenRenderbuffers(1, &renderbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());
                bool isGlFormat = GetParam().stride & kGlFormat;
                if (isGlFormat) {
                    glRenderbufferStorage(GL_RENDERBUFFER, GetParam().format, width, height);
                } else {
                    ASSERT_FALSE(FormatIsYuv(GetParam().format)) << "YUV renderbuffers unsupported";
                    glEGLImageTargetRenderbufferStorageOES(GL_RENDERBUFFER,
                                                           static_cast<GLeglImageOES>(mEGLImage));
                }
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_points[i],
                                          GL_RENDERBUFFER, renderbuffer);
                if (isGlFormat)
                    glClear(clear_bits[i]);
                break;
            }
            case kRenderbuffer: {
                ASSERT_EQ(0, layer);
                GLuint renderbuffer = 0;
                glGenRenderbuffers(1, &renderbuffer);
                glBindRenderbuffer(GL_RENDERBUFFER, renderbuffer);
                glRenderbufferStorage(GL_RENDERBUFFER, default_formats[i], width, height);
                glFramebufferRenderbuffer(GL_FRAMEBUFFER, attachment_points[i],
                                          GL_RENDERBUFFER, renderbuffer);
                glClear(clear_bits[i]);
                break;
            }
            default: FAIL() << "Unrecognized binding type";
        }
    }
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during framebuffer setup";
    ASSERT_EQ(GLenum{GL_FRAMEBUFFER_COMPLETE},
              glCheckFramebufferStatus(GL_FRAMEBUFFER)) << "Framebuffer not complete";
    glViewport(0, 0, width, height);
}

void AHardwareBufferGLTest::TearDown() {
    MakeCurrentNone();
    for (int i = 0; i < 2; ++i) {
        // All GL objects will be deleted along with the context.
        eglDestroyContext(mDisplay, mContext[i]);
    }
    if (mBuffer != nullptr) {
        eglDestroyImageKHR(mDisplay, mEGLImage);
        AHardwareBuffer_release(mBuffer);
    }
    if (mSurface != EGL_NO_SURFACE) {
        eglDestroySurface(mDisplay, mSurface);
    }
    eglTerminate(mDisplay);
}


class BlobTest : public AHardwareBufferGLTest {
public:
    bool SetUpBuffer(const AHardwareBuffer_Desc& desc) override {
        if (!HasGLExtension("GL_EXT_external_buffer")) {
            ALOGI("Test skipped: GL_EXT_external_buffer not present");
            return false;
        }
        return AHardwareBufferGLTest::SetUpBuffer(desc);
    }
};

// Verifies that a blob buffer can be used to supply vertex attributes to a shader.
TEST_P(BlobTest, GpuDataBufferVertexBuffer) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = sizeof kQuadPositions;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER;
    if (!SetUpBuffer(desc)) return;

    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(kVertexShader, kColorFragmentShader, kQuadPositions, 0.5f));

    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(
            SetUpBufferObject(desc.width, GL_ARRAY_BUFFER,
                              GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_WRITE_BIT));
    }
    float* data = static_cast<float*>(
        glMapBufferRange(GL_ARRAY_BUFFER, 0, desc.width,
                         GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
    ASSERT_NE(data, nullptr) << "glMapBufferRange on a blob buffer failed";
    memcpy(data, kQuadPositions, desc.width);
    glUnmapBuffer(GL_ARRAY_BUFFER);
    glFinish();

    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    GLint a_position_location = glGetAttribLocation(mProgram, "aPosition");
    glVertexAttribPointer(a_position_location, 2, GL_FLOAT, GL_TRUE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels. There should be a red square in the middle.
    std::vector<GoldenPixel> goldens{
        {5, 35, kZero}, {15, 35, kZero}, {25, 35, kZero}, {35, 35, kZero},
        {5, 25, kZero}, {15, 25, kRed},  {25, 25, kRed},  {35, 25, kZero},
        {5, 15, kZero}, {15, 15, kRed},  {25, 15, kRed},  {35, 15, kZero},
        {5,  5, kZero}, {15,  5, kZero}, {25, 5,  kZero}, {35, 5,  kZero},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// Verifies that a blob buffer can be directly accessed from the CPU.
TEST_P(BlobTest, GpuDataBufferCpuWrite) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = sizeof kQuadPositions;
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY | AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER;
    if (!SetUpBuffer(desc)) return;

    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(kVertexShader, kColorFragmentShader, kQuadPositions, 0.5f));

    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(
            SetUpBufferObject(desc.width, GL_ARRAY_BUFFER,
                              GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_WRITE_BIT));
    }

    // Clear the buffer to zero
    std::vector<float> zero_data(desc.width / sizeof(float), 0.f);
    glBufferSubData(GL_ARRAY_BUFFER, 0, desc.width, zero_data.data());
    glFinish();

    // Upload actual data with CPU access
    float* data = nullptr;
    int result = AHardwareBuffer_lock(mBuffer, AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY,
                                      -1, nullptr, reinterpret_cast<void**>(&data));
    ASSERT_EQ(NO_ERROR, result);
    memcpy(data, kQuadPositions, desc.width);
    AHardwareBuffer_unlock(mBuffer, nullptr);

    // Render the buffer in the other context
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    GLint a_position_location = glGetAttribLocation(mProgram, "aPosition");
    glVertexAttribPointer(a_position_location, 2, GL_FLOAT, GL_TRUE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels. There should be a red square in the middle.
    std::vector<GoldenPixel> goldens{
        {5, 35, kZero}, {15, 35, kZero}, {25, 35, kZero}, {35, 35, kZero},
        {5, 25, kZero}, {15, 25, kRed},  {25, 25, kRed},  {35, 25, kZero},
        {5, 15, kZero}, {15, 15, kRed},  {25, 15, kRed},  {35, 15, kZero},
        {5,  5, kZero}, {15,  5, kZero}, {25, 5,  kZero}, {35, 5,  kZero},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// Verifies that data written into a blob buffer from the GPU can be read on the CPU.
TEST_P(BlobTest, GpuDataBufferCpuRead) {
    if (mGLVersion < 31) {
        ALOGI("Test skipped: shader storage buffer objects require ES 3.1+, found %d.%d",
              mGLVersion / 10, mGLVersion % 10);
        return;
    }
    const int kBufferElements = 16;
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = kBufferElements * sizeof(int);
    desc.usage = AHARDWAREBUFFER_USAGE_CPU_READ_RARELY | AHARDWAREBUFFER_USAGE_GPU_DATA_BUFFER;
    if (!SetUpBuffer(desc)) return;

    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(
            SetUpBufferObject(desc.width, GL_SHADER_STORAGE_BUFFER,
                              GL_DYNAMIC_STORAGE_BIT_EXT | GL_MAP_READ_BIT));
    }

    // Clear the buffer to zero
    std::vector<unsigned int> expected_data(kBufferElements, 0U);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, desc.width, expected_data.data());
    glFinish();

    // Write into the buffer with a compute shader
    GLint status = 0;
    mProgram = glCreateProgram();
    GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(shader, 1, &kSsboComputeShaderEs31, nullptr);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    ASSERT_EQ(GL_TRUE, status) << "Compute shader compilation failed";
    glAttachShader(mProgram, shader);
    glLinkProgram(mProgram);
    glGetProgramiv(mProgram, GL_LINK_STATUS, &status);
    ASSERT_EQ(GL_TRUE, status) << "Shader program linking failed";
    glDetachShader(mProgram, shader);
    glDeleteShader(shader);
    glUseProgram(mProgram);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during compute shader setup";
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, mBufferObjects[mWhich]);
    glDispatchCompute(kBufferElements, 1, 1);
    glMemoryBarrier(GL_BUFFER_UPDATE_BARRIER_BIT);
    glFinish();
    EXPECT_EQ(GLenum{GL_NO_ERROR}, glGetError()) << "GL error during compute shader execution";

    // Inspect the data written into the buffer using CPU access.
    MakeCurrent(0);
    unsigned int* data = nullptr;
    int result = AHardwareBuffer_lock(mBuffer, AHARDWAREBUFFER_USAGE_CPU_READ_RARELY,
                                      -1, nullptr, reinterpret_cast<void**>(&data));
    ASSERT_EQ(NO_ERROR, result) << "AHardwareBuffer_lock failed with error " << result;
    std::ostringstream s;
    for (int i = 0; i < kBufferElements; ++i) {
        expected_data[i] = static_cast<unsigned int>(i * 3);
        s << data[i] << ", ";
    }
    EXPECT_EQ(0, memcmp(expected_data.data(), data, desc.width)) << s.str();
    AHardwareBuffer_unlock(mBuffer, nullptr);
}

// The first case tests an ordinary GL buffer, while the second one tests an AHB-backed buffer.
INSTANTIATE_TEST_CASE_P(
    Blob, BlobTest,
    ::testing::Values(
        AHardwareBuffer_Desc{1, 1, 1, AHARDWAREBUFFER_FORMAT_BLOB, 0, 0, 0, 0}),
    &GetTestName);


class ColorTest : public AHardwareBufferGLTest {
public:
    bool SetUpBuffer(const AHardwareBuffer_Desc& desc) override {
        if ((desc.usage & AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT) &&
            !IsFormatColorRenderable(desc.format, desc.stride & kUseSrgb)) {
            ALOGI("Test skipped: requires GPU_COLOR_OUTPUT, but format is not color-renderable");
            return false;
        }
        return AHardwareBufferGLTest::SetUpBuffer(desc);
    }
};

// Verify that when allocating an AHardwareBuffer succeeds with GPU_COLOR_OUTPUT,
// it can be bound as a framebuffer attachment, glClear'ed and then read from
// another context using glReadPixels.
TEST_P(ColorTest, GpuColorOutputIsRenderable) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = 100;
    desc.height = 100;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    if (FormatIsYuv(desc.format)) {
        // YUV formats are only supported for textures, so add texture usage.
        desc.usage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    }
    // This test does not make sense for layered buffers - don't bother testing them.
    if (desc.layers > 1) return;
    if (!SetUpBuffer(desc)) return;

    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);

        // YUV renderbuffers are unsupported, so we attach as a texture in this case.
        AttachmentType attachmentType;
        if (FormatIsYuv(desc.format)) {
            ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, 1));
            attachmentType = kBufferAsTexture;
        } else {
            attachmentType = kBufferAsRenderbuffer;
        }

        ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(desc.width, desc.height, 0, attachmentType));
    }

    // Draw a simple checkerboard pattern in the second context, which will
    // be current after the loop above, then read it in the first.
    DrawCheckerboard(desc.width, desc.height, desc.format);
    glFinish();

    MakeCurrent(0);
    std::vector<GoldenPixel> goldens{
        {10, 90, kRed},  {40, 90, kRed},  {60, 90, kBlue},  {90, 90, kBlue},
        {10, 60, kRed},  {40, 60, kRed},  {60, 60, kBlue},  {90, 60, kBlue},
        {10, 40, kZero}, {40, 40, kZero}, {60, 40, kGreen}, {90, 40, kGreen},
        {10, 10, kZero}, {40, 10, kZero}, {60, 10, kGreen}, {90, 10, kGreen},
    };
    CheckGoldenPixels(goldens, desc.format);
}

// Verifies that the content of GPU_COLOR_OUTPUT buffers can be read on the CPU directly by
// locking the HardwareBuffer.
TEST_P(ColorTest, GpuColorOutputCpuRead) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = 16;
    desc.height = 16;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_CPU_READ_RARELY;
    if (FormatIsYuv(desc.format)) {
        // YUV formats are only supported for textures, so add texture usage.
        desc.usage |= AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    }
    // This test does not make sense for GL formats. Layered buffers do not support CPU access.
    if ((desc.stride & kGlFormat) || desc.layers > 1) {
        ALOGI("Test skipped: Test is for single-layer HardwareBuffer formats only.");
        return;
    }
    if (!SetUpBuffer(desc)) return;

    MakeCurrent(1);

    // YUV renderbuffers are unsupported, so we attach as a texture in this case.
    AttachmentType attachmentType;
    if (FormatIsYuv(desc.format)) {
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, 1));
        attachmentType = kBufferAsTexture;
    } else {
        attachmentType = kBufferAsRenderbuffer;
    }

    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(desc.width, desc.height, 0, attachmentType));

    // Draw a simple checkerboard pattern in the second context, which will
    // be current after the loop above, then read it in the first.
    DrawCheckerboard(desc.width, desc.height, desc.format);
    glFinish();

    MakeCurrent(0);
    std::vector<GoldenPixel> goldens{
        {0, 15, kRed},  {7, 15, kRed},  {8, 15, kBlue},  {15, 15, kBlue},
        {0,  8, kRed},  {7,  8, kRed},  {8,  8, kBlue},  {15,  8, kBlue},
        {0,  7, kZero}, {7,  7, kZero}, {8,  7, kGreen}, {15,  7, kGreen},
        {0,  0, kZero}, {7,  0, kZero}, {8,  0, kGreen}, {15,  0, kGreen},
    };

    // As we glCleared the colors, the YUV colors will simply be the RGB values
    CheckCpuGoldenPixels(goldens, mBuffer);
}

// Verifies that the CPU can write directly to a HardwareBuffer, and the GPU can then read from
// that buffer.
TEST_P(ColorTest, CpuWriteColorGpuRead) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = 16;
    desc.height = 16;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE | AHARDWAREBUFFER_USAGE_CPU_WRITE_RARELY;
    // This test does not make sense for GL formats. Layered buffers do not support CPU access.
    if ((desc.stride & kGlFormat) || desc.layers > 1) {
        ALOGI("Test skipped: Test is for single-layer HardwareBuffer formats only.");
        return;
    }

    if (!SetUpBuffer(desc)) return;

    // Write into buffer when no context is active
    MakeCurrentNone();
    WriteCheckerBoard(mBuffer);

    // Now setup a texture in a context to sample from this buffer
    MakeCurrent(0);
    const int kTextureUnit = 6 % mMaxTextureUnits;
    ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
    glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    // Draw a quad that samples from the texture.
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(16, 16, 0, kRenderbuffer));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    std::string vertex_shader = GetTextureVertexShader(desc.format, desc.stride);
    std::string fragment_shader = GetTextureFragmentShader(desc.format, desc.stride);
    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(vertex_shader, fragment_shader, kQuadPositions,
            1.0f, kTextureUnit));

    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels.
    // Non-alpha formats will render black instead of zero
    GoldenColor dark = FormatHasAlpha(desc.format) ? kZero : kBlack;
    std::vector<GoldenPixel> goldens{
        {0, 15, kRed},  {7, 15, kRed},  {8, 15, kBlue},  {15, 15, kBlue},
        {0,  8, kRed},  {7,  8, kRed},  {8,  8, kBlue},  {15,  8, kBlue},
        {0,  7, dark},  {7,  7, dark},  {8,  7, kGreen}, {15,  7, kGreen},
        {0,  0, dark},  {7,  0, dark},  {8,  0, kGreen}, {15,  0, kGreen},
    };
    // If source was YUV, there may be some conversion imprecision, so we allow some error
    CheckGoldenPixels(goldens, GL_RGBA8, GetMaxExpectedColorError(desc.format, desc.stride));
}

// Verify that when allocating an AHardwareBuffer succeeds with GPU_SAMPLED_IMAGE,
// it can be bound as a texture, set to a color with glTexSubImage2D and sampled
// from in a fragment shader.
TEST_P(ColorTest, GpuSampledImageCanBeSampled) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;

    // This test requires using glTexImage2d to assign image data. YUV formats do not
    // support this. Other tests using glClear and CPU access test the YUV variants.
    if (FormatIsYuv(desc.format)) {
        ALOGI("Test Skipped: YUV formats do not support glTexImage2d and variants.");
        return;
    }

    if (!SetUpBuffer(desc)) return;

    // Bind the EGLImage to textures in both contexts.
    const int kTextureUnit = 6 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    // In the second context, upload opaque red to the texture.
    ASSERT_NO_FATAL_FAILURE(UploadRedPixels(desc));
    glFinish();

    // In the first context, draw a quad that samples from the texture.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (desc.layers > 1) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(std::string("#version 300 es") + kVertexShaderEs3x,
                         kArrayFragmentShaderEs30, kQuadPositions, 0.5f, kTextureUnit));
    } else {
        std::string vertex_shader = GetTextureVertexShader(desc.format, desc.stride);
        std::string fragment_shader = GetTextureFragmentShader(desc.format, desc.stride);
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(vertex_shader, fragment_shader, kQuadPositions,
                         0.5f, kTextureUnit));
    }
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels. There should be a red square in the middle.
    GoldenColor color = kRed;
    if (desc.stride & kUseSrgb) {
        color = FormatHasAlpha(desc.format) ? kRed50 : kRed50Alpha100;
    }
    std::vector<GoldenPixel> goldens{
        {5, 35, kZero}, {15, 35, kZero}, {25, 35, kZero}, {35, 35, kZero},
        {5, 25, kZero}, {15, 25, color}, {25, 25, color}, {35, 25, kZero},
        {5, 15, kZero}, {15, 15, color}, {25, 15, color}, {35, 15, kZero},
        {5,  5, kZero}, {15,  5, kZero}, {25, 5,  kZero}, {35, 5,  kZero},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// Verify that buffers which have both GPU_SAMPLED_IMAGE and GPU_COLOR_OUTPUT
// can be both rendered and sampled as a texture.
TEST_P(ColorTest, GpuColorOutputAndSampledImage) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    if (!SetUpBuffer(desc)) return;

    // Bind the EGLImage to textures in both contexts.
    const int kTextureUnit = 1 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // In the second context, draw a checkerboard pattern.
    ASSERT_NO_FATAL_FAILURE(
        SetUpFramebuffer(desc.width, desc.height, desc.layers - 1, kBufferAsTexture));
    DrawCheckerboard(desc.width, desc.height, desc.format);
    glFinish();

    // In the first context, draw a quad that samples from the texture.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (desc.layers > 1) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(std::string("#version 300 es") + kVertexShaderEs3x,
                         kArrayFragmentShaderEs30, kQuadPositions, 0.5f, kTextureUnit));
    } else {
        std::string vertex_shader = GetTextureVertexShader(desc.format, desc.stride);
        std::string fragment_shader = GetTextureFragmentShader(desc.format, desc.stride);
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(vertex_shader, fragment_shader, kQuadPositions,
                         0.5f, kTextureUnit));
    }
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels. The lower left area of the checkerboard will
    // be either transparent or opaque black depending on whether the texture
    // format has an alpha channel.
    const GoldenColor kCBBlack = FormatHasAlpha(desc.format) ? kZero : kBlack;
    std::vector<GoldenPixel> goldens{
        {5, 35, kZero}, {15, 35, kZero},    {25, 35, kZero},  {35, 35, kZero},
        {5, 25, kZero}, {15, 25, kRed},     {25, 25, kBlue},  {35, 25, kZero},
        {5, 15, kZero}, {15, 15, kCBBlack}, {25, 15, kGreen}, {35, 15, kZero},
        {5, 5,  kZero}, {15, 5,  kZero},    {25, 5,  kZero},  {35, 5,  kZero},
    };
    CheckGoldenPixels(goldens, GL_RGBA8, GetMaxExpectedColorError(desc.format, desc.stride));
}

TEST_P(ColorTest, MipmapComplete) {
    if (mGLVersion < 30) {
        ALOGI("Test skipped: reading from nonzero level of a mipmap requires ES 3.0+, "
              "found %d.%d", mGLVersion / 10, mGLVersion % 10);
        return;
    }
    const int kNumTiles = 8;
    AHardwareBuffer_Desc desc = GetParam();
    // Ensure that the checkerboard tiles have equal size at every level of the mipmap.
    desc.width = std::max(8u, RoundUpToPowerOf2(desc.width));
    desc.height = std::max(8u, RoundUpToPowerOf2(desc.height));
    desc.usage =
        AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
        AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
        AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE;
    if (!SetUpBuffer(desc)) return;

    const int kTextureUnit = 7 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    }
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Draw checkerboard for mipmapping.
    const int kTileWidth = desc.width / kNumTiles;
    const int kTileHeight = desc.height / kNumTiles;
    ASSERT_NO_FATAL_FAILURE(
        SetUpFramebuffer(desc.width, desc.height, desc.layers - 1, kBufferAsTexture));
    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < kNumTiles; ++i) {
        for (int j = 0; j < kNumTiles; ++j) {
            const float v = (i & 1) ^ (j & 1) ? 1.f : 0.f;
            glClearColor(v, 0.f, 0.f, v);
            glScissor(i * kTileWidth, j * kTileHeight, kTileWidth, kTileHeight);
            glClear(GL_COLOR_BUFFER_BIT);
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glGenerateMipmap(mTexTarget);
    glFinish();
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(
        SetUpFramebuffer(1, 1, desc.layers - 1, kBufferAsTexture, kNone, kNone, kNone,
                         MipLevelCount(desc.width, desc.height) - 1));
    std::vector<GoldenPixel> goldens{{0, 0, (desc.stride & kUseSrgb) ? kRed50Srgb : kRed50}};
    CheckGoldenPixels(goldens, desc.format);
}

TEST_P(ColorTest, CubemapSampling) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage =
        AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
        AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
        AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP;
    desc.height = desc.width;
    desc.layers *= 6;
    if (!SetUpBuffer(desc)) return;

    const int kTextureUnit = 4 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
    }

    for (int i = 0; i < 6; ++i) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(desc.width, desc.height, desc.layers - 6 + i, kBufferAsTexture));
        DrawCheckerboard(desc.width, desc.height, desc.format);
    }
    glFinish();

    MakeCurrent(0);
    if (desc.layers > 6) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(std::string("#version 320 es") + kVertexShaderEs3x,
                         kCubeMapArrayFragmentShaderEs32, kQuadPositions, 0.5f, kTextureUnit));
    } else {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(kVertexShader, kCubeMapFragmentShader, kQuadPositions, 0.5f, kTextureUnit));
    }
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    for (int i = 0; i < 6; ++i) {
        float face_vector[3] = {0.f, 0.f, 0.f};
        face_vector[i / 2] = (i % 2) ? -1.f : 1.f;
        glUniform3fv(mFaceVectorLocation, 1, face_vector);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);

        const GoldenColor kCBBlack = FormatHasAlpha(desc.format) ? kZero : kBlack;
        std::vector<GoldenPixel> goldens{
            {5, 35, kZero}, {15, 35, kZero},    {25, 35, kZero},  {35, 35, kZero},
            {5, 25, kZero}, {15, 25, kRed},     {25, 25, kBlue},  {35, 25, kZero},
            {5, 15, kZero}, {15, 15, kCBBlack}, {25, 15, kGreen}, {35, 15, kZero},
            {5, 5,  kZero}, {15, 5,  kZero},    {25, 5,  kZero},  {35, 5,  kZero},
        };
        CheckGoldenPixels(goldens, GL_RGBA8);
    }
}

TEST_P(ColorTest, CubemapMipmaps) {
    if (mGLVersion < 30) {
        ALOGI("Test skipped: reading from nonzero level of a mipmap requires ES 3.0+, "
              "found %d.%d", mGLVersion / 10, mGLVersion % 10);
        return;
    }
    const int kNumTiles = 8;
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage =
        AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
        AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
        AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP |
        AHARDWAREBUFFER_USAGE_GPU_MIPMAP_COMPLETE;
    // Ensure that the checkerboard tiles have equal size at every level of the mipmap.
    desc.width = std::max(8u, RoundUpToPowerOf2(desc.width));
    desc.height = desc.width;
    desc.layers *= 6;
    if (!SetUpBuffer(desc)) return;

    const int kTextureUnit = 5 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
    }

    const int kTileSize = desc.width / kNumTiles;
    glEnable(GL_SCISSOR_TEST);
    for (int face = 0; face < 6; ++face) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(desc.width, desc.height, desc.layers - 6 + face, kBufferAsTexture));
        for (int i = 0; i < kNumTiles; ++i) {
                for (int j = 0; j < kNumTiles; ++j) {
                const float v = (i & 1) ^ (j & 1) ? 1.f : 0.f;
                glClearColor(v, 0.f, 0.f, v);
                glScissor(i * kTileSize, j * kTileSize, kTileSize, kTileSize);
                glClear(GL_COLOR_BUFFER_BIT);
            }
        }
    }
    glDisable(GL_SCISSOR_TEST);
    glGenerateMipmap(mTexTarget);
    glFinish();

    MakeCurrent(0);
    for (int face = 0; face < 6; ++face) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(1, 1, desc.layers - 6 + face, kBufferAsTexture, kNone, kNone, kNone,
                             MipLevelCount(desc.width, desc.height) - 1));
        std::vector<GoldenPixel> goldens{{0, 0, (desc.stride & kUseSrgb) ? kRed50Srgb : kRed50}};
        CheckGoldenPixels(goldens, desc.format);
    }
}

// The 'stride' field is used to pass a combination of TestFlags.
INSTANTIATE_TEST_CASE_P(
    SingleLayer, ColorTest,
    ::testing::Values(
        AHardwareBuffer_Desc{75, 33, 1, GL_RGB8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{64, 80, 1, GL_RGBA8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{49, 23, 1, GL_SRGB8_ALPHA8, 0, kGlFormat | kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{63, 78, 1, GL_RGB565, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{42, 41, 1, GL_RGBA16F, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{37, 63, 1, GL_RGB10_A2, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{33, 20, 1, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{33, 20, 1, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{20, 10, 1, AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{20, 10, 1, AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{16, 20, 1, AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{16, 20, 1, AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{10, 20, 1, AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{10, 20, 1, AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{10, 20, 1, AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{64, 80, 1, AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420, 0, 0, 0, 0},
        AHardwareBuffer_Desc{64, 80, 1, AHARDWAREBUFFER_FORMAT_Y8Cb8Cr8_420, 0,
                                        kExplicitYuvSampling, 0, 0}),
    &GetTestName);

INSTANTIATE_TEST_CASE_P(
    MultipleLayers, ColorTest,
    ::testing::Values(
        AHardwareBuffer_Desc{75, 33, 5, GL_RGB8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{64, 80, 6, GL_RGBA8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{33, 28, 4, GL_SRGB8_ALPHA8, 0, kGlFormat | kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{42, 41, 3, GL_RGBA16F, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{63, 78, 3, GL_RGB565, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{37, 63, 4, GL_RGB10_A2, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{25, 77, 7, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{25, 77, 7, AHARDWAREBUFFER_FORMAT_R8G8B8A8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{30, 30, 3, AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{30, 30, 3, AHARDWAREBUFFER_FORMAT_R8G8B8X8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{50, 50, 4, AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{50, 50, 4, AHARDWAREBUFFER_FORMAT_R8G8B8_UNORM, 0, kUseSrgb, 0, 0},
        AHardwareBuffer_Desc{20, 10, 2, AHARDWAREBUFFER_FORMAT_R5G6B5_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{20, 20, 4, AHARDWAREBUFFER_FORMAT_R16G16B16A16_FLOAT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{30, 20, 16, AHARDWAREBUFFER_FORMAT_R10G10B10A2_UNORM, 0, 0, 0, 0}),
    &GetTestName);


class DepthTest : public AHardwareBufferGLTest {};

// Verify that depth testing against a depth buffer rendered in another context
// works correctly.
TEST_P(DepthTest, DepthAffectsDrawAcrossContexts) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = 40;
    desc.height = 40;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    // This test does not make sense for layered buffers - don't bother testing them.
    if (desc.layers > 1) return;
    if (!SetUpBuffer(desc)) return;

    // Bind the EGLImage to renderbuffers and framebuffers in both contexts.
    // The depth buffer is shared, but the color buffer is not.
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(40, 40, 0, kRenderbuffer, kBufferAsRenderbuffer));
    }

    // In the second context, clear the depth buffer to a checkerboard pattern.
    DrawCheckerboard(40, 40, desc.format);
    glFinish();

    // In the first context, clear the color buffer only, then draw a red pyramid.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(kVertexShader, kColorFragmentShader, kPyramidPositions, 1.f));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glDrawArrays(GL_TRIANGLES, 0, kPyramidVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check golden pixels.
    std::vector<GoldenPixel> goldens{
        {5, 35, kRed}, {15, 35, kRed},  {25, 35, kZero}, {35, 35, kZero},
        {5, 25, kRed}, {15, 25, kZero}, {25, 25, kZero}, {35, 25, kZero},
        {5, 15, kRed}, {15, 15, kRed},  {25, 15, kRed},  {35, 15, kRed},
        {5, 5,  kRed}, {15, 5,  kRed},  {25, 5,  kRed},  {35, 5,  kRed},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// Verify that depth buffers with usage GPU_SAMPLED_IMAGE can be used as textures.
TEST_P(DepthTest, DepthCanBeSampled) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    if (!SetUpBuffer(desc)) return;

    // Bind the EGLImage to renderbuffers and framebuffers in both contexts.
    // The depth buffer is shared, but the color buffer is not.
    const int kTextureUnit = 3 % mMaxTextureUnits;
    for (int i = 0; i < 2; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    // In the second context, attach the depth texture to the framebuffer and clear to 1.
    ASSERT_NO_FATAL_FAILURE(
        SetUpFramebuffer(desc.width, desc.height, desc.layers - 1, kNone, kBufferAsTexture));
    glClearDepthf(1.f);
    glClear(GL_DEPTH_BUFFER_BIT);
    glFinish();

    // In the first context, draw a quad using the depth texture.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (desc.layers > 1) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(std::string("#version 300 es") + kVertexShaderEs3x, kArrayFragmentShaderEs30,
                         kQuadPositions, 0.5f, kTextureUnit));
    } else {
        std::string vertex_shader = GetTextureVertexShader(desc.format, desc.stride);
        std::string fragment_shader = GetTextureFragmentShader(desc.format, desc.stride);
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(vertex_shader, fragment_shader, kQuadPositions, 0.5f, kTextureUnit));
    }
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    glFinish();
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check the rendered pixels. There should be a square in the middle.
    const GoldenColor kDepth = mGLVersion < 30 ? kWhite : kRed;
    std::vector<GoldenPixel> goldens{
        {5, 35, kZero}, {15, 35, kZero},  {25, 35, kZero},  {35, 35, kZero},
        {5, 25, kZero}, {15, 25, kDepth}, {25, 25, kDepth}, {35, 25, kZero},
        {5, 15, kZero}, {15, 15, kDepth}, {25, 15, kDepth}, {35, 15, kZero},
        {5,  5, kZero}, {15,  5, kZero},  {25, 5,  kZero},  {35, 5,  kZero},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

TEST_P(DepthTest, DepthCubemapSampling) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage =
        AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT |
        AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE |
        AHARDWAREBUFFER_USAGE_GPU_CUBE_MAP;
    desc.height = desc.width;
    desc.layers *= 6;
    if (!SetUpBuffer(desc)) return;

    const int kTextureUnit = 9 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }

    glEnable(GL_SCISSOR_TEST);
    for (int i = 0; i < 6; ++i) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(desc.width, desc.height, desc.layers - 6 + i, kNone, kBufferAsTexture));
        glClearDepthf(0.f);
        glScissor(0, 0, desc.width, desc.height);
        glClear(GL_DEPTH_BUFFER_BIT);
        glClearDepthf(1.f);
        glScissor(0, 0, desc.width / 2, desc.height / 2);
        glClear(GL_DEPTH_BUFFER_BIT);
        glScissor(desc.width / 2, desc.height / 2, desc.width / 2, desc.height / 2);
        glClear(GL_DEPTH_BUFFER_BIT);
    }
    glDisable(GL_SCISSOR_TEST);
    glFinish();
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    MakeCurrent(0);
    if (desc.layers > 6) {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(std::string("#version 320 es") + kVertexShaderEs3x,
                         kCubeMapArrayFragmentShaderEs32, kQuadPositions, 0.5f, kTextureUnit));
    } else {
        ASSERT_NO_FATAL_FAILURE(
            SetUpProgram(kVertexShader, kCubeMapFragmentShader, kQuadPositions, 0.5f, kTextureUnit));
    }
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    const GoldenColor kDepth = mGLVersion < 30 ? kWhite: kRed;
    for (int i = 0; i < 6; ++i) {
        float face_vector[3] = {0.f, 0.f, 0.f};
        face_vector[i / 2] = (i % 2) ? -1.f : 1.f;
        glUniform3fv(mFaceVectorLocation, 1, face_vector);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
        ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

        std::vector<GoldenPixel> goldens{
            {5, 35, kZero}, {15, 35, kZero},  {25, 35, kZero},  {35, 35, kZero},
            {5, 25, kZero}, {15, 25, kBlack}, {25, 25, kDepth}, {35, 25, kZero},
            {5, 15, kZero}, {15, 15, kDepth}, {25, 15, kBlack}, {35, 15, kZero},
            {5, 5,  kZero}, {15, 5,  kZero},  {25, 5,  kZero},  {35, 5,  kZero},
        };
        CheckGoldenPixels(goldens, GL_RGBA8);
    }
}

// The 'stride' field is used to pass a combination of TestFlags.
INSTANTIATE_TEST_CASE_P(
    SingleLayer, DepthTest,
    ::testing::Values(
        AHardwareBuffer_Desc{16, 24, 1, GL_DEPTH_COMPONENT16, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{16, 24, 1, AHARDWAREBUFFER_FORMAT_D16_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{44, 21, 1, AHARDWAREBUFFER_FORMAT_D24_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 1, AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{20, 10, 1, AHARDWAREBUFFER_FORMAT_D32_FLOAT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 1, AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT, 0, 0, 0, 0}),
    &GetTestName);


INSTANTIATE_TEST_CASE_P(
    MultipleLayers, DepthTest,
    ::testing::Values(
        AHardwareBuffer_Desc{16, 24, 6, GL_DEPTH_COMPONENT16, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{16, 24, 6, AHARDWAREBUFFER_FORMAT_D16_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{44, 21, 4, AHARDWAREBUFFER_FORMAT_D24_UNORM, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 7, AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{20, 10, 5, AHARDWAREBUFFER_FORMAT_D32_FLOAT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 3, AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT, 0, 0, 0, 0}),
    &GetTestName);


class StencilTest : public AHardwareBufferGLTest {};

// Verify that stencil testing against a stencil buffer rendered in another context
// works correctly.
TEST_P(StencilTest, StencilAffectsDrawAcrossContexts) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.width = 40;
    desc.height = 40;
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT;
    // This test does not make sense for layered buffers - don't bother testing them.
    if (desc.layers > 1) return;
    if (!SetUpBuffer(desc)) return;

    // Bind the EGLImage to renderbuffers and framebuffers in both contexts.
    // The depth buffer is shared, but the color buffer is not.
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(
            SetUpFramebuffer(40, 40, 0, kRenderbuffer, kNone, kBufferAsRenderbuffer));
    }

    // In the second context, clear the stencil buffer to a checkerboard pattern.
    DrawCheckerboard(40, 40, desc.format);
    glFinish();
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // In the first context, clear the color buffer only, then draw a flat quad.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(kVertexShader, kColorFragmentShader, kQuadPositions, 1.f));
    glClearColor(0.f, 0.f, 0.f, 0.f);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xFF);
    glStencilOp(GL_KEEP, GL_INCR, GL_INCR);
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    glClear(GL_COLOR_BUFFER_BIT);
    glStencilFunc(GL_EQUAL, 2, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    glUniform4f(mColorLocation, 0.f, 1.f, 0.f, 1.f);
    glStencilFunc(GL_EQUAL, 4, 0xFF);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check golden pixels.
    std::vector<GoldenPixel> goldens{
        {5, 35, kRed},  {15, 35, kRed},  {25, 35, kZero},  {35, 35, kZero},
        {5, 25, kRed},  {15, 25, kRed},  {25, 25, kZero},  {35, 25, kZero},
        {5, 15, kZero}, {15, 15, kZero}, {25, 15, kGreen}, {35, 15, kGreen},
        {5, 5,  kZero}, {15, 5,  kZero}, {25, 5,  kGreen}, {35, 5,  kGreen},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// Verify that stencil testing against a stencil buffer rendered in another context
// works correctly.
TEST_P(StencilTest, StencilTexture) {
    AHardwareBuffer_Desc desc = GetParam();
    desc.usage = AHARDWAREBUFFER_USAGE_GPU_COLOR_OUTPUT | AHARDWAREBUFFER_USAGE_GPU_SAMPLED_IMAGE;
    const bool kPureStencil =
        desc.format == GL_STENCIL_INDEX8 || desc.format == AHARDWAREBUFFER_FORMAT_S8_UINT;
    // Pure stencil textures are only supported with an extension. Note: we don't exit for the
    // AHB format here, because we want to ensure that buffer allocation fails with the
    // GPU_SAMPLED_IMAGE usage flag if the implementation doesn't support pure stencil textures.
    if (desc.format == GL_STENCIL_INDEX8 && !HasGLExtension("GL_OES_texture_stencil8")) return;
    // Stencil sampling from depth-stencil textures was introduced in ES 3.1.
    if (!kPureStencil && mGLVersion < 31) return;
    if (!SetUpBuffer(desc)) return;

    const int kTextureUnit = 8 % mMaxTextureUnits;
    for (int i = 0; i < mContextCount; ++i) {
        MakeCurrent(i);
        ASSERT_NO_FATAL_FAILURE(SetUpTexture(desc, kTextureUnit));
        glTexParameteri(mTexTarget, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(mTexTarget, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (!kPureStencil) {
            glTexParameteri(mTexTarget, GL_DEPTH_STENCIL_TEXTURE_MODE, GL_STENCIL_INDEX);
        }
    }

    // In the second context, clear the stencil buffer to a checkerboard pattern.
    ASSERT_NO_FATAL_FAILURE(
        SetUpFramebuffer(desc.width, desc.height, desc.layers - 1,
                         kNone, kNone, kBufferAsTexture));
    DrawCheckerboard(desc.width, desc.height, desc.format);
    glFinish();
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // In the first context, reconstruct the checkerboard with a special shader.
    MakeCurrent(0);
    ASSERT_NO_FATAL_FAILURE(
        SetUpProgram(std::string("#version 300 es") + kVertexShaderEs3x,
                     desc.layers > 1 ? kStencilArrayFragmentShaderEs30 : kStencilFragmentShaderEs30,
                     kQuadPositions, 1.f, kTextureUnit));
    ASSERT_NO_FATAL_FAILURE(SetUpFramebuffer(40, 40, 0, kRenderbuffer));
    glDrawArrays(GL_TRIANGLES, 0, kQuadVertexCount);
    ASSERT_EQ(GLenum{GL_NO_ERROR}, glGetError());

    // Check golden pixels.
    std::vector<GoldenPixel> goldens{
        {5, 35, kRed},  {15, 35, kRed},  {25, 35, kBlue},  {35, 35, kBlue},
        {5, 25, kRed},  {15, 25, kRed},  {25, 25, kBlue},  {35, 25, kBlue},
        {5, 15, kZero}, {15, 15, kZero}, {25, 15, kGreen}, {35, 15, kGreen},
        {5, 5,  kZero}, {15, 5,  kZero}, {25, 5,  kGreen}, {35, 5,  kGreen},
    };
    CheckGoldenPixels(goldens, GL_RGBA8);
}

// The 'stride' field is used to pass a combination of TestFlags.
INSTANTIATE_TEST_CASE_P(
    SingleLayer, StencilTest,
    ::testing::Values(
        AHardwareBuffer_Desc{49, 57, 1, GL_STENCIL_INDEX8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{36, 50, 1, GL_DEPTH24_STENCIL8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{26, 29, 1, AHARDWAREBUFFER_FORMAT_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 1, AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{17, 23, 1, AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT, 0, 0, 0, 0}),
    &GetTestName);

INSTANTIATE_TEST_CASE_P(
    MultipleLayers, StencilTest,
    ::testing::Values(
        AHardwareBuffer_Desc{49, 57, 3, GL_STENCIL_INDEX8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{36, 50, 6, GL_DEPTH24_STENCIL8, 0, kGlFormat, 0, 0},
        AHardwareBuffer_Desc{26, 29, 5, AHARDWAREBUFFER_FORMAT_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{57, 33, 4, AHARDWAREBUFFER_FORMAT_D24_UNORM_S8_UINT, 0, 0, 0, 0},
        AHardwareBuffer_Desc{17, 23, 7, AHARDWAREBUFFER_FORMAT_D32_FLOAT_S8_UINT, 0, 0, 0, 0}),
    &GetTestName);

}  // namespace android
