/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.telecom.cts;

import static android.telecom.cts.TestUtils.shouldTestTelecom;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.telecom.Call;
import android.telecom.InCallService;
import android.telecom.TelecomManager;
import android.telecom.cts.MockInCallService.InCallServiceCallbacks;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;

import com.android.compatibility.common.util.CddTest;

import org.junit.Test;

import java.util.List;
import java.util.concurrent.TimeUnit;

/**
 * Sanity test that adding a new call via the CALL intent works correctly.
 */
public class BasicInCallServiceTest extends InstrumentationTestCase {

    private static final Uri TEST_NUMBER = Uri.fromParts("tel", "555-1234", null);

    private Context mContext;
    private String mPreviousDefaultDialer = null;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
        mPreviousDefaultDialer = TestUtils.getDefaultDialer(getInstrumentation());
        TestUtils.setDefaultDialer(getInstrumentation(), TestUtils.PACKAGE);
    }

    @Override
    protected void tearDown() throws Exception {
        if (!TextUtils.isEmpty(mPreviousDefaultDialer)) {
            TestUtils.setDefaultDialer(getInstrumentation(), mPreviousDefaultDialer);
        }
        super.tearDown();
    }

    @CddTest(requirement = "7.4.1.2/C-1-3")
    public void testResolveInCallIntent() {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        PackageManager packageManager = mContext.getPackageManager();
        Intent serviceIntent = new Intent(InCallService.SERVICE_INTERFACE);
        List<ResolveInfo> resolveInfo = packageManager.queryIntentServices(serviceIntent,
                PackageManager.GET_META_DATA);

        assertNotNull(resolveInfo);
        assertTrue(resolveInfo.size() >= 1);

        // Ensure at least one InCallService is able to handle the UI for calls.
        assertTrue(resolveInfo
                .stream()
                .filter(r -> r.serviceInfo != null
                        && r.serviceInfo.metaData != null
                        && r.serviceInfo.metaData.containsKey(
                                TelecomManager.METADATA_IN_CALL_SERVICE_UI)
                        && r.serviceInfo.permission.equals(
                                android.Manifest.permission.BIND_INCALL_SERVICE)
                        && r.serviceInfo.name != null)
                .count() >= 1);
    }

    /**
     * Tests that when sending a CALL intent via the Telecom -> Telephony stack, Telecom
     * binds to the registered {@link InCallService}s and adds a new call. This test will
     * actually place a phone call to the number 7. It should still pass even if there is no
     * SIM card inserted.
     */
    public void testTelephonyCall_bindsToInCallServiceAndAddsCall() {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        final Intent intent = new Intent(Intent.ACTION_CALL, TEST_NUMBER);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        final InCallServiceCallbacks callbacks = createCallbacks();

        MockInCallService.setCallbacks(callbacks);

        mContext.startActivity(intent);

        try {
            if (callbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                        TimeUnit.SECONDS)) {
                return;
            }
        } catch (InterruptedException e) {
        }

        fail("No call added to InCallService.");
    }

    private MockInCallService.InCallServiceCallbacks createCallbacks() {
        final InCallServiceCallbacks callbacks = new InCallServiceCallbacks() {
            @Override
            public void onCallAdded(Call call, int numCalls) {
                assertEquals("InCallService should have 1 call after adding call", 1, numCalls);
                call.disconnect();
                lock.release();
            }
        };
        return callbacks;
    }
}
