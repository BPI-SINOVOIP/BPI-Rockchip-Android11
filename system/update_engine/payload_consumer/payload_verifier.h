//
// Copyright (C) 2014 The Android Open Source Project
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

#ifndef UPDATE_ENGINE_PAYLOAD_CONSUMER_PAYLOAD_VERIFIER_H_
#define UPDATE_ENGINE_PAYLOAD_CONSUMER_PAYLOAD_VERIFIER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <brillo/secure_blob.h>
#include <openssl/evp.h>

#include "update_engine/update_metadata.pb.h"

// This class holds the public keys and implements methods used for payload
// signature verification. See payload_generator/payload_signer.h for payload
// signing.

namespace chromeos_update_engine {

class PayloadVerifier {
 public:
  // Pads a SHA256 hash so that it may be encrypted/signed with RSA2048 or
  // RSA4096 using the PKCS#1 v1.5 scheme.
  // hash should be a pointer to vector of exactly 256 bits. |rsa_size| must be
  // one of 256 or 512 bytes. The vector will be modified in place and will
  // result in having a length of 2048 or 4096 bits, depending on the rsa size.
  // Returns true on success, false otherwise.
  static bool PadRSASHA256Hash(brillo::Blob* hash, size_t rsa_size);

  // Parses the input as a PEM encoded public string. And creates a
  // PayloadVerifier with that public key for signature verification.
  static std::unique_ptr<PayloadVerifier> CreateInstance(
      const std::string& pem_public_key);

  // Extracts the public keys from the certificates contained in the input
  // zip file. And creates a PayloadVerifier with these public keys.
  static std::unique_ptr<PayloadVerifier> CreateInstanceFromZipPath(
      const std::string& certificate_zip_path);

  // Interprets |signature_proto| as a protocol buffer containing the
  // |Signatures| message and decrypts each signature data using the stored
  // public key. Pads the 32 bytes |sha256_hash_data| to 256 or 512 bytes
  // according to the PKCS#1 v1.5 standard; and returns whether *any* of the
  // decrypted hashes matches the padded hash data. In case of any error parsing
  // the signatures, returns false.
  bool VerifySignature(const std::string& signature_proto,
                       const brillo::Blob& sha256_hash_data) const;

  // Verifies if |sig_data| is a raw signature of the hash |sha256_hash_data|.
  // If PayloadVerifier is using RSA as the public key, further puts the
  // decrypted data of |sig_data| into |decrypted_sig_data|.
  bool VerifyRawSignature(const brillo::Blob& sig_data,
                          const brillo::Blob& sha256_hash_data,
                          brillo::Blob* decrypted_sig_data) const;

 private:
  explicit PayloadVerifier(
      std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>>&&
          public_keys)
      : public_keys_(std::move(public_keys)) {}

  // Decrypts |sig_data| with the given |public_key| and populates
  // |out_hash_data| with the decoded raw hash. Returns true if successful,
  // false otherwise.
  bool GetRawHashFromSignature(const brillo::Blob& sig_data,
                               const EVP_PKEY* public_key,
                               brillo::Blob* out_hash_data) const;

  std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>> public_keys_;
};

}  // namespace chromeos_update_engine

#endif  // UPDATE_ENGINE_PAYLOAD_CONSUMER_PAYLOAD_VERIFIER_H_
