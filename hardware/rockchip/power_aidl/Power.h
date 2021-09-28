/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd
 */

#pragma once

#include <aidl/android/hardware/power/BnPower.h>
#include <string>

namespace aidl {
namespace android {
namespace hardware {
namespace power {
namespace impl {
namespace rockchip {

using aidl::android::hardware::power::Boost;

class Power : public BnPower {
    ndk::ScopedAStatus setMode(Mode type, bool enabled) override;
    ndk::ScopedAStatus isModeSupported(Mode type, bool* _aidl_return) override;
    ndk::ScopedAStatus setBoost(Boost type, int32_t durationMs) override;
    ndk::ScopedAStatus isBoostSupported(Boost type, bool* _aidl_return) override;

private:
    int64_t _boost_support_int = -1;
    int64_t _mode_support_int = -1;
    int8_t _boot_complete = -1;
    std::string _gpu_path = "";

    void getSupportedPlatform();
    void initPlatform();
    void interactive();
    void performanceBoost(bool on);
    void powerSave(bool on);
};

}  // namespace rockchip
}  // namespace impl
}  // namespace power
}  // namespace hardware
}  // namespace android
}  // namespace aidl
