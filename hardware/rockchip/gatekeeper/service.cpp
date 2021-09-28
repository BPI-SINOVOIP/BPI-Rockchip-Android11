#define LOG_TAG "android.hardware.gatekeeper@1.0-service.optee"

#include <android-base/logging.h>
#include <android/hardware/gatekeeper/1.0/IGatekeeper.h>

#include <hidl/LegacySupport.h>

#include "RkGateKeeperDevice.h"

// Generated HIDL files
using android::RkGateKeeperDevice;
using android::hardware::gatekeeper::V1_0::IGatekeeper;

int main() {
    ::android::hardware::configureRpcThreadpool(1, true /* willJoinThreadpool */);
    android::sp<RkGateKeeperDevice> gatekeeper(new RkGateKeeperDevice());
    auto status = gatekeeper->registerAsService();
    if (status != android::OK) {
        LOG(FATAL) << "Could not register service for Gatekeeper 1.0 (software) (" << status << ")";
    }

    android::hardware::joinRpcThreadpool();
    return -1;  // Should never get here.
}
