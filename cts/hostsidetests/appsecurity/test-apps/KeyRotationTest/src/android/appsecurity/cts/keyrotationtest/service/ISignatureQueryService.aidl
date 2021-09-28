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

package android.appsecurity.cts.keyrotationtest.service;

/**
 * AIDL service definition intended to test PackageManager key rotation APIs.
 */
interface ISignatureQueryService {
    const int RESULT_SUCCESS = 0;
    const int RESULT_PACKAGE_NOT_FOUND = 1;
    const int RESULT_GET_SIGNATURES_NO_RESULTS = 2;
    const int RESULT_GET_SIGNATURES_MULTIPLE_SIGNATURES = 3;
    const int RESULT_GET_SIGNATURES_MISMATCH = 4;
    const int RESULT_GET_SIGNING_CERTIFICATES_NO_RESULTS = 5;
    const int RESULT_GET_SIGNING_CERTIFICATES_MULTIPLE_SIGNERS = 6;
    const int RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_NUMBER_OF_SIGNATURES = 7;
    const int RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_SIGNATURE = 8;
    const int RESULT_HAS_SIGNING_CERTIFICATE_BY_NAME_FAILED = 9;
    const int RESULT_HAS_SIGNING_CERTIFICATE_BY_UID_FAILED = 10;
    const int RESULT_NO_EXPECTED_SIGNATURES_PROVIDED = 11;
    const int RESULT_SHA256_MESSAGE_DIGEST_NOT_AVAILABLE = 12;
    const int RESULT_COMPANION_PACKAGE_NOT_FOUND = 13;
    const int RESULT_CHECK_SIGNATURES_BY_NAME_NO_MATCH = 14;
    const int RESULT_CHECK_SIGNATURES_BY_UID_NO_MATCH = 15;

    const String KEY_GET_SIGNATURES_RESULTS = "GET_SIGNATURES_RESULTS";
    const String KEY_GET_SIGNING_CERTIFICATES_RESULTS = "GET_SIGNING_CERTIFICATES_RESULTS";
    const String KEY_VERIFY_SIGNATURES_RESULT = "VERIFY_SIGNATURES_RESULT";

    /**
     * Queries PackageManager key rotation APIs and verifies the results against
     * the provided {@code expectedSignatureDigests}; the signature(s) of the
     * specified {@code companionPackageName} are also compared against the app
     * within which this service is running.
     */
    Bundle verifySignatures(in String[] expectedSignatureDigests, in String companionPackageName);
}

