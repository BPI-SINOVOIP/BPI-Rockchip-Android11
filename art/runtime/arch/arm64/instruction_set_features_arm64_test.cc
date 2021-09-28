/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include "instruction_set_features_arm64.h"

#include <gtest/gtest.h>

namespace art {

TEST(Arm64InstructionSetFeaturesTest, Arm64Features) {
  // Build features for an ARM64 processor.
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> arm64_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "default", &error_msg));
  ASSERT_TRUE(arm64_features.get() != nullptr) << error_msg;
  EXPECT_EQ(arm64_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(arm64_features->Equals(arm64_features.get()));
  EXPECT_STREQ("a53,crc,-lse,-fp16,-dotprod,-sve", arm64_features->GetFeatureString().c_str());
  EXPECT_EQ(arm64_features->AsBitmap(), 3U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a57_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a57", &error_msg));
  ASSERT_TRUE(cortex_a57_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a57_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a57_features->Equals(cortex_a57_features.get()));
  EXPECT_TRUE(cortex_a57_features->HasAtLeast(arm64_features.get()));
  EXPECT_STREQ("a53,crc,-lse,-fp16,-dotprod,-sve",
               cortex_a57_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a57_features->AsBitmap(), 3U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a73_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a73", &error_msg));
  ASSERT_TRUE(cortex_a73_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a73_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a73_features->Equals(cortex_a73_features.get()));
  EXPECT_TRUE(cortex_a73_features->AsArm64InstructionSetFeatures()->HasCRC());
  EXPECT_FALSE(cortex_a73_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_FALSE(cortex_a73_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_FALSE(cortex_a73_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(cortex_a73_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("a53,crc,-lse,-fp16,-dotprod,-sve",
               cortex_a73_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a73_features->AsBitmap(), 3U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a35_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a35", &error_msg));
  ASSERT_TRUE(cortex_a35_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a35_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a35_features->Equals(cortex_a35_features.get()));
  EXPECT_STREQ("-a53,crc,-lse,-fp16,-dotprod,-sve",
               cortex_a35_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a35_features->AsBitmap(), 2U);

  std::unique_ptr<const InstructionSetFeatures> kryo_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "kryo", &error_msg));
  ASSERT_TRUE(kryo_features.get() != nullptr) << error_msg;
  EXPECT_EQ(kryo_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(kryo_features->Equals(kryo_features.get()));
  EXPECT_TRUE(kryo_features->Equals(cortex_a35_features.get()));
  EXPECT_FALSE(kryo_features->Equals(cortex_a57_features.get()));
  EXPECT_STREQ("-a53,crc,-lse,-fp16,-dotprod,-sve", kryo_features->GetFeatureString().c_str());
  EXPECT_EQ(kryo_features->AsBitmap(), 2U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a55_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a55", &error_msg));
  ASSERT_TRUE(cortex_a55_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a55_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a55_features->Equals(cortex_a55_features.get()));
  EXPECT_FALSE(cortex_a55_features->Equals(cortex_a35_features.get()));
  EXPECT_FALSE(cortex_a55_features->Equals(cortex_a57_features.get()));
  EXPECT_TRUE(cortex_a35_features->HasAtLeast(arm64_features.get()));
  EXPECT_STREQ("-a53,crc,lse,fp16,dotprod,-sve", cortex_a55_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a55_features->AsBitmap(), 30U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a75_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a75", &error_msg));
  ASSERT_TRUE(cortex_a75_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a75_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a75_features->Equals(cortex_a75_features.get()));
  EXPECT_FALSE(cortex_a75_features->Equals(cortex_a35_features.get()));
  EXPECT_FALSE(cortex_a75_features->Equals(cortex_a57_features.get()));
  EXPECT_TRUE(cortex_a75_features->HasAtLeast(arm64_features.get()));
  EXPECT_TRUE(cortex_a75_features->HasAtLeast(cortex_a55_features.get()));
  EXPECT_FALSE(cortex_a35_features->HasAtLeast(cortex_a75_features.get()));
  EXPECT_FALSE(cortex_a75_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_835769());
  EXPECT_FALSE(cortex_a75_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_843419());
  EXPECT_TRUE(cortex_a75_features->AsArm64InstructionSetFeatures()->HasCRC());
  EXPECT_TRUE(cortex_a75_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_TRUE(cortex_a75_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_TRUE(cortex_a75_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(cortex_a75_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("-a53,crc,lse,fp16,dotprod,-sve", cortex_a75_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a75_features->AsBitmap(), 30U);

  std::unique_ptr<const InstructionSetFeatures> cortex_a76_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "cortex-a76", &error_msg));
  ASSERT_TRUE(cortex_a76_features.get() != nullptr) << error_msg;
  EXPECT_EQ(cortex_a76_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(cortex_a76_features->Equals(cortex_a76_features.get()));
  EXPECT_FALSE(cortex_a76_features->Equals(cortex_a35_features.get()));
  EXPECT_FALSE(cortex_a76_features->Equals(cortex_a57_features.get()));
  EXPECT_TRUE(cortex_a76_features->Equals(cortex_a75_features.get()));
  EXPECT_TRUE(cortex_a76_features->HasAtLeast(arm64_features.get()));
  EXPECT_TRUE(cortex_a76_features->HasAtLeast(cortex_a55_features.get()));
  EXPECT_FALSE(cortex_a35_features->HasAtLeast(cortex_a76_features.get()));
  EXPECT_FALSE(cortex_a76_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_835769());
  EXPECT_FALSE(cortex_a76_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_843419());
  EXPECT_TRUE(cortex_a76_features->AsArm64InstructionSetFeatures()->HasCRC());
  EXPECT_TRUE(cortex_a76_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_TRUE(cortex_a76_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_TRUE(cortex_a76_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(cortex_a76_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("-a53,crc,lse,fp16,dotprod,-sve", cortex_a76_features->GetFeatureString().c_str());
  EXPECT_EQ(cortex_a76_features->AsBitmap(), 30U);
}

TEST(Arm64InstructionSetFeaturesTest, Arm64AddFeaturesFromString) {
  std::string error_msg;
  std::unique_ptr<const InstructionSetFeatures> base_features(
      InstructionSetFeatures::FromVariant(InstructionSet::kArm64, "generic", &error_msg));
  ASSERT_TRUE(base_features.get() != nullptr) << error_msg;

  // Build features for a Cortex-A76 processor (with ARMv8.2 and Dot Product exentions support).
  std::unique_ptr<const InstructionSetFeatures> a76_features(
      base_features->AddFeaturesFromString("-a53,armv8.2-a,dotprod", &error_msg));
  ASSERT_TRUE(a76_features.get() != nullptr) << error_msg;
  ASSERT_EQ(a76_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(a76_features->Equals(a76_features.get()));
  EXPECT_FALSE(a76_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_835769());
  EXPECT_FALSE(a76_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_843419());
  EXPECT_TRUE(a76_features->AsArm64InstructionSetFeatures()->HasCRC());
  EXPECT_TRUE(a76_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_TRUE(a76_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_TRUE(a76_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(a76_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("-a53,crc,lse,fp16,dotprod,-sve", a76_features->GetFeatureString().c_str());
  EXPECT_EQ(a76_features->AsBitmap(), 30U);

  // Build features for a default ARM64 processor.
  std::unique_ptr<const InstructionSetFeatures> generic_features(
      base_features->AddFeaturesFromString("default", &error_msg));
  ASSERT_TRUE(generic_features.get() != nullptr) << error_msg;
  ASSERT_EQ(generic_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(generic_features->Equals(generic_features.get()));
  EXPECT_FALSE(generic_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_FALSE(generic_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_FALSE(generic_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(generic_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("a53,crc,-lse,-fp16,-dotprod,-sve", generic_features->GetFeatureString().c_str());
  EXPECT_EQ(generic_features->AsBitmap(), 3U);

  // Build features for a ARM64 processor that supports up to ARMv8.2.
  std::unique_ptr<const InstructionSetFeatures> armv8_2a_cpu_features(
      base_features->AddFeaturesFromString("-a53,armv8.2-a", &error_msg));
  ASSERT_TRUE(armv8_2a_cpu_features.get() != nullptr) << error_msg;
  ASSERT_EQ(armv8_2a_cpu_features->GetInstructionSet(), InstructionSet::kArm64);
  EXPECT_TRUE(armv8_2a_cpu_features->Equals(armv8_2a_cpu_features.get()));
  EXPECT_FALSE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_835769());
  EXPECT_FALSE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->NeedFixCortexA53_843419());
  EXPECT_TRUE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->HasCRC());
  EXPECT_TRUE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->HasLSE());
  EXPECT_TRUE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->HasFP16());
  EXPECT_FALSE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->HasDotProd());
  EXPECT_FALSE(armv8_2a_cpu_features->AsArm64InstructionSetFeatures()->HasSVE());
  EXPECT_STREQ("-a53,crc,lse,fp16,-dotprod,-sve",
               armv8_2a_cpu_features->GetFeatureString().c_str());
  EXPECT_EQ(armv8_2a_cpu_features->AsBitmap(), 14U);
}

TEST(Arm64InstructionSetFeaturesTest, IsRuntimeDetectionSupported) {
  if (kRuntimeISA == InstructionSet::kArm64) {
    EXPECT_TRUE(InstructionSetFeatures::IsRuntimeDetectionSupported());
  }
}

TEST(Arm64InstructionSetFeaturesTest, FeaturesFromRuntimeDetection) {
  if (kRuntimeISA != InstructionSet::kArm64) {
    return;
  }

  std::unique_ptr<const InstructionSetFeatures> hwcap_features(
      InstructionSetFeatures::FromHwcap());
  std::unique_ptr<const InstructionSetFeatures> runtime_detected_features(
      InstructionSetFeatures::FromRuntimeDetection());
  std::unique_ptr<const InstructionSetFeatures> cpp_defined_features(
      InstructionSetFeatures::FromCppDefines());
  EXPECT_NE(runtime_detected_features, nullptr);
  EXPECT_TRUE(InstructionSetFeatures::IsRuntimeDetectionSupported());
  EXPECT_TRUE(runtime_detected_features->Equals(hwcap_features.get()));
  EXPECT_TRUE(runtime_detected_features->HasAtLeast(cpp_defined_features.get()));
}

TEST(Arm64InstructionSetFeaturesTest, AddFeaturesFromStringRuntime) {
  std::unique_ptr<const InstructionSetFeatures> features(
      InstructionSetFeatures::FromBitmap(InstructionSet::kArm64, 0x0));
  std::unique_ptr<const InstructionSetFeatures> hwcap_features(
      InstructionSetFeatures::FromHwcap());

  std::string error_msg;
  features = features->AddFeaturesFromString("runtime", &error_msg);

  EXPECT_NE(features, nullptr);
  EXPECT_TRUE(error_msg.empty());

  if (kRuntimeISA == InstructionSet::kArm64) {
    EXPECT_TRUE(features->Equals(hwcap_features.get()));
    EXPECT_EQ(features->GetFeatureString(), hwcap_features->GetFeatureString());
  }

  std::unique_ptr<const InstructionSetFeatures> a53_features(
      Arm64InstructionSetFeatures::FromVariant("cortex-a53", &error_msg));
  features = a53_features->AddFeaturesFromString("runtime", &error_msg);
  EXPECT_NE(features, nullptr);
  EXPECT_TRUE(error_msg.empty()) << error_msg;
  const Arm64InstructionSetFeatures *arm64_features = features->AsArm64InstructionSetFeatures();
  EXPECT_TRUE(arm64_features->NeedFixCortexA53_835769());
  EXPECT_TRUE(arm64_features->NeedFixCortexA53_843419());
}

}  // namespace art
