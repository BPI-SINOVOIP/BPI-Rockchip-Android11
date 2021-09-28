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
#define LOG_TAG "VtsHalGnssV2_1TargetTest"

#include <gtest/gtest.h>
#include <hidl/GtestPrinter.h>
#include <hidl/ServiceManagement.h>

#include "gnss_hal_test.h"

using android::hardware::gnss::V2_1::IGnss;

INSTANTIATE_TEST_SUITE_P(
        PerInstance, GnssHalTest,
        testing::ValuesIn(android::hardware::getAllHalInstanceNames(IGnss::descriptor)),
        android::hardware::PrintInstanceNameToString);