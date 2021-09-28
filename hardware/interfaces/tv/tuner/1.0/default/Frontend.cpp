/*
 * Copyright (C) 2019 The Android Open Source Project
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

#define LOG_TAG "android.hardware.tv.tuner@1.0-Frontend"

#include "Frontend.h"
#include <android/hardware/tv/tuner/1.0/IFrontendCallback.h>
#include <utils/Log.h>

namespace android {
namespace hardware {
namespace tv {
namespace tuner {
namespace V1_0 {
namespace implementation {

Frontend::Frontend(FrontendType type, FrontendId id, sp<Tuner> tuner) {
    mType = type;
    mId = id;
    mTunerService = tuner;
    // Init callback to nullptr
    mCallback = nullptr;
}

Frontend::~Frontend() {}

Return<Result> Frontend::close() {
    ALOGV("%s", __FUNCTION__);
    // Reset callback
    mCallback = nullptr;
    mIsLocked = false;

    return Result::SUCCESS;
}

Return<Result> Frontend::setCallback(const sp<IFrontendCallback>& callback) {
    ALOGV("%s", __FUNCTION__);
    if (callback == nullptr) {
        ALOGW("[   WARN   ] Set Frontend callback with nullptr");
        return Result::INVALID_ARGUMENT;
    }

    mCallback = callback;
    return Result::SUCCESS;
}

Return<Result> Frontend::tune(const FrontendSettings& /* settings */) {
    ALOGV("%s", __FUNCTION__);
    if (mCallback == nullptr) {
        ALOGW("[   WARN   ] Frontend callback is not set when tune");
        return Result::INVALID_STATE;
    }

    mTunerService->frontendStartTune(mId);
    mCallback->onEvent(FrontendEventType::LOCKED);
    mIsLocked = true;
    return Result::SUCCESS;
}

Return<Result> Frontend::stopTune() {
    ALOGV("%s", __FUNCTION__);

    mTunerService->frontendStopTune(mId);
    mIsLocked = false;

    return Result::SUCCESS;
}

Return<Result> Frontend::scan(const FrontendSettings& settings, FrontendScanType type) {
    ALOGV("%s", __FUNCTION__);

    if (mType == FrontendType::ATSC) {
        FrontendScanMessage msg;
        msg.isLocked(true);
        mCallback->onScanMessage(FrontendScanMessageType::LOCKED, msg);
        mIsLocked = true;
        return Result::SUCCESS;
    }
    if (mType != FrontendType::DVBT) {
        return Result::UNAVAILABLE;
    }

    FrontendScanMessage msg;

    if (mIsLocked) {
        msg.isEnd(true);
        mCallback->onScanMessage(FrontendScanMessageType::END, msg);
        return Result::SUCCESS;
    }

    uint32_t frequency = settings.dvbt().frequency;
    if (type == FrontendScanType::SCAN_BLIND) {
        frequency += 100;
    }
    msg.frequencies({frequency});
    mCallback->onScanMessage(FrontendScanMessageType::FREQUENCY, msg);
    msg.isLocked(true);
    mCallback->onScanMessage(FrontendScanMessageType::LOCKED, msg);
    mIsLocked = true;

    return Result::SUCCESS;
}

Return<Result> Frontend::stopScan() {
    ALOGV("%s", __FUNCTION__);

    mIsLocked = false;
    return Result::SUCCESS;
}

Return<void> Frontend::getStatus(const hidl_vec<FrontendStatusType>& statusTypes,
                                 getStatus_cb _hidl_cb) {
    ALOGV("%s", __FUNCTION__);

    vector<FrontendStatus> statuses;
    for (int i = 0; i < statusTypes.size(); i++) {
        FrontendStatusType type = statusTypes[i];
        FrontendStatus status;
        // assign randomly selected values for testing.
        switch (type) {
            case FrontendStatusType::DEMOD_LOCK: {
                status.isDemodLocked(true);
                break;
            }
            case FrontendStatusType::SNR: {
                status.snr(221);
                break;
            }
            case FrontendStatusType::BER: {
                status.ber(1);
                break;
            }
            case FrontendStatusType::PER: {
                status.per(2);
                break;
            }
            case FrontendStatusType::PRE_BER: {
                status.preBer(3);
                break;
            }
            case FrontendStatusType::SIGNAL_QUALITY: {
                status.signalQuality(4);
                break;
            }
            case FrontendStatusType::SIGNAL_STRENGTH: {
                status.signalStrength(5);
                break;
            }
            case FrontendStatusType::SYMBOL_RATE: {
                status.symbolRate(6);
                break;
            }
            case FrontendStatusType::FEC: {
                status.innerFec(FrontendInnerFec::FEC_2_9);  // value = 1 << 7
                break;
            }
            case FrontendStatusType::MODULATION: {
                FrontendModulationStatus modulationStatus;
                modulationStatus.isdbt(FrontendIsdbtModulation::MOD_16QAM);  // value = 1 << 3
                status.modulation(modulationStatus);
                break;
            }
            case FrontendStatusType::SPECTRAL: {
                status.inversion(FrontendDvbcSpectralInversion::NORMAL);
                break;
            }
            case FrontendStatusType::LNB_VOLTAGE: {
                status.lnbVoltage(LnbVoltage::VOLTAGE_5V);
                break;
            }
            case FrontendStatusType::PLP_ID: {
                status.plpId(101);  // type uint8_t
                break;
            }
            case FrontendStatusType::EWBS: {
                status.isEWBS(false);
                break;
            }
            case FrontendStatusType::AGC: {
                status.agc(7);
                break;
            }
            case FrontendStatusType::LNA: {
                status.isLnaOn(false);
                break;
            }
            case FrontendStatusType::LAYER_ERROR: {
                vector<bool> v = {false, true, true};
                status.isLayerError(v);
                break;
            }
            case FrontendStatusType::MER: {
                status.mer(8);
                break;
            }
            case FrontendStatusType::FREQ_OFFSET: {
                status.freqOffset(9);
                break;
            }
            case FrontendStatusType::HIERARCHY: {
                status.hierarchy(FrontendDvbtHierarchy::HIERARCHY_1_NATIVE);
                break;
            }
            case FrontendStatusType::RF_LOCK: {
                status.isRfLocked(false);
                break;
            }
            case FrontendStatusType::ATSC3_PLP_INFO: {
                vector<FrontendStatusAtsc3PlpInfo> v;
                FrontendStatusAtsc3PlpInfo info1{
                        .plpId = 3,
                        .isLocked = false,
                        .uec = 313,
                };
                FrontendStatusAtsc3PlpInfo info2{
                        .plpId = 5,
                        .isLocked = true,
                        .uec = 515,
                };
                v.push_back(info1);
                v.push_back(info2);
                status.plpInfo(v);
                break;
            }
            default: {
                continue;
            }
        }
        statuses.push_back(status);
    }
    _hidl_cb(Result::SUCCESS, statuses);

    return Void();
}

Return<Result> Frontend::setLna(bool /* bEnable */) {
    ALOGV("%s", __FUNCTION__);

    return Result::SUCCESS;
}

Return<Result> Frontend::setLnb(uint32_t /* lnb */) {
    ALOGV("%s", __FUNCTION__);
    if (!supportsSatellite()) {
        return Result::INVALID_STATE;
    }
    return Result::SUCCESS;
}

FrontendType Frontend::getFrontendType() {
    return mType;
}

FrontendId Frontend::getFrontendId() {
    return mId;
}

bool Frontend::supportsSatellite() {
    return mType == FrontendType::DVBS || mType == FrontendType::ISDBS ||
           mType == FrontendType::ISDBS3;
}

bool Frontend::isLocked() {
    return mIsLocked;
}
}  // namespace implementation
}  // namespace V1_0
}  // namespace tuner
}  // namespace tv
}  // namespace hardware
}  // namespace android
