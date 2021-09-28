#include <iostream>
#include <string>
#include <vector>

#include "fake-pipeline2/Base.h"
#include "fake-pipeline2/Scene.h"
#include "QemuClient.h"
#include <gralloc_cb_bp.h>

#include <ui/GraphicBufferAllocator.h>
#include <ui/GraphicBufferMapper.h>
#include <ui/Rect.h>

#include <linux/videodev2.h>
#include <utils/Timers.h>

using namespace android;


const nsecs_t kExposureTimeRange[2] =
    {1000L, 300000000L} ; // 1 us - 0.3 sec
const nsecs_t kFrameDurationRange[2] =
    {33331760L, 300000000L}; // ~1/30 s - 0.3 sec

const nsecs_t kMinVerticalBlank = 10000L;

const uint8_t kColorFilterArrangement =
    ANDROID_SENSOR_INFO_COLOR_FILTER_ARRANGEMENT_RGGB;

// Output image data characteristics
const uint32_t kMaxRawValue = 4000;
const uint32_t kBlackLevel  = 1000;

// Sensor sensitivity
const float kSaturationVoltage      = 0.520f;
const uint32_t kSaturationElectrons = 2000;
const float kVoltsPerLuxSecond      = 0.100f;

const float kElectronsPerLuxSecond =
        kSaturationElectrons / kSaturationVoltage
        * kVoltsPerLuxSecond;

const float kBaseGainFactor = (float)kMaxRawValue /
            kSaturationElectrons;

const float kReadNoiseStddevBeforeGain = 1.177; // in electrons
const float kReadNoiseStddevAfterGain =  2.100; // in digital counts
const float kReadNoiseVarBeforeGain =
            kReadNoiseStddevBeforeGain *
            kReadNoiseStddevBeforeGain;
const float kReadNoiseVarAfterGain =
            kReadNoiseStddevAfterGain *
            kReadNoiseStddevAfterGain;

const int32_t kSensitivityRange[2] = {100, 1600};
const uint32_t kDefaultSensitivity = 100;

void captureRGBA(uint8_t *img, uint32_t gain, uint32_t width, uint32_t height, Scene& scene, uint32_t sWidth, uint32_t sHeight) {
    float totalGain = gain/100.0 * kBaseGainFactor;
    // In fixed-point math, calculate total scaling from electrons to 8bpp
    int scale64x = 64 * totalGain * 255 / kMaxRawValue;
    unsigned int DivH= (float)sHeight/height * (0x1 << 10);
    unsigned int DivW = (float)sWidth/width * (0x1 << 10);

    for (unsigned int outY = 0; outY < height; outY++) {
        unsigned int y = outY * DivH >> 10;
        uint8_t *px = img + outY * width * 4;
        scene.setReadoutPixel(0, y);
        unsigned int lastX = 0;
        const uint32_t *pixel = scene.getPixelElectrons();
        for (unsigned int outX = 0; outX < width; outX++) {
            uint32_t rCount, gCount, bCount;
            unsigned int x = outX * DivW >> 10;
            if (x - lastX > 0) {
                for (unsigned int k = 0; k < (x-lastX); k++) {
                     pixel = scene.getPixelElectrons();
                }
            }
            lastX = x;
            // TODO: Perfect demosaicing is a cheat
            rCount = pixel[Scene::R]  * scale64x;
            gCount = pixel[Scene::Gr] * scale64x;
            bCount = pixel[Scene::B]  * scale64x;

            *px++ = rCount < 255*64 ? rCount / 64 : 255;
            *px++ = gCount < 255*64 ? gCount / 64 : 255;
            *px++ = bCount < 255*64 ? bCount / 64 : 255;
            *px++ = 255;
         }
        // TODO: Handle this better
        //simulatedTime += mRowReadoutTime;
    }
}

void captureYU12(uint8_t *img, uint32_t gain, uint32_t width, uint32_t height, Scene& scene, uint32_t sWidth, uint32_t sHeight) {
    float totalGain = gain/100.0 * kBaseGainFactor;
    // Using fixed-point math with 6 bits of fractional precision.
    // In fixed-point math, calculate total scaling from electrons to 8bpp
    const int scale64x = 64 * totalGain * 255 / kMaxRawValue;
    // In fixed-point math, saturation point of sensor after gain
    const int saturationPoint = 64 * 255;
    // Fixed-point coefficients for RGB-YUV transform
    // Based on JFIF RGB->YUV transform.
    // Cb/Cr offset scaled by 64x twice since they're applied post-multiply
    float rgbToY[]  = {19.0, 37.0, 7.0, 0.0};
    float rgbToCb[] = {-10.0,-21.0, 32.0, 524288.0};
    float rgbToCr[] = {32.0,-26.0, -5.0, 524288.0};
    // Scale back to 8bpp non-fixed-point
    const int scaleOut = 64;
    const int scaleOutSq = scaleOut * scaleOut; // after multiplies
    const double invscaleOutSq = 1.0/scaleOutSq;
    for (int i=0; i < 4; ++i) {
        rgbToY[i] *= invscaleOutSq;
        rgbToCb[i] *= invscaleOutSq;
        rgbToCr[i] *= invscaleOutSq;
    }

    unsigned int DivH= (float)sHeight/height * (0x1 << 10);
    unsigned int DivW = (float)sWidth/width * (0x1 << 10);
    for (unsigned int outY = 0; outY < height; outY++) {
        unsigned int y = outY * DivH >> 10;
        uint8_t *pxY = img + outY * width;
        uint8_t *pxVU = img + (height + outY / 2) * width;
        uint8_t *pxU = img + height * width + (outY / 2) * (width / 2);
        uint8_t *pxV = pxU + (height / 2) * (width / 2);
        scene.setReadoutPixel(0, y);
        unsigned int lastX = 0;
        const uint32_t *pixel = scene.getPixelElectrons();
         for (unsigned int outX = 0; outX < width; outX++) {
            int32_t rCount, gCount, bCount;
            unsigned int x = outX * DivW >> 10;
            if (x - lastX > 0) {
                for (unsigned int k = 0; k < (x-lastX); k++) {
                     pixel = scene.getPixelElectrons();
                }
            }
            lastX = x;
            rCount = pixel[Scene::R]  * scale64x;
            rCount = rCount < saturationPoint ? rCount : saturationPoint;
            gCount = pixel[Scene::Gr] * scale64x;
            gCount = gCount < saturationPoint ? gCount : saturationPoint;
            bCount = pixel[Scene::B]  * scale64x;
            bCount = bCount < saturationPoint ? bCount : saturationPoint;
            *pxY++ = (rgbToY[0] * rCount + rgbToY[1] * gCount + rgbToY[2] * bCount);
            if (outY % 2 == 0 && outX % 2 == 0) {
                *pxV++ = (rgbToCr[0] * rCount + rgbToCr[1] * gCount + rgbToCr[2] * bCount + rgbToCr[3]);
                *pxU++ = (rgbToCb[0] * rCount + rgbToCb[1] * gCount + rgbToCb[2] * bCount + rgbToCb[3]);
            }
        }
    }
}

// Test the capture speed of qemu camera, e.g., webcam and virtual scene
int main(int argc, char* argv[]) {
    using ::android::GraphicBufferAllocator;
    using ::android::GraphicBufferMapper;


    uint32_t pixFmt;
    int uiFmt;
    bool v1 = false;
    bool fake = false;
    std::vector<nsecs_t> report;
    uint32_t sceneWidth;
    uint32_t sceneHeight;

    if (!strncmp(argv[1], "RGB", 3)) {
        pixFmt = V4L2_PIX_FMT_RGB32;
        uiFmt = HAL_PIXEL_FORMAT_RGBA_8888;
    } else if (!strncmp(argv[1], "NV21", 3)) {
        pixFmt = V4L2_PIX_FMT_NV21;
        uiFmt = HAL_PIXEL_FORMAT_YCbCr_420_888;
    } else if (!strncmp(argv[1], "YV12", 3)) {
        pixFmt = V4L2_PIX_FMT_YVU420;
        uiFmt = HAL_PIXEL_FORMAT_YCbCr_420_888;
    } else if (!strncmp(argv[1], "YU12", 3)) {
        pixFmt = V4L2_PIX_FMT_YUV420;
        uiFmt = HAL_PIXEL_FORMAT_YCbCr_420_888;
    } else {
        printf("format error, use RGB, NV21, YV12 or YU12");
        return -1;
    }
    uint32_t width = atoi(argv[2]);
    uint32_t height = atoi(argv[3]);
    uint32_t repeated = atoi(argv[4]);
    std::string deviceName;
    if (!strncmp(argv[5], "web", 3)) {
        deviceName = "name=/dev/video0";
    } else if (!strncmp(argv[5], "vir", 3)) {
        deviceName = "name=virtualscene";
    } else if (!strncmp(argv[5], "fak", 3)){
        fake = true;
        sceneWidth = atoi(argv[6]);
        sceneHeight = atoi(argv[7]);
    } else {
        printf("device error, use web or virtual");
        return -1;
    }

    if (fake) {
        std::vector<uint8_t> buf(width * height * 4);
        Scene scene(width, height, kElectronsPerLuxSecond);
        for (int i = 0 ; i < repeated; i++) {
            nsecs_t start = systemTime();
            if (pixFmt == V4L2_PIX_FMT_RGB32) {
                captureRGBA(buf.data(), 0, width, height, scene, sceneWidth, sceneHeight);
            } else {
                captureYU12(buf.data(), 0, width, height, scene, sceneWidth, sceneHeight);
            }
            nsecs_t end = systemTime();
            report.push_back(end - start);
        }
    }
    else {
        if (argc > 6 && !strncmp(argv[6], "v1", 2)) {
            v1 = true;
        }
        // Open qemu pipe
        CameraQemuClient client;
        int ret = client.connectClient(deviceName.c_str());
        if (ret != NO_ERROR) {
            printf("Failed to connect device\n");
            return -1;
        }
        ret = client.queryConnect();
        if (ret == NO_ERROR) {
            printf("Connected to device\n");
        } else {
            printf("Failed to connect device\n");
            return -1;
        }
        // Caputre ASAP
        if (v1) {
            //ret = client.queryStart();
            ret = client.queryStart(pixFmt, width, height);
        } else {
            ret = client.queryStart(pixFmt, width, height);
        }
        if (ret != NO_ERROR) {
            printf("Failed to configure device for query\n");
            return -1;
        }
        if (v1) {
            const uint64_t usage =
                GRALLOC_USAGE_HW_CAMERA_READ |
                GRALLOC_USAGE_HW_CAMERA_WRITE |
                GRALLOC_USAGE_HW_TEXTURE;
            uint32_t stride;

            buffer_handle_t handle;
            if (GraphicBufferAllocator::get().allocate(
                    width, height, uiFmt, 1, usage,
                    &handle, &stride,
                    0, "EmulatorCameraTest") != ::android::OK) {
                printf("GraphicBufferAllocator::allocate failed\n");
                return -1;
            }

            void* addr;
            if (uiFmt == HAL_PIXEL_FORMAT_RGBA_8888) {
                GraphicBufferMapper::get().lock(
                    handle,
                    GRALLOC_USAGE_HW_CAMERA_WRITE,
                    Rect(0, 0, width, height),
                    &addr);
            } else {
                android_ycbcr ycbcr;
                GraphicBufferMapper::get().lockYCbCr(
                    handle,
                    GRALLOC_USAGE_HW_CAMERA_WRITE,
                    Rect(0, 0, width, height),
                    &ycbcr);
                addr = ycbcr.y;
            }

            const cb_handle_t* cbHandle = cb_handle_t::from(handle);

            uint64_t offset = cbHandle->getMmapedOffset();
            printf("offset is 0x%llx\n", offset);
            float whiteBalance[] = {1.0f, 1.0f, 1.0f};
            float exposureCompensation = 1.0f;
            for (int i = 0 ; i < repeated; i++) {
                nsecs_t start = systemTime();
                client.queryFrame(width, height, pixFmt, offset,
                                  whiteBalance[0], whiteBalance[1], whiteBalance[2],
                                  exposureCompensation, nullptr);
                nsecs_t end = systemTime();
                report.push_back(end - start);
            }
            GraphicBufferMapper::get().unlock(handle);
            GraphicBufferAllocator::get().free(handle);
        } else {
            size_t bufferSize;
            if (pixFmt == V4L2_PIX_FMT_RGB32) {
                bufferSize = width * height * 4;
            } else {
                bufferSize = width * height * 12 / 8;
            }
            std::vector<char> buffer(bufferSize, 0);
            float whiteBalance[] = {1.0f, 1.0f, 1.0f};
            float exposureCompensation = 1.0f;
            for (int i = 0 ; i < repeated; i++) {
                nsecs_t start = systemTime();
                client.queryFrame(buffer.data(), nullptr, 0, bufferSize,
                                  whiteBalance[0], whiteBalance[1], whiteBalance[2],
                                  exposureCompensation, nullptr);
                nsecs_t end = systemTime();
                report.push_back(end - start);
            }
        }
    }
    // Report
    nsecs_t average, sum = 0;
    for (int i = 0; i < repeated; i++) {
        sum += report[i];
    }
    average = sum / repeated;
    printf("Report for reading %d frames\n", repeated);
    printf("\ttime total: %lld\n", sum);
    printf("\tframe average: %lld\n", average);

    return 0;
}

