// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//#define LOG_NDEBUG 0
#define LOG_TAG "android.hardware.media.c2@1.0-service-v4l2"

#include <base/logging.h>
#include <C2Component.h>
#include <codec2/hidl/1.0/ComponentStore.h>
#include <hidl/HidlTransportSupport.h>
#include <log/log.h>
#include <minijail.h>

#include <v4l2_codec2/store/V4L2ComponentStore.h>

// Default policy for codec2.0 service.
static constexpr char kBaseSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/android.hardware.media.c2@1.1-default-seccomp_policy";

// Additional device-specific seccomp permissions can be added in this file.
static constexpr char kExtSeccompPolicyPath[] =
        "/vendor/etc/seccomp_policy/codec2.vendor.ext.policy";

int main(int /* argc */, char** /* argv */) {
    ALOGD("Service starting...");

    signal(SIGPIPE, SIG_IGN);
    android::SetUpMinijail(kBaseSeccompPolicyPath, kExtSeccompPolicyPath);

    // Extra threads may be needed to handle a stacked IPC sequence that
    // contains alternating binder and hwbinder calls. (See b/35283480.)
    android::hardware::configureRpcThreadpool(8, true /* callerWillJoin */);

#if LOG_NDEBUG == 0
    ALOGD("Enable all verbose logging of libchrome");
    logging::SetMinLogLevel(-5);
#endif

    // Create IComponentStore service.
    {
        using namespace ::android::hardware::media::c2::V1_0;

        ALOGD("Instantiating Codec2's V4L2 IComponentStore service...");
        android::sp<IComponentStore> store(
                new utils::ComponentStore(android::V4L2ComponentStore::Create()));
        if (store == nullptr) {
            ALOGE("Cannot create Codec2's V4L2 IComponentStore service.");
        } else if (store->registerAsService("default") != android::OK) {
            ALOGE("Cannot register Codec2's IComponentStore service.");
        } else {
            ALOGI("Codec2's IComponentStore service created.");
        }
    }

    android::hardware::joinRpcThreadpool();
    ALOGD("Service shutdown.");
    return 0;
}
