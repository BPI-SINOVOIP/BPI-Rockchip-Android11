#include <android-base/logging.h>
#include <android/hardware/keymaster/4.0/IKeymasterDevice.h>
#include <hidl/HidlTransportSupport.h>

#include <RkKeymaster4Device.h>

using android::hardware::keymaster::V4_0::SecurityLevel;

int main() {
    ::android::hardware::configureRpcThreadpool(1, true /* willJoinThreadpool */);
    setenv("ANDROID_LOG_TAGS", "*:v", 1);

    auto keymaster = ::keymaster::V4_0::ng::CreateKeymasterDevice(SecurityLevel::TRUSTED_ENVIRONMENT);

    auto status = keymaster->registerAsService();
    if (status != android::OK) {
        LOG(FATAL) << "Could not register service for Keymaster 4.0 (" << status << ")";
    }

    android::hardware::joinRpcThreadpool();
    return -1;  // Should never get here.
}
