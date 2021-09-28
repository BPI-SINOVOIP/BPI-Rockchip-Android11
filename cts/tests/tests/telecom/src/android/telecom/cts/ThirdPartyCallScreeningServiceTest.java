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
 * limitations under the License
 */

package android.telecom.cts;

import static android.telecom.cts.TestUtils.shouldTestTelecom;
import static android.telecom.cts.TestUtils.waitOnAllHandlers;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.Manifest;
import android.app.role.RoleManager;
import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.os.UserHandle;

import android.provider.CallLog;
import android.telecom.Call;
import android.telecom.CallScreeningService;
import android.telecom.TelecomManager;
import android.telecom.cts.screeningtestapp.CallScreeningServiceControl;
import android.telecom.cts.screeningtestapp.CtsCallScreeningService;
import android.telecom.cts.screeningtestapp.ICallScreeningControl;
import android.text.TextUtils;

import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class ThirdPartyCallScreeningServiceTest extends BaseTelecomTestWithMockServices {
    public static final String EXTRA_NETWORK_IDENTIFIED_EMERGENCY_CALL = "identifiedEmergencyCall";
    private static final String TAG = ThirdPartyCallScreeningServiceTest.class.getSimpleName();
    private static final String TEST_APP_NAME = "CTSCSTest";
    private static final String TEST_APP_PACKAGE = "android.telecom.cts.screeningtestapp";
    private static final String TEST_APP_COMPONENT =
            "android.telecom.cts.screeningtestapp/"
                    + "android.telecom.cts.screeningtestapp.CtsCallScreeningService";
    private static final int ASYNC_TIMEOUT = 10000;
    private static final String ROLE_CALL_SCREENING = RoleManager.ROLE_CALL_SCREENING;
    private static final Uri TEST_OUTGOING_NUMBER = Uri.fromParts("tel", "6505551212", null);

    private ICallScreeningControl mCallScreeningControl;
    private RoleManager mRoleManager;
    private String mPreviousCallScreeningPackage;
    private PackageManager mPackageManager;
    private Uri mContactUri;
    private ContentResolver mContentResolver;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (!mShouldTestTelecom) {
            return;
        }
        mRoleManager = (RoleManager) mContext.getSystemService(Context.ROLE_SERVICE);
        mPackageManager = mContext.getPackageManager();
        revokeReadContactPermission();
        setupControlBinder();
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
        rememberPreviousCallScreeningApp();
        // Ensure CTS app holds the call screening role.
        addRoleHolder(ROLE_CALL_SCREENING,
                CtsCallScreeningService.class.getPackage().getName());
        mContentResolver = getInstrumentation().getTargetContext().getContentResolver();
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        if (!mShouldTestTelecom) {
            return;
        }

        if (mCallScreeningControl != null) {
            mCallScreeningControl.reset();
        }

        // Remove the test app from the screening role.
        removeRoleHolder(ROLE_CALL_SCREENING, CtsCallScreeningService.class.getPackage().getName());

        if (!TextUtils.isEmpty(mPreviousCallScreeningPackage)) {
            addRoleHolder(ROLE_CALL_SCREENING, mPreviousCallScreeningPackage);
        }
    }

    /**
     * Verifies that a {@link android.telecom.CallScreeningService} can reject an incoming call.
     * Ensures that the system logs the blocked call to the call log.
     *
     * @throws Exception
     */
    public void testRejectCall() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        // Tell the test app to block the call.
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);

        addIncomingAndVerifyBlocked(false /* addContact */);
    }

    /**
     * Similar to {@link #testRejectCall()}, except the {@link android.telecom.CallScreeningService}
     * tries to skip logging the call to the call log.  We verify that Telecom still logs the call
     * to the call log, retaining the API behavior documented in
     * {@link android.telecom.CallScreeningService#respondToCall(Call.Details, CallScreeningService.CallResponse)}
     * @throws Exception
     */
    public void testRejectCallAndTryToSkipCallLog() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        // Tell the test app to block the call; also try to skip logging the call.
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                true /* shouldSkipCallLog */, true /* shouldSkipNotification */);

        addIncomingAndVerifyBlocked(false /* addContact */);
    }

    /**
     * Verifies that a {@link android.telecom.CallScreeningService} set the extra to silence a call.
     * @throws Exception
     */
    public void testIncomingCallHasSilenceExtra() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        // Tell the test app to silence the call.
        mCallScreeningControl.setCallResponse(false /* shouldDisallowCall */,
            false /* shouldRejectCall */, true /* shouldSilenceCall */,
            false /* shouldSkipCallLog */, false /* shouldSkipNotification */);

        addIncomingAndVerifyCallExtraForSilence(true);
    }

    /**
     * Verifies that a {@link android.telecom.CallScreeningService} did not set the extra to silence an incoming call.
     * @throws Exception
     */
    public void testIncomingCallDoesNotHaveHaveSilenceExtra() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        // Tell the test app to not silence the call.
        mCallScreeningControl.setCallResponse(false /* shouldDisallowCall */,
                false /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, false /* shouldSkipNotification */);

        addIncomingAndVerifyCallExtraForSilence(false);
    }

    public void testHasPermissionAndNoContactIncoming() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        grantReadContactPermission();
        verifyPermission(true);
        // Tell the test app to block the call.
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);
        addIncomingAndVerifyBlocked(false /* addContact */);
    }

    public void testNoPermissionAndNoContactIncoming() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        verifyPermission(false);
        // Tell the test app to block the call.
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);
        addIncomingAndVerifyBlocked(false /* addContact */);
    }

    public void testHasPermissionAndHasContactIncoming() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        grantReadContactPermission();
        verifyPermission(true);
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);
        addIncomingAndVerifyBlocked(true /* addContact */);
    }

    public void testNoPermissionAndHasContactIncoming() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        verifyPermission(false);
        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);
        addIncomingAndVerifyAllowed(true /* addContact */);
    }

    public void testHasPermissionAndNoContactOutgoing() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        grantReadContactPermission();
        verifyPermission(true);
        placeOutgoingCall(false /* addContact */);
        assertTrue(mCallScreeningControl.waitForBind());
    }

    public void testNoPermissionAndNoContactOutgoing() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        verifyPermission(false);
        placeOutgoingCall(false /* addContact */);
        assertTrue(mCallScreeningControl.waitForBind());
    }

    public void testHasPermissionAndHasContactOutgoing() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        grantReadContactPermission();
        verifyPermission(true);
        placeOutgoingCall(true /* addCountact */);
        assertTrue(mCallScreeningControl.waitForBind());
    }

    public void testNoPermissionAndHasContactOutgoing() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        verifyPermission(false);
        placeOutgoingCall(true /* addCountact */);
        assertFalse(mCallScreeningControl.waitForBind());
    }

    public void testNoPostCallActivityWithoutRole() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        removeRoleHolder(ROLE_CALL_SCREENING, CtsCallScreeningService.class.getPackage().getName());
        addIncomingAndVerifyAllowed(false);
        assertFalse(mCallScreeningControl.waitForActivity());
    }

    public void testAllowCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        mCallScreeningControl.setCallResponse(false /* shouldDisallowCall */,
                false /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, false /* shouldSkipNotification */);
        addIncomingAndVerifyAllowed(false /* addContact */);
        assertTrue(mCallScreeningControl.waitForActivity());
    }

    public void testNoPostCallActivityWhenBlocked() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        mCallScreeningControl.setCallResponse(true /* shouldDisallowCall */,
                true /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, true /* shouldSkipNotification */);
        addIncomingAndVerifyBlocked(false /* addContact */);
        assertFalse(mCallScreeningControl.waitForActivity());
    }

    public void testNoPostCallActivityWhenAudioProcessing() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        mCallScreeningControl.setCallResponse(false /* shouldDisallowCall */,
                false /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, false /* shouldSkipNotification */);
        Uri testNumber = createRandomTestNumber();
        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, testNumber);
        mTelecomManager.addNewIncomingCall(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, extras);

        // Wait until the new incoming call is processed.
        waitOnAllHandlers(getInstrumentation());

        assertEquals(1, mInCallCallbacks.getService().getCallCount());
        Call call = mInCallCallbacks.getService().getLastCall();
        call.enterBackgroundAudioProcessing();

        waitOnAllHandlers(getInstrumentation());
        mInCallCallbacks.getService().disconnectAllCalls();
        assertFalse(mCallScreeningControl.waitForActivity());
    }

    public void testNoPostCallActivityForOutgoingEmergencyCall() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }

        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);
        Bundle extras = new Bundle();
        extras.putParcelable(TestUtils.EXTRA_PHONE_NUMBER, TEST_EMERGENCY_URI);
        placeAndVerifyCall(extras);

        // Wait until the new incoming call is processed.
        waitOnAllHandlers(getInstrumentation());
        mInCallCallbacks.getService().disconnectAllCalls();
        assertFalse(mCallScreeningControl.waitForActivity());
    }

    public void testNoPostCallActivityForIncomingEmergencyCall() throws Exception {
        if (!shouldTestTelecom(mContext)) {
            return;
        }
        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);
        mCallScreeningControl.setCallResponse(false /* shouldDisallowCall */,
                false /* shouldRejectCall */, false /* shouldSilenceCall */,
                false /* shouldSkipCallLog */, false /* shouldSkipNotification */);
        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, TEST_EMERGENCY_URI);
        extras.putBoolean(EXTRA_NETWORK_IDENTIFIED_EMERGENCY_CALL, true);
        mTelecomManager.addNewIncomingCall(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, extras);

        // Wait until the new incoming call is processed.
        waitOnAllHandlers(getInstrumentation());
        mInCallCallbacks.getService().disconnectAllCalls();

        assertFalse(mCallScreeningControl.waitForActivity());
    }

    private void placeOutgoingCall(boolean addContact) throws Exception {
        // Setup content observer to notify us when we call log entry is added.
        CountDownLatch callLogEntryLatch = getCallLogEntryLatch();

        Uri contactUri = null;
        if (addContact) {
            contactUri = TestUtils.insertContact(mContentResolver,
                    TEST_OUTGOING_NUMBER.getSchemeSpecificPart());
        }
        Bundle extras = new Bundle();
        extras.putParcelable(TestUtils.EXTRA_PHONE_NUMBER, TEST_OUTGOING_NUMBER);
        // Create a new outgoing call.
        placeAndVerifyCall(extras);

        if (addContact) {
            assertEquals(1, TestUtils.deleteContact(mContentResolver, contactUri));
        }

        mInCallCallbacks.getService().disconnectAllCalls();
        assertNumCalls(mInCallCallbacks.getService(), 0);

        // Wait for it to log.
        callLogEntryLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
    }

    private Uri addIncoming(boolean disconnectImmediately, boolean addContact) throws Exception {
        // Add call through TelecomManager; we can't use the test methods since they assume a call
        // makes it through to the InCallService; this is blocked so it shouldn't.
        Uri testNumber = createRandomTestNumber();
        if (addContact) {
            mContactUri = TestUtils.insertContact(mContentResolver,
                    testNumber.getSchemeSpecificPart());
        }

        // Setup content observer to notify us when we call log entry is added.
        CountDownLatch callLogEntryLatch = getCallLogEntryLatch();

        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, testNumber);
        mTelecomManager.addNewIncomingCall(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, extras);

        // Wait until the new incoming call is processed.
        waitOnAllHandlers(getInstrumentation());

        if (disconnectImmediately) {
            // Disconnect the call
            mInCallCallbacks.getService().disconnectAllCalls();
            assertNumCalls(mInCallCallbacks.getService(), 0);
        }

        // Wait for the content observer to report that we have gotten a new call log entry.
        callLogEntryLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
        return testNumber;
    }

    private void addIncomingAndVerifyAllowed(boolean addContact) throws Exception {
        Uri testNumber = addIncoming(true, addContact);

        // Query the latest entry into the call log.
        Cursor callsCursor = mContentResolver.query(CallLog.Calls.CONTENT_URI, null,
                null, null, CallLog.Calls._ID + " DESC limit 1;");
        int numberIndex = callsCursor.getColumnIndex(CallLog.Calls.NUMBER);
        int callTypeIndex = callsCursor.getColumnIndex(CallLog.Calls.TYPE);
        int blockReasonIndex = callsCursor.getColumnIndex(CallLog.Calls.BLOCK_REASON);
        if (callsCursor.moveToNext()) {
            String number = callsCursor.getString(numberIndex);
            int callType = callsCursor.getInt(callTypeIndex);
            int blockReason = callsCursor.getInt(blockReasonIndex);
            assertEquals(testNumber.getSchemeSpecificPart(), number);
            assertEquals(CallLog.Calls.INCOMING_TYPE, callType);
            assertEquals(CallLog.Calls.BLOCK_REASON_NOT_BLOCKED, blockReason);
        } else {
            fail("Call not logged");
        }

        if (addContact && mContactUri != null) {
            assertEquals(1, TestUtils.deleteContact(mContentResolver, mContactUri));
        }
    }

    private void addIncomingAndVerifyBlocked(boolean addContact) throws Exception {
        Uri testNumber = addIncoming(false, addContact);

        // Query the latest entry into the call log.
        Cursor callsCursor = mContentResolver.query(CallLog.Calls.CONTENT_URI, null,
                null, null, CallLog.Calls._ID + " DESC limit 1;");
        int numberIndex = callsCursor.getColumnIndex(CallLog.Calls.NUMBER);
        int callTypeIndex = callsCursor.getColumnIndex(CallLog.Calls.TYPE);
        int blockReasonIndex = callsCursor.getColumnIndex(CallLog.Calls.BLOCK_REASON);
        int callScreeningAppNameIndex = callsCursor.getColumnIndex(
                CallLog.Calls.CALL_SCREENING_APP_NAME);
        int callScreeningCmpNameIndex = callsCursor.getColumnIndex(
                CallLog.Calls.CALL_SCREENING_COMPONENT_NAME);
        if (callsCursor.moveToNext()) {
            String number = callsCursor.getString(numberIndex);
            int callType = callsCursor.getInt(callTypeIndex);
            int blockReason = callsCursor.getInt(blockReasonIndex);
            String screeningAppName = callsCursor.getString(callScreeningAppNameIndex);
            String screeningComponentName = callsCursor.getString(callScreeningCmpNameIndex);
            assertEquals(testNumber.getSchemeSpecificPart(), number);
            assertEquals(CallLog.Calls.BLOCKED_TYPE, callType);
            assertEquals(CallLog.Calls.BLOCK_REASON_CALL_SCREENING_SERVICE, blockReason);
            assertEquals(TEST_APP_NAME, screeningAppName);
            assertEquals(TEST_APP_COMPONENT, screeningComponentName);
        } else {
            fail("Blocked call was not logged.");
        }

        if (addContact && mContactUri != null) {
            assertEquals(1, TestUtils.deleteContact(mContentResolver, mContactUri));
        }
    }

    private void addIncomingAndVerifyCallExtraForSilence(boolean expectedIsSilentRingingExtraSet)
            throws Exception {
        Uri testNumber = addIncoming(false, false);

        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return true;
                    }

                    @Override
                    public Object actual() {
                        // Verify that the call extra matches expectation
                        Call call = mInCallCallbacks.getService().getLastCall();
                        return expectedIsSilentRingingExtraSet ==
                                call.getDetails().getExtras().getBoolean(
                                        Call.EXTRA_SILENT_RINGING_REQUESTED);
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                        "Call extra - verification failed, expected the extra " +
                        "EXTRA_SILENT_RINGING_REQUESTED to be set:" +
                        expectedIsSilentRingingExtraSet);
    }

    /**
     * Sets up a binder used to control the CallScreeningServiceCtsTestApp.
     * This app is a standalone APK so that it can reside in a package name outside of the one the
     * CTS test itself runs in (since that APK is where the CTS InCallService resides).
     * @throws InterruptedException
     */
    private void setupControlBinder() throws InterruptedException {
        Intent bindIntent = new Intent(CallScreeningServiceControl.CONTROL_INTERFACE_ACTION);
        bindIntent.setComponent(CallScreeningServiceControl.CONTROL_INTERFACE_COMPONENT);
        final CountDownLatch bindLatch = new CountDownLatch(1);

        boolean success = mContext.bindService(bindIntent, new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                mCallScreeningControl = ICallScreeningControl.Stub.asInterface(service);
                bindLatch.countDown();
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
                mCallScreeningControl = null;
            }
        }, Context.BIND_AUTO_CREATE);
        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        bindLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
    }

    /**
     * Use RoleManager to query the previous call screening app so we can restore it later.
     */
    private void rememberPreviousCallScreeningApp() {
        runWithShellPermissionIdentity(() -> {
            List<String> callScreeningApps = mRoleManager.getRoleHolders(ROLE_CALL_SCREENING);
            if (!callScreeningApps.isEmpty()) {
                mPreviousCallScreeningPackage = callScreeningApps.get(0);
            } else {
                mPreviousCallScreeningPackage = null;
            }
        });
    }

    private void addRoleHolder(String roleName, String packageName)
            throws Exception {
        UserHandle user = Process.myUserHandle();
        Executor executor = mContext.getMainExecutor();
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue(1);

        runWithShellPermissionIdentity(() -> mRoleManager.addRoleHolderAsUser(roleName,
                packageName, RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP, user, executor,
                successful -> {
                    try {
                        queue.put(successful);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }));
        boolean result = queue.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
        assertTrue(result);
    }

    private void removeRoleHolder(String roleName, String packageName)
            throws Exception {
        UserHandle user = Process.myUserHandle();
        Executor executor = mContext.getMainExecutor();
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue(1);

        runWithShellPermissionIdentity(() -> mRoleManager.removeRoleHolderAsUser(roleName,
                packageName, RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP, user, executor,
                successful -> {
                    try {
                        queue.put(successful);
                    } catch (InterruptedException e) {
                        e.printStackTrace();
                    }
                }));
        boolean result = queue.poll(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);
        assertTrue(result);
    }

    private void grantReadContactPermission() {
        runWithShellPermissionIdentity(() -> {
            if (mPackageManager != null) {
                mPackageManager.grantRuntimePermission(TEST_APP_PACKAGE,
                        Manifest.permission.READ_CONTACTS, mContext.getUser());
            }});
    }

    private void revokeReadContactPermission() {
        runWithShellPermissionIdentity(() -> {
                if (mPackageManager != null) {
                    mPackageManager.revokeRuntimePermission(TEST_APP_PACKAGE,
                            Manifest.permission.READ_CONTACTS, mContext.getUser());
                }});
    }

    private void verifyPermission(boolean hasPermission) {
        assertEquals(hasPermission,
                mPackageManager.checkPermission
                        (Manifest.permission.READ_CONTACTS, TEST_APP_PACKAGE)
                        == PackageManager.PERMISSION_GRANTED);
    }
}
