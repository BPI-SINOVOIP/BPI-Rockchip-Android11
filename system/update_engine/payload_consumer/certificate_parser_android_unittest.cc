//
// Copyright (C) 2019 The Android Open Source Project
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "update_engine/payload_consumer/certificate_parser_interface.h"

#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/test_utils.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/payload_verifier.h"
#include "update_engine/payload_generator/payload_signer.h"

namespace chromeos_update_engine {

extern const char* kUnittestPrivateKeyPath;
const char* kUnittestOtacertsPath = "otacerts.zip";

TEST(CertificateParserAndroidTest, ParseZipArchive) {
  std::string ota_cert =
      test_utils::GetBuildArtifactsPath(kUnittestOtacertsPath);
  ASSERT_TRUE(utils::FileExists(ota_cert.c_str()));

  std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>> keys;
  auto parser = CreateCertificateParser();
  ASSERT_TRUE(parser->ReadPublicKeysFromCertificates(ota_cert, &keys));
  ASSERT_EQ(1u, keys.size());
}

TEST(CertificateParserAndroidTest, VerifySignature) {
  brillo::Blob hash_blob;
  ASSERT_TRUE(HashCalculator::RawHashOfData({'x'}, &hash_blob));
  brillo::Blob sig_blob;
  ASSERT_TRUE(PayloadSigner::SignHash(
      hash_blob,
      test_utils::GetBuildArtifactsPath(kUnittestPrivateKeyPath),
      &sig_blob));

  auto verifier = PayloadVerifier::CreateInstanceFromZipPath(
      test_utils::GetBuildArtifactsPath(kUnittestOtacertsPath));
  ASSERT_TRUE(verifier != nullptr);
  ASSERT_TRUE(verifier->VerifyRawSignature(sig_blob, hash_blob, nullptr));
}

}  // namespace chromeos_update_engine
