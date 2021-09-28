/*
 * Copyright (C) 2016 The Android Open Source Project
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

#define LOG_TAG "android.hardware.tv.input@1.0-service"
#include <android-base/logging.h>
#include <log/log.h>
#include <string.h>

#include "RkOutputManager.h"
using namespace android;

namespace rockchip {
namespace hardware {
namespace outputmanager {
namespace V1_0 {
namespace implementation {

RkOutputManager::RkOutputManager(hw_output_device* dev){
    mHwOutput = dev;
}

RkOutputManager::~RkOutputManager() {
}

Return<void> RkOutputManager::initial()
{
    mHwOutput->initialize(mHwOutput, NULL);
    return Void();
}

Return<Result> RkOutputManager::setMode(Display display, const hidl_string& mode)
{
    std::string modeStd(mode.c_str());

    mHwOutput->setMode(mHwOutput, display, modeStd.c_str());
    return Result::OK;
}

Return<Result> RkOutputManager::set3DMode(const hidl_string& mode)
{
    std::string modeStd(mode.c_str());
    mHwOutput->set3DMode(mHwOutput, modeStd.c_str());
    return Result::OK;
}

Return<Result> RkOutputManager::setGamma(Display display, uint32_t size, const hidl_vec<uint16_t>& r, const hidl_vec<uint16_t>& g, const hidl_vec<uint16_t>& b)
{
    uint16_t red[size];
    uint16_t green[size];
    uint16_t blue[size];
    Result res = Result::UNKNOWN;
    int ret = 0;

    for (uint32_t i=0;i<size;i++){
        red[i] = r[i];
        green[i] = g[i];
        blue[i] = b[i];
    }

    ret = mHwOutput->setGamma(mHwOutput, display, size, red, green, blue);
    if (ret == 0)
        res = Result::OK;
    return res;
}

Return<Result> RkOutputManager::set3DLut(Display display, uint32_t size, const hidl_vec<uint16_t>& r, const hidl_vec<uint16_t>& g, const hidl_vec<uint16_t>& b)
{
    uint16_t red[size];
    uint16_t green[size];
    uint16_t blue[size];
    Result res = Result::UNKNOWN;
    int ret = 0;

    for (uint32_t i=0;i<size;i++){
        red[i] = r[i];
        green[i] = g[i];
        blue[i] = b[i];
    }

    ret = mHwOutput->set3DLut(mHwOutput, display, size, red, green, blue);
    if (ret == 0)
        res = Result::OK;
    return res;
}

Return<Result> RkOutputManager::setBrightness(Display display, uint32_t value)
{
    mHwOutput->setBrightness(mHwOutput, display, value);
    return Result::OK;
}

Return<Result> RkOutputManager::setContrast(Display display, uint32_t value)
{
    mHwOutput->setContrast(mHwOutput, display, value);
    return Result::OK;
}

Return<Result> RkOutputManager::setSaturation(Display display, uint32_t value)
{
    mHwOutput->setSat(mHwOutput, display, value);
    return Result::OK;
}

Return<Result> RkOutputManager::setHue(Display display, uint32_t value)
{
    mHwOutput->setHue(mHwOutput, display, value);
    return Result::OK;
}

Return<Result> RkOutputManager::setScreenScale(Display display, uint32_t direction, uint32_t value)
{
    mHwOutput->setScreenScale(mHwOutput, display, direction, value);
    return Result::OK;
}

Return<Result> RkOutputManager::setHdrMode(Display display, uint32_t hdrmode)
{
    mHwOutput->setHdrMode(mHwOutput, display, hdrmode);
    return Result::OK;
}

Return<Result> RkOutputManager::setColorMode(Display display, const hidl_string& mode)
{
    std::string modeStd(mode.c_str());

    mHwOutput->setColorMode(mHwOutput, display, modeStd.c_str());
    return Result::OK;
}

Return<void> RkOutputManager::getCurCorlorMode(Display display, getCurCorlorMode_cb hidl_cb)
{
    hidl_string hidl_mode;
    char mCurMode[256];
    int ret = mHwOutput->getCurColorMode(mHwOutput, display, mCurMode);
    Result res = Result::UNKNOWN;

    if (ret == 0)
        res = Result::OK;

    hidl_mode = mCurMode;
    hidl_cb(res, hidl_mode);
    return Void();
}

Return<void> RkOutputManager::getCurMode(Display display, getCurMode_cb _hidl_cb)
{
    hidl_string hidl_mode;
    char mCurMode[256];
    int ret = mHwOutput->getCurMode(mHwOutput, display, mCurMode);
    Result res = Result::UNKNOWN;

    if (ret == 0)
        res = Result::OK;
    hidl_mode = mCurMode;
    _hidl_cb(res, hidl_mode);
    return Void();
}

Return<void> RkOutputManager::getNumConnectors(Display display, getNumConnectors_cb _hidl_cb)
{
    int numConnectors;
    int ret = mHwOutput->getNumConnectors(mHwOutput, display, &numConnectors);
    Result res = Result::UNKNOWN;

    if (ret == 0)
        res = Result::OK;

    _hidl_cb(res, numConnectors);
    ALOGV("%s:%d numConnectors:%d ", __FUNCTION__, __LINE__, numConnectors);
	return Void();
}

Return<void> RkOutputManager::getConnectState(Display display, getConnectState_cb _hidl_cb)
{
    int state;
    int ret = mHwOutput->getConnectorState(mHwOutput, display, &state);
    Result res = Result::UNKNOWN;

    if (ret == 0)
        res = Result::OK;

    _hidl_cb(res, state);
    ALOGV("%s:%d state:%d ", __FUNCTION__, __LINE__, state);
    return Void();
}

Return<void> RkOutputManager::getBuiltIn(Display display, getBuiltIn_cb _hidl_cb)
{
    int builtin;
    int ret = mHwOutput->getBuiltIn(mHwOutput, display, &builtin);
    Result res = Result::UNKNOWN;

    if (ret == 0)
        res = Result::OK;

    _hidl_cb(res, builtin);
    ALOGV("%s:%d builtin:%d ", __FUNCTION__, __LINE__, builtin);
    return Void();
}

Return<void> RkOutputManager::getCorlorModeConfigs(Display display, getCorlorModeConfigs_cb _hidl_cb)
{
    int capa[2];
    int ret = mHwOutput->getColorConfigs(mHwOutput, display, capa);
    Result res = Result::UNKNOWN;
    hidl_vec<uint32_t> hidl_cfgs;

    if (ret == 0) {
        res = Result::OK;
        hidl_cfgs.resize(2);
        hidl_cfgs[0] = capa[0];
        hidl_cfgs[1] = capa[1];
    }

    _hidl_cb(res, hidl_cfgs);
    ALOGV("%s:%d hidl_cfgs:%d %d", __FUNCTION__, __LINE__, hidl_cfgs[0], hidl_cfgs[1]);
    return Void();
}

Return<void> RkOutputManager::getOverscan(Display display, getOverscan_cb _hidl_cb)
{
    uint32_t mOverscans[4];
    int ret = mHwOutput->getOverscan(mHwOutput, display, mOverscans);
    hidl_vec<uint32_t> hidl_overscan;
    Result res = Result::UNKNOWN;

    if (ret == 0) {
        res = Result::OK;
        hidl_overscan.resize(4);
        for (int i=0;i<4;i++)
            hidl_overscan[i] = mOverscans[i];
    }

    _hidl_cb(res, hidl_overscan);
    ALOGV("%s:%d hidl_overscan:%d %d %d %d", __FUNCTION__, __LINE__,
          hidl_overscan[0], hidl_overscan[1], hidl_overscan[2], hidl_overscan[3]);
    return Void();
}

Return<void> RkOutputManager::getBcsh(Display display, getBcsh_cb _hidl_cb)
{
    uint32_t mBcshs[4];
    int ret = mHwOutput->getBcsh(mHwOutput, display, mBcshs);
    hidl_vec<uint32_t> hidl_bcsh;
    Result res = Result::UNKNOWN;

    if (ret == 0) {
        res = Result::OK;
        hidl_bcsh.resize(4);
        for (int i=0;i<4;i++)
            hidl_bcsh[i] = mBcshs[i];
    }

    ALOGV("%s:%d bcsh:%d %d %d %d", __FUNCTION__, __LINE__, hidl_bcsh[0], hidl_bcsh[1], hidl_bcsh[2], hidl_bcsh[3]);
    _hidl_cb(res, hidl_bcsh);
    return Void();
}

Return<void> RkOutputManager::getDisplayModes(Display display, getDisplayModes_cb _hidl_cb)
{
    drm_mode_t* mModes = NULL;
    uint32_t size=0;
    mModes = mHwOutput->getDisplayModes(mHwOutput, display, &size);
    Result res = Result::UNKNOWN;
    hidl_vec<RkDrmMode> mDisplayModes;

    if (mModes != NULL) {
        res = Result::OK;
        mDisplayModes.resize((size_t)size);
        ALOGV("RkOutputManager::getDisplayModes .size = %d", (int)mDisplayModes.size());
        for (uint32_t i=0;i<size;i++) {
            mDisplayModes[i].width = mModes[i].width;
            mDisplayModes[i].height = mModes[i].height;
            mDisplayModes[i].refreshRate = mModes[i].refreshRate;
            mDisplayModes[i].clock = mModes[i].clock;
            mDisplayModes[i].flags = mModes[i].flags;
            mDisplayModes[i].interlaceFlag = mModes[i].interlaceFlag ;
            mDisplayModes[i].yuvFlag = mModes[i].yuvFlag;
            mDisplayModes[i].connectorId = mModes[i].connectorId;
            mDisplayModes[i].mode_type = mModes[i].mode_type;
            mDisplayModes[i].idx = mModes[i].idx;
            mDisplayModes[i].hsync_start = mModes[i].hsync_start;
            mDisplayModes[i].hsync_end = mModes[i].hsync_end ;
            mDisplayModes[i].htotal = mModes[i].htotal;
            mDisplayModes[i].hskew = mModes[i].hskew;
            mDisplayModes[i].vsync_start = mModes[i].vsync_start;
            mDisplayModes[i].vsync_end = mModes[i].vsync_end;
            mDisplayModes[i].vtotal = mModes[i].vtotal;
            mDisplayModes[i].vscan = mModes[i].vscan;
        }
    }

    _hidl_cb(res, mDisplayModes);
    if (mModes)
        free(mModes);
    return Void();
}

Return<void> RkOutputManager::getConnectorInfo(getConnectorInfo_cb _hidl_cb)
{
    connector_info_t* mInfo = NULL;
    uint32_t size=0;
    mInfo = mHwOutput->getConnectorInfo(mHwOutput, &size);
    Result res = Result::UNKNOWN;
    hidl_vec<RkConnectorInfo> mRkConnectorInfo;

    if (mInfo != NULL) {
        res = Result::OK;
        mRkConnectorInfo.resize((size_t)size);
        for (uint32_t i=0;i<size;i++) {
            mRkConnectorInfo[i].type = mInfo[i].type;
            mRkConnectorInfo[i].id = mInfo[i].id;
            mRkConnectorInfo[i].state = mInfo[i].state;
        }
    }

    _hidl_cb(res, mRkConnectorInfo);
    if (mInfo)
        free(mInfo);
    return Void();
}

Return<Result> RkOutputManager::updateDispHeader()
{
    Result res = Result::UNKNOWN;
    int ret = mHwOutput->updateDispHeader(mHwOutput);
    if(ret == 0){
        res = Result::OK;
    }
    return res;
}

Return<void> RkOutputManager::saveConfig()
{
    mHwOutput->saveConfig(mHwOutput);
    return Void();
}

Return<void> RkOutputManager::hotPlug()
{
    mHwOutput->hotplug(mHwOutput);
    return Void();
}

IRkOutputManager* HIDL_FETCH_IRkOutputManager(const char* /* name */) {
    struct hw_output_device*   mHwOutput;
    const hw_module_t* hw_module = nullptr;
    int ret = hw_get_module(HW_OUTPUT_HARDWARE_MODULE_ID, &hw_module);

    if (ret == 0) {
        ret = hw_output_open(hw_module, &mHwOutput);
        if (ret == 0) {
            return new RkOutputManager(mHwOutput);
        } else {
            LOG(ERROR) << "Passthrough failed to load legacy HAL.";
            return nullptr;
        }
    }
    else {
        LOG(ERROR) << "hw_get_module " << HWC_HARDWARE_MODULE_ID
                   << " failed: " << ret;
        return nullptr;
    }
}

}  // namespace implementation
}  // namespace V1_0
}  // namespace composer
}  // namespace hardware
}  // namespace vendor
