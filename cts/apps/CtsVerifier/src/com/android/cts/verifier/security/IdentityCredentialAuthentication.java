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

package com.android.cts.verifier.security;

import android.Manifest;
import android.app.KeyguardManager;
import android.content.pm.PackageManager;
import android.hardware.biometrics.BiometricManager;
import android.hardware.biometrics.BiometricManager.Authenticators;
import android.hardware.biometrics.BiometricPrompt;
import android.hardware.biometrics.BiometricPrompt.AuthenticationCallback;
import android.hardware.biometrics.BiometricPrompt.AuthenticationResult;
import android.hardware.biometrics.BiometricPrompt.CryptoObject;
import android.os.Bundle;
import android.os.CancellationSignal;
import android.security.identity.AccessControlProfile;
import android.security.identity.AccessControlProfileId;
import android.security.identity.IdentityCredential;
import android.security.identity.IdentityCredentialStore;
import android.security.identity.PersonalizationData;
import android.security.identity.ResultData;
import android.security.identity.WritableIdentityCredential;
import android.widget.Button;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.android.cts.verifier.PassFailButtons;
import com.android.cts.verifier.R;

import java.io.ByteArrayOutputStream;
import java.security.cert.X509Certificate;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.LinkedList;
import java.util.Map;

import co.nstant.in.cbor.CborBuilder;
import co.nstant.in.cbor.CborEncoder;
import co.nstant.in.cbor.CborException;
import co.nstant.in.cbor.builder.MapBuilder;

public class IdentityCredentialAuthentication extends PassFailButtons.Activity {
    private static final boolean DEBUG = false;
    private static final String TAG = "IdentityCredentialAuthentication";

    private static final int BIOMETRIC_REQUEST_PERMISSION_CODE = 0;

    private BiometricManager mBiometricManager;
    private KeyguardManager mKeyguardManager;

    protected int getTitleRes() {
        return R.string.sec_identity_credential_authentication_test;
    }

    private int getDescriptionRes() {
        return R.string.sec_identity_credential_authentication_test_info;
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.sec_screen_lock_keys_main);
        setPassFailButtonClickListeners();
        setInfoResources(getTitleRes(), getDescriptionRes(), -1);
        getPassButton().setEnabled(false);
        requestPermissions(new String[]{Manifest.permission.USE_BIOMETRIC},
                BIOMETRIC_REQUEST_PERMISSION_CODE);
    }

    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] state) {
        if (requestCode == BIOMETRIC_REQUEST_PERMISSION_CODE
                && state[0] == PackageManager.PERMISSION_GRANTED) {
            mBiometricManager = getSystemService(BiometricManager.class);
            mKeyguardManager = getSystemService(KeyguardManager.class);
            Button startTestButton = findViewById(R.id.sec_start_test_button);

            if (!mKeyguardManager.isKeyguardSecure()) {
                // Show a message that the user hasn't set up a lock screen.
                showToast( "Secure lock screen hasn't been set up.\n Go to "
                                + "'Settings -> Security -> Screen lock' to set up a lock screen");
                startTestButton.setEnabled(false);
                return;
            }

            startTestButton.setOnClickListener(v -> startTest());
        }
    }

    protected void showToast(String message) {
        Toast.makeText(this, message, Toast.LENGTH_LONG).show();
    }

    private void provisionFoo(IdentityCredentialStore store) throws Exception {
        store.deleteCredentialByName("test");
        WritableIdentityCredential wc = store.createCredential("test",
                "org.iso.18013-5.2019.mdl");

        // 'Bar' encoded as CBOR tstr
        byte[] barCbor = {0x63, 0x42, 0x61, 0x72};

        AccessControlProfile acp = new AccessControlProfile.Builder(new AccessControlProfileId(0))
                .setUserAuthenticationRequired(true)
                .setUserAuthenticationTimeout(0)
                .build();
        LinkedList<AccessControlProfileId> idsProfile0 = new LinkedList<AccessControlProfileId>();
        idsProfile0.add(new AccessControlProfileId(0));
        PersonalizationData pd = new PersonalizationData.Builder()
                                 .addAccessControlProfile(acp)
                                 .putEntry("org.iso.18013-5.2019", "Foo", idsProfile0, barCbor)
                                 .build();
        byte[] proofOfProvisioningSignature = wc.personalize(pd);

        // Create authentication keys.
        IdentityCredential credential = store.getCredentialByName("test",
                IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
        credential.setAvailableAuthenticationKeys(1, 10);
        Collection<X509Certificate> dynAuthKeyCerts = credential.getAuthKeysNeedingCertification();
        credential.storeStaticAuthenticationData(dynAuthKeyCerts.iterator().next(), new byte[0]);
    }

    private boolean getFooAndCheckNotRetrievable(IdentityCredential credential) throws Exception {
        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019", Arrays.asList("Foo"));

        ResultData rd = credential.getEntries(
            createItemsRequest(entriesToRequest, null),
            entriesToRequest,
            null,  // sessionTranscript
            null); // readerSignature
        if (rd.getStatus("org.iso.18013-5.2019", "Foo")
                != ResultData.STATUS_USER_AUTHENTICATION_FAILED) {
            return false;
        }
        return true;
    }

    private boolean getFooAndCheckRetrievable(IdentityCredential credential) throws Exception {
        Map<String, Collection<String>> entriesToRequest = new LinkedHashMap<>();
        entriesToRequest.put("org.iso.18013-5.2019", Arrays.asList("Foo"));

        ResultData rd = credential.getEntries(
            createItemsRequest(entriesToRequest, null),
            entriesToRequest,
            null,  // sessionTranscript
            null); // readerSignature
        if (rd.getStatus("org.iso.18013-5.2019", "Foo") != ResultData.STATUS_OK) {
            return false;
        }
        return true;
    }

    protected void startTest() {
        IdentityCredentialStore store = IdentityCredentialStore.getInstance(this);
        if (store == null) {
            showToast("No Identity Credential support, test passed.");
            getPassButton().setEnabled(true);
            return;
        }

        final int result = mBiometricManager.canAuthenticate(Authenticators.BIOMETRIC_STRONG);
        switch (result) {
            case BiometricManager.BIOMETRIC_ERROR_NONE_ENROLLED:
                showToast("No biometrics enrolled.\n"
                        + "Go to 'Settings -> Security' to enroll");
                Button startTestButton = findViewById(R.id.sec_start_test_button);
                startTestButton.setEnabled(false);
                return;
            case BiometricManager.BIOMETRIC_ERROR_NO_HARDWARE:
                showToast("No strong biometrics, test passed.");
                showToast("No Identity Credential support, test passed.");
                getPassButton().setEnabled(true);
                return;
        }

        try {
            provisionFoo(store);

            // First, check that Foo cannot be retrieved without authentication.
            //
            IdentityCredential credentialWithoutAuth = store.getCredentialByName("test",
                    IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
            if (!getFooAndCheckNotRetrievable(credentialWithoutAuth)) {
                showToast("Failed while checking that data element cannot be retrieved without"
                        + " authentication");
                return;
            }

            // Try one more time, this time with a CryptoObject that we'll use with
            // BiometricPrompt. This should work.
            //
            final IdentityCredential credentialWithAuth = store.getCredentialByName("test",
                    IdentityCredentialStore.CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
            CryptoObject cryptoObject = new BiometricPrompt.CryptoObject(credentialWithAuth);
            BiometricPrompt.Builder builder = new BiometricPrompt.Builder(this);
            builder.setAllowedAuthenticators(Authenticators.BIOMETRIC_STRONG);
            builder.setTitle("Identity Credential");
            builder.setDescription("Authenticate to unlock credential.");
            builder.setNegativeButton("Cancel",
                    getMainExecutor(),
                    (dialogInterface, i) -> showToast("Canceled biometric prompt."));
            final BiometricPrompt prompt = builder.build();
            final AuthenticationCallback callback = new AuthenticationCallback() {
                @Override
                public void onAuthenticationSucceeded(AuthenticationResult authResult) {
                    try {
                        // Check that Foo can be retrieved because we used
                        // the CryptoObject to auth with.
                        if (!getFooAndCheckRetrievable(credentialWithAuth)) {
                            showToast("Failed while checking that data element can be"
                                    + " retrieved with authentication");
                            return;
                        }

                        // Finally, check that Foo cannot be retrieved again.
                        IdentityCredential credentialWithoutAuth2 = store.getCredentialByName(
                                "test",
                                IdentityCredentialStore
                                        .CIPHERSUITE_ECDHE_HKDF_ECDSA_WITH_AES_256_GCM_SHA256);
                        if (!getFooAndCheckNotRetrievable(credentialWithoutAuth2)) {
                            showToast("Failed while checking that data element cannot be"
                                    + " retrieved without authentication");
                            return;
                        }

                        showToast("Test passed.");
                        getPassButton().setEnabled(true);

                    } catch (Exception e) {
                        showToast("Unexpection exception " + e);
                    }
                }
            };

            prompt.authenticate(cryptoObject, new CancellationSignal(), getMainExecutor(),
                    callback);
        } catch (Exception e) {
            showToast("Unexpection exception " + e);
        }
    }


    /*
     * Helper function to create a CBOR data for requesting data items. The IntentToRetain
     * value will be set to false for all elements.
     *
     * <p>The returned CBOR data conforms to the following CDDL schema:</p>
     *
     * <pre>
     *   ItemsRequest = {
     *     ? "docType" : DocType,
     *     "nameSpaces" : NameSpaces,
     *     ? "RequestInfo" : {* tstr => any} ; Additional info the reader wants to provide
     *   }
     *
     *   NameSpaces = {
     *     + NameSpace => DataElements     ; Requested data elements for each NameSpace
     *   }
     *
     *   DataElements = {
     *     + DataElement => IntentToRetain
     *   }
     *
     *   DocType = tstr
     *
     *   DataElement = tstr
     *   IntentToRetain = bool
     *   NameSpace = tstr
     * </pre>
     *
     * @param entriesToRequest       The entries to request, organized as a map of namespace
     *                               names with each value being a collection of data elements
     *                               in the given namespace.
     * @param docType                  The document type or {@code null} if there is no document
     *                                 type.
     * @return CBOR data conforming to the CDDL mentioned above.
     */
    private static @NonNull byte[] createItemsRequest(
            @NonNull Map<String, Collection<String>> entriesToRequest,
            @Nullable String docType) {
        CborBuilder builder = new CborBuilder();
        MapBuilder<CborBuilder> mapBuilder = builder.addMap();
        if (docType != null) {
            mapBuilder.put("docType", docType);
        }

        MapBuilder<MapBuilder<CborBuilder>> nsMapBuilder = mapBuilder.putMap("nameSpaces");
        for (String namespaceName : entriesToRequest.keySet()) {
            Collection<String> entryNames = entriesToRequest.get(namespaceName);
            MapBuilder<MapBuilder<MapBuilder<CborBuilder>>> entryNameMapBuilder =
                    nsMapBuilder.putMap(namespaceName);
            for (String entryName : entryNames) {
                entryNameMapBuilder.put(entryName, false);
            }
        }

        ByteArrayOutputStream baos = new ByteArrayOutputStream();
        CborEncoder encoder = new CborEncoder(baos);
        try {
            encoder.encode(builder.build());
        } catch (CborException e) {
            throw new RuntimeException("Error encoding CBOR", e);
        }
        return baos.toByteArray();
    }



}
