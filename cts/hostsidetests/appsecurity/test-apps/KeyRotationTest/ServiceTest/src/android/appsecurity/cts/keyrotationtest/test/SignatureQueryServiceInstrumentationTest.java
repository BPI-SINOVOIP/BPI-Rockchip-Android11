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

package android.appsecurity.cts.keyrotationtest.test;

import static org.junit.Assert.assertEquals;

import android.appsecurity.cts.keyrotationtest.service.ISignatureQueryService;
import android.appsecurity.cts.keyrotationtest.service.SignatureQueryService;
import android.content.Context;
import android.content.Intent;
import android.os.Bundle;
import android.os.IBinder;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.platform.app.InstrumentationRegistry;
import androidx.test.rule.ServiceTestRule;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Verifies that the SignatureQueryService test app is functioning as expected and signed with the
 * expected signatures.
 */
@RunWith(AndroidJUnit4.class)
public final class SignatureQueryServiceInstrumentationTest {
    private Context context;
    private ISignatureQueryService signatureQueryService;

    // These are the sha256 digests of the DER encoding of the ec-p256 and ec-p256_2 signing
    // certificates used to sign this and the app under test.
    private static final String FIRST_SIGNATURE_DIGEST =
            "6a8b96e278e58f62cfe3584022cec1d0527fcb85a9e5d2e1694eb0405be5b599";
    private static final String SECOND_SIGNATURE_DIGEST =
            "d78405f761ff6236cc9b570347a570aba0c62a129a3ac30c831c64d09ad95469";

    @Rule
    public final ServiceTestRule serviceTestRule = new ServiceTestRule();

    @Before
    public void setUp() throws Exception {
        context = InstrumentationRegistry.getInstrumentation().getContext();
        IBinder binder = serviceTestRule.bindService(
                new Intent(ApplicationProvider.getApplicationContext(),
                        SignatureQueryService.class));
        signatureQueryService = ISignatureQueryService.Stub.asInterface(binder);
    }

    @Test
    public void verifySignatures_noRotation_succeeds() throws Exception {
        // Verifies the signatures of the app under test when it is only signed with the original
        // signing key.
        Bundle responseBundle = signatureQueryService.verifySignatures(
                new String[]{FIRST_SIGNATURE_DIGEST}, context.getPackageName());

        assertEquals(0, responseBundle.getInt(ISignatureQueryService.KEY_VERIFY_SIGNATURES_RESULT));
    }

    @Test
    public void verifySignatures_withRotation_succeeds() throws Exception {
        // Verifies the signatures of the test app when it is signed with the rotated key and
        // lineage.
        Bundle responseBundle = signatureQueryService.verifySignatures(
                new String[]{FIRST_SIGNATURE_DIGEST, SECOND_SIGNATURE_DIGEST},
                context.getPackageName());

        assertEquals(0, responseBundle.getInt(ISignatureQueryService.KEY_VERIFY_SIGNATURES_RESULT));
    }
}

