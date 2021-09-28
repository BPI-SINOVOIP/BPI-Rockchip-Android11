/*
 * Copyright (C) 2020 The Android Open Source Project
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

#include <android-base/properties.h>
#include <gtest/gtest.h>

static constexpr const char kEnabled[] = "ro.virtual_ab.enabled";
static constexpr const char kRetrofit[] = "ro.virtual_ab.retrofit";

using android::base::GetBoolProperty;

TEST(VirtualAbRequirementTest, EnabledOnLaunchR) {
  EXPECT_TRUE(GetBoolProperty(kEnabled, false))
      << "Device must use Virtual A/B.";
  EXPECT_FALSE(GetBoolProperty(kRetrofit, false))
      << "Device must not retrofit Virtual A/B.";
}
