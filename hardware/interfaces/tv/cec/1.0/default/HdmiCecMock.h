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

#ifndef ANDROID_HARDWARE_TV_CEC_V1_0_HDMICEC_H
#define ANDROID_HARDWARE_TV_CEC_V1_0_HDMICEC_H

#include <android/hardware/tv/cec/1.0/IHdmiCec.h>
#include <hardware/hardware.h>
#include <hardware/hdmi_cec.h>
#include <hidl/MQDescriptor.h>
#include <hidl/Status.h>
#include <algorithm>
#include <vector>

using namespace std;

namespace android {
namespace hardware {
namespace tv {
namespace cec {
namespace V1_0 {
namespace implementation {

using ::android::sp;
using ::android::hardware::hidl_string;
using ::android::hardware::hidl_vec;
using ::android::hardware::Return;
using ::android::hardware::Void;
using ::android::hardware::tv::cec::V1_0::CecLogicalAddress;
using ::android::hardware::tv::cec::V1_0::CecMessage;
using ::android::hardware::tv::cec::V1_0::HdmiPortInfo;
using ::android::hardware::tv::cec::V1_0::IHdmiCec;
using ::android::hardware::tv::cec::V1_0::IHdmiCecCallback;
using ::android::hardware::tv::cec::V1_0::MaxLength;
using ::android::hardware::tv::cec::V1_0::OptionKey;
using ::android::hardware::tv::cec::V1_0::Result;
using ::android::hardware::tv::cec::V1_0::SendMessageResult;

#define CEC_MSG_IN_FIFO "/dev/cec_in_pipe"
#define CEC_MSG_OUT_FIFO "/dev/cec_out_pipe"

struct HdmiCecMock : public IHdmiCec, public hidl_death_recipient {
    HdmiCecMock();
    // Methods from ::android::hardware::tv::cec::V1_0::IHdmiCec follow.
    Return<Result> addLogicalAddress(CecLogicalAddress addr) override;
    Return<void> clearLogicalAddress() override;
    Return<void> getPhysicalAddress(getPhysicalAddress_cb _hidl_cb) override;
    Return<SendMessageResult> sendMessage(const CecMessage& message) override;
    Return<void> setCallback(const sp<IHdmiCecCallback>& callback) override;
    Return<int32_t> getCecVersion() override;
    Return<uint32_t> getVendorId() override;
    Return<void> getPortInfo(getPortInfo_cb _hidl_cb) override;
    Return<void> setOption(OptionKey key, bool value) override;
    Return<void> setLanguage(const hidl_string& language) override;
    Return<void> enableAudioReturnChannel(int32_t portId, bool enable) override;
    Return<bool> isConnected(int32_t portId) override;

    virtual void serviceDied(uint64_t /*cookie*/,
                             const wp<::android::hidl::base::V1_0::IBase>& /*who*/) {
        setCallback(nullptr);
    }

    void cec_set_option(int flag, int value);
    void printCecMsgBuf(const char* msg_buf, int len);

  private:
    static void* __threadLoop(void* data);
    void threadLoop();
    int readMessageFromFifo(unsigned char* buf, int msgCount);
    int sendMessageToFifo(const CecMessage& message);
    void handleHotplugMessage(unsigned char* msgBuf);
    void handleCecMessage(unsigned char* msgBuf, int length);

  private:
    sp<IHdmiCecCallback> mCallback;

    // Variables for the virtual cec hal impl
    uint16_t mPhysicalAddress = 0xFFFF;
    vector<CecLogicalAddress> mLogicalAddresses;
    int32_t mCecVersion = 0;
    uint32_t mCecVendorId = 0;

    // Port configuration
    int mTotalPorts = 1;
    hidl_vec<HdmiPortInfo> mPortInfo;
    hidl_vec<bool> mPortConnectionStatus;

    // CEC Option value
    int mOptionWakeUp = 0;
    int mOptionEnableCec = 0;
    int mOptionSystemCecControl = 0;
    int mOptionLanguage = 0;

    // Testing variables
    // Input file descriptor
    int mInputFile;
    // Output file descriptor
    int mOutputFile;
    bool mCecThreadRun = true;
    pthread_t mThreadId = 0;
};
}  // namespace implementation
}  // namespace V1_0
}  // namespace cec
}  // namespace tv
}  // namespace hardware
}  // namespace android

#endif  // ANDROID_HARDWARE_TV_CEC_V1_0_HDMICEC_H
