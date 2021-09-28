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

#include "update_engine/payload_consumer/certificate_parser_android.h"

#include <memory>
#include <utility>

#include <base/logging.h>
#include <openssl/bio.h>
#include <openssl/pem.h>
#include <ziparchive/zip_archive.h>

#include "update_engine/payload_consumer/certificate_parser_interface.h"

namespace {
bool IterateZipEntriesAndSearchForKeys(
    const ZipArchiveHandle& handle, std::vector<std::vector<uint8_t>>* result) {
  void* cookie;
  int32_t iter_status = StartIteration(handle, &cookie, "", "x509.pem");
  if (iter_status != 0) {
    LOG(ERROR) << "Failed to iterate over entries in the certificate zipfile: "
               << ErrorCodeString(iter_status);
    return false;
  }
  std::unique_ptr<void, decltype(&EndIteration)> guard(cookie, EndIteration);

  std::vector<std::vector<uint8_t>> pem_keys;
  std::string_view name;
  ZipEntry entry;
  while ((iter_status = Next(cookie, &entry, &name)) == 0) {
    std::vector<uint8_t> pem_content(entry.uncompressed_length);
    if (int32_t extract_status = ExtractToMemory(
            handle, &entry, pem_content.data(), pem_content.size());
        extract_status != 0) {
      LOG(ERROR) << "Failed to extract " << name << ": "
                 << ErrorCodeString(extract_status);
      return false;
    }
    pem_keys.push_back(pem_content);
  }

  if (iter_status != -1) {
    LOG(ERROR) << "Error while iterating over zip entries: "
               << ErrorCodeString(iter_status);
    return false;
  }

  *result = std::move(pem_keys);
  return true;
}

}  // namespace

namespace chromeos_update_engine {
bool CertificateParserAndroid::ReadPublicKeysFromCertificates(
    const std::string& path,
    std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>>*
        out_public_keys) {
  out_public_keys->clear();

  ZipArchiveHandle handle;
  if (int32_t open_status = OpenArchive(path.c_str(), &handle);
      open_status != 0) {
    LOG(ERROR) << "Failed to open " << path << ": "
               << ErrorCodeString(open_status);
    return false;
  }

  std::vector<std::vector<uint8_t>> pem_certs;
  if (!IterateZipEntriesAndSearchForKeys(handle, &pem_certs)) {
    CloseArchive(handle);
    return false;
  }
  CloseArchive(handle);

  // Convert the certificates into public keys. Stop and return false if we
  // encounter an error.
  std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>> result;
  for (const auto& cert : pem_certs) {
    std::unique_ptr<BIO, decltype(&BIO_free)> input(
        BIO_new_mem_buf(cert.data(), cert.size()), BIO_free);

    std::unique_ptr<X509, decltype(&X509_free)> x509(
        PEM_read_bio_X509(input.get(), nullptr, nullptr, nullptr), X509_free);
    if (!x509) {
      LOG(ERROR) << "Failed to read x509 certificate";
      return false;
    }

    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> public_key(
        X509_get_pubkey(x509.get()), EVP_PKEY_free);
    if (!public_key) {
      LOG(ERROR) << "Failed to extract the public key from x509 certificate";
      return false;
    }
    result.push_back(std::move(public_key));
  }

  *out_public_keys = std::move(result);
  return true;
}

std::unique_ptr<CertificateParserInterface> CreateCertificateParser() {
  return std::make_unique<CertificateParserAndroid>();
}

}  // namespace chromeos_update_engine
