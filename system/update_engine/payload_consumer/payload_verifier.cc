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

#include "update_engine/payload_consumer/payload_verifier.h"

#include <utility>
#include <vector>

#include <base/logging.h>
#include <openssl/pem.h>

#include "update_engine/common/constants.h"
#include "update_engine/common/hash_calculator.h"
#include "update_engine/common/utils.h"
#include "update_engine/payload_consumer/certificate_parser_interface.h"
#include "update_engine/update_metadata.pb.h"

using std::string;

namespace chromeos_update_engine {

namespace {

// The ASN.1 DigestInfo prefix for encoding SHA256 digest. The complete 51-byte
// DigestInfo consists of 19-byte SHA256_DIGEST_INFO_PREFIX and 32-byte SHA256
// digest.
//
// SEQUENCE(2+49) {
//   SEQUENCE(2+13) {
//     OBJECT(2+9) id-sha256
//     NULL(2+0)
//   }
//   OCTET STRING(2+32) <actual signature bytes...>
// }
const uint8_t kSHA256DigestInfoPrefix[] = {
    0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x01,
    0x65, 0x03, 0x04, 0x02, 0x01, 0x05, 0x00, 0x04, 0x20,
};

}  // namespace

std::unique_ptr<PayloadVerifier> PayloadVerifier::CreateInstance(
    const std::string& pem_public_key) {
  std::unique_ptr<BIO, decltype(&BIO_free)> bp(
      BIO_new_mem_buf(pem_public_key.data(), pem_public_key.size()), BIO_free);
  if (!bp) {
    LOG(ERROR) << "Failed to read " << pem_public_key << " into buffer.";
    return nullptr;
  }

  auto pub_key = std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>(
      PEM_read_bio_PUBKEY(bp.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
  if (!pub_key) {
    LOG(ERROR) << "Failed to parse the public key in: " << pem_public_key;
    return nullptr;
  }

  std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>> keys;
  keys.emplace_back(std::move(pub_key));
  return std::unique_ptr<PayloadVerifier>(new PayloadVerifier(std::move(keys)));
}

std::unique_ptr<PayloadVerifier> PayloadVerifier::CreateInstanceFromZipPath(
    const std::string& certificate_zip_path) {
  auto parser = CreateCertificateParser();
  if (!parser) {
    LOG(ERROR) << "Failed to create certificate parser from "
               << certificate_zip_path;
    return nullptr;
  }

  std::vector<std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)>> public_keys;
  if (!parser->ReadPublicKeysFromCertificates(certificate_zip_path,
                                              &public_keys) ||
      public_keys.empty()) {
    LOG(ERROR) << "Failed to parse public keys in: " << certificate_zip_path;
    return nullptr;
  }

  return std::unique_ptr<PayloadVerifier>(
      new PayloadVerifier(std::move(public_keys)));
}

bool PayloadVerifier::VerifySignature(
    const string& signature_proto, const brillo::Blob& sha256_hash_data) const {
  TEST_AND_RETURN_FALSE(!public_keys_.empty());

  Signatures signatures;
  LOG(INFO) << "signature blob size = " << signature_proto.size();
  TEST_AND_RETURN_FALSE(signatures.ParseFromString(signature_proto));

  if (!signatures.signatures_size()) {
    LOG(ERROR) << "No signatures stored in the blob.";
    return false;
  }

  std::vector<brillo::Blob> tested_hashes;
  // Tries every signature in the signature blob.
  for (int i = 0; i < signatures.signatures_size(); i++) {
    const Signatures::Signature& signature = signatures.signatures(i);
    brillo::Blob sig_data;
    if (signature.has_unpadded_signature_size()) {
      TEST_AND_RETURN_FALSE(signature.unpadded_signature_size() <=
                            signature.data().size());
      LOG(INFO) << "Truncating the signature to its unpadded size: "
                << signature.unpadded_signature_size() << ".";
      sig_data.assign(
          signature.data().begin(),
          signature.data().begin() + signature.unpadded_signature_size());
    } else {
      sig_data.assign(signature.data().begin(), signature.data().end());
    }

    brillo::Blob sig_hash_data;
    if (VerifyRawSignature(sig_data, sha256_hash_data, &sig_hash_data)) {
      LOG(INFO) << "Verified correct signature " << i + 1 << " out of "
                << signatures.signatures_size() << " signatures.";
      return true;
    }
    if (!sig_hash_data.empty()) {
      tested_hashes.push_back(sig_hash_data);
    }
  }
  LOG(ERROR) << "None of the " << signatures.signatures_size()
             << " signatures is correct. Expected hash before padding:";
  utils::HexDumpVector(sha256_hash_data);
  LOG(ERROR) << "But found RSA decrypted hashes:";
  for (const auto& sig_hash_data : tested_hashes) {
    utils::HexDumpVector(sig_hash_data);
  }
  return false;
}

bool PayloadVerifier::VerifyRawSignature(
    const brillo::Blob& sig_data,
    const brillo::Blob& sha256_hash_data,
    brillo::Blob* decrypted_sig_data) const {
  TEST_AND_RETURN_FALSE(!public_keys_.empty());

  for (const auto& public_key : public_keys_) {
    int key_type = EVP_PKEY_id(public_key.get());
    if (key_type == EVP_PKEY_RSA) {
      brillo::Blob sig_hash_data;
      if (!GetRawHashFromSignature(
              sig_data, public_key.get(), &sig_hash_data)) {
        LOG(WARNING)
            << "Failed to get the raw hash with RSA key. Trying other keys.";
        continue;
      }

      if (decrypted_sig_data != nullptr) {
        *decrypted_sig_data = sig_hash_data;
      }

      brillo::Blob padded_hash_data = sha256_hash_data;
      TEST_AND_RETURN_FALSE(
          PadRSASHA256Hash(&padded_hash_data, sig_hash_data.size()));

      if (padded_hash_data == sig_hash_data) {
        return true;
      }
    }

    if (key_type == EVP_PKEY_EC) {
      EC_KEY* ec_key = EVP_PKEY_get0_EC_KEY(public_key.get());
      TEST_AND_RETURN_FALSE(ec_key != nullptr);
      if (ECDSA_verify(0,
                       sha256_hash_data.data(),
                       sha256_hash_data.size(),
                       sig_data.data(),
                       sig_data.size(),
                       ec_key) == 1) {
        return true;
      }
    }

    LOG(ERROR) << "Unsupported key type " << key_type;
    return false;
  }
  LOG(INFO) << "Failed to verify the signature with " << public_keys_.size()
            << " keys.";
  return false;
}

bool PayloadVerifier::GetRawHashFromSignature(
    const brillo::Blob& sig_data,
    const EVP_PKEY* public_key,
    brillo::Blob* out_hash_data) const {
  // The code below executes the equivalent of:
  //
  // openssl rsautl -verify -pubin -inkey <(echo pem_public_key)
  //   -in |sig_data| -out |out_hash_data|
  RSA* rsa = EVP_PKEY_get0_RSA(public_key);

  TEST_AND_RETURN_FALSE(rsa != nullptr);
  unsigned int keysize = RSA_size(rsa);
  if (sig_data.size() > 2 * keysize) {
    LOG(ERROR) << "Signature size is too big for public key size.";
    return false;
  }

  // Decrypts the signature.
  brillo::Blob hash_data(keysize);
  int decrypt_size = RSA_public_decrypt(
      sig_data.size(), sig_data.data(), hash_data.data(), rsa, RSA_NO_PADDING);
  TEST_AND_RETURN_FALSE(decrypt_size > 0 &&
                        decrypt_size <= static_cast<int>(hash_data.size()));
  hash_data.resize(decrypt_size);
  out_hash_data->swap(hash_data);
  return true;
}

bool PayloadVerifier::PadRSASHA256Hash(brillo::Blob* hash, size_t rsa_size) {
  TEST_AND_RETURN_FALSE(hash->size() == kSHA256Size);
  TEST_AND_RETURN_FALSE(rsa_size == 256 || rsa_size == 512);

  // The following is a standard PKCS1-v1_5 padding for SHA256 signatures, as
  // defined in RFC3447 section 9.2. It is prepended to the actual signature
  // (32 bytes) to form a sequence of 256|512 bytes (2048|4096 bits) that is
  // amenable to RSA signing. The padded hash will look as follows:
  //
  //    0x00 0x01 0xff ... 0xff 0x00  ASN1HEADER  SHA256HASH
  //   |-----------205|461----------||----19----||----32----|
  size_t padding_string_size =
      rsa_size - hash->size() - sizeof(kSHA256DigestInfoPrefix) - 3;
  brillo::Blob padded_result = brillo::CombineBlobs({
      {0x00, 0x01},
      brillo::Blob(padding_string_size, 0xff),
      {0x00},
      brillo::Blob(kSHA256DigestInfoPrefix,
                   kSHA256DigestInfoPrefix + sizeof(kSHA256DigestInfoPrefix)),
      *hash,
  });

  *hash = std::move(padded_result);
  TEST_AND_RETURN_FALSE(hash->size() == rsa_size);
  return true;
}

}  // namespace chromeos_update_engine
