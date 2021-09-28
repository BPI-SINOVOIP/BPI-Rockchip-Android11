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
package com.android.cts.deviceandprofileowner;

import static android.app.admin.DevicePolicyManager.ID_TYPE_BASE_INFO;
import static android.app.admin.DevicePolicyManager.ID_TYPE_IMEI;
import static android.app.admin.DevicePolicyManager.ID_TYPE_INDIVIDUAL_ATTESTATION;
import static android.app.admin.DevicePolicyManager.ID_TYPE_MEID;
import static android.app.admin.DevicePolicyManager.ID_TYPE_SERIAL;
import static android.keystore.cts.CertificateUtils.createCertificate;
import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import android.content.ComponentName;
import android.content.Context;
import android.content.pm.PackageManager;
import android.content.res.AssetManager;
import android.keystore.cts.Attestation;
import android.keystore.cts.AuthorizationList;
import android.net.Uri;
import android.os.Build;
import android.security.AttestedKeyPair;
import android.security.KeyChain;
import android.security.KeyChainAliasCallback;
import android.security.KeyChainException;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.security.keystore.StrongBoxUnavailableException;
import android.support.test.uiautomator.UiDevice;
import android.telephony.TelephonyManager;
import com.android.compatibility.common.util.FakeKeys.FAKE_RSA_1;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.GeneralSecurityException;
import java.security.KeyFactory;
import java.security.KeyPair;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.PublicKey;
import java.security.Signature;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.cert.CertificateParsingException;
import java.security.cert.X509Certificate;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.ArrayList;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;
import javax.security.auth.x500.X500Principal;

public class KeyManagementTest extends BaseDeviceAdminTest {
    private static final long KEYCHAIN_TIMEOUT_MINS = 6;

    private static class SupportedKeyAlgorithm {
        public final String keyAlgorithm;
        public final String signatureAlgorithm;
        public final String[] signaturePaddingSchemes;

        public SupportedKeyAlgorithm(
                String keyAlgorithm, String signatureAlgorithm,
                String[] signaturePaddingSchemes) {
            this.keyAlgorithm = keyAlgorithm;
            this.signatureAlgorithm = signatureAlgorithm;
            this.signaturePaddingSchemes = signaturePaddingSchemes;
        }
    }

    private final SupportedKeyAlgorithm[] SUPPORTED_KEY_ALGORITHMS = new SupportedKeyAlgorithm[] {
        new SupportedKeyAlgorithm(KeyProperties.KEY_ALGORITHM_RSA, "SHA256withRSA",
                new String[] {KeyProperties.SIGNATURE_PADDING_RSA_PSS,
                    KeyProperties.SIGNATURE_PADDING_RSA_PKCS1}),
        new SupportedKeyAlgorithm(KeyProperties.KEY_ALGORITHM_EC, "SHA256withECDSA", null)
    };

    private KeyManagementActivity mActivity;

    @Override
    public void setUp() throws Exception {
        super.setUp();
        final UiDevice device = UiDevice.getInstance(getInstrumentation());
        mActivity = launchActivity(getInstrumentation().getTargetContext().getPackageName(),
                KeyManagementActivity.class, null);
        device.waitForIdle();
    }

    @Override
    public void tearDown() throws Exception {
        mActivity.finish();
        super.tearDown();
    }

    public void testCanInstallAndRemoveValidRsaKeypair() throws Exception {
        final String alias = "com.android.test.valid-rsa-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);

        // Install keypair.
        assertThat(mDevicePolicyManager.installKeyPair(getWho(), privKey, cert, alias)).isTrue();
        try {
            // Request and retrieve using the alias.
            assertGranted(alias, false);
            assertThat(new KeyChainAliasFuture(alias).get()).isEqualTo(alias);
            assertGranted(alias, true);

            // Verify key is at least something like the one we put in.
            assertThat(KeyChain.getPrivateKey(mActivity, alias).getAlgorithm()).isEqualTo("RSA");
        } finally {
            // Delete regardless of whether the test succeeded.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
        // Verify alias is actually deleted.
        assertGranted(alias, false);
    }

    public void testCanInstallWithAutomaticAccess() throws Exception {
        final String grant = "com.android.test.autogrant-key-1";
        final String withhold = "com.android.test.nongrant-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);

        // Install keypairs.
        assertThat(
                mDevicePolicyManager.installKeyPair(
                        getWho(), privKey, new Certificate[] {cert}, grant, true))
                .isTrue();
        assertThat(
                mDevicePolicyManager.installKeyPair(
                        getWho(), privKey, new Certificate[] {cert}, withhold, false))
                .isTrue();
        try {
            // Verify only the requested key was actually granted.
            assertGranted(grant, true);
            assertGranted(withhold, false);

            // Verify the granted key is actually obtainable in PrivateKey form.
            assertThat(KeyChain.getPrivateKey(mActivity, grant).getAlgorithm()).isEqualTo("RSA");
        } finally {
            // Delete both keypairs.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), grant)).isTrue();
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), withhold)).isTrue();
        }
        // Verify they're actually gone.
        assertGranted(grant, false);
        assertGranted(withhold, false);
    }

    private List<Certificate> loadCertificateChain(String assetName) throws Exception {
        final Collection<Certificate> certs = loadCertificatesFromAsset(assetName);
        final ArrayList<Certificate> certChain = new ArrayList(certs);
        // Some sanity check on the cert chain
        assertThat(certs.size()).isGreaterThan(1);
        for (int i = 1; i < certChain.size(); i++) {
            certChain.get(i - 1).verify(certChain.get(i).getPublicKey());
        }
        return certChain;
    }

    public void testCanInstallCertChain() throws Exception {
        // Use assets/generate-client-cert-chain.sh to regenerate the client cert chain.
        final PrivateKey privKey = loadPrivateKeyFromAsset("user-cert-chain.key");
        final Certificate[] certChain = loadCertificateChain("user-cert-chain.crt")
                .toArray(new Certificate[0]);
        final String alias = "com.android.test.clientkeychain";

        // Install keypairs.
        assertThat(mDevicePolicyManager.installKeyPair(getWho(), privKey, certChain, alias, true))
                .isTrue();
        try {
            // Verify only the requested key was actually granted.
            assertGranted(alias, true);

            // Verify the granted key is actually obtainable in PrivateKey form.
            assertThat(KeyChain.getPrivateKey(mActivity, alias).getAlgorithm()).isEqualTo("RSA");

            // Verify the certificate chain is correct
            assertThat(KeyChain.getCertificateChain(mActivity, alias)).isEqualTo(certChain);
        } finally {
            // Delete both keypairs.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
        // Verify they're actually gone.
        assertGranted(alias, false);
    }

    public void testGrantsDoNotPersistBetweenInstallations() throws Exception {
        final String alias = "com.android.test.persistent-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);

        // Install keypair.
        assertThat(
                mDevicePolicyManager.installKeyPair(
                        getWho(), privKey, new Certificate[] {cert}, alias, true))
                .isTrue();
        try {
            assertGranted(alias, true);
        } finally {
            // Delete and verify.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
        assertGranted(alias, false);

        // Install again.
        assertThat(
                mDevicePolicyManager.installKeyPair(
                        getWho(), privKey, new Certificate[] {cert}, alias, false))
                .isTrue();
        try {
            assertGranted(alias, false);
        } finally {
            // Delete.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    public void testNullKeyParamsFailPredictably() throws Exception {
        final String alias = "com.android.test.null-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);
        try {
            mDevicePolicyManager.installKeyPair(getWho(), null, cert, alias);
            fail("Exception should have been thrown for null PrivateKey");
        } catch (NullPointerException expected) {
        }
        try {
            mDevicePolicyManager.installKeyPair(getWho(), privKey, null, alias);
            fail("Exception should have been thrown for null Certificate");
        } catch (NullPointerException expected) {
        }
    }

    public void testNullAdminComponentIsDenied() throws Exception {
        final String alias = "com.android.test.null-admin-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);
        try {
            mDevicePolicyManager.installKeyPair(null, privKey, cert, alias);
            fail("Exception should have been thrown for null ComponentName");
        } catch (SecurityException expected) {
        }
    }

    public void testNotUserSelectableAliasCanBeChosenViaPolicy() throws Exception {
        final String alias = "com.android.test.not-selectable-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);

        // Install keypair.
        assertThat(
                mDevicePolicyManager.installKeyPair(
                        getWho(), privKey, new Certificate[] {cert}, alias, 0))
                .isTrue();
        try {
            // Request and retrieve using the alias.
            assertGranted(alias, false);
            assertThat(new KeyChainAliasFuture(alias).get()).isEqualTo(alias);
            assertGranted(alias, true);
        } finally {
            // Delete regardless of whether the test succeeded.
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    byte[] signDataWithKey(String algoIdentifier, PrivateKey privateKey) throws Exception {
        byte[] data = new String("hello").getBytes();
        Signature sign = Signature.getInstance(algoIdentifier);
        sign.initSign(privateKey);
        sign.update(data);
        return sign.sign();
    }

    void verifySignature(String algoIdentifier, PublicKey publicKey, byte[] signature)
            throws Exception {
        byte[] data = new String("hello").getBytes();
        Signature verify = Signature.getInstance(algoIdentifier);
        verify.initVerify(publicKey);
        verify.update(data);
        assertThat(verify.verify(signature)).isTrue();
    }

    void verifySignatureOverData(String algoIdentifier, KeyPair keyPair) throws Exception {
        verifySignature(algoIdentifier, keyPair.getPublic(),
                signDataWithKey(algoIdentifier, keyPair.getPrivate()));
    }

    private KeyGenParameterSpec buildRsaKeySpec(String alias, boolean useStrongBox) {
        return new KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
            .setKeySize(2048)
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setSignaturePaddings(KeyProperties.SIGNATURE_PADDING_RSA_PSS,
                    KeyProperties.SIGNATURE_PADDING_RSA_PKCS1)
            .setIsStrongBoxBacked(useStrongBox)
            .build();
    }

    public void testCanGenerateRSAKeyPair() throws Exception {
        final String alias = "com.android.test.generated-rsa-1";
        try {
            AttestedKeyPair generated = mDevicePolicyManager.generateKeyPair(
                    getWho(), "RSA", buildRsaKeySpec(alias, false /* useStrongBox */), 0);
            assertThat(generated).isNotNull();
            verifySignatureOverData("SHA256withRSA", generated.getKeyPair());
        } finally {
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    public void testCanGenerateRSAKeyPairUsingStrongBox() throws Exception {
        final String alias = "com.android.test.generated-rsa-sb-1";
        try {
            AttestedKeyPair generated = mDevicePolicyManager.generateKeyPair(
                    getWho(), "RSA", buildRsaKeySpec(alias, true /* useStrongBox */), 0);
            verifySignatureOverData("SHA256withRSA", generated.getKeyPair());
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        } catch (StrongBoxUnavailableException expected) {
            assertThat(hasStrongBox()).isFalse();
        }
    }

    private KeyGenParameterSpec buildEcKeySpec(String alias, boolean useStrongBox) {
        return new KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
            .setDigests(KeyProperties.DIGEST_SHA256)
            .setIsStrongBoxBacked(useStrongBox)
            .build();
    }

    public void testCanGenerateECKeyPair() throws Exception {
        final String alias = "com.android.test.generated-ec-1";
        try {
            AttestedKeyPair generated = mDevicePolicyManager.generateKeyPair(
                    getWho(), "EC", buildEcKeySpec(alias, false /* useStrongBox */), 0);
            assertThat(generated).isNotNull();
            verifySignatureOverData("SHA256withECDSA", generated.getKeyPair());
        } finally {
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    public void testCanGenerateECKeyPairUsingStrongBox() throws Exception {
        final String alias = "com.android.test.generated-ec-sb-1";
        try {
            AttestedKeyPair generated = mDevicePolicyManager.generateKeyPair(
                    getWho(), "EC", buildEcKeySpec(alias, true /* useStrongBox */), 0);
            verifySignatureOverData("SHA256withECDSA", generated.getKeyPair());
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        } catch (StrongBoxUnavailableException expected) {
            assertThat(hasStrongBox()).isFalse();
        }
    }

    private void validateDeviceIdAttestationData(Certificate leaf,
            String expectedSerial, String expectedImei, String expectedMeid)
            throws CertificateParsingException {
        Attestation attestationRecord = new Attestation((X509Certificate) leaf);
        AuthorizationList teeAttestation = attestationRecord.getTeeEnforced();
        assertThat(teeAttestation).isNotNull();
        validateBrandAttestationRecord(teeAttestation);
        assertThat(teeAttestation.getDevice()).isEqualTo(Build.DEVICE);
        assertThat(teeAttestation.getProduct()).isEqualTo(Build.PRODUCT);
        assertThat(teeAttestation.getManufacturer()).isEqualTo(Build.MANUFACTURER);
        assertThat(teeAttestation.getModel()).isEqualTo(Build.MODEL);
        assertThat(teeAttestation.getSerialNumber()).isEqualTo(expectedSerial);
        assertThat(teeAttestation.getImei()).isEqualTo(expectedImei);
        assertThat(teeAttestation.getMeid()).isEqualTo(expectedMeid);
    }

    private void validateBrandAttestationRecord(AuthorizationList teeAttestation) {
        if (!Build.MODEL.equals("Pixel 2")) {
            assertThat(teeAttestation.getBrand()).isEqualTo(Build.BRAND);
        } else {
            assertThat(teeAttestation.getBrand()).isAnyOf(Build.BRAND, "htc");
        }
    }

    private void validateAttestationRecord(List<Certificate> attestation, byte[] providedChallenge)
            throws CertificateParsingException {
        assertThat(attestation).isNotNull();
        assertThat(attestation.size()).isGreaterThan(1);
        X509Certificate leaf = (X509Certificate) attestation.get(0);
        Attestation attestationRecord = new Attestation(leaf);
        assertThat(attestationRecord.getAttestationChallenge()).isEqualTo(providedChallenge);
    }

    private void validateSignatureChain(List<Certificate> chain, PublicKey leafKey)
            throws GeneralSecurityException {
        X509Certificate leaf = (X509Certificate) chain.get(0);
        PublicKey keyFromCert = leaf.getPublicKey();
        assertThat(keyFromCert.getEncoded()).isEqualTo(leafKey.getEncoded());
        // Check that the certificate chain is valid.
        for (int i = 1; i < chain.size(); i++) {
            X509Certificate intermediate = (X509Certificate) chain.get(i);
            PublicKey intermediateKey = intermediate.getPublicKey();
            leaf.verify(intermediateKey);
            leaf = intermediate;
        }

        // leaf is now the root, verify the root is self-signed.
        PublicKey rootKey = leaf.getPublicKey();
        leaf.verify(rootKey);
    }

    private boolean isDeviceIdAttestationSupported() {
        return mDevicePolicyManager.isDeviceIdAttestationSupported();
    }

    private boolean isDeviceIdAttestationRequested(int deviceIdAttestationFlags) {
        return deviceIdAttestationFlags != 0;
    }

    /**
     * Generates a key using DevicePolicyManager.generateKeyPair using the given key algorithm,
     * then test signing and verifying using generated key.
     * If {@code signaturePaddings} is not null, it is added to the key parameters specification.
     * Returns the Attestation leaf certificate.
     */
    private Certificate generateKeyAndCheckAttestation(
            String keyAlgorithm, String signatureAlgorithm,
            String[] signaturePaddings, boolean useStrongBox,
            int deviceIdAttestationFlags) throws Exception {
        final String alias =
                String.format("com.android.test.attested-%s", keyAlgorithm.toLowerCase());
        byte[] attestationChallenge = new byte[] {0x01, 0x02, 0x03};
        try {
            KeyGenParameterSpec.Builder specBuilder =  new KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAttestationChallenge(attestationChallenge)
                    .setIsStrongBoxBacked(useStrongBox);
            if (signaturePaddings != null) {
                specBuilder.setSignaturePaddings(signaturePaddings);
            }

            KeyGenParameterSpec spec = specBuilder.build();
            AttestedKeyPair generated = mDevicePolicyManager.generateKeyPair(
                    getWho(), keyAlgorithm, spec, deviceIdAttestationFlags);
            // If Device ID attestation was requested, check it succeeded if and only if device ID
            // attestation is supported.
            if (isDeviceIdAttestationRequested(deviceIdAttestationFlags)) {
                if (generated == null) {
                    assertWithMessage(
                            String.format(
                                    "Failed getting Device ID attestation for key "
                                    + "algorithm %s, with flags %s, despite device declaring support.",
                                    keyAlgorithm, deviceIdAttestationFlags))
                            .that(isDeviceIdAttestationSupported())
                            .isFalse();
                    return null;
                } else {
                    assertWithMessage(
                            String.format(
                                    "Device ID attestation for key "
                                    + "algorithm %s, with flags %d should not have succeeded.",
                                    keyAlgorithm, deviceIdAttestationFlags))
                            .that(isDeviceIdAttestationSupported())
                            .isTrue();
                }
            } else {
                assertWithMessage(
                        String.format(
                                "Key generation (of type %s) must succeed when Device ID "
                                + "attestation was not requested.",
                                keyAlgorithm))
                        .that(generated)
                        .isNotNull();
            }
            final KeyPair keyPair = generated.getKeyPair();
            verifySignatureOverData(signatureAlgorithm, keyPair);
            List<Certificate> attestation = generated.getAttestationRecord();
            validateAttestationRecord(attestation, attestationChallenge);
            validateSignatureChain(attestation, keyPair.getPublic());
            return attestation.get(0);
        } catch (UnsupportedOperationException ex) {
            assertWithMessage(
                    String.format(
                            "Unexpected failure while generating key %s with ID flags %d: %s",
                            keyAlgorithm, deviceIdAttestationFlags, ex))
                    .that(
                            isDeviceIdAttestationRequested(deviceIdAttestationFlags)
                            && !isDeviceIdAttestationSupported())
                    .isTrue();
            return null;
        } finally {
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    /**
     * Test key generation, including requesting Key Attestation, for all supported key
     * algorithms.
     */
    public void testCanGenerateKeyPairWithKeyAttestation() throws Exception {
        for (SupportedKeyAlgorithm supportedKey : SUPPORTED_KEY_ALGORITHMS) {
            assertThat(
                    generateKeyAndCheckAttestation(
                            supportedKey.keyAlgorithm,
                            supportedKey.signatureAlgorithm,
                            supportedKey.signaturePaddingSchemes,
                            false /* useStrongBox */,
                            0))
                    .isNotNull();
        }
    }

    public void testCanGenerateKeyPairWithKeyAttestationUsingStrongBox() throws Exception {
        try {
            for (SupportedKeyAlgorithm supportedKey : SUPPORTED_KEY_ALGORITHMS) {
                assertThat(
                        generateKeyAndCheckAttestation(
                                supportedKey.keyAlgorithm,
                                supportedKey.signatureAlgorithm,
                                supportedKey.signaturePaddingSchemes,
                                true /* useStrongBox */,
                                0))
                        .isNotNull();
            }
        } catch (StrongBoxUnavailableException expected) {
            assertThat(hasStrongBox()).isFalse();
        }
    }

    public void assertAllVariantsOfDeviceIdAttestation(boolean useStrongBox) throws Exception {
        List<Integer> modesToTest = new ArrayList<Integer>();
        String imei = null;
        String meid = null;
        // All devices must support at least basic device information attestation as well as serial
        // number attestation. Although attestation of unique device ids are only callable by device
        // owner.
        modesToTest.add(ID_TYPE_BASE_INFO);
        if (isDeviceOwner()) {
            modesToTest.add(ID_TYPE_SERIAL);
            // Get IMEI and MEID of the device.
            TelephonyManager telephonyService =
                    (TelephonyManager) mActivity.getSystemService(Context.TELEPHONY_SERVICE);
            assertWithMessage("Need to be able to read device identifiers")
                    .that(telephonyService)
                    .isNotNull();
            imei = telephonyService.getImei(0);
            meid = telephonyService.getMeid(0);
            // If the device has a valid IMEI it must support attestation for it.
            if (imei != null) {
                modesToTest.add(ID_TYPE_IMEI);
            }
            // Same for MEID
            if (meid != null) {
                modesToTest.add(ID_TYPE_MEID);
            }
        }
        int numCombinations = 1 << modesToTest.size();
        for (int i = 1; i < numCombinations; i++) {
            // Set the bits in devIdOpt to be passed into generateKeyPair according to the
            // current modes tested.
            int devIdOpt = 0;
            for (int j = 0; j < modesToTest.size(); j++) {
                if ((i & (1 << j)) != 0) {
                    devIdOpt = devIdOpt | modesToTest.get(j);
                }
            }
            try {
                // Now run the test with all supported key algorithms
                for (SupportedKeyAlgorithm supportedKey: SUPPORTED_KEY_ALGORITHMS) {
                    Certificate attestation = generateKeyAndCheckAttestation(
                            supportedKey.keyAlgorithm, supportedKey.signatureAlgorithm,
                            supportedKey.signaturePaddingSchemes, useStrongBox, devIdOpt);
                    // generateKeyAndCheckAttestation should return null if device ID attestation
                    // is not supported. Simply continue to test the next combination.
                    if (attestation == null && !isDeviceIdAttestationSupported()) {
                        continue;
                    }
                    assertWithMessage(
                            String.format(
                                    "Attestation should be valid for key %s with attestation modes %s",
                                    supportedKey.keyAlgorithm, devIdOpt))
                            .that(attestation)
                            .isNotNull();
                    // Set the expected values for serial, IMEI and MEID depending on whether
                    // attestation for them was requested.
                    String expectedSerial = null;
                    if ((devIdOpt & ID_TYPE_SERIAL) != 0) {
                        expectedSerial = Build.getSerial();
                    }
                    String expectedImei = null;
                    if ((devIdOpt & ID_TYPE_IMEI) != 0) {
                        expectedImei = imei;
                    }
                    String expectedMeid = null;
                    if ((devIdOpt & ID_TYPE_MEID) != 0) {
                        expectedMeid = meid;
                    }
                    validateDeviceIdAttestationData(attestation, expectedSerial, expectedImei,
                            expectedMeid);
                }
            } catch (UnsupportedOperationException expected) {
                // Make sure the test only fails if the device is not meant to support Device
                // ID attestation.
                assertThat(isDeviceIdAttestationSupported()).isFalse();
            } catch (StrongBoxUnavailableException expected) {
                // This exception must only be thrown if StrongBox attestation was requested.
                assertThat(useStrongBox && !hasStrongBox()).isTrue();
            }
        }
    }

    public void testAllVariationsOfDeviceIdAttestation() throws Exception {
        assertAllVariantsOfDeviceIdAttestation(false /* useStrongBox */);
    }

    public void testAllVariationsOfDeviceIdAttestationUsingStrongBox() throws Exception {
        assertAllVariantsOfDeviceIdAttestation(true /* useStrongBox */);
    }

    public void testProfileOwnerCannotAttestDeviceUniqueIds() throws Exception {
        if (isDeviceOwner()) {
            return;
        }
        int[] forbiddenModes = new int[] {ID_TYPE_SERIAL, ID_TYPE_IMEI, ID_TYPE_MEID};
        for (int i = 0; i < forbiddenModes.length; i++) {
            try {
                for (SupportedKeyAlgorithm supportedKey : SUPPORTED_KEY_ALGORITHMS) {
                    generateKeyAndCheckAttestation(
                            supportedKey.keyAlgorithm,
                            supportedKey.signatureAlgorithm,
                            supportedKey.signaturePaddingSchemes,
                            false /* useStrongBox */,
                            forbiddenModes[i]);
                    fail("Attestation of device UID (" + forbiddenModes[i] + ") should not be "
                            + "possible from profile owner");
                }
            } catch (SecurityException e) {
                assertThat(e.getMessage()).contains(
                        "Profile Owner is not allowed to access Device IDs.");
            }
        }
    }

    public void testUniqueDeviceAttestationUsingDifferentAttestationCert() throws Exception {
        // This test is only applicable in modes where Device ID attestation can be performed
        // _and_ the device has StrongBox, which is provisioned with individual attestation
        // certificates.
        // The functionality tested should equally work for when the Profile Owner can perform
        // Device ID attestation, but since the underlying StrongBox implementation cannot
        // differentiate between PO and DO modes, for simplicity, it is only tested in DO mode.
        if (!isDeviceOwner() || !hasStrongBox() || !isUniqueDeviceAttestationSupported()) {
            return;
        }
        final String no_id_alias = "com.android.test.key_attested";
        final String dev_unique_alias = "com.android.test.individual_dev_attested";

        byte[] attestationChallenge = new byte[] {0x01, 0x02, 0x03};
        try {
            KeyGenParameterSpec specKeyAttOnly = new KeyGenParameterSpec.Builder(
                    no_id_alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAttestationChallenge(attestationChallenge)
                    .setIsStrongBoxBacked(true)
                    .build();

            AttestedKeyPair attestedKeyPair = mDevicePolicyManager.generateKeyPair(
                    getWho(), KeyProperties.KEY_ALGORITHM_EC, specKeyAttOnly,
                    0 /* device id attestation flags */);
            assertWithMessage(
                    String.format("Failed generating a key with attestation in StrongBox."))
                    .that(attestedKeyPair)
                    .isNotNull();

            KeyGenParameterSpec specIndividualAtt = new KeyGenParameterSpec.Builder(
                    dev_unique_alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAttestationChallenge(attestationChallenge)
                    .setIsStrongBoxBacked(true)
                    .build();

            AttestedKeyPair individuallyAttestedPair = mDevicePolicyManager.generateKeyPair(
                    getWho(), KeyProperties.KEY_ALGORITHM_EC, specIndividualAtt,
                    ID_TYPE_INDIVIDUAL_ATTESTATION /* device id attestation flags */);
            assertWithMessage(
                    String.format("Failed generating a key for unique attestation in StrongBox."))
                    .that(individuallyAttestedPair)
                    .isNotNull();

            X509Certificate keyAttestationIntermediate = (X509Certificate)
                    attestedKeyPair.getAttestationRecord().get(1);
            X509Certificate devUniqueAttestationIntermediate = (X509Certificate)
                    individuallyAttestedPair.getAttestationRecord().get(1);
            assertWithMessage("Device unique attestation intermediate certificate"
                    + " should be different to the key attestation certificate.")
                    .that(devUniqueAttestationIntermediate.getEncoded())
                    .isNotEqualTo(keyAttestationIntermediate.getEncoded());
        } finally {
            mDevicePolicyManager.removeKeyPair(getWho(), no_id_alias);
            mDevicePolicyManager.removeKeyPair(getWho(), dev_unique_alias);
        }
    }

    public void testUniqueDeviceAttestationFailsWhenUnsupported() {
        if (!isDeviceOwner() || !hasStrongBox()) {
            return;
        }

        if (isUniqueDeviceAttestationSupported()) {
            // testUniqueDeviceAttestationUsingDifferentAttestationCert is the positive test case.
            return;
        }

        byte[] attestationChallenge = new byte[] {0x01, 0x02, 0x03};
        final String someAlias = "com.android.test.should_not_exist";
        try {
            KeyGenParameterSpec specIndividualAtt = new KeyGenParameterSpec.Builder(
                    someAlias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .setAttestationChallenge(attestationChallenge)
                    .setIsStrongBoxBacked(true)
                    .build();

            AttestedKeyPair individuallyAttestedPair = mDevicePolicyManager.generateKeyPair(
                    getWho(), KeyProperties.KEY_ALGORITHM_EC, specIndividualAtt,
                    ID_TYPE_INDIVIDUAL_ATTESTATION /* device id attestation flags */);
            fail("When unique attestation is not supported, key generation should fail.");
        }catch (UnsupportedOperationException expected) {
        } finally {
            mDevicePolicyManager.removeKeyPair(getWho(), someAlias);
        }
    }


        public void testCanSetKeyPairCert() throws Exception {
        final String alias = "com.android.test.set-ec-1";
        try {
            KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .build();

            AttestedKeyPair generated =
                    mDevicePolicyManager.generateKeyPair(getWho(), "EC", spec, 0);
            assertThat(generated).isNotNull();
            // Create a self-signed cert to go with it.
            X500Principal issuer = new X500Principal("CN=SelfSigned, O=Android, C=US");
            X500Principal subject = new X500Principal("CN=Subject, O=Android, C=US");
            X509Certificate cert = createCertificate(generated.getKeyPair(), subject, issuer);
            // Set the certificate chain
            List<Certificate> certs = new ArrayList<Certificate>();
            certs.add(cert);
            mDevicePolicyManager.setKeyPairCertificate(getWho(), alias, certs, true);
            // Make sure that the alias can now be obtained.
            assertThat(new KeyChainAliasFuture(alias).get()).isEqualTo(alias);
            // And can be retrieved from KeyChain
            X509Certificate[] fetchedCerts = KeyChain.getCertificateChain(mActivity, alias);
            assertThat(fetchedCerts.length).isEqualTo(certs.size());
            assertThat(fetchedCerts[0].getEncoded()).isEqualTo(certs.get(0).getEncoded());
        } finally {
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    public void testCanSetKeyPairCertChain() throws Exception {
        final String alias = "com.android.test.set-ec-2";
        try {
            KeyGenParameterSpec spec = new KeyGenParameterSpec.Builder(
                    alias,
                    KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                    .setDigests(KeyProperties.DIGEST_SHA256)
                    .build();

            AttestedKeyPair generated =
                    mDevicePolicyManager.generateKeyPair(getWho(), "EC", spec, 0);
            assertThat(generated).isNotNull();
            List<Certificate> chain = loadCertificateChain("user-cert-chain.crt");
            mDevicePolicyManager.setKeyPairCertificate(getWho(), alias, chain, true);
            // Make sure that the alias can now be obtained.
            assertThat(new KeyChainAliasFuture(alias).get()).isEqualTo(alias);
            // And can be retrieved from KeyChain
            X509Certificate[] fetchedCerts = KeyChain.getCertificateChain(mActivity, alias);
            assertThat(fetchedCerts.length).isEqualTo(chain.size());
            for (int i = 0; i < chain.size(); i++) {
                assertThat(fetchedCerts[i].getEncoded()).isEqualTo(chain.get(i).getEncoded());
            }
        } finally {
            assertThat(mDevicePolicyManager.removeKeyPair(getWho(), alias)).isTrue();
        }
    }

    private void assertGranted(String alias, boolean expected)
            throws InterruptedException, KeyChainException {
        boolean granted = (KeyChain.getPrivateKey(mActivity, alias) != null);
        assertWithMessage("Grant for alias: \"" + alias + "\"").that(granted).isEqualTo(expected);
    }

    private static PrivateKey getPrivateKey(final byte[] key, String type)
            throws NoSuchAlgorithmException, InvalidKeySpecException {
        return KeyFactory.getInstance(type).generatePrivate(
                new PKCS8EncodedKeySpec(key));
    }

    private static Certificate getCertificate(byte[] cert) throws CertificateException {
        return CertificateFactory.getInstance("X.509").generateCertificate(
                new ByteArrayInputStream(cert));
    }

    private Collection<Certificate> loadCertificatesFromAsset(String assetName) {
        try {
            final CertificateFactory certFactory = CertificateFactory.getInstance("X.509");
            AssetManager am = mActivity.getAssets();
            InputStream is = am.open(assetName);
            return (Collection<Certificate>) certFactory.generateCertificates(is);
        } catch (IOException | CertificateException e) {
            e.printStackTrace();
        }
        return null;
    }

    private PrivateKey loadPrivateKeyFromAsset(String assetName) {
        try {
            AssetManager am = mActivity.getAssets();
            InputStream is = am.open(assetName);
            ByteArrayOutputStream output = new ByteArrayOutputStream();
            int length;
            byte[] buffer = new byte[4096];
            while ((length = is.read(buffer, 0, buffer.length)) != -1) {
              output.write(buffer, 0, length);
            }
            return getPrivateKey(output.toByteArray(), "RSA");
        } catch (IOException | NoSuchAlgorithmException | InvalidKeySpecException e) {
            e.printStackTrace();
        }
        return null;
    }

    private class KeyChainAliasFuture implements KeyChainAliasCallback {
        private final CountDownLatch mLatch = new CountDownLatch(1);
        private String mChosenAlias = null;

        @Override
        public void alias(final String chosenAlias) {
            mChosenAlias = chosenAlias;
            mLatch.countDown();
        }

        public KeyChainAliasFuture(String alias)
                throws UnsupportedEncodingException {
            /* Pass the alias as a GET to an imaginary server instead of explicitly asking for it,
             * to make sure the DPC actually has to do some work to grant the cert.
             */
            final Uri uri =
                    Uri.parse("https://example.org/?alias=" + URLEncoder.encode(alias, "UTF-8"));
            KeyChain.choosePrivateKeyAlias(mActivity, this,
                    null /* keyTypes */, null /* issuers */, uri, null /* alias */);
        }

        public String get() throws InterruptedException {
            assertWithMessage("Chooser timeout")
                    .that(mLatch.await(KEYCHAIN_TIMEOUT_MINS, TimeUnit.MINUTES))
                    .isTrue();
            return mChosenAlias;
        }
    }

    protected ComponentName getWho() {
        return ADMIN_RECEIVER_COMPONENT;
    }

    boolean hasStrongBox() {
        return mActivity.getPackageManager()
            .hasSystemFeature(PackageManager.FEATURE_STRONGBOX_KEYSTORE);
    }

    boolean isUniqueDeviceAttestationSupported() {
        return mDevicePolicyManager.isUniqueDeviceAttestationSupported();
    }
}
