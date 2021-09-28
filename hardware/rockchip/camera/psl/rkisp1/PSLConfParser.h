/*
 * Copyright (C) 2014-2017 Intel Corporation
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

#ifndef _CAMERA3_RKISP1CONFPARSER_H_
#define _CAMERA3_RKISP1CONFPARSER_H_

#include <vector>
#include <string>
#include "PlatformData.h"
#include "IPSLConfParser.h"
#include "RKISP1CameraCapInfo.h"
#include "MediaCtlPipeConfig.h"

#define VIDEO_DEV_NAME "Unimplemented"
#define ANDROID_CONTROL_CAPTURE_INTENT_START 0x40000000
#define CAMERA_TEMPLATE_COUNT (ANDROID_CONTROL_CAPTURE_INTENT_MANUAL+1)

namespace android {
namespace camera2 {

class PSLConfParser : public IPSLConfParser {
public:
    static IPSLConfParser *getInstance(std::string &xmlConfigName, const std::vector<SensorDriverDescriptor>& sensorNames);
    static void deleteInstance();
    static std::string getSensorMediaDevice(int cameraId);
    static std::string getImguMediaDevice(int cameraId);

    virtual CameraCapInfo *getCameraCapInfo(int cameraId);
    virtual camera_metadata_t *constructDefaultMetadata(int cameraId, int reqTemplate);

    // Deprecated: not valid from commit5977220afeb6bdc8bc674f836f193c1c4e37ac3f
    static const char *getSensorMediaDeviceName(int cameraId) {
        const RKISP1CameraCapInfo *cap = getRKISP1CameraCapInfo(cameraId);
        std::string entityName;
        entityName = cap->getMediaCtlEntityName("isys_backend");
        if (entityName.find("isp") != std::string::npos)
            return "rkisp1";
        else
            return "rkcif";
    }
    // Deprecated: not valid from commit5977220afeb6bdc8bc674f836f193c1c4e37ac3f
    static const char *getImguEntityMediaDevice(int cameraId) {
        const RKISP1CameraCapInfo *cap = getRKISP1CameraCapInfo(cameraId);
        std::string entityName;
        entityName = cap->getMediaCtlEntityName("isys_backend");
        if (entityName.find("isp") != std::string::npos)
            return "rkisp1";
        else
            return "rkcif";
    }

    static std::vector<std::string> getSensorMediaDevicePath();
    static std::vector<std::string> getMediaDeviceByName(std::string deviceName);

// disable copy constructor and assignment operator
private:
    PSLConfParser(PSLConfParser const&);
    PSLConfParser& operator=(PSLConfParser const&);

private:
    static IPSLConfParser *sInstance; // the singleton instance

    PSLConfParser(std::string& xmlName, const std::vector<SensorDriverDescriptor>& sensorNames);
    virtual ~PSLConfParser();

    static const int mBufSize = 1*1024;  // For xml file

    static std::vector<std::string> mImguMediaDevice;
    static std::vector<std::string> mSensorMediaDevice;

    enum DataField {
        FIELD_INVALID = 0,
        FIELD_HAL_TUNING_RKISP1,
        FIELD_SENSOR_INFO_RKISP1,
        FIELD_MEDIACTL_ELEMENTS_RKISP1,
        FIELD_MEDIACTL_CONFIG_RKISP1
    } mCurrentDataField;
    unsigned mSensorIndex;
    MediaCtlConfig mMediaCtlCamConfig;     // one-selected camera pipe config.
    bool mUseProfile;   /**< internal variable to disable parsing of profiles of
                             sensor not found at run time */

    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);
    void checkField(const char *name, const char **atts);
    void getDataFromXmlFile(void);
    bool isSensorPresent(const std::string &sensorName, const char* moduleId);
    status_t addCamera(int cameraId, const std::string &sensorName, const char* moduleIdStr);
    void handleHALTuning(const char *name, const char **atts);
    void handleSensorInfo(const char *name, const char **atts);
    void handleMediaCtlElements(const char *name, const char **atts);
    void getPSLDataFromXmlFile(void);
    int convertXmlData(void * dest, int destMaxNum, const char * src, int type);
    void checkRequestMetadata(const camera_metadata *request, int cameraId);
    uint8_t selectAfMode(const camera_metadata_t *staticMeta, int reqTemplate);
    uint8_t selectEdgeMode(const camera_metadata_t *staticMeta, int reqTemplate);
    uint8_t selectNrMode(const camera_metadata_t *staticMeta, int reqTemplate);
    uint8_t selectAeAntibandingMode(const camera_metadata_t *staticMeta, int reqTemplate);

    int getStreamFormatAsValue(const char* format);
    int getSelectionTargetAsValue(const char* target);
    int getControlIdAsValue(const char* format);
    int getIsysNodeNameAsValue(const char* isysNodeName);

    int readNvmData();
    void dumpHalTuningSection(int cameraId);
    void dumpSensorInfoSection(int cameraId);
    void dumpMediaCtlElementsSection(int cameraId);
    void dump(void);

    std::vector<std::string> mElementNames;
};

} // namespace camera2
} // namespace android
#endif
