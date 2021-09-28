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

package com.android.services.telephony;

import static junit.framework.Assert.assertTrue;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.mockito.Matchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.net.Uri;
import android.os.Bundle;
import android.os.Looper;
import android.telecom.Call;
import android.telecom.Conference;
import android.telecom.Connection;
import android.telecom.PhoneAccountHandle;
import android.telecom.StatusHints;
import android.telecom.TelecomManager;
import android.telephony.TelephonyManager;
import android.test.suitebuilder.annotation.SmallTest;

import com.android.ims.internal.ConferenceParticipant;

import org.junit.Before;
import org.junit.Test;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

public class ImsConferenceTest {
    @Mock
    private TelephonyConnectionServiceProxy mMockTelephonyConnectionServiceProxy;

    @Mock
    private TelecomAccountRegistry mMockTelecomAccountRegistry;

    private TestTelephonyConnection mConferenceHost;

    @Before
    public void setUp() throws Exception {
        MockitoAnnotations.initMocks(this);
        if (Looper.myLooper() == null) {
            Looper.prepare();
        }
        mConferenceHost = new TestTelephonyConnection();
        mConferenceHost.setManageImsConferenceCallSupported(true);
        when(mMockTelecomAccountRegistry.getAddress(any(PhoneAccountHandle.class)))
                .thenReturn(null);
    }
    @Test
    @SmallTest
    public void testPropertyPropagation() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);
        mConferenceHost.setConnectionProperties(Connection.PROPERTY_ASSISTED_DIALING
                | Connection.PROPERTY_WIFI);
        Bundle extras = new Bundle();
        extras.putInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE, TelephonyManager.NETWORK_TYPE_IWLAN);
        mConferenceHost.putTelephonyExtras(extras);
        mConferenceHost.setStatusHints(new StatusHints("WIFIs", null, null));

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_OUTGOING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        ArgumentCaptor<Connection> captor = ArgumentCaptor.forClass(Connection.class);
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), captor.capture(),
                eq(imsConference));

        // Make sure they're set on the initially created participants.
        for (Connection c : captor.getAllValues()) {
            assertEquals(0, c.getConnectionProperties() & Connection.PROPERTY_ASSISTED_DIALING);
            assertEquals(Connection.PROPERTY_WIFI,
                    c.getConnectionProperties() & Connection.PROPERTY_WIFI);
            assertNotNull(c.getStatusHints());
            assertEquals(TelephonyManager.NETWORK_TYPE_IWLAN, c.getExtras().getInt(
                    TelecomManager.EXTRA_CALL_NETWORK_TYPE));
        }
    }

    @Test
    @SmallTest
    public void testSinglePartyEmulation() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_OUTGOING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));

        // Because we're pretending its a single party, there should be no participants any more.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1));
        assertEquals(0, imsConference.getNumberOfParticipants());
        // Ensure the call direction is set correctly during emulation
        assertEquals(Call.Details.DIRECTION_OUTGOING, imsConference.getCallDirection());
        reset(mMockTelephonyConnectionServiceProxy);

        // Back to 2!
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));
    }

    /**
     * Tests CEPs with disconnected participants present with disconnected state.
     */
    @Test
    @SmallTest
    public void testDisconnectParticipantViaDisconnectState() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        // Start off with 3 participants.
        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);

        ConferenceParticipant participant3 = new ConferenceParticipant(
                Uri.parse("tel:6505551214"),
                "A",
                Uri.parse("sip:6505551214@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2, participant3));
        assertEquals(3, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(3)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));


        // Mark one participant as disconnected.
        ConferenceParticipant participant3Disconnected = new ConferenceParticipant(
                Uri.parse("tel:6505551214"),
                "A",
                Uri.parse("sip:6505551214@testims.com"),
                Connection.STATE_DISCONNECTED,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2, participant3Disconnected));
        assertEquals(2, imsConference.getNumberOfParticipants());
        reset(mMockTelephonyConnectionServiceProxy);

        // Now remove it from another CEP update; should still be the same number of participants
        // and no updates.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, never()).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                any(Conference.class));
    }

    /**
     * Tests CEPs with removed participants.
     */
    @Test
    @SmallTest
    public void testDisconnectParticipantViaRemoval() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        // Start off with 3 participants.
        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);

        ConferenceParticipant participant3 = new ConferenceParticipant(
                Uri.parse("tel:6505551214"),
                "A",
                Uri.parse("sip:6505551214@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2, participant3));
        assertEquals(3, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(3)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));
        reset(mMockTelephonyConnectionServiceProxy);

        // Remove one from the CEP (don't disconnect first); should have 2 participants now.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, never()).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                any(Conference.class));
    }

    /**
     * Typically when a participant disconnects from a conference it is either:
     * 1. Removed from a subsequent CEP update.
     * 2. Marked as disconnected in a CEP update, and then removed from another CEP update.
     *
     * When a participant disconnects from a conference, some carriers will mark the disconnected
     * participant as disconnected, but fail to send another CEP update with it removed.
     *
     * This test verifies that we can still enter single party emulation in this case.
     */
    @Test
    @SmallTest
    public void testSinglePartyEmulationEnterOnDisconnectParticipant() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        // Setup the initial conference state with 2 participants.
        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));

        // Some carriers keep disconnected participants around in the CEP; this will cause problems
        // when we want to enter single party conference mode. Verify that this case is handled.
        ConferenceParticipant participant2Disconnected = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_DISCONNECTED,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2Disconnected));
        assertEquals(0, imsConference.getNumberOfParticipants());
        reset(mMockTelephonyConnectionServiceProxy);

        // Pretend to merge someone else into the conference.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));
    }

    /**
     * We have seen a scenario on a carrier where a conference event package comes in just prior to
     * the call disconnecting with only the conference host in it.  This caused a problem because
     * it triggered exiting single party conference mode (due to a bug) and caused the call to not
     * be logged.
     */
    @Test
    @SmallTest
    public void testSinglePartyEmulationWithPreDisconnectParticipantUpdate() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        final boolean[] isConferenceState = new boolean[1];
        TelephonyConferenceBase.TelephonyConferenceListener conferenceListener =
                new TelephonyConferenceBase.TelephonyConferenceListener() {
            @Override
            public void onConferenceMembershipChanged(Connection connection) {
                isConferenceState[0] = connection.getConference() != null;
            }
        };
        imsConference.addTelephonyConferenceListener(conferenceListener);

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
        verify(mMockTelephonyConnectionServiceProxy, times(2)).addExistingConnection(
                any(PhoneAccountHandle.class), any(Connection.class),
                eq(imsConference));

        // Because we're pretending its a single party, there should be only a single participant.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1));
        assertEquals(0, imsConference.getNumberOfParticipants());

        // Emulate a pre-disconnect conference event package; there will be zero participants.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList());

        // We should still not be considered a conference (hence we should be logging this call).
        assertFalse(isConferenceState[0]);
    }

    /**
     * Verify that the single party emulate correctly when the conference start with only
     * one participant.
     */
    @Test
    @SmallTest
    public void testSinglePartyEmulationWithOneParticipantAtBeginning() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1));
        assertFalse(imsConference.isMultiparty());
    }

    /**
     * Verify that the single party emulate correctly when the conference starts with a single
     * party as part of the initial setup.  This mimics how an ImsCall can get CEP data prior to the
     * ImsConference being created.
     */
    @Test
    @SmallTest
    public void testSinglePartyEmulationWithSinglePartyAtCreation() {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551214"),
                "A",
                Uri.parse("sip:6505551214@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        List<ConferenceParticipant> cps = Arrays.asList(participant1);
        when(mConferenceHost.mMockRadioConnection.getConferenceParticipants())
                .thenReturn(cps);
        imsConference.updateConferenceParticipantsAfterCreation();

        assertFalse(imsConference.isMultiparty());
    }

    /**
     * Verify that we do not use single party emulation when a sim call manager is in use.
     */
    @Test
    @SmallTest
    public void testNoSinglePartyEmulationWithSimCallManager() {
        // Make it look like there is a sim call manager in use.
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(
                any(PhoneAccountHandle.class))).thenReturn(true);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> true /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());

        // Because we're not using single party emulation, should still be one participant.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1));
        assertEquals(1, imsConference.getNumberOfParticipants());

        // Back to 2!
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());
    }

    @Test
    @SmallTest
    public void testNormalConference() throws Exception {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> false /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder().build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());

        // Not emulating single party; should still be one.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1));
        assertEquals(1, imsConference.getNumberOfParticipants());

        // Drop to 0 participants; should not hangup the conf now
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost, Collections.emptyList());
        assertEquals(0, imsConference.getNumberOfParticipants());
        verify(mConferenceHost.mMockCall, never()).hangup();
    }

    @Test
    @SmallTest
    public void testLocalDisconnectOnEmptyConference() throws Exception {
        when(mMockTelecomAccountRegistry.isUsingSimCallManager(any(PhoneAccountHandle.class)))
                .thenReturn(false);

        ImsConference imsConference = new ImsConference(mMockTelecomAccountRegistry,
                mMockTelephonyConnectionServiceProxy, mConferenceHost,
                null /* phoneAccountHandle */, () -> false /* featureFlagProxy */,
                new ImsConference.CarrierConfiguration.Builder()
                        .setShouldLocalDisconnectEmptyConference(true)
                        .build());

        ConferenceParticipant participant1 = new ConferenceParticipant(
                Uri.parse("tel:6505551212"),
                "A",
                Uri.parse("sip:6505551212@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        ConferenceParticipant participant2 = new ConferenceParticipant(
                Uri.parse("tel:6505551213"),
                "A",
                Uri.parse("sip:6505551213@testims.com"),
                Connection.STATE_ACTIVE,
                Call.Details.DIRECTION_INCOMING);
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost,
                Arrays.asList(participant1, participant2));
        assertEquals(2, imsConference.getNumberOfParticipants());

        // Drop to 0 participants; should have a hangup request.
        imsConference.handleConferenceParticipantsUpdate(mConferenceHost, Collections.emptyList());
        assertEquals(0, imsConference.getNumberOfParticipants());
        verify(mConferenceHost.mMockCall).hangup();
    }

    /**
     * Verifies that an ImsConference can handle SIP and TEL URIs for both the P-Associated-Uri and
     * conference event package identities.
     */
    @Test
    public void testIsParticipantHost() {
        // Simplest case, assume P-Associated-Uri is a tel URI and that the CEP participant is also
        // a tel URI.
        assertTrue(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("tel:+8616505551234")},
                Uri.parse("tel:+8616505551234")));

        // Assume P-Associated-Uri is a tel URI and the CEP participant is a sip URI.
        assertTrue(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("tel:+8616505551234")},
                Uri.parse("sip:+8616505551234@bj.ims.mnc011.mcc460.3gppnetwork.org")));

        // Assume P-Associated-Uri is a sip URI and the CEP participant is a tel URI.
        assertTrue(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("sip:+8616505551234@bj.ims.mnc011.mcc460.3gppnetwork.org")},
                Uri.parse("tel:+8616505551234")));

        // Assume both P-Associated-Uri and the CEP participant are SIP URIs.
        assertTrue(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("sip:+8616505551234@bj.ims.mnc011.mcc460.3gppnetwork.org")},
                Uri.parse("sip:+8616505551234@bj.ims.mnc011.mcc460.3gppnetwork.org")));

        // Corner cases
        assertFalse(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("tel:+8616505551234")}, Uri.fromParts("", "", "")));
        assertFalse(ImsConference.isParticipantHost(new Uri[] {
                        Uri.parse("tel:+8616505551234")}, null));
        assertFalse(ImsConference.isParticipantHost(null, null));
        assertFalse(ImsConference.isParticipantHost(new Uri[0], null));
    }
}
