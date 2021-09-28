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

package com.android.server.telecom.tests;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.nullable;
import static org.mockito.Matchers.any;
import static org.mockito.Matchers.anyBoolean;
import static org.mockito.Matchers.anyInt;
import static org.mockito.Matchers.anyString;
import static org.mockito.Matchers.eq;
import static org.mockito.Matchers.isNotNull;
import static org.mockito.Matchers.isNull;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.Manifest;
import android.app.Activity;
import android.app.AppOpsManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.UserHandle;
import android.telecom.GatewayInfo;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.DisconnectCause;
import android.telephony.TelephonyManager;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.server.telecom.Call;
import com.android.server.telecom.CallsManager;
import com.android.server.telecom.DefaultDialerCache;
import com.android.server.telecom.NewOutgoingCallIntentBroadcaster;
import com.android.server.telecom.PhoneAccountRegistrar;
import com.android.server.telecom.PhoneNumberUtilsAdapter;
import com.android.server.telecom.PhoneNumberUtilsAdapterImpl;
import com.android.server.telecom.RoleManagerAdapter;
import com.android.server.telecom.SystemStateHelper;
import com.android.server.telecom.TelecomSystem;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;

@RunWith(JUnit4.class)
public class NewOutgoingCallIntentBroadcasterTest extends TelecomTestCase {
    private static class ReceiverIntentPair {
        public BroadcastReceiver receiver;
        public Intent intent;

        public ReceiverIntentPair(BroadcastReceiver receiver, Intent intent) {
            this.receiver = receiver;
            this.intent = intent;
        }
    }

    @Mock private CallsManager mCallsManager;
    @Mock private Call mCall;
    @Mock private SystemStateHelper mSystemStateHelper;
    @Mock private UserHandle mUserHandle;
    @Mock private PhoneAccount mPhoneAccount;
    @Mock private PhoneAccountRegistrar mPhoneAccountRegistrar;
    @Mock private RoleManagerAdapter mRoleManagerAdapter;
    @Mock private DefaultDialerCache mDefaultDialerCache;

    private PhoneNumberUtilsAdapter mPhoneNumberUtilsAdapter = new PhoneNumberUtilsAdapterImpl();

    @Override
    @Before
    public void setUp() throws Exception {
        super.setUp();
        when(mCall.getInitiatingUser()).thenReturn(UserHandle.CURRENT);
        when(mCallsManager.getLock()).thenReturn(new TelecomSystem.SyncRoot() { });
        when(mCallsManager.getSystemStateHelper()).thenReturn(mSystemStateHelper);
        when(mCallsManager.getCurrentUserHandle()).thenReturn(mUserHandle);
        when(mCallsManager.getPhoneAccountRegistrar()).thenReturn(mPhoneAccountRegistrar);
        when(mCallsManager.getRoleManagerAdapter()).thenReturn(mRoleManagerAdapter);
        when(mPhoneAccountRegistrar.getSubscriptionIdForPhoneAccount(
                any(PhoneAccountHandle.class))).thenReturn(-1);
        when(mPhoneAccountRegistrar.getPhoneAccountUnchecked(
            any(PhoneAccountHandle.class))).thenReturn(mPhoneAccount);
        when(mPhoneAccount.isSelfManaged()).thenReturn(true);
        when(mSystemStateHelper.isCarMode()).thenReturn(false);
    }

    @Override
    @After
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @SmallTest
    @Test
    public void testSelfManagedCall() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent selfManagedCallIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        selfManagedCallIntent.putExtra(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE,
                new PhoneAccountHandle(new ComponentName("fakeTestPackage", "fakeTestClass"),
                        "id_tSMC"));
        NewOutgoingCallIntentBroadcaster.CallDisposition callDisposition = processIntent(
                selfManagedCallIntent, true);
        assertEquals(false, callDisposition.requestRedirection);
        assertEquals(DisconnectCause.NOT_DISCONNECTED, callDisposition.disconnectCause);
    }

    @SmallTest
    @Test
    public void testNullHandle() {
        Intent intent = new Intent(Intent.ACTION_CALL, null);
        int result = processIntent(intent, true).disconnectCause;
        assertEquals(DisconnectCause.INVALID_NUMBER, result);
        verifyNoBroadcastSent();
        verifyNoCallPlaced();
    }

    @SmallTest
    @Test
    public void testVoicemailCall() {
        String voicemailNumber = "voicemail:18005551234";
        Intent intent = new Intent(Intent.ACTION_CALL, Uri.parse(voicemailNumber));
        intent.putExtra(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, true);

        NewOutgoingCallIntentBroadcaster.CallDisposition callDisposition = processIntent(
                intent, true);
        assertEquals(false, callDisposition.requestRedirection);
        assertEquals(DisconnectCause.NOT_DISCONNECTED, callDisposition.disconnectCause);
        verify(mCallsManager).placeOutgoingCall(eq(mCall), eq(Uri.parse(voicemailNumber)),
                nullable(GatewayInfo.class), eq(true), eq(VideoProfile.STATE_AUDIO_ONLY));
    }

    @SmallTest
    @Test
    public void testVoicemailCallWithBadAction() {
        badCallActionHelper(Uri.parse("voicemail:18005551234"), DisconnectCause.OUTGOING_CANCELED);
    }

    @SmallTest
    @Test
    public void testTelCallWithBadCallAction() {
        badCallActionHelper(Uri.parse("tel:6505551234"), DisconnectCause.INVALID_NUMBER);
    }

    @SmallTest
    @Test
    public void testSipCallWithBadCallAction() {
        badCallActionHelper(Uri.parse("sip:testuser@testsite.com"), DisconnectCause.INVALID_NUMBER);
    }

    private void badCallActionHelper(Uri handle, int expectedCode) {
        Intent intent = new Intent(Intent.ACTION_ALARM_CHANGED, handle);

        int result = processIntent(intent, true).disconnectCause;

        assertEquals(expectedCode, result);
        verifyNoBroadcastSent();
        verifyNoCallPlaced();
    }

    @SmallTest
    @Test
    public void testAlreadyDisconnectedCall() {
        Uri handle = Uri.parse("tel:6505551234");
        doReturn(true).when(mCall).isDisconnected();
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        result.receiver.setResultData(
                result.intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER));

        result.receiver.onReceive(mContext, result.intent);
        verifyNoCallPlaced();
    }

    @SmallTest
    @Test
    public void testNoNumberSupplied() {
        Uri handle = Uri.parse("tel:");
        Intent intent = new Intent(Intent.ACTION_CALL, handle);

        int result = processIntent(intent, true).disconnectCause;

        assertEquals(DisconnectCause.NO_PHONE_NUMBER_SUPPLIED, result);
        verifyNoBroadcastSent();
        verifyNoCallPlaced();
    }

    @SmallTest
    @Test
    public void testEmergencyCallWithNonDefaultDialer() {
        Uri handle = Uri.parse("tel:6505551911");
        doReturn(true).when(mComponentContextFixture.getTelephonyManager())
                .isPotentialEmergencyNumber(eq(handle.getSchemeSpecificPart()));
        Intent intent = new Intent(Intent.ACTION_CALL, handle);

        String ui_package_string = "sample_string_1";
        String dialer_default_class_string = "sample_string_2";
        mComponentContextFixture.putResource(com.android.internal.R.string.config_defaultDialer,
                ui_package_string);
        mComponentContextFixture.putResource(R.string.dialer_default_class,
                dialer_default_class_string);
        when(mDefaultDialerCache.getSystemDialerApplication()).thenReturn(ui_package_string);
        when(mDefaultDialerCache.getSystemDialerComponent()).thenReturn(
                new ComponentName(ui_package_string, dialer_default_class_string));

        int result = processIntent(intent, false).disconnectCause;

        assertEquals(DisconnectCause.OUTGOING_CANCELED, result);
        verifyNoBroadcastSent();
        verifyNoCallPlaced();

        ArgumentCaptor<Intent> dialerIntentCaptor = ArgumentCaptor.forClass(Intent.class);
        verify(mContext).startActivityAsUser(dialerIntentCaptor.capture(), any(UserHandle.class));
        Intent dialerIntent = dialerIntentCaptor.getValue();
        assertEquals(new ComponentName(ui_package_string, dialer_default_class_string),
                dialerIntent.getComponent());
        assertEquals(Intent.ACTION_DIAL, dialerIntent.getAction());
        assertEquals(handle, dialerIntent.getData());
        assertEquals(Intent.FLAG_ACTIVITY_NEW_TASK, dialerIntent.getFlags());
    }

    @SmallTest
    @Test
    public void testActionCallEmergencyCall() {
        Uri handle = Uri.parse("tel:6505551911");
        Intent intent = buildIntent(handle, Intent.ACTION_CALL, null);
        emergencyCallTestHelper(intent, null);
    }

    @SmallTest
    @Test
    public void testActionEmergencyWithEmergencyNumber() {
        Uri handle = Uri.parse("tel:6505551911");
        Intent intent = buildIntent(handle, Intent.ACTION_CALL_EMERGENCY, null);
        emergencyCallTestHelper(intent, null);
    }

    @SmallTest
    @Test
    public void testActionPrivCallWithEmergencyNumber() {
        Uri handle = Uri.parse("tel:6505551911");
        Intent intent = buildIntent(handle, Intent.ACTION_CALL_PRIVILEGED, null);
        emergencyCallTestHelper(intent, null);
    }

    @SmallTest
    @Test
    public void testEmergencyCallWithGatewayExtras() {
        Uri handle = Uri.parse("tel:6505551911");
        Bundle gatewayExtras = new Bundle();
        gatewayExtras.putString(NewOutgoingCallIntentBroadcaster.EXTRA_GATEWAY_PROVIDER_PACKAGE,
                "sample1");
        gatewayExtras.putString(NewOutgoingCallIntentBroadcaster.EXTRA_GATEWAY_URI, "sample2");

        Intent intent = buildIntent(handle, Intent.ACTION_CALL, gatewayExtras);
        emergencyCallTestHelper(intent, gatewayExtras);
    }

    @SmallTest
    @Test
    public void testActionEmergencyWithNonEmergencyNumber() {
        Uri handle = Uri.parse("tel:6505551911");
        doReturn(false).when(mComponentContextFixture.getTelephonyManager())
                .isPotentialEmergencyNumber(eq(handle.getSchemeSpecificPart()));
        Intent intent = new Intent(Intent.ACTION_CALL_EMERGENCY, handle);
        int result = processIntent(intent, true).disconnectCause;

        assertEquals(DisconnectCause.OUTGOING_CANCELED, result);
        verifyNoCallPlaced();
        verifyNoBroadcastSent();
    }

    private void emergencyCallTestHelper(Intent intent, Bundle expectedAdditionalExtras) {
        Uri handle = intent.getData();
        int videoState = VideoProfile.STATE_BIDIRECTIONAL;
        boolean isSpeakerphoneOn = true;
        doReturn(true).when(mComponentContextFixture.getTelephonyManager())
                .isPotentialEmergencyNumber(eq(handle.getSchemeSpecificPart()));
        intent.putExtra(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, isSpeakerphoneOn);
        intent.putExtra(TelecomManager.EXTRA_START_CALL_WITH_VIDEO_STATE, videoState);

        NewOutgoingCallIntentBroadcaster.CallDisposition callDisposition = processIntent(
            intent, true);
        assertEquals(false, callDisposition.requestRedirection);
        assertEquals(DisconnectCause.NOT_DISCONNECTED, callDisposition.disconnectCause);

        verify(mCallsManager).placeOutgoingCall(eq(mCall), eq(handle), isNull(GatewayInfo.class),
                eq(isSpeakerphoneOn), eq(videoState));

        Bundle expectedExtras = createNumberExtras(handle.getSchemeSpecificPart());
        if (expectedAdditionalExtras != null) {
            expectedExtras.putAll(expectedAdditionalExtras);
        }
        BroadcastReceiver receiver = verifyBroadcastSent(handle.getSchemeSpecificPart(),
                expectedExtras).receiver;
        assertNull(receiver);
    }

    @SmallTest
    @Test
    public void testUnmodifiedRegularCall() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        result.receiver.setResultData(
                result.intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER));

        result.receiver.onReceive(mContext, result.intent);

        verify(mCallsManager).placeOutgoingCall(eq(mCall), eq(handle), isNull(GatewayInfo.class),
                eq(true), eq(VideoProfile.STATE_BIDIRECTIONAL));
    }

    @SmallTest
    @Test
    public void testUnmodifiedSipCall() {
        Uri handle = Uri.parse("sip:test@test.com");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        result.receiver.setResultData(
                result.intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER));

        result.receiver.onReceive(mContext, result.intent);

        Uri encHandle = Uri.fromParts(handle.getScheme(),
                handle.getSchemeSpecificPart(), null);
        verify(mCallsManager).placeOutgoingCall(eq(mCall), eq(encHandle), isNull(GatewayInfo.class),
                eq(true), eq(VideoProfile.STATE_BIDIRECTIONAL));
    }

    @SmallTest
    @Test
    public void testCallWithGatewayInfo() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);

        callIntent.putExtra(NewOutgoingCallIntentBroadcaster
                        .EXTRA_GATEWAY_PROVIDER_PACKAGE, "sample1");
        callIntent.putExtra(NewOutgoingCallIntentBroadcaster.EXTRA_GATEWAY_URI, "sample2");
        ReceiverIntentPair result = regularCallTestHelper(callIntent, callIntent.getExtras());

        result.receiver.setResultData(
                result.intent.getStringExtra(Intent.EXTRA_PHONE_NUMBER));

        result.receiver.onReceive(mContext, result.intent);

        verify(mCallsManager).placeOutgoingCall(eq(mCall), eq(handle),
                isNotNull(GatewayInfo.class), eq(true), eq(VideoProfile.STATE_BIDIRECTIONAL));
    }

    @SmallTest
    @Test
    public void testCallNumberModifiedToNull() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        result.receiver.setResultData(null);

        result.receiver.onReceive(mContext, result.intent);
        verifyNoCallPlaced();
        ArgumentCaptor<Long> timeoutCaptor = ArgumentCaptor.forClass(Long.class);
        verify(mCall).disconnect(timeoutCaptor.capture());
        assertTrue(timeoutCaptor.getValue() > 0);
    }

    @SmallTest
    @Test
    public void testCallNumberModifiedToNullWithLongCustomTimeout() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        long customTimeout = 100000000;
        Bundle bundle = new Bundle();
        bundle.putLong(TelecomManager.EXTRA_NEW_OUTGOING_CALL_CANCEL_TIMEOUT, customTimeout);
        result.receiver.setResultData(null);
        result.receiver.setResultExtras(bundle);

        result.receiver.onReceive(mContext, result.intent);
        verifyNoCallPlaced();
        ArgumentCaptor<Long> timeoutCaptor = ArgumentCaptor.forClass(Long.class);
        verify(mCall).disconnect(timeoutCaptor.capture());
        assertTrue(timeoutCaptor.getValue() < customTimeout);
    }

    @SmallTest
    @Test
    public void testCallModifiedToEmergency() {
        Uri handle = Uri.parse("tel:6505551234");
        Intent callIntent = buildIntent(handle, Intent.ACTION_CALL, null);
        ReceiverIntentPair result = regularCallTestHelper(callIntent, null);

        String newEmergencyNumber = "1234567890";
        result.receiver.setResultData(newEmergencyNumber);

        doReturn(true).when(mComponentContextFixture.getTelephonyManager())
                .isPotentialEmergencyNumber(eq(newEmergencyNumber));
        result.receiver.onReceive(mContext, result.intent);
        verify(mCall).disconnect(eq(0L));
    }

    /**
     * Ensure if {@link TelephonyManager#isPotentialEmergencyNumber(String)} throws an exception of
     * any sort that we don't crash Telecom.
     */
    @SmallTest
    @Test
    public void testThrowOnIsPotentialEmergencyNumber() {
        doThrow(new IllegalStateException()).when(mComponentContextFixture.getTelephonyManager())
                .isPotentialEmergencyNumber(anyString());
        testUnmodifiedRegularCall();
    }

    private ReceiverIntentPair regularCallTestHelper(Intent intent,
            Bundle expectedAdditionalExtras) {
        Uri handle = intent.getData();
        int videoState = VideoProfile.STATE_BIDIRECTIONAL;
        boolean isSpeakerphoneOn = true;
        intent.putExtra(TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE, isSpeakerphoneOn);
        intent.putExtra(TelecomManager.EXTRA_START_CALL_WITH_VIDEO_STATE, videoState);

        NewOutgoingCallIntentBroadcaster.CallDisposition callDisposition = processIntent(
            intent, true);
        assertEquals(true, callDisposition.requestRedirection);
        assertEquals(DisconnectCause.NOT_DISCONNECTED, callDisposition.disconnectCause);

        Bundle expectedExtras = createNumberExtras(handle.getSchemeSpecificPart());
        if (expectedAdditionalExtras != null) {
            expectedExtras.putAll(expectedAdditionalExtras);
        }
        return verifyBroadcastSent(handle.getSchemeSpecificPart(), expectedExtras);
    }

    private Intent buildIntent(Uri handle, String action, Bundle extras) {
        Intent i = new Intent(action, handle);
        if (extras != null) {
            i.putExtras(extras);
        }
        return i;
    }

    private NewOutgoingCallIntentBroadcaster.CallDisposition processIntent(Intent intent,
            boolean isDefaultPhoneApp) {
        NewOutgoingCallIntentBroadcaster b = new NewOutgoingCallIntentBroadcaster(
                mContext, mCallsManager, intent, mPhoneNumberUtilsAdapter,
                isDefaultPhoneApp, mDefaultDialerCache);
        NewOutgoingCallIntentBroadcaster.CallDisposition cd = b.evaluateCall();
        if (cd.disconnectCause == DisconnectCause.NOT_DISCONNECTED) {
            b.processCall(mCall, cd);
        }
        return cd;
    }

    private ReceiverIntentPair verifyBroadcastSent(String number, Bundle expectedExtras) {
        ArgumentCaptor<Intent> intentCaptor = ArgumentCaptor.forClass(Intent.class);
        ArgumentCaptor<BroadcastReceiver> receiverCaptor =
                ArgumentCaptor.forClass(BroadcastReceiver.class);

        verify(mContext).sendOrderedBroadcastAsUser(
                intentCaptor.capture(),
                eq(UserHandle.CURRENT),
                eq(Manifest.permission.PROCESS_OUTGOING_CALLS),
                eq(AppOpsManager.OP_PROCESS_OUTGOING_CALLS),
                any(Bundle.class),
                receiverCaptor.capture(),
                isNull(Handler.class),
                eq(Activity.RESULT_OK),
                eq(number),
                isNull(Bundle.class));

        Intent capturedIntent = intentCaptor.getValue();
        assertEquals(Intent.ACTION_NEW_OUTGOING_CALL, capturedIntent.getAction());
        assertEquals(Intent.FLAG_RECEIVER_FOREGROUND | Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND,
                capturedIntent.getFlags());
        assertTrue(areBundlesEqual(expectedExtras, capturedIntent.getExtras()));

        BroadcastReceiver receiver = receiverCaptor.getValue();
        if (receiver != null) {
            receiver.setPendingResult(
                    new BroadcastReceiver.PendingResult(0, "", null, 0, true, false, null, 0, 0));
        }

        return new ReceiverIntentPair(receiver, capturedIntent);
    }

    private Bundle createNumberExtras(String number) {
        Bundle b = new Bundle();
        b.putString(Intent.EXTRA_PHONE_NUMBER, number);
        return b;
    }

    private void verifyNoCallPlaced() {
        verify(mCallsManager, never()).placeOutgoingCall(any(Call.class), any(Uri.class),
                any(GatewayInfo.class), anyBoolean(), anyInt());
    }

    private void verifyNoBroadcastSent() {
        verify(mContext, never()).sendOrderedBroadcastAsUser(
                any(Intent.class),
                any(UserHandle.class),
                anyString(),
                anyInt(),
                any(Bundle.class),
                any(BroadcastReceiver.class),
                any(Handler.class),
                anyInt(),
                anyString(),
                any(Bundle.class));
    }

    private static boolean areBundlesEqual(Bundle b1, Bundle b2) {
        for (String key1 : b1.keySet()) {
            if (!b1.get(key1).equals(b2.get(key1))) {
                return false;
            }
        }

        for (String key2 : b2.keySet()) {
            if (!b2.get(key2).equals(b1.get(key2))) {
                return false;
            }
        }
        return true;
    }
}
