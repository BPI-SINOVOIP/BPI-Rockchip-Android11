package android.telecom.cts;

import static android.app.role.RoleManager.ROLE_CALL_SCREENING;
import static android.telecom.cts.TestUtils.TEST_PHONE_ACCOUNT_HANDLE;
import static android.telecom.cts.TestUtils.waitOnAllHandlers;
import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.app.role.RoleManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.IBinder;
import android.os.Process;
import android.os.UserHandle;
import android.provider.CallLog;
import android.telecom.Call;
import android.telecom.Call.Details;
import android.telecom.CallScreeningService.CallResponse;
import android.telecom.Connection;
import android.telecom.DisconnectCause;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telecom.cts.MockCallScreeningService.CallScreeningServiceCallbacks;
import android.telecom.cts.api29incallservice.CtsApi29InCallService;
import android.telecom.cts.api29incallservice.CtsApi29InCallServiceControl;
import android.telecom.cts.api29incallservice.ICtsApi29InCallServiceControl;
import android.text.TextUtils;
import android.util.Log;

import androidx.test.InstrumentationRegistry;


import java.util.concurrent.Executor;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;

public class BackgroundCallAudioTest extends BaseTelecomTestWithMockServices {
    private static final String LOG_TAG = BackgroundCallAudioTest.class.getSimpleName();

    private static final int ASYNC_TIMEOUT = 10000;
    private RoleManager mRoleManager;
    private ServiceConnection mApiCompatControlServiceConnection;

    // copied from AudioSystem.java -- defined here because that change isn't in AOSP yet.
    private static final int MODE_CALL_SCREENING = 4;

    // true if there's platform support for call screening in the audio stack.
    private boolean doesAudioManagerSupportCallScreening = false;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (mShouldTestTelecom) {
            mRoleManager = (RoleManager) mContext.getSystemService(Context.ROLE_SERVICE);
            clearRoleHoldersAsUser(ROLE_CALL_SCREENING);
            mPreviousDefaultDialer = TestUtils.getDefaultDialer(getInstrumentation());
            TestUtils.setDefaultDialer(getInstrumentation(), TestUtils.PACKAGE);
            setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
            // Some of the tests expect changes in audio mode when the ringer starts, so we're
            // going to turn up the ring stream volume.
            AudioManager audioManager = mContext.getSystemService(AudioManager.class);
            audioManager.adjustStreamVolume(AudioManager.STREAM_RING,
                    AudioManager.ADJUST_UNMUTE, 0);
            doesAudioManagerSupportCallScreening =
                    audioManager.isCallScreeningModeSupported();
        }
    }

    @Override
    protected void tearDown() throws Exception {
        if (mShouldTestTelecom && !TextUtils.isEmpty(mPreviousDefaultDialer)) {
            TestUtils.setDefaultDialer(getInstrumentation(), mPreviousDefaultDialer);
            mTelecomManager.unregisterPhoneAccount(TEST_PHONE_ACCOUNT_HANDLE);
            CtsConnectionService.tearDown();
            MockCallScreeningService.disableService(mContext);
        }
        super.tearDown();
    }

    public void testAudioProcessingFromCallScreeningAllow() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        verifySimulateRingAndUserPickup(call, connection);
    }

    public void testHoldAfterAudioProcessingFromCallScreening() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        verifySimulateRingAndUserPickup(call, connection);

        call.hold();
        assertCallState(call, Call.STATE_HOLDING);
        call.unhold();
        assertCallState(call, Call.STATE_ACTIVE);
    }

    public void testAudioProcessingFromCallScreeningDisallow() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        call.disconnect();
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertEquals(DisconnectCause.REJECTED, call.getDetails().getDisconnectCause().getCode());
    }

    public void testAudioProcessingFromCallScreeningMissed() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        verifySimulateRingAndUserMissed(call, connection);
    }

    public void testAudioProcessingFromCallScreeningRemoteHangupDuringRing() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        call.exitBackgroundAudioProcessing(true);
        assertCallState(call, Call.STATE_SIMULATED_RINGING);

        waitOnAllHandlers(getInstrumentation());
        // We expect the audio mode to stay in CALL_SCREENING when going into simulated ringing.
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        connection.setDisconnected(new DisconnectCause(DisconnectCause.REMOTE));
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertEquals(DisconnectCause.MISSED, call.getDetails().getDisconnectCause().getCode());
        connection.destroy();
    }

    public void testAudioProcessingFromCallScreeningAllowPlaceEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);
        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        call.exitBackgroundAudioProcessing(true);
        assertCallState(call, Call.STATE_SIMULATED_RINGING);
        waitOnAllHandlers(getInstrumentation());
        // We expect the audio mode to stay in CALL_SCREENING when going into simulated ringing.
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        placeAndVerifyEmergencyCall(false /*supportsHold*/);
        waitOnAllHandlers(getInstrumentation());
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);
        // Even though the connection was technically active, it is "simulated ringing", so
        // disconnect as you would a normal ringing call in favor of an emergency call.
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertConnectionState(connection, Connection.STATE_DISCONNECTED);
        // Notify as missed instead of rejected, since the user did not explicitly reject.
        verifyCallLogging(connection.getAddress(), CallLog.Calls.MISSED_TYPE);
    }

    public void testAudioProcessingFromIncomingActivePlaceEmergencyCall() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);
        setupIncomingCallWithCallScreening();

        final MockConnection connection = verifyConnectionForIncomingCall();

        if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("No call added to InCallService.");
        }

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        verifySimulateRingAndUserPickup(call, connection);
        // Go back into audio processing for hold case
        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        waitOnAllHandlers(getInstrumentation());

        placeAndVerifyEmergencyCall(false /*supportsHold*/);
        waitOnAllHandlers(getInstrumentation());
        Call eCall = getInCallService().getLastCall();
        assertCallState(eCall, Call.STATE_DIALING);
        // Even though the connection was technically active, it is "simulated ringing", so
        // disconnect as you would a normal ringing call in favor of an emergency call.
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertConnectionState(connection, Connection.STATE_DISCONNECTED);
        // Notify as incoming, since the user has already answered the call.
        verifyCallLogging(connection.getAddress(), CallLog.Calls.INCOMING_TYPE);
    }

    public void testAudioProcessActiveCall() {
        if (!mShouldTestTelecom) {
            return;
        }

        Connection connection = placeActiveOutgoingCall();
        Call call = mInCallCallbacks.getService().getLastCall();

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        verifySimulateRingAndUserPickup(call, connection);
    }

    public void testAudioProcessActiveCallMissed() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        Connection connection = placeActiveOutgoingCall();
        Call call = mInCallCallbacks.getService().getLastCall();

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        verifySimulateRingAndUserMissed(call, connection);
    }

    public void testAudioProcessActiveCallRemoteHangup() {
        if (!mShouldTestTelecom) {
            return;
        }

        Connection connection = placeActiveOutgoingCall();
        Call call = mInCallCallbacks.getService().getLastCall();

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        connection.setDisconnected(new DisconnectCause(DisconnectCause.REMOTE));
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertEquals(DisconnectCause.REMOTE, call.getDetails().getDisconnectCause().getCode());
        connection.destroy();
    }

    public void testAudioProcessOutgoingActiveEmergencyCallPlaced() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        setupForEmergencyCalling(TEST_EMERGENCY_NUMBER);

        Connection connection = placeActiveOutgoingCall();
        Call call = mInCallCallbacks.getService().getLastCall();

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        placeAndVerifyEmergencyCall(false /*supportsHold*/);
        waitOnAllHandlers(getInstrumentation());
        Call eCall = getInCallService().getLastCall();
        // emergency call should be dialing
        assertCallState(eCall, Call.STATE_DIALING);
        // audio processing call should be disconnected
        assertConnectionState(connection, Connection.STATE_DISCONNECTED);
        assertCallState(call, Call.STATE_DISCONNECTED);
        // If we went to AUDIO_PROCESSING from an active outgoing call, Make sure the call is
        // marked outgoing, not missed.
        verifyCallLogging(connection.getAddress(), CallLog.Calls.OUTGOING_TYPE);
    }

    public void testManualAudioCallScreenAccept() {
        if (!mShouldTestTelecom) {
            return;
        }

        addAndVerifyNewIncomingCall(createTestNumber(), null);
        final MockConnection connection = verifyConnectionForIncomingCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_RINGING);

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        call.exitBackgroundAudioProcessing(false);
        assertCallState(call, Call.STATE_ACTIVE);
        waitOnAllHandlers(getInstrumentation());
        assertAudioMode(audioManager, AudioManager.MODE_IN_CALL);
    }

    public void testManualAudioCallScreenReject() {
        if (!mShouldTestTelecom) {
            return;
        }

        addAndVerifyNewIncomingCall(createTestNumber(), null);
        final MockConnection connection = verifyConnectionForIncomingCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_RINGING);

        call.enterBackgroundAudioProcessing();
        assertCallState(call, Call.STATE_AUDIO_PROCESSING);
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        waitOnAllHandlers(getInstrumentation());
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }

        call.disconnect();
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertEquals(DisconnectCause.REJECTED, call.getDetails().getDisconnectCause().getCode());
    }

    public void testEnterAudioProcessingWithoutPermission() {
        if (!mShouldTestTelecom) {
            return;
        }

        if (true) {
            // TODO: enable test
            return;
        }

        placeAndVerifyCall();
        final MockConnection connection = verifyConnectionForOutgoingCall();

        final MockInCallService inCallService = mInCallCallbacks.getService();

        connection.setActive();
        final Call call = inCallService.getLastCall();
        assertCallState(call, Call.STATE_ACTIVE);

        try {
            call.enterBackgroundAudioProcessing();
            fail("Expected SecurityException");
        } catch (SecurityException e) {
            // expected
        }
    }

    public void testLowerApiLevelCompatibility1() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.CONTROL_INCALL_EXPERIENCE");
        try {
            ICtsApi29InCallServiceControl controlInterface = setUpControl();

            setupIncomingCallWithCallScreening();

            final MockConnection connection = verifyConnectionForIncomingCall();

            if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                fail("No call added to InCallService.");
            }

            Call call = mInCallCallbacks.getService().getLastCall();
            assertCallState(call, Call.STATE_AUDIO_PROCESSING);
            assertConnectionState(connection, Connection.STATE_ACTIVE);
            // Make sure that the dummy app never got any calls
            assertEquals(0, controlInterface.getHistoricalCallCount());

            call.exitBackgroundAudioProcessing(true);
            assertCallState(call, Call.STATE_SIMULATED_RINGING);
            waitOnAllHandlers(getInstrumentation());
            assertConnectionState(connection, Connection.STATE_ACTIVE);
            // Make sure that the dummy app sees a ringing call.
            assertEquals(Call.STATE_RINGING,
                    controlInterface.getCallState(call.getDetails().getTelecomCallId()));

            call.answer(VideoProfile.STATE_AUDIO_ONLY);
            assertCallState(call, Call.STATE_ACTIVE);
            waitOnAllHandlers(getInstrumentation());
            assertConnectionState(connection, Connection.STATE_ACTIVE);
            // Make sure that the dummy app sees an active call.
            assertEquals(Call.STATE_ACTIVE,
                    controlInterface.getCallState(call.getDetails().getTelecomCallId()));

            tearDownControl();
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    public void testLowerApiLevelCompatibility2() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.CONTROL_INCALL_EXPERIENCE");
        try {
            ICtsApi29InCallServiceControl controlInterface = setUpControl();

            setupIncomingCallWithCallScreening();

            final MockConnection connection = verifyConnectionForIncomingCall();

            if (!mInCallCallbacks.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                    TimeUnit.SECONDS)) {
                fail("No call added to InCallService.");
            }

            Call call = mInCallCallbacks.getService().getLastCall();
            assertCallState(call, Call.STATE_AUDIO_PROCESSING);
            assertConnectionState(connection, Connection.STATE_ACTIVE);
            // Make sure that the dummy app never got any calls
            assertEquals(0, controlInterface.getHistoricalCallCount());

            call.disconnect();
            assertCallState(call, Call.STATE_DISCONNECTED);
            waitOnAllHandlers(getInstrumentation());
            assertConnectionState(connection, Connection.STATE_DISCONNECTED);
            // Under some rare circumstances, the dummy app might get a flash of the disconnection
            // call, so we won't do the call count check again.

            tearDownControl();
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    private Connection placeActiveOutgoingCall() {
        placeAndVerifyCall();

        Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_DIALING);

        final MockConnection connection = verifyConnectionForOutgoingCall();
        connection.setActive();
        assertCallState(call, Call.STATE_ACTIVE);
        return connection;
    }

    private void verifySimulateRingAndUserPickup(Call call, Connection connection) {
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);

        call.exitBackgroundAudioProcessing(true);
        assertCallState(call, Call.STATE_SIMULATED_RINGING);
        waitOnAllHandlers(getInstrumentation());
        // We expect the audio mode to stay in CALL_SCREENING when going into simulated ringing.
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        call.answer(VideoProfile.STATE_AUDIO_ONLY);
        assertCallState(call, Call.STATE_ACTIVE);
        waitOnAllHandlers(getInstrumentation());
        assertAudioMode(audioManager, AudioManager.MODE_IN_CALL);
        assertConnectionState(connection, Connection.STATE_ACTIVE);
    }

    private void verifySimulateRingAndUserMissed(Call call, Connection connection) {
        AudioManager audioManager = mContext.getSystemService(AudioManager.class);

        call.exitBackgroundAudioProcessing(true);
        assertCallState(call, Call.STATE_SIMULATED_RINGING);
        waitOnAllHandlers(getInstrumentation());
        // We expect the audio mode to stay in CALL_SCREENING when going into simulated ringing.
        if (doesAudioManagerSupportCallScreening) {
            assertAudioMode(audioManager, MODE_CALL_SCREENING);
        }
        assertConnectionState(connection, Connection.STATE_ACTIVE);

        call.disconnect();
        assertCallState(call, Call.STATE_DISCONNECTED);
        assertConnectionState(connection, Connection.STATE_DISCONNECTED);
        assertEquals(DisconnectCause.MISSED, call.getDetails().getDisconnectCause().getCode());
    }

    private void setupIncomingCallWithCallScreening() throws Exception {
        CallScreeningServiceCallbacks callback = new CallScreeningServiceCallbacks() {
            @Override
            public void onScreenCall(Details callDetails) {
                getService().respondToCall(callDetails, new CallResponse.Builder()
                        .setDisallowCall(false)
                        .setShouldScreenCallViaAudioProcessing(true)
                        .build());
                lock.release();
            }
        };
        MockCallScreeningService.enableService(mContext);
        MockCallScreeningService.setCallbacks(callback);
        Bundle extras = new Bundle();
        extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, createTestNumber());
        mTelecomManager.addNewIncomingCall(TEST_PHONE_ACCOUNT_HANDLE, extras);

        if (!callback.lock.tryAcquire(TestUtils.WAIT_FOR_CALL_ADDED_TIMEOUT_S,
                TimeUnit.SECONDS)) {
            fail("Call screening service never got the call");
        }

    }

    private ICtsApi29InCallServiceControl setUpControl() throws Exception {
        TestUtils.executeShellCommand(getInstrumentation(),
                "telecom add-or-remove-call-companion-app " + CtsApi29InCallService.PACKAGE_NAME
                        + " 1");

        Intent bindIntent = new Intent(CtsApi29InCallServiceControl.CONTROL_INTERFACE_ACTION);
        ComponentName controlComponentName =
                ComponentName.createRelative(
                        CtsApi29InCallServiceControl.class.getPackage().getName(),
                        CtsApi29InCallServiceControl.class.getName());

        bindIntent.setComponent(controlComponentName);
        LinkedBlockingQueue<ICtsApi29InCallServiceControl> result = new LinkedBlockingQueue<>(1);

        mApiCompatControlServiceConnection = new ServiceConnection() {
            @Override
            public void onServiceConnected(ComponentName name, IBinder service) {
                Log.i(LOG_TAG, "Service Connected: " + name);
                result.offer(ICtsApi29InCallServiceControl.Stub.asInterface(service));
            }

            @Override
            public void onServiceDisconnected(ComponentName name) {
            }
        };

        boolean success = mContext.bindService(bindIntent,
                mApiCompatControlServiceConnection, Context.BIND_AUTO_CREATE);

        if (!success) {
            fail("Failed to get control interface -- bind error");
        }
        return result.poll(TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS, TimeUnit.MILLISECONDS);
    }

    private void tearDownControl() throws Exception {
        mContext.unbindService(mApiCompatControlServiceConnection);

        TestUtils.executeShellCommand(getInstrumentation(),
                "telecom add-or-remove-call-companion-app " + CtsApi29InCallService.PACKAGE_NAME
                        + " 0");
    }

    private void clearRoleHoldersAsUser(String roleName)
            throws Exception {
        UserHandle user = Process.myUserHandle();
        Executor executor = mContext.getMainExecutor();
        LinkedBlockingQueue<Boolean> queue = new LinkedBlockingQueue(1);

        runWithShellPermissionIdentity(() -> mRoleManager.clearRoleHoldersAsUser(roleName,
                RoleManager.MANAGE_HOLDERS_FLAG_DONT_KILL_APP, user, executor,
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
}
