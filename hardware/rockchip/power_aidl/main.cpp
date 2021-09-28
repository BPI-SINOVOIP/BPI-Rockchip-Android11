/*
 * Copyright (c) 2020 Rockchip Electronics Co., Ltd
 */
#include "Power.h"

#include <android-base/logging.h>
#include <android/binder_manager.h>
#include <android/binder_process.h>

using aidl::android::hardware::power::impl::rockchip::Power;

int main() {
    ABinderProcess_setThreadPoolMaxThreadCount(0);
    std::shared_ptr<Power> vib = ndk::SharedRefBase::make<Power>();

    const std::string instance = std::string() + Power::descriptor + "/default";
    binder_status_t status = AServiceManager_addService(vib->asBinder().get(), instance.c_str());
    CHECK(status == STATUS_OK);

    ABinderProcess_joinThreadPool();
    return EXIT_FAILURE;  // should not reach
}
