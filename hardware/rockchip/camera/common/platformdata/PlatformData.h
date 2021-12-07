/*
 * Copyright (C) 2013-2017 Intel Corporation
 * Copyright (c) 2017, Fuzhou Rockchip Electronics Co., Ltd
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

#ifndef _CAMERA3_HAL_PLATFORMDATA_H_
#define _CAMERA3_HAL_PLATFORMDATA_H_

#include <expat.h>
#include <linux/media.h>
#include <string>
#include <vector>
#include "Camera3V4l2Format.h"
#include "CameraWindow.h"
#ifdef CAMERA_RKISP2_SUPPORT
#include "RKISP2GraphConfigManager.h"
#else
#include "GraphConfigManager.h"
#endif
#include "Metadata.h"

#define DEFAULT_ENTRY_CAP 256
#define DEFAULT_DATA_CAP 2048

#define ENTRY_RESERVED 16
#define DATA_RESERVED 128

#define METERING_RECT_SIZE 5

/**
 * Platform capability: max num of in-flight requests
 * Limited by streams buffers number
 */
#define MAX_REQUEST_IN_PROCESS_NUM 10

/**
 * Fake HAL pixel format that we define to use it as index in the table
 * that maps the Gfx HAL pixel formats to concrete V4L2 formats.
 * The original one is HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, which is the
 * pixel format that goes to the display or Gfx.
 *
 * This one is the pixel format that is implementation defined but it goes
 * to the Video HW
 */
#define HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED_VIDEO   0x7fff5001

/**
 * Maximum number of CPF files cached by the HAL library.
 * On loading the HAL library we will detect all cameras in the system and try
 * to load the CPF files.
 * This define control the maximum number of cameras that we can keep the their
 * CPF loaded in memory.
 * This should  be always higher than the maximum number of cameras in the system
 *
 */
#define MAX_CPF_CACHED  16

/* Maximum number of subdev to lookup */
#define MAX_SUBDEV_ENUMERATE    256

/* These should be read from the platform configure file */
#define MAX_CAMERAS 2
#define BACK_CAMERA_ID 0
#define FRONT_CAMERA_ID 1

#define RESOLUTION_14MP_WIDTH   4352
#define RESOLUTION_14MP_HEIGHT  3264
#define RESOLUTION_8MP_WIDTH    3264
#define RESOLUTION_8MP_HEIGHT   2448
#define RESOLUTION_UHD_WIDTH    3840
#define RESOLUTION_UHD_HEIGHT   2160
#define RESOLUTION_5MP_WIDTH    2560
#define RESOLUTION_5MP_HEIGHT   1920
#define RESOLUTION_1_3MP_WIDTH    1280
#define RESOLUTION_1_3MP_HEIGHT   960
#define RESOLUTION_1080P_WIDTH  1920
#define RESOLUTION_1080P_HEIGHT 1080
#define RESOLUTION_720P_WIDTH   1280
#define RESOLUTION_720P_HEIGHT  720
#define RESOLUTION_480P_WIDTH   768
#define RESOLUTION_480P_HEIGHT  480
#define RESOLUTION_VGA_WIDTH   640
#define RESOLUTION_VGA_HEIGHT  480
#define RESOLUTION_POSTVIEW_WIDTH    320
#define RESOLUTION_POSTVIEW_HEIGHT   240

#define ALIGNED_128 128
#define ALIGNED_64  64

#define MAX_LSC_GRID_WIDTH  64
#define MAX_LSC_GRID_HEIGHT  64
#define MAX_LSC_GRID_SIZE (MAX_LSC_GRID_WIDTH * MAX_LSC_GRID_HEIGHT)

NAMESPACE_DECLARATION {
typedef enum {
    SUPPORTED_HW_RKISP1,
    SUPPORTED_HW_RKISP2,
    SUPPORTED_HW_UNKNOWN
} CameraHwType;

enum SensorType {
    SENSOR_TYPE_NONE = 0,
    SENSOR_TYPE_RAW,           // Raw sensor
    SENSOR_TYPE_SOC            // SOC sensor
};

enum SensorFlip {
    SENSOR_FLIP_NA     = -1,   // Support Not-Available
    SENSOR_FLIP_OFF    = 0x00, // Both flip ctrls set to 0
    SENSOR_FLIP_H      = 0x01, // V4L2_CID_HFLIP 1
    SENSOR_FLIP_V      = 0x02, // V4L2_CID_VFLIP 1
};

class CameraProfiles;

enum ISP_PORT{
    PRIMARY = 0,
    SECONDARY,
    TERTIARY,
    UNKNOWN_PORT,
};

enum SensorDeviceType {
    SENSOR_DEVICE_MAIN, // Main device sensor
    SENSOR_DEVICE_MC    // Media controller sensor
};

#define SENSOR_ATTACHED_FLASH_MAX_NUM 2
struct SensorDriverDescriptor {
    /* sensor entity name format:
     * m01_b_ov13850 1-0010, where 'm01' means
     * module index number, 'b' means
     * back or front, 'ov13850' is real
     * sensor name, '1-0010' means the i2c bus
     * and sensor i2c slave address
     */
    std::string mSensorName;
    std::string mDeviceName;
    std::string mI2CAddress;
    std::string mParentMediaDev;
    enum ISP_PORT mIspPort;
    enum SensorDeviceType mSensorDevType;
    int csiPort;
    std::string mModuleLensDevName; // matched using mPhyModuleIndex
    int mFlashNum;
    std::string mModuleFlashDevName[SENSOR_ATTACHED_FLASH_MAX_NUM]; // matched using mPhyModuleIndex
    std::string mModuleRealSensorName; //parsed frome sensor entity name
    std::string mModuleIndexStr; // parsed from sensor entity name
    char mPhyModuleOrient; // parsed from sensor entity name
};

struct SensorFrameSize {
    uint32_t min_width;
    uint32_t min_height;
    uint32_t max_width;
    uint32_t max_height;
};
typedef std::map<uint32_t, std::vector<struct SensorFrameSize>> SensorFormat;

enum ExtensionGroups {
    CAPABILITY_NONE = 0,
    CAPABILITY_CV = 1 << 0,
    CAPABILITY_STATISTICS = 1 << 1,
    CAPABILITY_ENHANCEMENT = 1 << 2,
    CAPABILITY_DEVICE = 1 << 3,
};

struct FrameSize_t {
    uint32_t width;
    uint32_t height;
};

/**
 * \class CameraHWInfo
 *
 * this class is the one that stores the information
 * that comes from the common section in the XML
 * and that keeps the run-time generated list of sensor drivers registered.
 *
 */
typedef std::vector<std::pair<uint32_t, std::string>> SensorModeVector;

class CameraHWInfo {
public:
    CameraHWInfo(); // TODO: proper constructor with member variable initializations
    ~CameraHWInfo() {};
    status_t init(const std::vector<std::string> &mediaDevicePath);

    const char* boardName(void) const { return mBoardName.c_str(); }
    const char* productName(void) const { return mProductName.c_str(); }
    const char* manufacturerName(void) const { return mManufacturerName.c_str(); }
    bool supportDualVideo(void) const { return mSupportDualVideo; }
    int getCameraDeviceAPIVersion(void) const { return mCameraDeviceAPIVersion; }
    bool supportExtendedMakernote(void) const { return mSupportExtendedMakernote; }
    bool supportFullColorRange(void) const { return mSupportFullColorRange; }
    bool supportIPUAcceleration(void) const { return mSupportIPUAcceleration; }
    status_t getAvailableSensorModes(const std::string &sensorName,
                                     SensorModeVector &sensorModes) const;
    status_t getSensorEntityName(int32_t cameraId,
                                 std::string &sensorEntityName) const;
    status_t getAvailableSensorOutputFormats(int32_t cameraId,
                                     SensorFormat &OutputFormats) const;
    status_t getSensorBayerPattern(int32_t cameraId,
                                   int32_t &bayerPattern) const;
    status_t getSensorFrameDuration(int32_t cameraId, int32_t &duration) const;
    status_t getDvTimings(int32_t cameraId, struct v4l2_dv_timings &timings) const;
    void getMediaCtlElementNames(std::vector<std::string> &elementNames, bool isFirst = false) const;
    bool isIspSupportRawPath() const;
    std::string getFullMediaCtlElementName(const std::vector<std::string> elementNames,
                                           const char *value) const;
    const struct SensorDriverDescriptor* getSensorDrvDes(int32_t cameraId) const;

    std::string mProductName;
    std::string mManufacturerName;
    std::string mBoardName;
    std::vector<std::string> mMediaControllerPathName;
    std::vector<std::string> mMediaCtlElementNames;
    std::string mMainDevicePathName;
    int mPreviewHALFormat;  // specify the preview format for multi configured streams
    int mCameraDeviceAPIVersion;
    bool mSupportDualVideo;
    bool mSupportExtendedMakernote;
    bool mSupportIPUAcceleration;
    bool mSupportFullColorRange;
    bool mHasMediaController; // TODO: REMOVE. WA to overcome BXT MC-related issue with camera ID <-> ISP port
    media_device_info mDeviceInfo;
    std::vector<struct SensorDriverDescriptor> mSensorInfo;
private:
    // the below functions are used to init the mSensorInfo
    status_t initDriverList();
    status_t readProperty();
    status_t findMediaControllerSensors(const std::string &mcPath);
    status_t findMediaDeviceInfo(const std::string &mcPath);
    status_t initDriverListHelper(unsigned major, unsigned minor, const std::string &mcPath, SensorDriverDescriptor &drvInfo);
    status_t getCSIPortID(const std::string &deviceName, const std::string &mcPath, int &portId);
    // parse module info from sensor entity name
    status_t parseModuleInfo(const std::string &entity_name, SensorDriverDescriptor &drv_info);
    // get VCM/FLASH etc. attached to the camera module
    status_t findAttachedSubdevs(const std::string &mcPath, struct SensorDriverDescriptor &drv_info);
};

/**
 * \class CameraCapInfo
 *
 * Base class for all PSL specific CameraCapInfo.
 * The PlatformData::getCameraCapInfo shall return a value of this type
 *
 * This class is used to retrieve the information stored in the XML sections
 * that are per sensor.
 *
 * The methods defined here are to retrieve common information across all
 * PSL's. They are stored in the XML section: HAL_TUNNING
 *
 * Each PSL subclass will  implement extra methods to expose the PSL specific
 * fields
 *
 */
class CameraCapInfo {
public:
    CameraCapInfo() : mSensorType(SENSOR_TYPE_NONE), mGCMNodes(nullptr) {};
    virtual ~CameraCapInfo() {};
    virtual int sensorType(void) const = 0;
	virtual bool getForceAutoGenAndroidMetas(void) const = 0;
    virtual const std::string& getIqTuningFile(void) const = 0;
#ifdef CAMERA_RKISP2_SUPPORT
    const rkisp2::GraphConfigNodes* getGraphConfigNodes() const { return mGCMNodes; }
#else
    const GraphConfigNodes* getGraphConfigNodes() const { return mGCMNodes; }
#endif
    virtual void setSupportTuningSizes(std::vector<struct FrameSize_t> frameSize) = 0; 

protected:
    friend class IPSLConfParser;
    /* Common fields for all PSL's stored in the XML section HAL_tuning */
    SensorType mSensorType;    /*!> Whether is RAW or SOC */
    /**
     *  Table to map the Gfx HAL pixel formats to V4L2 pixel formats
     *  We need this because there are certain Gfx-HAL Pixel formats that do not
     *  concretely define a pixel layout. We use this table to resolve this
     *  ambiguity
     *  The current GfxHAL pixel formats that are not concrete enough are:
     *  HAL_PIXEL_FORMAT_RAW16
     *  HAL_PIXEL_FORMAT_RAW_OPAQUE
     *  HAL_PIXEL_FORMAT_BLOB
     *  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED
     *  HAL_PIXEL_FORMAT_YCbCr_420_888
     *
     *  Also the implementation defined format may differ depending if
     *  it goes to Gfx of to video encoder. So one fake format are defined
     *  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED_VIDEO to differentiate
     *  between both of them
     */
    std::map<int, int> mGfxHalToV4L2PixelFmtTable;
#ifdef CAMERA_RKISP2_SUPPORT
    rkisp2::GraphConfigNodes* mGCMNodes;
#else
    GraphConfigNodes* mGCMNodes;
#endif
};

class GcssKeyMap {
public:
    GcssKeyMap();
    ~GcssKeyMap();
    void gcssKeyMapInsert(std::map<std::string, ia_uid>& customMap);
    int gcssKeyMapSize();
    const char* key2str(const ia_uid key);
    ia_uid str2key(const std::string& key_str);

private:
    std::map<std::string, ia_uid> mMap;
};

class PlatformData {
public:
    static void init();     // called when HAL is loaded
    static void deinit();   // called when HAL is unloaded
private:
    static bool mInitialized;

    static CameraProfiles* mInstance;
    static CameraProfiles* getInstance(void);
    static CameraHWInfo* mCameraHWInfo;
    static GcssKeyMap* mGcssKeyMap;

public:

    static bool isInitialized() { return mInitialized; }


    static GcssKeyMap* getGcssKeyMap();

    static int numberOfCameras(void);
    static void getCameraInfo(int cameraId, struct camera_info* info);
    static camera_metadata_t* getStaticMetadata(int cameraId);
    static camera_metadata_t* getDefaultMetadata(int cameraId, int requestType);
    static CameraHwType getCameraHwType(int cameraId);
    static const CameraCapInfo* getCameraCapInfo(int cameraId);
    static const CameraHWInfo* getCameraHWInfo() { return mCameraHWInfo; }
    static int getXmlCameraId(int cameraId);
    static const CameraCapInfo* getCameraCapInfoForXmlCameraId(int xmlCameraId);
    static status_t getDeviceIds(std::vector<std::string>& names);

    static const char* boardName(void);
    static const char* productName(void);
    static const char* manufacturerName(void);
    static bool supportDualVideo(void);
    static int getCameraDeviceAPIVersion(void);
    static bool supportExtendedMakernote(void);
    static bool supportIPUAcceleration(void);
    static bool supportFullColorRange(void);
    /**
     * get the number of CPU cores
     * \return the number of CPU cores
     */
    static unsigned int getNumOfCPUCores();
    static int facing(int cameraId);
    static int orientation(int cameraId);
    static float getStepEv(int cameraId);
    static int getPartialMetadataCount(int cameraId);
    static CameraWindow getActivePixelArray(int cameraId);

};
} NAMESPACE_DECLARATION_END

#endif // _CAMERA3_HAL_PLATFORMDATA_H_
