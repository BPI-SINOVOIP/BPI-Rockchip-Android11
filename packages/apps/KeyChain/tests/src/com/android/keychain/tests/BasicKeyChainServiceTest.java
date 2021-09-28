/*
 * Copyright (C) 2019 The Android Open Source Project
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
package com.android.keychain.tests;

import static android.os.Process.WIFI_UID;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.ConditionVariable;
import android.os.IBinder;
import android.os.Process;
import android.os.RemoteException;
import android.platform.test.annotations.LargeTest;
import android.security.Credentials;
import android.security.IKeyChainService;
import android.security.KeyChain;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.security.keystore.ParcelableKeyGenParameterSpec;
import android.util.Log;
import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;
import com.android.keychain.tests.support.IKeyChainServiceTestSupport;
import java.io.IOException;
import java.security.KeyStore.PrivateKeyEntry;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import libcore.java.security.TestKeyStore;
import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@LargeTest
@RunWith(AndroidJUnit4.class)
public class BasicKeyChainServiceTest {
    private static final String TAG = "BasicKeyChainServiceTest";
    private static final String ALIAS_1 = "client";
    private static final String ALIAS_IMPORTED = "imported";
    private static final String ALIAS_GENERATED = "generated";
    public static final byte[] DUMMY_CHALLENGE = {'a', 'b', 'c'};
    private static final String ALIAS_NON_EXISTING = "nonexisting";

    private Context mContext;

    private final ConditionVariable mSupportServiceAvailable = new ConditionVariable(false);
    private IKeyChainServiceTestSupport mTestSupportService;
    private boolean mIsSupportServiceBound;

    private ServiceConnection mSupportConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    Log.d(TAG, "test support service connected!");
                    mTestSupportService = IKeyChainServiceTestSupport.Stub.asInterface(service);
                    mSupportServiceAvailable.open();
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    mSupportServiceAvailable.close();
                    mTestSupportService = null;
                }
            };

    private final ConditionVariable mKeyChainAvailable = new ConditionVariable(false);
    private IKeyChainService mKeyChainService;
    private boolean mIsKeyChainServiceBound;

    private ServiceConnection mServiceConnection =
            new ServiceConnection() {
                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    Log.d(TAG, "KeyChain service connected!");
                    mKeyChainService = IKeyChainService.Stub.asInterface(service);
                    mKeyChainAvailable.open();
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    mKeyChainAvailable.close();
                    mKeyChainService = null;
                }
            };

    @Before
    public void setUp() {
        mContext = InstrumentationRegistry.getTargetContext();
        bindTestSupportService();
        assertThat(mIsSupportServiceBound).isTrue();
        bindKeyChainService();
        assertThat(mIsKeyChainServiceBound).isTrue();

        waitForSupportService();
        waitForKeyChainService();
    }

    @After
    public void tearDown() {
        // Clean up keys that might have been left over
        try {
            removeKeyPair(ALIAS_IMPORTED);
            removeKeyPair(ALIAS_GENERATED);
            removeKeyPair(ALIAS_1);
        } catch (RemoteException e) {
            // Nothing to do here but warn that clean-up was not successful.
            Log.w(TAG, "Failed cleaning up installed keys", e);
        }
        unbindTestSupportService();
        assertThat(mIsSupportServiceBound).isFalse();
        unbindKeyChainService();
        assertThat(mIsKeyChainServiceBound).isFalse();
    }

    @Test
    public void testCanAccessKeyAfterGettingGrant()
            throws RemoteException, IOException, CertificateException {
        Log.d(TAG, "Testing access to imported key after getting grant.");
        assertThat(mTestSupportService.keystoreReset()).isTrue();
        installFirstKey();
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_1)).isNull();
        mTestSupportService.grantAppPermission(Process.myUid(), ALIAS_1);
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_1)).isNotNull();
    }

    @Test
    public void testInstallAndRemoveKeyPair()
            throws RemoteException, IOException, CertificateException {
        Log.d(TAG, "Testing importing key.");
        assertThat(mTestSupportService.keystoreReset()).isTrue();
        // No key installed, all should fail.
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_IMPORTED)).isNull();
        assertThat(mKeyChainService.getCertificate(ALIAS_IMPORTED)).isNull();
        assertThat(mKeyChainService.getCaCertificates(ALIAS_IMPORTED)).isNull();

        PrivateKeyEntry privateKeyEntry =
                TestKeyStore.getClientCertificate().getPrivateKey("RSA", "RSA");
        assertThat(mTestSupportService.installKeyPair(privateKeyEntry.getPrivateKey().getEncoded(),
                    privateKeyEntry.getCertificate().getEncoded(),
                    Credentials.convertToPem(privateKeyEntry.getCertificateChain()),
                    ALIAS_IMPORTED)).isTrue();

        // No grant, all should still fail.
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_IMPORTED)).isNull();
        assertThat(mKeyChainService.getCertificate(ALIAS_IMPORTED)).isNull();
        assertThat(mKeyChainService.getCaCertificates(ALIAS_IMPORTED)).isNull();
        // Grant access
        mTestSupportService.grantAppPermission(Process.myUid(), ALIAS_IMPORTED);
        // Has grant, all should succeed.
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_IMPORTED)).isNotNull();
        assertThat(mKeyChainService.getCertificate(ALIAS_IMPORTED)).isNotNull();
        assertThat(mKeyChainService.getCaCertificates(ALIAS_IMPORTED)).isNotNull();
        // Finally, test removal.
        assertThat(mTestSupportService.removeKeyPair(ALIAS_IMPORTED)).isTrue();
    }

    @Test
    public void testUserSelectability() throws RemoteException, IOException, CertificateException {
        Log.d(TAG, "Testing user-selectability of a key.");
        assertThat(mTestSupportService.keystoreReset()).isTrue();
        PrivateKeyEntry privateKeyEntry =
                TestKeyStore.getClientCertificate().getPrivateKey("RSA", "RSA");
        assertThat(mTestSupportService.installKeyPair(privateKeyEntry.getPrivateKey().getEncoded(),
                privateKeyEntry.getCertificate().getEncoded(),
                Credentials.convertToPem(privateKeyEntry.getCertificateChain()),
                ALIAS_IMPORTED)).isTrue();

        assertThat(mKeyChainService.isUserSelectable(ALIAS_IMPORTED)).isFalse();
        mTestSupportService.setUserSelectable(ALIAS_IMPORTED, true);
        assertThat(mKeyChainService.isUserSelectable(ALIAS_IMPORTED)).isTrue();
        mTestSupportService.setUserSelectable(ALIAS_IMPORTED, false);
        assertThat(mKeyChainService.isUserSelectable(ALIAS_IMPORTED)).isFalse();

        // Remove key
        assertThat(mTestSupportService.removeKeyPair(ALIAS_IMPORTED)).isTrue();
    }

    @Test
    public void testGenerateKeyPairErrorsOnBadUid() throws RemoteException {
        KeyGenParameterSpec specBadUid =
                new KeyGenParameterSpec.Builder(buildRsaKeySpec(ALIAS_GENERATED))
                .setUid(WIFI_UID)
                .build();
        ParcelableKeyGenParameterSpec parcelableSpec =
                new ParcelableKeyGenParameterSpec(specBadUid);
        assertThat(mTestSupportService.generateKeyPair("RSA", parcelableSpec)).isEqualTo(
                KeyChain.KEY_GEN_MISSING_ALIAS);
    }

    @Test
    public void testGenerateKeyPairErrorsOnSuperflousAttestationChallenge() throws RemoteException {
        KeyGenParameterSpec specWithChallenge =
                new KeyGenParameterSpec.Builder(buildRsaKeySpec(ALIAS_GENERATED))
                        .setAttestationChallenge(DUMMY_CHALLENGE)
                        .build();
        ParcelableKeyGenParameterSpec parcelableSpec =
                new ParcelableKeyGenParameterSpec(specWithChallenge);
        assertThat(mTestSupportService.generateKeyPair("RSA", parcelableSpec)).isEqualTo(
                KeyChain.KEY_GEN_SUPERFLUOUS_ATTESTATION_CHALLENGE);
    }

    @Test
    public void testGenerateKeyPairErrorsOnInvalidAlgorithm() throws RemoteException {
        ParcelableKeyGenParameterSpec parcelableSpec = new ParcelableKeyGenParameterSpec(
                buildRsaKeySpec(ALIAS_GENERATED));
        assertThat(mTestSupportService.generateKeyPair("BADBAD", parcelableSpec)).isEqualTo(
                KeyChain.KEY_GEN_NO_SUCH_ALGORITHM);
    }

    @Test
    public void testGenerateKeyPairErrorsOnInvalidAlgorithmParameters() throws RemoteException {
        ParcelableKeyGenParameterSpec parcelableSpec = new ParcelableKeyGenParameterSpec(
                buildRsaKeySpec(ALIAS_GENERATED));
        // RSA key parameters do not make sense for Elliptic Curve
        assertThat(mTestSupportService.generateKeyPair("EC", parcelableSpec)).isEqualTo(
                KeyChain.KEY_GEN_INVALID_ALGORITHM_PARAMETERS);
    }

    @Test
    public void testGenerateKeyPairSucceeds() throws RemoteException {
        generateRsaKey(ALIAS_GENERATED);
        // Test that there are no grants by default
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_GENERATED)).isNull();
        // And is not user-selectable by default
        assertThat(mKeyChainService.isUserSelectable(ALIAS_GENERATED)).isFalse();
        // But after granting access, it can be used.
        mTestSupportService.grantAppPermission(Process.myUid(), ALIAS_GENERATED);
        assertThat(mKeyChainService.requestPrivateKey(ALIAS_GENERATED)).isNotNull();
    }

    @Test
    public void testAttestKeyFailsOnMissingChallenge() throws RemoteException {
        generateRsaKey(ALIAS_GENERATED);
        assertThat(mTestSupportService.attestKey(ALIAS_GENERATED, null, new int[]{}
                )).isEqualTo(KeyChain.KEY_ATTESTATION_MISSING_CHALLENGE);
    }

    @Test
    public void testAttestKeyFailsOnNonExistentKey() throws RemoteException {
        assertThat(mTestSupportService.attestKey(ALIAS_NON_EXISTING, DUMMY_CHALLENGE, new int[]{}
                )).isEqualTo(KeyChain.KEY_ATTESTATION_FAILURE);
    }

    @Test
    public void testAttestKeySucceedsOnGeneratedKey() throws RemoteException {
        generateRsaKey(ALIAS_GENERATED);
        assertThat(mTestSupportService.attestKey(ALIAS_GENERATED, DUMMY_CHALLENGE,
                null)).isEqualTo(KeyChain.KEY_ATTESTATION_SUCCESS);
    }

    @Test
    public void testSetKeyPairCertificate() throws RemoteException {
        generateRsaKey(ALIAS_GENERATED);
        final byte[] userCert = new byte[] {'a', 'b', 'c'};
        final byte[] certChain = new byte[] {'d', 'e', 'f'};

        assertThat(mTestSupportService.setKeyPairCertificate(ALIAS_GENERATED, userCert,
                certChain)).isTrue();
        mTestSupportService.grantAppPermission(Process.myUid(), ALIAS_GENERATED);

        assertThat(mKeyChainService.getCertificate(ALIAS_GENERATED)).isEqualTo(userCert);
        assertThat(mKeyChainService.getCaCertificates(ALIAS_GENERATED)).isEqualTo(certChain);

        final byte[] newUserCert = new byte[] {'x', 'y', 'z'};
        assertThat(mTestSupportService.setKeyPairCertificate(ALIAS_GENERATED, newUserCert,
                null)).isTrue();
        assertThat(mKeyChainService.getCertificate(ALIAS_GENERATED)).isEqualTo(newUserCert);
        assertThat(mKeyChainService.getCaCertificates(ALIAS_GENERATED)).isNull();
    }

    @Test
    public void testInstallKeyPairErrorOnAliasSelectionDeniedKey() throws RemoteException,
            IOException, CertificateException {
        PrivateKeyEntry privateKeyEntry =
                TestKeyStore.getClientCertificate().getPrivateKey("RSA", "RSA");
        assertThrows(IllegalArgumentException.class, () -> {
                mTestSupportService.installKeyPair(
                        privateKeyEntry.getPrivateKey().getEncoded(),
                        privateKeyEntry.getCertificate().getEncoded(),
                        Credentials.convertToPem(privateKeyEntry.getCertificateChain()),
                        KeyChain.KEY_ALIAS_SELECTION_DENIED);
        });
    }

    @Test
    public void testGenerateKeyPairErrorOnAliasSelectionDeniedKey() throws RemoteException {
        ParcelableKeyGenParameterSpec parcelableSpec =
                new ParcelableKeyGenParameterSpec(buildRsaKeySpec(
                        KeyChain.KEY_ALIAS_SELECTION_DENIED));
        assertThrows(IllegalArgumentException.class, () -> {
                mTestSupportService.generateKeyPair("RSA", parcelableSpec);
        });
    }

    void generateRsaKey(String alias) throws RemoteException {
        ParcelableKeyGenParameterSpec parcelableSpec = new ParcelableKeyGenParameterSpec(
                buildRsaKeySpec(alias));
        assertThat(mTestSupportService.generateKeyPair("RSA", parcelableSpec)).isEqualTo(
                KeyChain.KEY_GEN_SUCCESS);
    }

    void removeKeyPair(String alias) throws RemoteException {
        assertThat(mTestSupportService.removeKeyPair(alias)).isTrue();
    }

    void bindTestSupportService() {
        Intent serviceIntent = new Intent(mContext, IKeyChainServiceTestSupport.class);
        serviceIntent.setComponent(
                new ComponentName(
                        "com.android.keychain.tests.support",
                        "com.android.keychain.tests.support.KeyChainServiceTestSupport"));
        Log.d(TAG, String.format("Binding intent: %s", serviceIntent));
        mIsSupportServiceBound =
                mContext.bindService(serviceIntent, mSupportConnection, Context.BIND_AUTO_CREATE);
        Log.d(TAG, String.format("Support service binding result: %b", mIsSupportServiceBound));
    }

    void unbindTestSupportService() {
        if (mIsSupportServiceBound) {
            mContext.unbindService(mSupportConnection);
            mIsSupportServiceBound = false;
        }
    }

    void bindKeyChainService() {
        Context appContext = mContext.getApplicationContext();
        Intent intent = new Intent(IKeyChainService.class.getName());
        ComponentName comp = intent.resolveSystemService(appContext.getPackageManager(), 0);
        intent.setComponent(comp);

        Log.d(TAG, String.format("Binding to KeyChain: %s", intent));
        mIsKeyChainServiceBound =
                appContext.bindServiceAsUser(
                        intent,
                        mServiceConnection,
                        Context.BIND_AUTO_CREATE,
                        Process.myUserHandle());
        Log.d(TAG, String.format("KeyChain service binding result: %b", mIsKeyChainServiceBound));
    }

    void unbindKeyChainService() {
        if (mIsKeyChainServiceBound) {
            mContext.getApplicationContext().unbindService(mServiceConnection);
            mIsKeyChainServiceBound = false;
        }
    }

    void installFirstKey() throws RemoteException, IOException, CertificateException {
        String intermediate = "-intermediate";
        String root = "-root";

        String alias1PrivateKey = Credentials.USER_PRIVATE_KEY + ALIAS_1;
        String alias1ClientCert = Credentials.USER_CERTIFICATE + ALIAS_1;
        String alias1IntermediateCert = (Credentials.CA_CERTIFICATE + ALIAS_1 + intermediate);
        String alias1RootCert = (Credentials.CA_CERTIFICATE + ALIAS_1 + root);
        PrivateKeyEntry privateKeyEntry =
                TestKeyStore.getClientCertificate().getPrivateKey("RSA", "RSA");
        Certificate intermediate1 = privateKeyEntry.getCertificateChain()[1];
        Certificate root1 = TestKeyStore.getClientCertificate().getRootCertificate("RSA");

        assertThat(
                mTestSupportService.keystoreImportKey(
                    alias1PrivateKey, privateKeyEntry.getPrivateKey().getEncoded()))
            .isTrue();
        assertThat(
                mTestSupportService.keystorePut(
                    alias1ClientCert,
                    Credentials.convertToPem(privateKeyEntry.getCertificate())))
            .isTrue();
        assertThat(
                mTestSupportService.keystorePut(
                    alias1IntermediateCert, Credentials.convertToPem(intermediate1)))
            .isTrue();
        assertThat(
                mTestSupportService.keystorePut(alias1RootCert, Credentials.convertToPem(root1)))
            .isTrue();
    }

    void waitForSupportService() {
        Log.d(TAG, "Waiting for support service.");
        assertThat(mSupportServiceAvailable.block(10000)).isTrue();;
        assertThat(mTestSupportService).isNotNull();
    }

    void waitForKeyChainService() {
        Log.d(TAG, "Waiting for keychain service.");
        assertThat(mKeyChainAvailable.block(10000)).isTrue();;
        assertThat(mKeyChainService).isNotNull();
    }

    private KeyGenParameterSpec buildRsaKeySpec(String alias) {
        return new KeyGenParameterSpec.Builder(
                alias,
                KeyProperties.PURPOSE_SIGN | KeyProperties.PURPOSE_VERIFY)
                .setKeySize(2048)
                .setDigests(KeyProperties.DIGEST_SHA256)
                .setSignaturePaddings(KeyProperties.SIGNATURE_PADDING_RSA_PSS,
                        KeyProperties.SIGNATURE_PADDING_RSA_PKCS1)
                .setIsStrongBoxBacked(false)
                .build();
    }
}
