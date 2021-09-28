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
 * limitations under the License
 */

package android.telecom.cts;

import static android.telecom.Connection.PROPERTY_HIGH_DEF_AUDIO;
import static android.telecom.Connection.PROPERTY_WIFI;
import static android.telecom.cts.TestUtils.*;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import static org.hamcrest.CoreMatchers.instanceOf;
import static org.junit.Assert.assertNotEquals;
import static org.junit.Assert.assertThat;

import android.content.Context;
import android.database.Cursor;
import android.graphics.drawable.Icon;
import android.media.AudioManager;
import android.os.Bundle;
import android.net.Uri;
import android.provider.CallLog;
import android.telecom.Call;
import android.telecom.Connection;
import android.telecom.ConnectionRequest;
import android.telecom.DisconnectCause;
import android.telecom.GatewayInfo;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.StatusHints;
import android.telecom.TelecomManager;
import android.telephony.TelephonyManager;
import android.text.TextUtils;

import java.util.Arrays;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

/**
 * Suites of tests that verifies the various Call details.
 */
public class CallDetailsTest extends BaseTelecomTestWithMockServices {

    public static final int CONNECTION_PROPERTIES =  PROPERTY_HIGH_DEF_AUDIO | PROPERTY_WIFI;
    public static final int CONNECTION_CAPABILITIES =
            Connection.CAPABILITY_HOLD | Connection.CAPABILITY_MUTE;
    public static final int CALL_CAPABILITIES =
            Call.Details.CAPABILITY_HOLD | Call.Details.CAPABILITY_MUTE;
    public static final int CALL_PROPERTIES =
            Call.Details.PROPERTY_HIGH_DEF_AUDIO | Call.Details.PROPERTY_WIFI;
    public static final String CALLER_DISPLAY_NAME = "CTS test";
    public static final int CALLER_DISPLAY_NAME_PRESENTATION = TelecomManager.PRESENTATION_ALLOWED;
    public static final String TEST_SUBJECT = "test";
    public static final String TEST_CHILD_NUMBER = "650-555-1212";
    public static final String TEST_FORWARDED_NUMBER = "650-555-1212";
    public static final String TEST_EXTRA_KEY = "android.test.extra.TEST";
    public static final String TEST_EXTRA_KEY2 = "android.test.extra.TEST2";
    public static final String TEST_EXTRA_KEY3 = "android.test.extra.TEST3";
    public static final String TEST_INVALID_EXTRA_KEY = "blah";
    public static final int TEST_EXTRA_VALUE = 10;
    public static final String TEST_EVENT = "com.test.event.TEST";
    public static final Uri TEST_DEFLECT_URI = Uri.fromParts("tel", "+16505551212", null);
    private static final int ASYNC_TIMEOUT = 10000;
    private StatusHints mStatusHints;
    private Bundle mExtras = new Bundle();

    private MockInCallService mInCallService;
    private Call mCall;
    private MockConnection mConnection;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        if (mShouldTestTelecom) {
            PhoneAccount account = setupConnectionService(
                    new MockConnectionService() {
                        @Override
                        public Connection onCreateOutgoingConnection(
                                PhoneAccountHandle connectionManagerPhoneAccount,
                                ConnectionRequest request) {
                            Connection connection = super.onCreateOutgoingConnection(
                                    connectionManagerPhoneAccount,
                                    request);
                            mConnection = (MockConnection) connection;
                            // Modify the connection object created with local values.
                            connection.setConnectionCapabilities(CONNECTION_CAPABILITIES);
                            connection.setConnectionProperties(CONNECTION_PROPERTIES);
                            connection.setCallerDisplayName(
                                    CALLER_DISPLAY_NAME,
                                    CALLER_DISPLAY_NAME_PRESENTATION);
                            connection.setExtras(mExtras);
                            mStatusHints = new StatusHints(
                                    "CTS test",
                                    Icon.createWithResource(
                                            getInstrumentation().getContext(),
                                            R.drawable.ic_phone_24dp),
                                            null);
                            connection.setStatusHints(mStatusHints);
                            lock.release();
                            return connection;
                        }
                    }, FLAG_REGISTER | FLAG_ENABLE);

            // Make sure there is another phone account.
            mTelecomManager.registerPhoneAccount(TestUtils.TEST_PHONE_ACCOUNT_2);
            TestUtils.enablePhoneAccount(
                    getInstrumentation(), TestUtils.TEST_PHONE_ACCOUNT_HANDLE_2);

            /** Place a call as a part of the setup before we test the various
             *  Call details.
             */
            placeAndVerifyCall();
            verifyConnectionForOutgoingCall();

            mInCallService = mInCallCallbacks.getService();
            mCall = mInCallService.getLastCall();

            assertCallState(mCall, Call.STATE_DIALING);
        }
    }

    /**
     * Tests whether the getAccountHandle() getter returns the correct object.
     */
    public void testAccountHandle() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getAccountHandle(), instanceOf(PhoneAccountHandle.class));
        assertEquals(TEST_PHONE_ACCOUNT_HANDLE, mCall.getDetails().getAccountHandle());
    }

    /**
     * Tests whether the getCallCapabilities() getter returns the correct object.
     */
    public void testCallCapabilities() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getCallCapabilities(), instanceOf(Integer.class));
        assertEquals(CALL_CAPABILITIES, mCall.getDetails().getCallCapabilities());
        assertTrue(mCall.getDetails().can(Call.Details.CAPABILITY_HOLD));
        assertTrue(mCall.getDetails().can(Call.Details.CAPABILITY_MUTE));
        assertFalse(mCall.getDetails().can(Call.Details.CAPABILITY_MANAGE_CONFERENCE));
        assertFalse(mCall.getDetails().can(Call.Details.CAPABILITY_RESPOND_VIA_TEXT));
    }

    /**
     * Tests propagation of the local video capabilities from telephony through to in-call.
     */
    public void testCallLocalVideoCapability() {
        if (!mShouldTestTelecom) {
            return;
        }

        // Note: Local support for video is disabled when a call is in dialing state.
        mConnection.setConnectionCapabilities(
                Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL);
        assertCallCapabilities(mCall, 0);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_LOCAL_RX);
        assertCallCapabilities(mCall, 0);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_LOCAL_TX);
        assertCallCapabilities(mCall, 0);

        mConnection.setConnectionCapabilities(
                Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL);
        assertCallCapabilities(mCall, 0);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_REMOTE_RX);
        assertCallCapabilities(mCall, 0);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_REMOTE_TX);
        assertCallCapabilities(mCall, 0);

        // Set call active; we expect the capabilities to make it through now.
        mConnection.setActive();

        mConnection.setConnectionCapabilities(
                Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_LOCAL_RX);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_LOCAL_RX);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_LOCAL_TX);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_LOCAL_TX);

        mConnection.setConnectionCapabilities(
                Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_REMOTE_RX);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_REMOTE_RX);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORTS_VT_REMOTE_TX);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORTS_VT_REMOTE_TX);
    }

    /**
     * Tests passing call capabilities from Connections to Calls.
     */
    public void testCallCapabilityPropagation() {
        if (!mShouldTestTelecom) {
            return;
        }

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_CAN_PAUSE_VIDEO);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_CAN_PAUSE_VIDEO);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_HOLD);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_HOLD);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_MANAGE_CONFERENCE);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_MANAGE_CONFERENCE);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_MERGE_CONFERENCE);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_MERGE_CONFERENCE);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_MUTE);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_MUTE);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_RESPOND_VIA_TEXT);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_RESPOND_VIA_TEXT);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SEPARATE_FROM_CONFERENCE);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SEPARATE_FROM_CONFERENCE);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SUPPORT_HOLD);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SUPPORT_HOLD);

        mConnection.setConnectionCapabilities(Connection.CAPABILITY_SWAP_CONFERENCE);
        assertCallCapabilities(mCall, Call.Details.CAPABILITY_SWAP_CONFERENCE);
    }

    /**
     * Tests passing call properties from Connections to Calls.
     */
    public void testCallPropertyPropagation() {
        if (!mShouldTestTelecom) {
            return;
        }

        mConnection.setConnectionProperties(Connection.PROPERTY_EMERGENCY_CALLBACK_MODE);
        assertCallProperties(mCall, Call.Details.PROPERTY_EMERGENCY_CALLBACK_MODE);

        mConnection.setConnectionProperties(Connection.PROPERTY_GENERIC_CONFERENCE);
        assertCallProperties(mCall, Call.Details.PROPERTY_GENERIC_CONFERENCE);

        mConnection.setConnectionProperties(Connection.PROPERTY_HIGH_DEF_AUDIO);
        assertCallProperties(mCall, Call.Details.PROPERTY_HIGH_DEF_AUDIO);

        mConnection.setConnectionProperties(Connection.PROPERTY_WIFI);
        assertCallProperties(mCall, Call.Details.PROPERTY_WIFI);

        mConnection.setConnectionProperties(Connection.PROPERTY_IS_EXTERNAL_CALL);
        assertCallProperties(mCall, Call.Details.PROPERTY_IS_EXTERNAL_CALL);

        mConnection.setConnectionProperties(Connection.PROPERTY_HAS_CDMA_VOICE_PRIVACY);
        assertCallProperties(mCall, Call.Details.PROPERTY_HAS_CDMA_VOICE_PRIVACY);

        mConnection.setConnectionProperties(Connection.PROPERTY_IS_DOWNGRADED_CONFERENCE);
        // Not propagated
        assertCallProperties(mCall, 0);

        mConnection.setConnectionProperties(Connection.PROPERTY_IS_RTT);
        assertCallProperties(mCall, Call.Details.PROPERTY_RTT);

        mConnection.setConnectionProperties(Connection.PROPERTY_ASSISTED_DIALING);
        assertCallProperties(mCall, Call.Details.PROPERTY_ASSISTED_DIALING);

        mConnection.setConnectionProperties(Connection.PROPERTY_NETWORK_IDENTIFIED_EMERGENCY_CALL);
        assertCallProperties(mCall, Call.Details.PROPERTY_NETWORK_IDENTIFIED_EMERGENCY_CALL);

        mConnection.setConnectionProperties(Connection.PROPERTY_REMOTELY_HOSTED);
        // Not propagated
        assertCallProperties(mCall, 0);
    }

    /**
     * Tests whether the getCallerDisplayName() getter returns the correct object.
     */
    public void testCallerDisplayName() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getCallerDisplayName(), instanceOf(String.class));
        assertEquals(CALLER_DISPLAY_NAME, mCall.getDetails().getCallerDisplayName());
    }

    /**
     * Tests whether the getCallerDisplayNamePresentation() getter returns the correct object.
     */
    public void testCallerDisplayNamePresentation() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getCallerDisplayNamePresentation(), instanceOf(Integer.class));
        assertEquals(CALLER_DISPLAY_NAME_PRESENTATION, mCall.getDetails().getCallerDisplayNamePresentation());
    }

    /**
     * Test the contacts display name. We don't have anything set up in contacts, so expect it to
     * be null
     */
    public void testContactDisplayName() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertTrue(TextUtils.isEmpty(mCall.getDetails().getContactDisplayName()));
    }

    /**
     * Tests whether the getCallProperties() getter returns the correct object.
     */
    public void testCallProperties() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getCallProperties(), instanceOf(Integer.class));

        assertEquals(CALL_PROPERTIES, mCall.getDetails().getCallProperties());
    }

    /**
     * Tests whether the getConnectTimeMillis() getter returns the correct object.
     */
    public void testConnectTimeMillis() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getConnectTimeMillis(), instanceOf(Long.class));
    }

    /**
     * Tests whether the getCreationTimeMillis() getter returns the correct object.
     */
    public void testCreationTimeMillis() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getCreationTimeMillis(), instanceOf(Long.class));
    }

    /**
     * Tests whether the getDisconnectCause() getter returns the correct object.
     */
    public void testDisconnectCause() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getDisconnectCause(), instanceOf(DisconnectCause.class));
    }

    /**
     * Tests whether the getExtras() getter returns the correct object.
     */
    public void testExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        if (mCall.getDetails().getExtras() != null) {
            assertThat(mCall.getDetails().getExtras(), instanceOf(Bundle.class));
        }
    }

    /**
     * Tests whether the getIntentExtras() getter returns the correct object.
     */
    public void testIntentExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getIntentExtras(), instanceOf(Bundle.class));
    }

    /**
     * Tests whether the getGatewayInfo() getter returns the correct object.
     */
    public void testGatewayInfo() {
        if (!mShouldTestTelecom) {
            return;
        }

        if (mCall.getDetails().getGatewayInfo() != null) {
            assertThat(mCall.getDetails().getGatewayInfo(), instanceOf(GatewayInfo.class));
        }
    }

    /**
     * Tests whether the getHandle() getter returns the correct object.
     */
    public void testHandle() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getHandle(), instanceOf(Uri.class));
        assertEquals(getTestNumber(), mCall.getDetails().getHandle());
    }

    /**
     * Tests whether the getHandlePresentation() getter returns the correct object.
     */
    public void testHandlePresentation() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getHandlePresentation(), instanceOf(Integer.class));
        assertEquals(MockConnectionService.CONNECTION_PRESENTATION, mCall.getDetails().getHandlePresentation());
    }

    /**
     * Tests whether the getStatusHints() getter returns the correct object.
     */
    public void testStatusHints() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getStatusHints(), instanceOf(StatusHints.class));
        assertEquals(mStatusHints.getLabel(), mCall.getDetails().getStatusHints().getLabel());
        assertEquals(
                mStatusHints.getIcon().toString(),
                mCall.getDetails().getStatusHints().getIcon().toString());
        assertEquals(mStatusHints.getExtras(), mCall.getDetails().getStatusHints().getExtras());
    }

    /**
     * Tests whether the getVideoState() getter returns the correct object.
     */
    public void testVideoState() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertThat(mCall.getDetails().getVideoState(), instanceOf(Integer.class));
    }

    /**
     * Tests communication of {@link Connection#setExtras(Bundle)} through to
     * {@link Call.Details#getExtras()}.
     */
    public void testExtrasPropagation() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle exampleExtras = new Bundle();
        exampleExtras.putString(Connection.EXTRA_CALL_SUBJECT, TEST_SUBJECT);
        exampleExtras.putString(Connection.EXTRA_CHILD_ADDRESS, TEST_CHILD_NUMBER);
        exampleExtras.putString(Connection.EXTRA_LAST_FORWARDED_NUMBER, TEST_FORWARDED_NUMBER);
        exampleExtras.putInt(TEST_EXTRA_KEY, TEST_EXTRA_VALUE);
        exampleExtras.putInt(Connection.EXTRA_AUDIO_CODEC, Connection.AUDIO_CODEC_AMR);
        mConnection.setExtras(exampleExtras);

        // Make sure we got back a bundle with the call subject key set.
        assertCallExtras(mCall, Connection.EXTRA_CALL_SUBJECT);

        Bundle callExtras = mCall.getDetails().getExtras();
        assertEquals(TEST_SUBJECT, callExtras.getString(Connection.EXTRA_CALL_SUBJECT));
        assertEquals(TEST_CHILD_NUMBER, callExtras.getString(Connection.EXTRA_CHILD_ADDRESS));
        assertEquals(TEST_FORWARDED_NUMBER,
                callExtras.getString(Connection.EXTRA_LAST_FORWARDED_NUMBER));
        assertEquals(TEST_EXTRA_VALUE, callExtras.getInt(TEST_EXTRA_KEY));
        assertEquals(Connection.AUDIO_CODEC_AMR,
                callExtras.getInt(Connection.EXTRA_AUDIO_CODEC));
    }

    /**
     * Tests that extra keys outside of the standard android.* namespace are not sent to a third
     * party InCallService (the CTS test InCallService is technically not the OEM stock dialer).
     */
    public void testExtrasBlocking() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle exampleExtras = new Bundle();
        exampleExtras.putString(Connection.EXTRA_CALL_SUBJECT, TEST_SUBJECT);
        exampleExtras.putString(TEST_INVALID_EXTRA_KEY, "Secrets!");
        exampleExtras.putString(Connection.EXTRA_SIP_INVITE, "Sip headers!!");
        // Some ImsCallProfile system API extra keys we don't want to exposure to non-system
        // dialer apps; its a bit of an implementation detail but good to verify that these are
        // never exposed unintentionally.
        exampleExtras.putString("oi", "5551212");
        exampleExtras.putString("cna", "Joe");
        exampleExtras.putInt("oir", TelecomManager.PRESENTATION_ALLOWED);
        exampleExtras.putInt("cnap", TelecomManager.PRESENTATION_ALLOWED);

        mConnection.setExtras(exampleExtras);

        // Make sure we got back a bundle with the call subject key set.
        assertCallExtras(mCall, Connection.EXTRA_CALL_SUBJECT);

        Bundle callExtras = mCall.getDetails().getExtras();
        assertEquals(TEST_SUBJECT, callExtras.getString(Connection.EXTRA_CALL_SUBJECT));
        assertFalse(callExtras.containsKey(TEST_INVALID_EXTRA_KEY));
        assertFalse(callExtras.containsKey(Connection.EXTRA_SIP_INVITE));
        // Some ImsCallProfile extra keys that should not be exposed to non-system dialers.
        assertFalse(callExtras.containsKey("oi"));
        assertFalse(callExtras.containsKey("cna"));
        assertFalse(callExtras.containsKey("oir"));
        assertFalse(callExtras.containsKey("cnap"));
    }

    /**
     * Tests that {@link Connection} extras changes made via {@link Connection#putExtras(Bundle)}
     * are propagated to the {@link Call} via
     * {@link android.telecom.Call.Callback#onDetailsChanged(Call, Call.Details)}.
     */
    public void testConnectionPutExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle testBundle = new Bundle();
        testBundle.putString(TEST_EXTRA_KEY, TEST_SUBJECT);
        testBundle.putInt(TEST_EXTRA_KEY2, TEST_EXTRA_VALUE);
        testBundle.putBoolean(Connection.EXTRA_DISABLE_ADD_CALL, true);
        mConnection.putExtras(testBundle);
        // Wait for the 2nd invocation; setExtras is called in the setup method.
        mOnExtrasChangedCounter.waitForCount(2, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);

        Bundle extras = mCall.getDetails().getExtras();
        assertTrue(extras.containsKey(TEST_EXTRA_KEY));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY2));
        assertEquals(TEST_EXTRA_VALUE, extras.getInt(TEST_EXTRA_KEY2));
        assertTrue(extras.getBoolean(Connection.EXTRA_DISABLE_ADD_CALL));
    }

    public void testConnectionHoldTone() {
        if (!mShouldTestTelecom) {
            return;
        }

        // These tests are admittedly lame; the events are absorbed in telecom and start a tone and
        // stop it.  There is no other signal that the tone is starting/stopping otherwise.
        mConnection.sendConnectionEvent(Connection.EVENT_ON_HOLD_TONE_START, null);
        mConnection.sendConnectionEvent(Connection.EVENT_ON_HOLD_TONE_END, null);
    }

    /**
     * Tests that {@link Connection} extras changes made via {@link Connection#removeExtras(List)}
     * are propagated to the {@link Call} via
     * {@link android.telecom.Call.Callback#onDetailsChanged(Call, Call.Details)}.
     */
    public void testConnectionRemoveExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        testConnectionPutExtras();

        mConnection.removeExtras(Arrays.asList(TEST_EXTRA_KEY));
        verifyRemoveConnectionExtras();
    }

    /**
     * Tests that {@link Connection} extras changes made via {@link Connection#removeExtras(List)}
     * are propagated to the {@link Call} via
     * {@link android.telecom.Call.Callback#onDetailsChanged(Call, Call.Details)}.
     */
    public void testConnectionRemoveExtras2() {
        if (!mShouldTestTelecom) {
            return;
        }

        testConnectionPutExtras();

        mConnection.removeExtras(TEST_EXTRA_KEY);
        // testConnectionPutExtra will have waited for the 2nd invocation, so wait for the 3rd here.
        verifyRemoveConnectionExtras();
    }

    private void verifyRemoveConnectionExtras() {
        // testConnectionPutExtra will have waited for the 2nd invocation, so wait for the 3rd here.
        mOnExtrasChangedCounter.waitForCount(3, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);

        Bundle extras = mCall.getDetails().getExtras();
        assertFalse(extras.containsKey(TEST_EXTRA_KEY));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY2));
        assertEquals(TEST_EXTRA_VALUE, extras.getInt(TEST_EXTRA_KEY2));
    }

    /**
     * Tests that {@link Call} extras changes made via {@link Call#putExtras(Bundle)} are propagated
     * to {@link Connection#onExtrasChanged(Bundle)}.
     */
    public void testCallPutExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle testBundle = new Bundle();
        testBundle.putString(TEST_EXTRA_KEY, TEST_SUBJECT);
        final InvokeCounter counter = mConnection.getInvokeCounter(
                MockConnection.ON_EXTRAS_CHANGED);
        mCall.putExtras(testBundle);
        counter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        Bundle extras = mConnection.getExtras();

        assertNotNull(extras);
        assertTrue(extras.containsKey(TEST_EXTRA_KEY));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY));
    }

    /**
     * Tests that {@link Call} extra operations using {@link Call#removeExtras(List)} are propagated
     * to the {@link Connection} via {@link Connection#onExtrasChanged(Bundle)}.
     *
     * This test specifically tests addition and removal of extras values.
     */
    public void testCallRemoveExtras() {
        if (!mShouldTestTelecom) {
            return;
        }

        final InvokeCounter counter = setupCallExtras();
        Bundle extras;

        mCall.removeExtras(Arrays.asList(TEST_EXTRA_KEY));
        counter.waitForCount(2, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        extras = mConnection.getExtras();
        assertNotNull(extras);
        assertFalse(extras.containsKey(TEST_EXTRA_KEY));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY2));
        assertEquals(TEST_EXTRA_VALUE, extras.getInt(TEST_EXTRA_KEY2));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY3));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY3));

        mCall.removeExtras(Arrays.asList(TEST_EXTRA_KEY2, TEST_EXTRA_KEY3));
        counter.waitForCount(3, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        extras = mConnection.getExtras();
        assertFalse(extras.containsKey(TEST_EXTRA_KEY2));
        assertFalse(extras.containsKey(TEST_EXTRA_KEY3));
    }

    /**
     * Tests that {@link Call} extra operations using {@link Call#removeExtras(String[])} are
     * propagated to the {@link Connection} via {@link Connection#onExtrasChanged(Bundle)}.
     *
     * This test specifically tests addition and removal of extras values.
     */
    public void testCallRemoveExtras2() {
        if (!mShouldTestTelecom) {
            return;
        }

        final InvokeCounter counter = setupCallExtras();
        Bundle extras;

        mCall.removeExtras(TEST_EXTRA_KEY);
        counter.waitForCount(2, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        extras = mConnection.getExtras();
        assertNotNull(extras);
        assertFalse(extras.containsKey(TEST_EXTRA_KEY));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY2));
        assertEquals(TEST_EXTRA_VALUE, extras.getInt(TEST_EXTRA_KEY2));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY3));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY3));
    }

    private InvokeCounter setupCallExtras() {
        Bundle testBundle = new Bundle();
        testBundle.putString(TEST_EXTRA_KEY, TEST_SUBJECT);
        testBundle.putInt(TEST_EXTRA_KEY2, TEST_EXTRA_VALUE);
        testBundle.putString(TEST_EXTRA_KEY3, TEST_SUBJECT);
        final InvokeCounter counter = mConnection.getInvokeCounter(
                MockConnection.ON_EXTRAS_CHANGED);
        mCall.putExtras(testBundle);
        counter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        Bundle extras = mConnection.getExtras();

        assertNotNull(extras);
        assertTrue(extras.containsKey(TEST_EXTRA_KEY));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY2));
        assertEquals(TEST_EXTRA_VALUE, extras.getInt(TEST_EXTRA_KEY2));
        assertTrue(extras.containsKey(TEST_EXTRA_KEY3));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY3));
        return counter;
    }

    /**
     * Tests that {@link Connection} events are propagated from
     * {@link Connection#sendConnectionEvent(String, Bundle)} to
     * {@link android.telecom.Call.Callback#onConnectionEvent(Call, String, Bundle)}.
     */
    public void testConnectionEvent() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle testBundle = new Bundle();
        testBundle.putString(TEST_EXTRA_KEY, TEST_SUBJECT);

        mConnection.sendConnectionEvent(Connection.EVENT_CALL_PULL_FAILED, testBundle);
        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        String event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        Bundle extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);

        assertEquals(Connection.EVENT_CALL_PULL_FAILED, event);
        assertNotNull(extras);
        assertTrue(extras.containsKey(TEST_EXTRA_KEY));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY));
        mOnConnectionEventCounter.reset();

        mConnection.sendConnectionEvent(Connection.EVENT_CALL_HOLD_FAILED, null);
        // Don't expect this to make it through; used internally in Telecom.

        mConnection.sendConnectionEvent(Connection.EVENT_MERGE_START, null);
        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);
        assertEquals(Connection.EVENT_MERGE_START, event);
        assertNull(extras);
        mOnConnectionEventCounter.reset();

        mConnection.sendConnectionEvent(Connection.EVENT_MERGE_COMPLETE, null);
        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);
        assertEquals(Connection.EVENT_MERGE_COMPLETE, event);
        assertNull(extras);
        mOnConnectionEventCounter.reset();

        mConnection.sendConnectionEvent(Connection.EVENT_CALL_REMOTELY_HELD, null);
        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);
        assertEquals(Connection.EVENT_CALL_REMOTELY_HELD, event);
        assertNull(extras);
        mOnConnectionEventCounter.reset();

        mConnection.sendConnectionEvent(Connection.EVENT_CALL_REMOTELY_UNHELD, null);
        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);
        assertEquals(Connection.EVENT_CALL_REMOTELY_UNHELD, event);
        assertNull(extras);
        mOnConnectionEventCounter.reset();
    }

    /**
     * Verifies that a request to deflect a ringing {@link Call} is relayed to a {@link Connection}.
     */
    public void testDeflect() {
        if (!mShouldTestTelecom) {
            return;
        }
        // Only ringing calls support deflection
        mConnection.setRinging();
        assertCallState(mCall, Call.STATE_RINGING);

        final InvokeCounter counter = mConnection.getInvokeCounter(MockConnection.ON_DEFLECT);
        mCall.deflect(TEST_DEFLECT_URI);
        counter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        Uri address = (Uri) (counter.getArgs(0)[0]);

        assertEquals(TEST_DEFLECT_URI, address);
    }

    /**
     * Tests that {@link Call} events are propagated from {@link Call#sendCallEvent(String, Bundle)}
     * to {@link Connection#onCallEvent(String, Bundle)}.
     */
    public void testCallEvent() {
        if (!mShouldTestTelecom) {
            return;
        }

        Bundle testBundle = new Bundle();
        testBundle.putString(TEST_EXTRA_KEY, TEST_SUBJECT);
        final InvokeCounter counter = mConnection.getInvokeCounter(MockConnection.ON_CALL_EVENT);
        mCall.sendCallEvent(TEST_EVENT, testBundle);
        counter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);

        String event = (String) (counter.getArgs(0)[0]);
        Bundle extras = (Bundle) (counter.getArgs(0)[1]);

        assertEquals(TEST_EVENT, event);
        assertNotNull(extras);
        assertTrue(extras.containsKey(TEST_EXTRA_KEY));
        assertEquals(TEST_SUBJECT, extras.getString(TEST_EXTRA_KEY));
    }

    /**
     * Verifies that the AudioManager audio mode changes as expected based on whether a connection
     * is using voip audio mode or not.
     */
    public void testSetVoipAudioMode() {
        if (!mShouldTestTelecom) {
            return;
        }
        mConnection.setAudioModeIsVoip(true);
        assertCallProperties(mCall, Call.Details.PROPERTY_VOIP_AUDIO_MODE);
        AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        assertAudioMode(audioManager, AudioManager.MODE_IN_COMMUNICATION);

        mConnection.setAudioModeIsVoip(false);
        assertDoesNotHaveCallProperties(mCall, Call.Details.PROPERTY_VOIP_AUDIO_MODE);
        assertAudioMode(audioManager, AudioManager.MODE_IN_CALL);
    }

    /**
     * Tests whether the getCallDirection() getter returns correct call direction.
     */
    public void testGetCallDirection() {
        if (!mShouldTestTelecom) {
            return;
        }

        assertEquals(Call.Details.DIRECTION_OUTGOING, mCall.getDetails().getCallDirection());
        assertFalse(Call.Details.DIRECTION_INCOMING == mCall.getDetails().getCallDirection());
        assertFalse(Call.Details.DIRECTION_UNKNOWN == mCall.getDetails().getCallDirection());
    }

    /**
     * Asserts that a call's extras contain a specified key.
     *
     * @param call The call.
     * @param expectedKey The expected extras key.
     */
    private void assertCallExtras(final Call call, final String expectedKey) {
        waitUntilConditionIsTrueOrTimeout(
                new Condition() {
                    @Override
                    public Object expected() {
                        return expectedKey;
                    }

                    @Override
                    public Object actual() {
                        return call.getDetails().getExtras().containsKey(expectedKey) ? expectedKey
                                : "";
                    }
                },
                TestUtils.WAIT_FOR_STATE_CHANGE_TIMEOUT_MS,
                "Call should have extras key " + expectedKey
        );
    }

    /**
     * Tests whether the CallLogManager logs the features of a call(HD call, Wifi call, VoLTE)
     * correctly.
     */
    public void testLogFeatures() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        // Register content observer on call log and get latch
        CountDownLatch callLogEntryLatch = getCallLogEntryLatch();

        Bundle testBundle = new Bundle();
        testBundle.putInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE,
                          TelephonyManager.NETWORK_TYPE_LTE);
        mConnection.putExtras(testBundle);
        // Wait for the 2nd invocation; setExtras is called in the setup method.
        mOnExtrasChangedCounter.waitForCount(2, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);

        Bundle extra = mCall.getDetails().getExtras();

        mCall.disconnect();

        // Wait on the call log latch.
        callLogEntryLatch.await(ASYNC_TIMEOUT, TimeUnit.MILLISECONDS);

        // Verify the contents of the call log
        Cursor callsCursor = mContext.getContentResolver().query(CallLog.Calls.CONTENT_URI, null,
                null, null, "_id DESC");
        callsCursor.moveToFirst();
        int features = callsCursor.getInt(callsCursor.getColumnIndex("features"));
        assertEquals(CallLog.Calls.FEATURES_HD_CALL,
                features & CallLog.Calls.FEATURES_HD_CALL);
        assertEquals(CallLog.Calls.FEATURES_WIFI, features & CallLog.Calls.FEATURES_WIFI);
        assertEquals(CallLog.Calls.FEATURES_VOLTE, features & CallLog.Calls.FEATURES_VOLTE);
    }

    /**
     * Verifies operation of the test telecom call ID system APIs.
     */
    public void testTelecomCallId() {
        if (!mShouldTestTelecom) {
            return;
        }
        mConnection.setTelecomCallId("Hello");
        assertEquals("Hello", mConnection.getTelecomCallId());
    }

    /**
     * Verifies propagation of radio tech extra.
     */
    public void testSetCallRadioTech() {
        if (!mShouldTestTelecom) {
            return;
        }
        Bundle radioTechExtras = new Bundle();
        radioTechExtras.putInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE,
                TelephonyManager.NETWORK_TYPE_LTE);
        mConnection.putExtras(radioTechExtras);

        assertCallExtras(mCall, TelecomManager.EXTRA_CALL_NETWORK_TYPE);
        assertEquals(TelephonyManager.NETWORK_TYPE_LTE,
                mCall.getDetails().getExtras().getInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE));
    }

    /**
     * Verifies resetting the connection time.
     */
    public void testResetConnectionTime() {
        if (!mShouldTestTelecom) {
            return;
        }

        long currentTime = mConnection.getConnectTimeMillis();
        mConnection.resetConnectionTime();

        // Make sure the connect time isn't the original value.
        assertCallConnectTimeChanged(mCall, currentTime);
    }

    /**
     * Verifies {@link Connection#notifyConferenceMergeFailed()} results in the
     * {@link Connection#EVENT_CALL_MERGE_FAILED} connection event being received by the telecom
     * call.
     */
    public void testMergeFail() {
        if (!mShouldTestTelecom) {
            return;
        }

        mConnection.notifyConferenceMergeFailed();

        mOnConnectionEventCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        String event = (String) (mOnConnectionEventCounter.getArgs(0)[1]);
        Bundle extras = (Bundle) (mOnConnectionEventCounter.getArgs(0)[2]);

        assertEquals(Connection.EVENT_CALL_MERGE_FAILED, event);
        assertNull(extras);
    }

    /**
     * Verifies {@link Connection#setPhoneAccountHandle(PhoneAccountHandle)} propagates to the
     * dialer app.
     */
    public void testChangePhoneAccount() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        mConnection.setActive();
        assertCallState(mCall, Call.STATE_ACTIVE);

        mConnection.setPhoneAccountHandle(TEST_PHONE_ACCOUNT_HANDLE_2);
        assertEquals(TEST_PHONE_ACCOUNT_HANDLE_2, mConnection.getPhoneAccountHandle());
        mOnPhoneAccountChangedCounter.waitForCount(1, WAIT_FOR_STATE_CHANGE_TIMEOUT_MS);
        assertEquals(TEST_PHONE_ACCOUNT_HANDLE_2, mOnPhoneAccountChangedCounter.getArgs(0)[1]);
    }
}
