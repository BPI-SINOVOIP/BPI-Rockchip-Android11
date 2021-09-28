/*
 * Copyright (C) 2014 The Android Open Source Project
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

import android.content.Context;
import android.os.PersistableBundle;
import android.telecom.Conference;
import android.telecom.Conferenceable;
import android.telecom.Connection;
import android.telecom.ConnectionService;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccountHandle;
import android.telephony.CarrierConfigManager;

import com.android.telephony.Rlog;

import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.phone.PhoneUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.stream.Collectors;

/**
 * Manages conferences for IMS connections.
 */
public class ImsConferenceController {
    private static final String LOG_TAG = "ImsConferenceController";

    /**
     * Conference listener; used to receive notification when a conference has been disconnected.
     */
    private final TelephonyConferenceBase.TelephonyConferenceListener mConferenceListener =
            new TelephonyConferenceBase.TelephonyConferenceListener() {
        @Override
        public void onDestroyed(Conference conference) {
            if (Log.VERBOSE) {
                Log.v(ImsConferenceController.class, "onDestroyed: %s", conference);
            }

            if (conference instanceof ImsConference) {
                // Ims Conference call ended, so UE may now have the ability to initiate
                // an Adhoc Conference call. Hence, try enabling adhoc conference capability
                mTelecomAccountRegistry.refreshAdhocConference(true);
            }
            mImsConferences.remove(conference);
        }

        @Override
        public void onStateChanged(Conference conference, int oldState, int newState) {
            Log.v(this, "onStateChanged: Conference = " + conference);
            recalculateConferenceable();
        }
    };

    private final TelephonyConnection.TelephonyConnectionListener mTelephonyConnectionListener =
            new TelephonyConnection.TelephonyConnectionListener() {
        @Override
        public void onConferenceStarted() {
            Log.v(this, "onConferenceStarted");
            recalculate();
        }

        @Override
        public void onConferenceSupportedChanged(Connection c, boolean isConferenceSupported) {
            Log.v(this, "onConferenceSupportedChanged");
            recalculate();
        }

        @Override
        public void onStateChanged(Connection c, int state) {
            Log.v(this, "onStateChanged: %s", Rlog.pii(LOG_TAG, c.getAddress()));
            recalculate();
        }

        @Override
        public void onDisconnected(Connection c, DisconnectCause disconnectCause) {
            Log.v(this, "onDisconnected: %s", Rlog.pii(LOG_TAG, c.getAddress()));
            recalculate();
        }

        @Override
        public void onDestroyed(Connection connection) {
            remove(connection);
        }
    };

    /**
     * The current {@link ConnectionService}.
     */
    private final TelephonyConnectionServiceProxy mConnectionService;

    private final ImsConference.FeatureFlagProxy mFeatureFlagProxy;

    /**
     * List of known {@link TelephonyConnection}s.
     */
    private final ArrayList<TelephonyConnection> mTelephonyConnections = new ArrayList<>();

    /**
     * List of known {@link ImsConference}s. There can be upto maximum two Ims conference calls.
     * One conference call can be a host conference call and another conference call formed as a
     * result of accepting incoming conference call.
     */
    private final ArrayList<ImsConference> mImsConferences = new ArrayList<>(2);

    private TelecomAccountRegistry mTelecomAccountRegistry;

    /**
     * Creates a new instance of the Ims conference controller.
     *
     * @param connectionService The current connection service.
     * @param featureFlagProxy
     */
    public ImsConferenceController(TelecomAccountRegistry telecomAccountRegistry,
            TelephonyConnectionServiceProxy connectionService,
            ImsConference.FeatureFlagProxy featureFlagProxy) {
        mConnectionService = connectionService;
        mTelecomAccountRegistry = telecomAccountRegistry;
        mFeatureFlagProxy = featureFlagProxy;
    }

    void addConference(ImsConference conference) {
        if (mImsConferences.contains(conference)) {
            // Adding a duplicate realistically shouldn't happen.
            Log.w(this, "addConference - conference already tracked; conference=%s", conference);
            return;
        }
        mImsConferences.add(conference);
        conference.addTelephonyConferenceListener(mConferenceListener);
        recalculateConferenceable();
    }

    /**
     * Adds a new connection to the IMS conference controller.
     *
     * @param connection
     */
    void add(TelephonyConnection connection) {
        // DO NOT add external calls; we don't want to consider them as a potential conference
        // member.
        if ((connection.getConnectionProperties() & Connection.PROPERTY_IS_EXTERNAL_CALL) ==
                Connection.PROPERTY_IS_EXTERNAL_CALL) {
            return;
        }

        if (mTelephonyConnections.contains(connection)) {
            // Adding a duplicate realistically shouldn't happen.
            Log.w(this, "add - connection already tracked; connection=%s", connection);
            return;
        }

        // Note: Wrap in Log.VERBOSE to avoid calling connection.toString if we are not going to be
        // outputting the value.
        if (Log.VERBOSE) {
            Log.v(this, "add connection %s", connection);
        }

        mTelephonyConnections.add(connection);
        connection.addTelephonyConnectionListener(mTelephonyConnectionListener);
        recalculateConference();
        recalculateConferenceable();
    }

    /**
     * Removes a connection from the IMS conference controller.
     *
     * @param connection
     */
    void remove(Connection connection) {
        // External calls are not part of the conference controller, so don't remove them.
        if ((connection.getConnectionProperties() & Connection.PROPERTY_IS_EXTERNAL_CALL) ==
                Connection.PROPERTY_IS_EXTERNAL_CALL) {
            return;
        }

        if (!mTelephonyConnections.contains(connection)) {
            // Debug only since TelephonyConnectionService tries to clean up the connections tracked
            // when the original connection changes.  It does this proactively.
            Log.d(this, "remove - connection not tracked; connection=%s", connection);
            return;
        }

        if (Log.VERBOSE) {
            Log.v(this, "remove connection: %s", connection);
        }

        if (connection instanceof TelephonyConnection) {
            TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
            telephonyConnection.removeTelephonyConnectionListener(mTelephonyConnectionListener);
        }
        mTelephonyConnections.remove(connection);
        recalculateConferenceable();
    }

    /**
     * Triggers both a re-check of conferenceable connections, as well as checking for new
     * conferences.
     */
    private void recalculate() {
        recalculateConferenceable();
        recalculateConference();
    }

    /**
     * Calculates the conference-capable state of all GSM connections in this connection service.
     */
    private void recalculateConferenceable() {
        Log.v(this, "recalculateConferenceable : %d", mTelephonyConnections.size());
        HashSet<Conferenceable> conferenceableSet = new HashSet<>(mTelephonyConnections.size() +
                mImsConferences.size());
        HashSet<Conferenceable> conferenceParticipantsSet = new HashSet<>();

        // Loop through and collect all calls which are active or holding
        for (TelephonyConnection connection : mTelephonyConnections) {
            if (Log.DEBUG) {
                Log.d(this, "recalc - %s %s supportsConf? %s", connection.getState(), connection,
                        connection.isConferenceSupported());
            }

            // If this connection is a member of a conference hosted on another device, it is not
            // conferenceable with any other connections.
            if (isMemberOfPeerConference(connection)) {
                if (Log.VERBOSE) {
                    Log.v(this, "Skipping connection in peer conference: %s", connection);
                }
                continue;
            }

            // If this connection does not support being in a conference call, then it is not
            // conferenceable with any other connection.
            if (!connection.isConferenceSupported()) {
                connection.setConferenceables(Collections.<Conferenceable>emptyList());
                continue;
            }

            switch (connection.getState()) {
                case Connection.STATE_ACTIVE:
                    // fall through
                case Connection.STATE_HOLDING:
                    conferenceableSet.add(connection);
                    continue;
                default:
                    break;
            }
            // This connection is not active or holding, so clear all conferencable connections
            connection.setConferenceables(Collections.<Conferenceable>emptyList());
        }
        // Also loop through all active conferences and collect the ones that are ACTIVE or HOLDING.
        for (ImsConference conference : mImsConferences) {
            if (Log.DEBUG) {
                Log.d(this, "recalc - %s %s", conference.getState(), conference);
            }

            if (!conference.isConferenceHost()) {
                if (Log.VERBOSE) {
                    Log.v(this, "skipping conference (not hosted on this device): %s", conference);
                }
                continue;
            }

            // Since UE cannot host two conference calls, remove the ability to initiate
            // another conference call as there already exists a conference call, which
            // is hosted on this device.
            mTelecomAccountRegistry.refreshAdhocConference(false);

            switch (conference.getState()) {
                case Connection.STATE_ACTIVE:
                    //fall through
                case Connection.STATE_HOLDING:
                    if (!conference.isFullConference()) {
                        conferenceParticipantsSet.addAll(conference.getConnections());
                        conferenceableSet.add(conference);
                    }
                    continue;
                default:
                    break;
            }
        }

        Log.v(this, "conferenceableSet size: " + conferenceableSet.size());

        for (Conferenceable c : conferenceableSet) {
            if (c instanceof Connection) {
                // Remove this connection from the Set and add all others
                List<Conferenceable> conferenceables = conferenceableSet
                        .stream()
                        .filter(conferenceable -> c != conferenceable)
                        .collect(Collectors.toList());
                // TODO: Remove this once RemoteConnection#setConferenceableConnections is fixed.
                // Add all conference participant connections as conferenceable with a standalone
                // Connection.  We need to do this to ensure that RemoteConnections work properly.
                // At the current time, a RemoteConnection will not be conferenceable with a
                // Conference, so we need to add its children to ensure the user can merge the call
                // into the conference.
                // We should add support for RemoteConnection#setConferenceables, which accepts a
                // list of remote conferences and connections in the future.
                conferenceables.addAll(conferenceParticipantsSet);

                ((Connection) c).setConferenceables(conferenceables);
            } else if (c instanceof ImsConference) {
                ImsConference imsConference = (ImsConference) c;

                // If the conference is full, don't allow anything to be conferenced with it.
                if (imsConference.isFullConference()) {
                    imsConference.setConferenceableConnections(Collections.<Connection>emptyList());
                }

                // Remove all conferences from the set, since we can not conference a conference
                // to another conference.
                List<Connection> connections = conferenceableSet
                        .stream()
                        .filter(conferenceable -> conferenceable instanceof Connection)
                        .map(conferenceable -> (Connection) conferenceable)
                        .collect(Collectors.toList());
                // Conference equivalent to setConferenceables that only accepts Connections
                imsConference.setConferenceableConnections(connections);
            }
        }
    }

    /**
     * Determines if a connection is a member of a conference hosted on another device.
     *
     * @param connection The connection.
     * @return {@code true} if the connection is a member of a conference hosted on another device.
     */
    private boolean isMemberOfPeerConference(Connection connection) {
        if (!(connection instanceof TelephonyConnection)) {
            return false;
        }
        TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
        com.android.internal.telephony.Connection originalConnection =
                telephonyConnection.getOriginalConnection();

        return originalConnection != null && originalConnection.isMultiparty() &&
                originalConnection.isMemberOfPeerConference();
    }

    /**
     * Starts a new ImsConference for a connection which just entered a multiparty state.
     */
    private void recalculateConference() {
        Log.v(this, "recalculateConference");

        Iterator<TelephonyConnection> it = mTelephonyConnections.iterator();
        while (it.hasNext()) {
            TelephonyConnection connection = it.next();
            if (connection.isImsConnection() && connection.getOriginalConnection() != null &&
                    connection.getOriginalConnection().isMultiparty()) {

                startConference(connection);
                it.remove();
            }
        }
    }

    /**
     * Starts a new {@link ImsConference} for the given IMS connection.
     * <p>
     * Creates a new IMS Conference to manage the conference represented by the connection.
     * Internally the ImsConference wraps the radio connection with a new TelephonyConnection
     * which is NOT reported to the connection service and Telecom.
     * <p>
     * Once the new IMS Conference has been created, the connection passed in is held and removed
     * from the connection service (removing it from Telecom).  The connection is put into a held
     * state to ensure that telecom removes the connection without putting it into a disconnected
     * state first.
     *
     * @param connection The connection to the Ims server.
     */
    private void startConference(TelephonyConnection connection) {
        if (Log.VERBOSE) {
            Log.v(this, "Start new ImsConference - connection: %s", connection);
        }

        if (connection.isAdhocConferenceCall()) {
            Log.w(this, "start new ImsConference - control should never come here");
            return;
        }
        // Make a clone of the connection which will become the Ims conference host connection.
        // This is necessary since the Connection Service does not support removing a connection
        // from Telecom.  Instead we create a new instance and remove the old one from telecom.
        TelephonyConnection conferenceHostConnection = connection.cloneConnection();
        conferenceHostConnection.setVideoPauseSupported(connection.getVideoPauseSupported());
        conferenceHostConnection.setManageImsConferenceCallSupported(
                connection.isManageImsConferenceCallSupported());
        // WARNING: do not try to copy the video provider from connection to
        // conferenceHostConnection here.  In connection.cloneConnection, part of the clone
        // process is to set the original connection so it's already set:
        // conferenceHostConnection.setVideoProvider(connection.getVideoProvider());
        // There is a subtle concurrency issue here where at the time of merge, the
        // TelephonyConnection potentially has the WRONG video provider set on it (compared to
        // the ImsPhoneConnection (ie original connection) which has the correct one.
        // If you follow the logic in ImsPhoneCallTracker#onCallMerged through, what happens is the
        // new post-merge video provider is set on the ImsPhoneConnection.  That informs it's
        // listeners (e.g. TelephonyConnection) via a handler.  We immediately change the multiparty
        // start of the host connection and ImsPhoneCallTracker starts the setup we are
        // performing here.  When cloning TelephonyConnection, we get the right VideoProvider
        // because it is copied from the originalConnection, not using the potentially stale value
        // in the TelephonyConnection.

        PhoneAccountHandle phoneAccountHandle = null;

        // Attempt to determine the phone account associated with the conference host connection.
        ImsConference.CarrierConfiguration carrierConfig = null;
        if (connection.getPhone() != null &&
                connection.getPhone().getPhoneType() == PhoneConstants.PHONE_TYPE_IMS) {
            Phone imsPhone = connection.getPhone();
            // The phone account handle for an ImsPhone is based on the default phone (ie the
            // base GSM or CDMA phone, not on the ImsPhone itself).
            phoneAccountHandle =
                    PhoneUtils.makePstnPhoneAccountHandle(imsPhone.getDefaultPhone());
            carrierConfig = getCarrierConfig(imsPhone);
        }

        ImsConference conference = new ImsConference(mTelecomAccountRegistry, mConnectionService,
                conferenceHostConnection, phoneAccountHandle, mFeatureFlagProxy, carrierConfig);
        conference.setState(conferenceHostConnection.getState());
        conference.setCallDirection(conferenceHostConnection.getCallDirection());
        conference.addTelephonyConferenceListener(mConferenceListener);
        conference.updateConferenceParticipantsAfterCreation();
        mConnectionService.addConference(conference);
        conferenceHostConnection.setTelecomCallId(conference.getTelecomCallId());

        // Cleanup TelephonyConnection which backed the original connection and remove from telecom.
        // Use the "Other" disconnect cause to ensure the call is logged to the call log but the
        // disconnect tone is not played.
        connection.removeTelephonyConnectionListener(mTelephonyConnectionListener);
        connection.setTelephonyConnectionDisconnected(new DisconnectCause(DisconnectCause.OTHER,
                android.telephony.DisconnectCause.toString(
                        android.telephony.DisconnectCause.IMS_MERGED_SUCCESSFULLY)));
        connection.close();
        mImsConferences.add(conference);
        // If one of the participants failed to join the conference, recalculate will set the
        // conferenceable connections for the conference to show merge calls option.
        recalculateConferenceable();
    }

    public static ImsConference.CarrierConfiguration getCarrierConfig(Phone phone) {
        ImsConference.CarrierConfiguration.Builder config =
                new ImsConference.CarrierConfiguration.Builder();
        if (phone == null) {
            return config.build();
        }

        CarrierConfigManager cfgManager = (CarrierConfigManager)
                phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (cfgManager != null) {
            PersistableBundle bundle = cfgManager.getConfigForSubId(phone.getSubId());
            boolean isMaximumConferenceSizeEnforced = bundle.getBoolean(
                    CarrierConfigManager.KEY_IS_IMS_CONFERENCE_SIZE_ENFORCED_BOOL);
            int maximumConferenceSize = bundle.getInt(
                    CarrierConfigManager.KEY_IMS_CONFERENCE_SIZE_LIMIT_INT);
            boolean isHoldAllowed = bundle.getBoolean(
                    CarrierConfigManager.KEY_ALLOW_HOLD_IN_IMS_CALL_BOOL);
            boolean shouldLocalDisconnectOnEmptyConference = bundle.getBoolean(
                    CarrierConfigManager.KEY_LOCAL_DISCONNECT_EMPTY_IMS_CONFERENCE_BOOL);

            config.setIsMaximumConferenceSizeEnforced(isMaximumConferenceSizeEnforced)
                    .setMaximumConferenceSize(maximumConferenceSize)
                    .setIsHoldAllowed(isHoldAllowed)
                    .setShouldLocalDisconnectEmptyConference(
                            shouldLocalDisconnectOnEmptyConference);
        }
        return config.build();
    }
}
