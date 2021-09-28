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

import static android.telecom.cts.TestUtils.*;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.content.ComponentName;
import android.content.Context;
import android.media.AudioManager;
import android.net.Uri;
import android.telecom.Call;
import android.telecom.Connection;
import android.telecom.ConnectionService;
import android.telecom.PhoneAccountHandle;

import androidx.test.InstrumentationRegistry;

import java.util.Collection;

/**
 * Test some additional {@link ConnectionService} and {@link Connection} APIs not already covered
 * by other tests.
 */
public class ConnectionServiceTest extends BaseTelecomTestWithMockServices {

    private static final Uri SELF_MANAGED_TEST_ADDRESS =
            Uri.fromParts("sip", "call1@test.com", null);

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
        if (mShouldTestTelecom) {
            setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);
            mTelecomManager.registerPhoneAccount(TestUtils.TEST_SELF_MANAGED_PHONE_ACCOUNT_1);
        }
    }

    public void testAddExistingConnection() {
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            placeAndVerifyCall();
            verifyConnectionForOutgoingCall();

            // Add second connection (add existing connection)
            final MockConnection connection = new MockConnection();
            connection.setOnHold();
            CtsConnectionService.addExistingConnectionToTelecom(TEST_PHONE_ACCOUNT_HANDLE,
                            connection);
            assertNumCalls(mInCallCallbacks.getService(), 2);
            mInCallCallbacks.lock.drainPermits();
            final Call call = mInCallCallbacks.getService().getLastCall();
            assertCallState(call, Call.STATE_HOLDING);
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    public void testAddExistingConnection_invalidPhoneAccountPackageName() {
        if (!mShouldTestTelecom) {
            return;
        }

        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // Add second connection (add existing connection)
        final MockConnection connection = new MockConnection();
        connection.setOnHold();
        ComponentName invalidName = new ComponentName("com.android.phone",
                "com.android.services.telephony.TelephonyConnectionService");
        // This command will fail and a SecurityException will be thrown by Telecom. The Exception
        // will then be absorbed by the ConnectionServiceAdapter.
        runWithShellPermissionIdentity(() ->
                CtsConnectionService.addExistingConnectionToTelecom(
                        new PhoneAccountHandle(invalidName, "Test"), connection));
        // Make sure that only the original Call exists.
        assertNumCalls(mInCallCallbacks.getService(), 1);
        mInCallCallbacks.lock.drainPermits();
        final Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_DIALING);
    }

    public void testAddExistingConnection_invalidPhoneAccountAccountId() {
        if (!mShouldTestTelecom) {
            return;
        }

        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // Add second connection (add existing connection)
        final MockConnection connection = new MockConnection();
        connection.setOnHold();
        ComponentName validName = new ComponentName(PACKAGE, COMPONENT);
        // This command will fail because the PhoneAccount is not registered to Telecom currently.
        runWithShellPermissionIdentity(() ->
                CtsConnectionService.addExistingConnectionToTelecom(
                        new PhoneAccountHandle(validName, "Invalid Account Id"), connection));
        // Make sure that only the original Call exists.
        assertNumCalls(mInCallCallbacks.getService(), 1);
        mInCallCallbacks.lock.drainPermits();
        final Call call = mInCallCallbacks.getService().getLastCall();
        assertCallState(call, Call.STATE_DIALING);
    }

    public void testVoipAudioModePropagation() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        placeAndVerifyCall();
        MockConnection connection = verifyConnectionForOutgoingCall();
        connection.setAudioModeIsVoip(true);
        waitOnAllHandlers(getInstrumentation());

        AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return AudioManager.MODE_IN_COMMUNICATION;
                    }

                    @Override
                    public Object actual() {
                        return audioManager.getMode();
                    }
                },
                WAIT_FOR_STATE_CHANGE_TIMEOUT_MS, "wait for mode in-communication"
        );

        connection.setAudioModeIsVoip(false);
        waitOnAllHandlers(getInstrumentation());
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return AudioManager.MODE_IN_CALL;
                    }

                    @Override
                    public Object actual() {
                        return audioManager.getMode();
                    }
                },
                WAIT_FOR_STATE_CHANGE_TIMEOUT_MS, "wait for mode in-call"
        );
    }

    public void testConnectionServiceFocusGainedWithNoConnectionService() {
        if (!mShouldTestTelecom) {
            return;
        }

        // WHEN place a managed call
        placeAndVerifyCall();

        // THEN managed connection service has gained the focus
        assertTrue(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_GAINED));
    }

    public void testConnectionServiceFocusGainedWithSameConnectionService() {
        if (!mShouldTestTelecom) {
            return;
        }

        // GIVEN a managed call
        placeAndVerifyCall();
        Connection outgoing = verifyConnectionForOutgoingCall();
        outgoing.setActive();
        assertTrue(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_GAINED));
        assertCallState(mInCallCallbacks.getService().getLastCall(), Call.STATE_ACTIVE);

        // WHEN place another call has the same ConnectionService as the existing call
        placeAndVerifyCall();
        verifyConnectionForOutgoingCall();

        // THEN the ConnectionService has not gained the focus again
        assertFalse(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_GAINED));
        // and the ConnectionService didn't lose the focus
        assertFalse(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_LOST));
    }

    public void testConnectionServiceFocusGainedWithDifferentConnectionService() {
        if (!mShouldTestTelecom) {
            return;
        }

        // GIVEN an existing managed call
        placeAndVerifyCall();
        verifyConnectionForOutgoingCall().setActive();
        assertTrue(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_GAINED));

        // WHEN a self-managed call is coming
        SelfManagedConnection selfManagedConnection =
                addIncomingSelfManagedCall(TEST_SELF_MANAGED_HANDLE_1, SELF_MANAGED_TEST_ADDRESS);

        // THEN the managed ConnectionService has lost the focus
        assertTrue(connectionService.waitForEvent(
                MockConnectionService.EVENT_CONNECTION_SERVICE_FOCUS_LOST));
        // and the self-managed ConnectionService has gained the focus
        assertTrue(CtsSelfManagedConnectionService.getConnectionService().waitForUpdate(
                        CtsSelfManagedConnectionService.FOCUS_GAINED_LOCK));

        // Disconnected the self-managed call
        selfManagedConnection.disconnectAndDestroy();
    }

    private SelfManagedConnection addIncomingSelfManagedCall(
            PhoneAccountHandle pah, Uri address) {

        TestUtils.addIncomingCall(getInstrumentation(), mTelecomManager, pah, address);

        // Ensure Telecom bound to the self managed CS
        if (!CtsSelfManagedConnectionService.waitForBinding()) {
            fail("Could not bind to Self-Managed ConnectionService");
        }

        SelfManagedConnection connection = TestUtils.waitForAndGetConnection(address);

        // Active the call
        connection.setActive();

        return connection;
    }

    public void testCallDirectionIncoming() {
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            // Need to add a call to ensure ConnectionService is up and bound.
            placeAndVerifyCall();
            verifyConnectionForOutgoingCall().setActive();

            final MockConnection connection = new MockConnection();
            connection.setActive();
            connection.setCallDirection(Call.Details.DIRECTION_INCOMING);
            CtsConnectionService.addExistingConnectionToTelecom(TEST_PHONE_ACCOUNT_HANDLE,
                    connection);
            assertNumCalls(mInCallCallbacks.getService(), 2);
            mInCallCallbacks.lock.drainPermits();
            final Call call = mInCallCallbacks.getService().getLastCall();
            assertCallState(call, Call.STATE_ACTIVE);
            assertEquals(Call.Details.DIRECTION_INCOMING, call.getDetails().getCallDirection());
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }

    }

    public void testCallDirectionOutgoing() {
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            // Ensure CS is up and bound.
            placeAndVerifyCall();
            verifyConnectionForOutgoingCall().setActive();

            final MockConnection connection = new MockConnection();
            connection.setActive();
            connection.setCallDirection(Call.Details.DIRECTION_OUTGOING);
            connection.setConnectTimeMillis(1000L);
            assertEquals(1000L, connection.getConnectTimeMillis());
            connection.setConnectionStartElapsedRealtimeMillis(100L);
            assertEquals(100L, connection.getConnectionStartElapsedRealtimeMillis());

            CtsConnectionService.addExistingConnectionToTelecom(TEST_PHONE_ACCOUNT_HANDLE,
                    connection);
            assertNumCalls(mInCallCallbacks.getService(), 2);
            mInCallCallbacks.lock.drainPermits();
            final Call call = mInCallCallbacks.getService().getLastCall();
            assertCallState(call, Call.STATE_ACTIVE);
            assertEquals(Call.Details.DIRECTION_OUTGOING, call.getDetails().getCallDirection());
            assertEquals(1000L, call.getDetails().getConnectTimeMillis());
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }

    public void testGetAllConnections() {
        if (!mShouldTestTelecom) {
            return;
        }

        InstrumentationRegistry.getInstrumentation().getUiAutomation()
                .adoptShellPermissionIdentity("android.permission.MODIFY_PHONE_STATE");
        try {
            // Add first connection (outgoing call)
            placeAndVerifyCall();
            final Connection connection1 = verifyConnectionForOutgoingCall();

            Collection<Connection> connections =
                    CtsConnectionService.getAllConnectionsFromTelecom();
            assertEquals(1, connections.size());
            assertTrue(connections.contains(connection1));
            // Need to move this to active since we reject the 3rd incoming call below if this is in
            // dialing state (b/23428950).
            connection1.setActive();
            assertCallState(mInCallCallbacks.getService().getLastCall(), Call.STATE_ACTIVE);

            // Add second connection (add existing connection)
            final Connection connection2 = new MockConnection();
            connection2.setActive();
            CtsConnectionService.addExistingConnectionToTelecom(TEST_PHONE_ACCOUNT_HANDLE,
                            connection2);
            assertNumCalls(mInCallCallbacks.getService(), 2);
            mInCallCallbacks.lock.drainPermits();
            connections = CtsConnectionService.getAllConnectionsFromTelecom();
            assertEquals(2, connections.size());
            assertTrue(connections.contains(connection2));

            // Add third connection (incoming call)
            addAndVerifyNewIncomingCall(createTestNumber(), null);
            final Connection connection3 = verifyConnectionForIncomingCall();
            connections = CtsConnectionService.getAllConnectionsFromTelecom();
            assertEquals(3, connections.size());
            assertTrue(connections.contains(connection3));
        } finally {
            InstrumentationRegistry.getInstrumentation().getUiAutomation()
                    .dropShellPermissionIdentity();
        }
    }
}
