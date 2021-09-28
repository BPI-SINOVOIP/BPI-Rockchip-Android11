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

#ifndef UPDATE_ENGINE_CERTIFICATE_PARSER_STUB_H_
#define UPDATE_ENGINE_CERTIFICATE_PARSER_STUB_H_

#include <memory>
#include <string>
#include <vector>

#include <base/macros.h>

#include "payload_consumer/certificate_parser_interface.h"

namespace chromeos_update_engine {
class CertificateParserStub : public CertificateParserInterface {
 public:
  CertificateParserStub() = default;

  bool ReadPublicKeysFromCertificates(
      const std::string& path,
      std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>>*
          out_public_keys) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(CertificateParserStub);
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_CERTIFICATE_PARSER_STUB_H_
