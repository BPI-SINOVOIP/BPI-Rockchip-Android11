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

#include <gtest/gtest.h>

#include <android/api-level.h>
#include <android-base/properties.h>
#include <meminfo/procmeminfo.h>
#include <meminfo/sysmeminfo.h>

namespace android {
namespace meminfo {

// /proc/<pid>/smaps_rollup support is required.
TEST(SmapsRollup, IsSupported) {
    // Use init's pid for this test since it's the only known pid.
    ASSERT_TRUE(IsSmapsRollupSupported(1));
}

// KReclaimable in /proc/meminfo is required.
TEST(SysMemInfo, TestKReclaimable) {
    SysMemInfo mi;

    if (android::base::GetIntProperty("ro.product.first_api_level", 0) < __ANDROID_API_R__) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(mi.ReadMemInfo());
    // KReclaimable includes SReclaimable, so they should be at least equal
    ASSERT_TRUE(mi.mem_kreclaimable_kb() >= mi.mem_slab_reclaimable_kb());
}

// /sys/kernel/ion/total_heaps_kb support is required.
TEST(SysMemInfo, TestIonTotalHeapsKb) {
    uint64_t size;

    if (android::base::GetIntProperty("ro.product.first_api_level", 0) < __ANDROID_API_R__) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(ReadIonHeapsSizeKb(&size));
}

// /sys/kernel/ion/total_pools_kb support is required.
TEST(SysMemInfo, TestIonTotalPoolsKb) {
    uint64_t size;

    if (android::base::GetIntProperty("ro.product.first_api_level", 0) < __ANDROID_API_R__) {
        GTEST_SKIP();
    }
    ASSERT_TRUE(ReadIonPoolsSizeKb(&size));
}

}  // namespace meminfo
}  // namespace android
