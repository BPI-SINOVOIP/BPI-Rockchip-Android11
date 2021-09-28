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

import android.content.ContentResolver;
import android.telecom.cts.MockCallScreeningService.CallScreeningServiceCallbacks;

import android.content.ComponentName;
import android.content.Context;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.Call;
import android.telecom.CallScreeningService;
import android.telecom.Connection;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.test.InstrumentationTestCase;
import android.text.TextUtils;

import java.util.concurrent.TimeUnit;

/**
 * Verify that call screening service gets a chance to block calls.
 */
public class CallScreeningServiceTest extends InstrumentationTestCase {
    private static final Uri TEST_NUMBER = Uri.fromParts("tel", "7", null);

    public static final PhoneAccountHandle TEST_PHONE_ACCOUNT_HANDLE = new PhoneAccountHandle(
            new ComponentName(TestUtils.PACKAGE, TestUtils.COMPONENT),
            TestUtils.ACCOUNT_ID_1);

    public static final PhoneAccount TEST_PHONE_ACCOUNT = PhoneAccount.builder(
            TEST_PHONE_ACCOUNT_HANDLE, TestUtils.ACCOUNT_LABEL)
            .setAddress(Uri.parse("tel:555-TEST"))
            .setCapabilities(PhoneAccount.CAPABILITY_CALL_PROVIDER |
                    PhoneAccount.CAPABILITY_CONNECTION_MANAGER)
            .addSupportedUriScheme(PhoneAccount.SCHEME_TEL)
            .build();

    private Context mContext;
    private TelecomManager mTelecomManager;
    private String mPreviousDefaultDialer;
    MockConnectionService mConnectionService;
    private boolean mCallFound;
    private ContentResolver mContentResolver;
    private int mCallerNumberVerificationStatus;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
        mTelecomManager = (TelecomManager) mContext.getSystemService(Context.TELECOM_SERVICE);
        if (shouldTestTelecom(mContext)) {
            mPreviousDefaultDialer = TestUtils.getDefaultDialer(getInstrumentation());
            TestUtils.setDefaultDialer(getInstrumentation(), TestUtils.PACKAGE);
            setupConnectionService();
            MockCallScreeningService.enableService(mContext);
        }
        mContentResolver = getInstrumentation().getTargetContext().getContentResolver();
    }

    @Override
    protected void tearDown() throws Exception {
        if (!TextUtils.isEmpty(mPreviousDefaultDialer)) {
            TestUtils.setDefaultDialer(getInstrumentation(), mPreviousDefaultDialer);
            mTelecomManager.unregisterPhoneAccount(TEST_PHONE_ACCOUNT_HANDLE);
            CtsConnectionService.tearDown();
            mConnectionService = null;
            MockCallScreeningService.disableService(mContext);
        }
        super.tearDown();
    }

    /**
     * Tests that when sending a CALL intent via the Telecom stack, Telecom binds to the registered
     * {@link CallScreeningService}s and invokes onScreenCall.
     */
    public void testTelephonyCall_bindsToCallScreeningService() {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CallScreeningServiceCallbacks callbacks = createCallbacks();
        MockCallScreeningService.setCallbacks(callbacks);
        addNewIncomingCall(TEST_NUMBER);

        try {
            if (callbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                        TimeUnit.SECONDS)) {
                assertTrue(mCallFound);
                return;
            }
        } catch (InterruptedException e) {
        }

        fail("No call added to CallScreeningService.");
    }

    /**
     * Tests that when sendinga a CALL intent via the Telecom stack and the test number is in the
     * user's contact, Telecom binds to the registered {@link CallScreeningService}s and invokes
     * onScreenCall.
     */
    public void testBindsToCallScreeningServiceWhenContactExist() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CallScreeningServiceCallbacks callbacks = createCallbacks();
        MockCallScreeningService.setCallbacks(callbacks);
        Uri contactUri = TestUtils.insertContact(mContentResolver,
                TEST_NUMBER.getSchemeSpecificPart());
        addNewIncomingCall(TEST_NUMBER);

        try {
            if (callbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                assertTrue(mCallFound);

                return;
            }
        } catch (InterruptedException e) {
        } finally {
            assertEquals(1, TestUtils.deleteContact(mContentResolver, contactUri));
        }

        fail("No call added to CallScreeningService.");
    }

    /**
     * Tests passing of number verification status.
     */
    public void testVerificationFailed() {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CallScreeningServiceCallbacks callbacks = createCallbacks();
        MockCallScreeningService.setCallbacks(callbacks);
        addNewIncomingCall(MockConnectionService.VERSTAT_FAILED_NUMBER);

        try {
            if (callbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                assertTrue(mCallFound);
                assertEquals(Connection.VERIFICATION_STATUS_FAILED,
                        mCallerNumberVerificationStatus);
                return;
            }
        } catch (InterruptedException e) {
        }
    }

    /**
     * Tests passing of number verification status.
     */
    public void testNumberNotVerified() {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        CallScreeningServiceCallbacks callbacks = createCallbacks();
        MockCallScreeningService.setCallbacks(callbacks);
        addNewIncomingCall(MockConnectionService.VERSTAT_NOT_VERIFIED_NUMBER);

        try {
            if (callbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                assertTrue(mCallFound);
                assertEquals(Connection.VERIFICATION_STATUS_NOT_VERIFIED,
                        mCallerNumberVerificationStatus);
                return;
            }
        } catch (InterruptedException e) {
        }

        fail("No call added to CallScreeningService.");
    }

    private void addNewIncomingCall(Uri incomingHandle) {
        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, incomingHandle);
        mTelecomManager.addNewIncomingCall(TEST_PHONE_ACCOUNT_HANDLE, extras);
    }

    private CallScreeningServiceCallbacks createCallbacks() {
        return new CallScreeningServiceCallbacks() {
            @Override
            public void onScreenCall(Call.Details callDetails) {
                mCallFound = true;
                mCallerNumberVerificationStatus = callDetails.getCallerNumberVerificationStatus();
                CallScreeningService.CallResponse response =
                        new CallScreeningService.CallResponse.Builder()
                        .setDisallowCall(true)
                        .setRejectCall(true)
                        .setSilenceCall(false)
                        .setSkipCallLog(true)
                        .setSkipNotification(true)
                        .build();
                getService().respondToCall(callDetails, response);
                lock.release();
            }
        };
    }

    private void setupConnectionService() throws Exception {
        mConnectionService = new MockConnectionService();
        CtsConnectionService.setUp(mConnectionService);

        mTelecomManager.registerPhoneAccount(TEST_PHONE_ACCOUNT);
        TestUtils.enablePhoneAccount(getInstrumentation(), TEST_PHONE_ACCOUNT_HANDLE);
        // Wait till the adb commands have executed and account is enabled in Telecom database.
        assertPhoneAccountEnabled(TEST_PHONE_ACCOUNT_HANDLE);
    }

    private void assertPhoneAccountEnabled(final PhoneAccountHandle handle) {
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return true;
                    }

                    @Override
                    public Object actual() {
                        PhoneAccount phoneAccount = mTelecomManager.getPhoneAccount(handle);
                        return (phoneAccount != null && phoneAccount.isEnabled());
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Phone account enable failed for " + handle
        );
    }

    private void waitUntilConditionIsTrueOrTimeout(Condition condition, long timeout,
            String description) {
        final long start = System.currentTimeMillis();
        while (!condition.expected().equals(condition.actual())
                && System.currentTimeMillis() - start < timeout) {
            sleep(50);
        }
        assertEquals(description, condition.expected(), condition.actual());
    }

    private void sleep(long ms) {
        try {
            Thread.sleep(ms);
        } catch (InterruptedException e) {
        }
    }

    protected interface Condition {
        Object expected();
        Object actual();
    }
}
