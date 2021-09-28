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

import android.app.Service;
import android.appsecurity.cts.keyrotationtest.utils.SignatureUtils;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.content.pm.SigningInfo;
import android.os.Bundle;
import android.os.IBinder;

/**
 * Provides a service that can be used within an Android application to verify the functionality of
 * key rotation PackageManager APIs.
 *
 * <p>This service is specifically designed to test key rotation APIs, so some of the functionality
 * is not generalized for multiple signers. For instance when testing {@link
 * PackageManager#getPackageInfo(String, int)} with {@link PackageManager#GET_SIGNATURES} a response
 * with multiple signatures is treated as an error. For the key rotation case this flag should only
 * result in the original signer in the lineage being returned; since multiple signers are not
 * supported with the V3 signature scheme this error is reported to the caller.
 */
public class SignatureQueryService extends Service {
    private SignatureQueryServiceImpl signatureQueryService;

    /** Service lifecycle callback method that is invoked when the service is first created. */
    @Override
    public void onCreate() {
        super.onCreate();
        signatureQueryService = new SignatureQueryServiceImpl(getPackageManager(),
                getPackageName());
    }

    /**
     * Service lifecycle callback method that is invoked when a client attempts to bind to this
     * service.
     */
    @Override
    public IBinder onBind(Intent intent) {
        return signatureQueryService;
    }

    /**
     * Implementation of the ISignatureQueryService to perform verification of the key rotation APIs
     * within the PackageManager.
     */
    static class SignatureQueryServiceImpl extends ISignatureQueryService.Stub {
        private final PackageManager packageManager;
        private final String packageName;

        /**
         * Protected constructor that accepts the {@code packageManager} and {@code packageName} to
         * be used for the key rotation API tests.
         */
        SignatureQueryServiceImpl(PackageManager packageManager, String packageName) {
            this.packageManager = packageManager;
            this.packageName = packageName;
        }

        /**
         * Performs the verification of the PackageManager key rotation APIs.
         *
         * <p>Verification tests are performed against the provided {@code
         * expectedSignatureDigests}, a
         * String array containing the hex representation of the expected signature(s) SHA-256
         * digest(s) for the package. Digests should be in the order of the lineage with the initial
         * signer at index 0. A non-null {@code companionPackageName} indicates that the specified
         * package is expected to have the same signing identity as the app within which this
         * service runs; the {@code PackageManager#checkSignature} APIs are used to verify this.
         *
         * <p>The following tests are performed with the provided digests and package name:
         *
         * <ul>
         *   <li>Verification of the original signer in the lineage returned from {@link
         *       PackageManager#getPackageInfo(String, int)} with the {@link
         *       PackageManager#GET_SIGNATURES} flag.
         *   <li>Verification of all of the signers in the lineage returned from {@link
         *       PackageManager#getPackageInfo(String, int)} with the {@link
         *       PackageManager#GET_SIGNING_CERTIFICATES} flag.
         *   <li>Verification of all of the signers in the lineage from {@link
         *       PackageManager#hasSigningCertificate(String, byte[], int)}; this method should
         *       return
         *       true for each of the provided digests when queried by package name.
         *   <li>Verification of all of the signers in the lineage from {@link
         *       PackageManager#hasSigningCertificate(int, byte[], int)}; this method should
         *       return true
         *       for each of the provided digests when queried by package uid.
         *   <li>Verification of the same signing identity between this app and the specified
         *   package
         *       from {@link PackageManager#checkSignatures(String, String)} and {@link
         *       PackageManager#checkSignatures(int, int)}.
         * </ul>
         *
         * If any failures are encountered during the verification the service will immediately
         * set the appropriate return code in the bundle and return.
         *
         * <p>A Bundle is returned containing the following elements under the specified keys:
         *
         * <ul>
         *   <li>{@link ISignatureQueryService#KEY_VERIFY_SIGNATURES_RESULT} - int representing the
         *       result of the test. For a list of return codes see {@link ISignatureQueryService}.
         *   <li>{@link ISignatureQueryService#KEY_GET_SIGNATURES_RESULTS} - a String array of
         *       the hex encoding of each of the signatures returned from the getPackageInfo
         *       invocation with the {@code PackageManager#GET_SIGNATURES} flag. If no signatures
         *       are returned from this query this value will be an empty array.
         *   <li>{@link ISignatureQueryService#KEY_GET_SIGNING_CERTIFICATES_RESULTS} - a String
         *       array of the hex encoding of each of the signatures returned from the
         *       getPackageInfo invocation with the {@code PackageManager#GET_SIGNING_CERTIIFCATES}
         *       flag. If no signature are returned from this query this value will be an empty
         *       array.
         * </ul>
         */
        @Override
        public Bundle verifySignatures(String[] expectedSignatureDigests,
                String companionPackageName) {
            Bundle responseBundle = new Bundle();
            if (expectedSignatureDigests == null || expectedSignatureDigests.length == 0) {
                responseBundle.putInt(KEY_VERIFY_SIGNATURES_RESULT,
                        RESULT_NO_EXPECTED_SIGNATURES_PROVIDED);
                return responseBundle;
            }

            // The SHA-256 MessageDigest is critical to all of the verification methods in this
            // service, so ensure it is available before proceeding.
            if (!SignatureUtils.computeSha256Digest(new byte[0]).isPresent()) {
                responseBundle.putInt(
                        KEY_VERIFY_SIGNATURES_RESULT, RESULT_SHA256_MESSAGE_DIGEST_NOT_AVAILABLE);
                return responseBundle;
            }

            int verificationResult = verifyGetSignatures(expectedSignatureDigests, responseBundle);
            if (verificationResult != RESULT_SUCCESS) {
                responseBundle.putInt(KEY_VERIFY_SIGNATURES_RESULT, verificationResult);
                return responseBundle;
            }

            verificationResult = verifyGetSigningCertificates(expectedSignatureDigests,
                    responseBundle);
            if (verificationResult != RESULT_SUCCESS) {
                responseBundle.putInt(KEY_VERIFY_SIGNATURES_RESULT, verificationResult);
                return responseBundle;
            }

            verificationResult = verifyHasSigningCertificate(expectedSignatureDigests);
            if (verificationResult != RESULT_SUCCESS) {
                responseBundle.putInt(KEY_VERIFY_SIGNATURES_RESULT, verificationResult);
                return responseBundle;
            }

            verificationResult = verifyCheckSignatures(companionPackageName);
            responseBundle.putInt(KEY_VERIFY_SIGNATURES_RESULT, verificationResult);
            return responseBundle;
        }

        /**
         * Verifies the {@code PackageManager#getPackageInfo(String, int)} API returns the expected
         * signature when invoked with the {@code PackageManager#GET_SIGNATURES} flag.
         *
         * <p>With this flag the API should return an array of {@link Signature} objects with a
         * single element, the first signer in the lineage. The signature(s) are added to the
         * provided {@code responseBundle}, then the signature is compared against the first element
         * in the specified {@code expectedSignatureDigests}.
         *
         * <p>The following return codes can be returned:
         *
         * <ul>
         *   <li>{@link ISignaturefervice#RESULT_SUCCESS} if a single signature is returned and
         *   matches the expected digest of the first signer in the provided lineage.
         *   <li>{@link ISignatureQueryService#RESULT_PACKAGE_NOT_FOUND} if the package name
         *   cannot be found by the PackageManager.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNATURES_NO_RESULTS} if no signatures
         *   are returned.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNATURES_MULTIPLE_SIGNATURES} if
         *   multiple signatures are returned.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNATURES_MISMATCH} if a single
         *   signature is returned but does not match the first signer in the provided lineage.
         * </ul>
         */
        private int verifyGetSignatures(String[] expectedSignatureDigests, Bundle responseBundle) {
            PackageInfo packageInfo;
            try {
                packageInfo = packageManager.getPackageInfo(packageName,
                        PackageManager.GET_SIGNATURES);
            } catch (PackageManager.NameNotFoundException e) {
                return RESULT_PACKAGE_NOT_FOUND;
            }
            Signature[] signatures = packageInfo.signatures;
            copySignaturesToBundle(signatures, responseBundle, KEY_GET_SIGNATURES_RESULTS);
            if (signatures == null || signatures.length == 0) {
                return RESULT_GET_SIGNATURES_NO_RESULTS;
            }
            // GET_SIGNATURES should only return the initial signing certificate in the linage.
            if (signatures.length > 1) {
                return RESULT_GET_SIGNATURES_MULTIPLE_SIGNATURES;
            }
            if (!verifySignatureDigest(signatures[0], expectedSignatureDigests[0])) {
                return RESULT_GET_SIGNATURES_MISMATCH;
            }
            return RESULT_SUCCESS;
        }

        /**
         * Verifies the {@code PackageManager#getPackageInfo(String, int)} API returns the expected
         * signatures when invoked with the {@code PackageManager#GET_SIGNING_CERTIFICATES} flag.
         *
         * <p>With this flag the API should return a {@link SigningInfo} object indicating whether
         * the app is signed with multiple signers along with an array of {@code Signature} objects
         * representing all of the signers in the lineage including the current signer. These
         * signatures are added to the provided {@code responseBundle}, then each is verified
         * against the specified {@code expectedSignatureDigests} to ensure that all of the expected
         * signatures are returned by the API.
         *
         * <p>The following return codes can be returned by this method:
         *
         * <ul>
         *   <li>{@link ISignatureQueryService#RESULT_SUCCESS} if the API returns that there is
         *       only a single signer and all of the expected signatures are in the linage.
         *   <li>{@link ISignatureQueryService#RESULT_PACKAGE_NOT_FOUND} if the package name
         *       cannot be found by the PackageManager.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNING_CERTIFICATES_NO_RESULTS} if no
         *       signatures are returned.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNING_CERTIFICATES_MULTIPLE_SIGNERS}
         *       if the response from the API indicates the app has been signed by multiple signers.
         *   <li>{@link
         *   ISignatureQueryService#RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_NUMBER_OF_SIGNATURES}
         *       if the returned number of signatures does not match the number of expected
         *       signatures.
         *   <li>{@link ISignatureQueryService#RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_SIGNATURE}
         *       if the results contains one or more signatures that are not in the array of
         *       expected signature digests.
         * </ul>
         */
        private int verifyGetSigningCertificates(
                String[] expectedSignatureDigests, Bundle responseBundle) {
            PackageInfo packageInfo;
            try {
                packageInfo =
                        packageManager.getPackageInfo(packageName,
                                PackageManager.GET_SIGNING_CERTIFICATES);
            } catch (PackageManager.NameNotFoundException e) {
                return RESULT_PACKAGE_NOT_FOUND;
            }
            SigningInfo signingInfo = packageInfo.signingInfo;
            Signature[] signatures =
                    signingInfo != null ? signingInfo.getSigningCertificateHistory() : null;
            copySignaturesToBundle(signatures, responseBundle,
                    KEY_GET_SIGNING_CERTIFICATES_RESULTS);
            if (signingInfo == null) {
                return RESULT_GET_SIGNING_CERTIFICATES_NO_RESULTS;
            }
            if (signingInfo.hasMultipleSigners()) {
                return RESULT_GET_SIGNING_CERTIFICATES_MULTIPLE_SIGNERS;
            }
            if (signatures.length != expectedSignatureDigests.length) {
                return RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_NUMBER_OF_SIGNATURES;
            }
            // The signing certificate history should be in the order of the lineage.
            for (int i = 0; i < signatures.length; i++) {
                if (!verifySignatureDigest(signatures[i], expectedSignatureDigests[i])) {
                    return RESULT_GET_SIGNING_CERTIFICATES_UNEXPECTED_SIGNATURE;
                }
            }
            return RESULT_SUCCESS;
        }

        /**
         * Verifies {@link PackageManager#hasSigningCertificate(String, byte[], int)} and {@link
         * PackageManager#hasSigningCertificate(int, byte[], int)} APIs indicate that this package
         * has the provided {@code expectedSignatureDigests} when querying by package name and uid.
         *
         * <p>The following return codes can be returned:
         *
         * <ul>
         *   <li>{@link ISignatureQueryService#RESULT_SUCCESS} if the API indicates each of the
         *       expected signing certificate digests is a signing certificate of the app.
         *   <li>{@link ISignatureQueryService#RESULT_PACKAGE_NOT_FOUND} if the package name
         *       cannot be found when querying for the app's uid.
         *   <li>{@link ISignatureQueryService#RESULT_HAS_SIGNING_CERTIFICATE_BY_NAME_FAILED} if
         *       the API returns one of the expected signing certificates is not found when querying
         *       by package name.
         *   <li>{@link ISignatureQueryService#RESULT_HAS_SIGNING_CERTIFICATE_BY_UID_FAILED} if
         *       the API returns one of the expected signing certificates is not found when querying
         *       by uid.
         * </ul>
         */
        private int verifyHasSigningCertificate(String[] expectedSignatureDigests) {
            int uid;
            try {
                ApplicationInfo applicationInfo = packageManager.getApplicationInfo(packageName, 0);
                uid = applicationInfo.uid;
            } catch (PackageManager.NameNotFoundException e) {
                return RESULT_PACKAGE_NOT_FOUND;
            }
            for (String expectedSignatureDigest : expectedSignatureDigests) {
                byte[] signature = new Signature(expectedSignatureDigest).toByteArray();
                if (!packageManager.hasSigningCertificate(
                        packageName, signature, PackageManager.CERT_INPUT_SHA256)) {
                    return RESULT_HAS_SIGNING_CERTIFICATE_BY_NAME_FAILED;
                }
                if (!packageManager.hasSigningCertificate(
                        uid, signature, PackageManager.CERT_INPUT_SHA256)) {
                    return RESULT_HAS_SIGNING_CERTIFICATE_BY_UID_FAILED;
                }
            }
            return RESULT_SUCCESS;
        }

        /**
         * Verifies {@link PackageManager#checkSignatures(String, String)} and {@link
         * PackageManager#checkSignatures(int, int)} APIs indicate that this package and the
         * provided {@code companionPackageName} have the same signing identity when querying by
         * package name and uid.
         *
         * <p>The following return codes can be returned:
         *
         * <ul>
         *   <li>{@link ISignatureQueryService#RESULT_SUCCESS} if the API indicates both this
         *       package and the companion package have the same signing identity.
         *   <li>{@link ISignatureQueryService#RESULT_COMPANION_PACKAGE_NOT_FOUND} if the companion
         *       package name cannot be found when querying for the companion app's uid.
         *   <li>{@link ISignatureQueryService#RESULT_CHECK_SIGNATURES_BY_NAME_NO_MATCH} if the API
         *       returns that this package and the companion package do not have the same signing
         *       identity when querying by name.
         *   <li>{@link ISignatureQueryService#RESULT_CHECK_SIGNATURES_BY_UID_NO_MATCH} if the API
         *       returns that this package and the companion package do not have the same signing
         *       identity when querying by uid.
         * </ul>
         *
         * <p>If a null companion package is specified the signing identity check is skipped, and
         * the method returns {@code ISignatureQueryService#RESULT_SUCCESS}.
         */
        private int verifyCheckSignatures(String companionPackageName) {
            if (companionPackageName == null) {
                return RESULT_SUCCESS;
            }
            int uid;
            int companionUid;
            try {
                ApplicationInfo applicationInfo = packageManager.getApplicationInfo(packageName, 0);
                uid = applicationInfo.uid;
            } catch (PackageManager.NameNotFoundException e) {
                return RESULT_PACKAGE_NOT_FOUND;
            }
            try {
                ApplicationInfo applicationInfo =
                        packageManager.getApplicationInfo(companionPackageName, 0);
                companionUid = applicationInfo.uid;
            } catch (PackageManager.NameNotFoundException e) {
                return RESULT_COMPANION_PACKAGE_NOT_FOUND;
            }
            if (packageManager.checkSignatures(packageName, companionPackageName)
                    != PackageManager.SIGNATURE_MATCH) {
                return RESULT_CHECK_SIGNATURES_BY_NAME_NO_MATCH;
            }
            if (packageManager.checkSignatures(uid, companionUid)
                    != PackageManager.SIGNATURE_MATCH) {
                return RESULT_CHECK_SIGNATURES_BY_UID_NO_MATCH;
            }
            return RESULT_SUCCESS;
        }

        /**
         * Returns whether the provided {@code signature} matches the specified {@code
         * expectedDigest}.
         */
        private static boolean verifySignatureDigest(Signature signature, String expectedDigest) {
            return SignatureUtils.computeSha256Digest(signature.toByteArray())
                    .get()
                    .equals(expectedDigest);
        }

        /**
         * Copies the provided {@code signatures} to the specified {@code responseBundle} using the
         * {@code bundleKey} as the key under which to store the signatures.
         *
         * <p>If the provided {@code Signature} array is null or empty then a String array of length
         * zero is written to the bundle.
         */
        private static void copySignaturesToBundle(
                Signature[] signatures, Bundle responseBundle, String bundleKey) {
            String[] bundleSignatures;
            if (signatures != null) {
                bundleSignatures = new String[signatures.length];
                for (int i = 0; i < signatures.length; i++) {
                    bundleSignatures[i] = signatures[i].toCharsString();
                }
            } else {
                bundleSignatures = new String[0];
            }
            responseBundle.putStringArray(bundleKey, bundleSignatures);
        }
    }
}

