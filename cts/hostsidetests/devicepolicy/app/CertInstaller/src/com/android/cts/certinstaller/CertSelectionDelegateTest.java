/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.cts.certinstaller;

import static com.google.common.truth.Truth.assertThat;
import static com.google.common.truth.Truth.assertWithMessage;

import static org.junit.Assert.assertNull;

import android.app.Activity;
import android.app.admin.DelegatedAdminReceiver;
import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Process;
import android.security.KeyChain;
import android.security.KeyChainAliasCallback;
import android.support.test.uiautomator.UiDevice;
import android.test.InstrumentationTestCase;

import com.android.compatibility.common.util.FakeKeys.FAKE_RSA_1;

import java.io.ByteArrayInputStream;
import java.io.UnsupportedEncodingException;
import java.net.URLEncoder;
import java.security.KeyFactory;
import java.security.NoSuchAlgorithmException;
import java.security.PrivateKey;
import java.security.cert.Certificate;
import java.security.cert.CertificateException;
import java.security.cert.CertificateFactory;
import java.security.spec.InvalidKeySpecException;
import java.security.spec.PKCS8EncodedKeySpec;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Tests a delegate app with DELEGATION_CERT_SELECTION receives the
 * {@link android.app.admin.DelegatedAdminReceiver#onChoosePrivateKeyAlias} callback when a
 * requesting app (in this case, ourselves) invokes {@link KeyChain#choosePrivateKeyAlias},
 * and is able to force a designated cert to be returned.
 *
 * This test is driven by hostside {@code DeviceAndProfileOwnerTest#testDelegationCertSelection},
 * which grants this app DELEGATION_CERT_SELECTION permission and executes tests in this class.
 */
public class CertSelectionDelegateTest extends InstrumentationTestCase {

    private static final long KEYCHAIN_TIMEOUT_MINS = 1;

    private Context mContext;
    private DevicePolicyManager mDpm;
    private Activity mActivity;

    public static class CertSelectionReceiver extends DelegatedAdminReceiver {

        @Override
        public String onChoosePrivateKeyAlias(Context context, Intent intent, int uid, Uri uri,
                String alias) {
            if (uid != Process.myUid() || uri == null) {
                return null;
            }
            return uri.getQueryParameter("delegate-alias");
        }
    }

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mContext = getInstrumentation().getContext();
        mDpm = mContext.getSystemService(DevicePolicyManager.class);

        final UiDevice device = UiDevice.getInstance(getInstrumentation());
        mActivity = launchActivity(getInstrumentation().getTargetContext().getPackageName(),
                Activity.class, null);
        device.waitForIdle();
    }

    @Override
    protected void tearDown() throws Exception {
        mActivity.finish();
        super.tearDown();
    }

    public void testCanSelectKeychainKeypairs() throws Exception {
        assertThat(mDpm.getDelegatedScopes(null, mContext.getPackageName())).contains(
                DevicePolicyManager.DELEGATION_CERT_SELECTION);

        final String alias = "com.android.test.valid-rsa-key-1";
        final PrivateKey privKey = getPrivateKey(FAKE_RSA_1.privateKey, "RSA");
        final Certificate cert = getCertificate(FAKE_RSA_1.caCertificate);
        // Install keypair.
        assertThat(mDpm.installKeyPair(null, privKey, cert, alias)).isTrue();
        try {
            assertThat(new KeyChainAliasFuture(alias).get()).isEqualTo(alias);

            // Verify key is at least something like the one we put in.
            assertThat(KeyChain.getPrivateKey(mActivity, alias).getAlgorithm()).isEqualTo("RSA");
        } finally {
            // Delete regardless of whether the test succeeded.
            assertThat(mDpm.removeKeyPair(null, alias)).isTrue();
        }
    }

    // Tests that if the delegated app returns {@link
    // android.security.KeyChain.KEY_ALIAS_SELECTION_DENIED}
    // the caller of {@link android.app.admin.DelegatedAdminReceiver#onChoosePrivateKeyAlias}
    // receives {@code null}.
    public void testNotChosenAnyAlias() throws Exception {
        assertThat(mDpm.getDelegatedScopes(null, mContext.getPackageName())).contains(
                DevicePolicyManager.DELEGATION_CERT_SELECTION);
        assertNull(new KeyChainAliasFuture(KeyChain.KEY_ALIAS_SELECTION_DENIED).get());
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
            final Uri uri = Uri.parse("https://example.org/?delegate-alias="
                    + URLEncoder.encode(alias, "UTF-8"));
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

    private static PrivateKey getPrivateKey(final byte[] key, String type)
            throws NoSuchAlgorithmException, InvalidKeySpecException {
        return KeyFactory.getInstance(type).generatePrivate(
                new PKCS8EncodedKeySpec(key));
    }

    private static Certificate getCertificate(byte[] cert) throws CertificateException {
        return CertificateFactory.getInstance("X.509").generateCertificate(
                new ByteArrayInputStream(cert));
    }
}
