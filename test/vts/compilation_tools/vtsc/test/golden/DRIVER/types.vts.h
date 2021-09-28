#ifndef __VTS_DRIVER__android_hardware_nfc_V1_0_types__
#define __VTS_DRIVER__android_hardware_nfc_V1_0_types__

#undef LOG_TAG
#define LOG_TAG "FuzzerExtended_android_hardware_nfc_V1_0_types"

#include <log/log.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <driver_base/DriverBase.h>
#include <driver_base/DriverCallbackBase.h>

#include <VtsDriverCommUtil.h>

#include <android/hardware/nfc/1.0/types.h>
#include <hidl/HidlSupport.h>


using namespace android::hardware::nfc::V1_0;
namespace android {
namespace vts {
extern "C" void MessageTo__android__hardware__nfc__V1_0__NfcEvent(const VariableSpecificationMessage& var_msg, ::android::hardware::nfc::V1_0::NfcEvent* arg, const string& callback_socket_name);
uint32_t Random__android__hardware__nfc__V1_0__NfcEvent();
bool Verify__android__hardware__nfc__V1_0__NfcEvent(const VariableSpecificationMessage& expected_result, const VariableSpecificationMessage& actual_result);
extern "C" void SetResult__android__hardware__nfc__V1_0__NfcEvent(VariableSpecificationMessage* result_msg, ::android::hardware::nfc::V1_0::NfcEvent result_value);
extern "C" void MessageTo__android__hardware__nfc__V1_0__NfcStatus(const VariableSpecificationMessage& var_msg, ::android::hardware::nfc::V1_0::NfcStatus* arg, const string& callback_socket_name);
uint32_t Random__android__hardware__nfc__V1_0__NfcStatus();
bool Verify__android__hardware__nfc__V1_0__NfcStatus(const VariableSpecificationMessage& expected_result, const VariableSpecificationMessage& actual_result);
extern "C" void SetResult__android__hardware__nfc__V1_0__NfcStatus(VariableSpecificationMessage* result_msg, ::android::hardware::nfc::V1_0::NfcStatus result_value);


}  // namespace vts
}  // namespace android
#endif
