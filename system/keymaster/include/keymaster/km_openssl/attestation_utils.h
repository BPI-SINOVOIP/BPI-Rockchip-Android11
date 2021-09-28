/*
**
** Copyright 2017, The Android Open Source Project
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**     http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#ifndef KM_OPENSSL_ATTESTATION_UTILS_H_
#define KM_OPENSSL_ATTESTATION_UTILS_H_

#include <hardware/keymaster_defs.h>
#include <keymaster/android_keymaster_utils.h>

#include <openssl/x509v3.h>

#include "openssl_utils.h"

namespace keymaster {

class AuthorizationSet;
class AttestationRecordContext;
class AsymmetricKey;

// Generate attestation certificate base on the AsymmetricKey key and other parameters
// passed in.  In attest_params, we expect the challenge, active time and expiration
// time, and app id.
//
// The active time and expiration time are expected in milliseconds.
//
// Hardware and software enforced AuthorizationSet are expected to be built into the AsymmetricKey
// input. In hardware enforced AuthorizationSet, we expect hardware related tags such as
// TAG_IDENTITY_CREDENTIAL_KEY.
keymaster_error_t generate_attestation(const AsymmetricKey& key,
        const AuthorizationSet& attest_params, const keymaster_cert_chain_t& attestation_chain,
        const keymaster_key_blob_t& attestation_signing_key,
        const AttestationRecordContext& context, CertChainPtr* cert_chain_out);

// Generate attestation certificate based on the EVP key and other parameters
// passed in.  Note that due to sub sub sub call setup, there are 3 AuthorizationSet passed in,
// hardware, software, and general.  In attest_params, we expect the challenge,
// active time and expiration time, and app id.  In hw_enforced, we expect
// hardware related tags such as TAG_IDENTITY_CREDENTIAL_KEY.
//
// The active time and expiration time are expected in milliseconds since Jan 1,
// 1970.
keymaster_error_t generate_attestation_from_EVP(
    const EVP_PKEY* evp_key,                  // input
    const AuthorizationSet& sw_enforced,      // input
    const AuthorizationSet& hw_enforced,      // input
    const AuthorizationSet& attest_params,    // input. Sub function require app id to be set here.
    const AttestationRecordContext& context,  // input
    const uint keymaster_version,             // input
    const keymaster_cert_chain_t& attestation_chain,      // input
    const keymaster_key_blob_t& attestation_signing_key,  // input
    const char* key_subject_common_name,                  // input
    CertChainPtr* cert_chain_out);                        // Output.

} // namespace keymaster

#endif  // KM_OPENSSL_ATTESTATION_UTILS_H_
