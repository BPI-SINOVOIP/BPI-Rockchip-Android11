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

import android.annotation.NonNull;
import android.content.Context;
import android.graphics.drawable.Icon;
import android.net.Uri;
import android.os.Bundle;
import android.telecom.Connection;
import android.telecom.Connection.VideoProvider;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccountHandle;
import android.telecom.StatusHints;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.PhoneNumberUtils;
import android.text.TextUtils;
import android.util.Pair;

import com.android.ims.internal.ConferenceParticipant;
import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.Call;
import com.android.internal.telephony.CallStateException;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.phone.PhoneUtils;
import com.android.phone.R;
import com.android.telephony.Rlog;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Represents an IMS conference call.
 * <p>
 * An IMS conference call consists of a conference host connection and potentially a list of
 * conference participants.  The conference host connection represents the radio connection to the
 * IMS conference server.  Since it is not a connection to any one individual, it is not represented
 * in Telecom/InCall as a call.  The conference participant information is received via the host
 * connection via a conference event package.  Conference participant connections do not represent
 * actual radio connections to the participants; they act as a virtual representation of the
 * participant, keyed by a unique endpoint {@link android.net.Uri}.
 * <p>
 * The {@link ImsConference} listens for conference event package data received via the host
 * connection and is responsible for managing the conference participant connections which represent
 * the participants.
 */
public class ImsConference extends TelephonyConferenceBase implements Holdable {

    private static final String LOG_TAG = "ImsConference";

    /**
     * Abstracts out fetching a feature flag.  Makes testing easier.
     */
    public interface FeatureFlagProxy {
        boolean isUsingSinglePartyCallEmulation();
    }

    /**
     * Abstracts out carrier configuration items specific to the conference.
     */
    public static class CarrierConfiguration {
        /**
         * Builds and instance of {@link CarrierConfiguration}.
         */
        public static class Builder {
            private boolean mIsMaximumConferenceSizeEnforced = false;
            private int mMaximumConferenceSize = 5;
            private boolean mShouldLocalDisconnectEmptyConference = false;
            private boolean mIsHoldAllowed = false;

            /**
             * Sets whether the maximum size of the conference is enforced.
             * @param isMaximumConferenceSizeEnforced {@code true} if conference size enforced.
             * @return builder instance.
             */
            public Builder setIsMaximumConferenceSizeEnforced(
                    boolean isMaximumConferenceSizeEnforced) {
                mIsMaximumConferenceSizeEnforced = isMaximumConferenceSizeEnforced;
                return this;
            }

            /**
             * Sets the maximum size of an IMS conference.
             * @param maximumConferenceSize Max conference size.
             * @return builder instance.
             */
            public Builder setMaximumConferenceSize(int maximumConferenceSize) {
                mMaximumConferenceSize = maximumConferenceSize;
                return this;
            }

            /**
             * Sets whether an empty conference should be locally disconnected.
             * @param shouldLocalDisconnectEmptyConference {@code true} if conference should be
             * locally disconnected if empty.
             * @return builder instance.
             */
            public Builder setShouldLocalDisconnectEmptyConference(
                    boolean shouldLocalDisconnectEmptyConference) {
                mShouldLocalDisconnectEmptyConference = shouldLocalDisconnectEmptyConference;
                return this;
            }

            /**
             * Sets whether holding the conference is allowed.
             * @param isHoldAllowed {@code true} if holding is allowed.
             * @return builder instance.
             */
            public Builder setIsHoldAllowed(boolean isHoldAllowed) {
                mIsHoldAllowed = isHoldAllowed;
                return this;
            }

            /**
             * Build instance of {@link CarrierConfiguration}.
             * @return carrier config instance.
             */
            public ImsConference.CarrierConfiguration build() {
                return new ImsConference.CarrierConfiguration(mIsMaximumConferenceSizeEnforced,
                        mMaximumConferenceSize, mShouldLocalDisconnectEmptyConference,
                        mIsHoldAllowed);
            }
        }

        private boolean mIsMaximumConferenceSizeEnforced;

        private int mMaximumConferenceSize;

        private boolean mShouldLocalDisconnectEmptyConference;

        private boolean mIsHoldAllowed;

        private CarrierConfiguration(boolean isMaximumConferenceSizeEnforced,
                int maximumConferenceSize, boolean shouldLocalDisconnectEmptyConference,
                boolean isHoldAllowed) {
            mIsMaximumConferenceSizeEnforced = isMaximumConferenceSizeEnforced;
            mMaximumConferenceSize = maximumConferenceSize;
            mShouldLocalDisconnectEmptyConference = shouldLocalDisconnectEmptyConference;
            mIsHoldAllowed = isHoldAllowed;
        }

        /**
         * Determines whether the {@link ImsConference} should enforce a size limit based on
         * {@link #getMaximumConferenceSize()}.
         * {@code true} if maximum size limit should be enforced, {@code false} otherwise.
         */
        public boolean isMaximumConferenceSizeEnforced() {
            return mIsMaximumConferenceSizeEnforced;
        }

        /**
         * Determines the maximum number of participants (not including the host) in a conference
         * which is enforced when {@link #isMaximumConferenceSizeEnforced()} is {@code true}.
         */
        public int getMaximumConferenceSize() {
            return mMaximumConferenceSize;
        }

        /**
         * Determines whether this {@link ImsConference} should locally disconnect itself when the
         * number of participants in the conference drops to zero.
         * {@code true} if empty conference should be locally disconnected, {@code false}
         * otherwise.
         */
        public boolean shouldLocalDisconnectEmptyConference() {
            return mShouldLocalDisconnectEmptyConference;
        }

        /**
         * Determines whether holding the conference is permitted or not.
         * {@code true} if hold is permitted, {@code false} otherwise.
         */
        public boolean isHoldAllowed() {
            return mIsHoldAllowed;
        }
    }

    /**
     * Listener used to respond to changes to the underlying radio connection for the conference
     * host connection.  Used to respond to SRVCC changes.
     */
    private final TelephonyConnection.TelephonyConnectionListener mTelephonyConnectionListener =
            new TelephonyConnection.TelephonyConnectionListener() {

                /**
                 * Updates the state of the conference based on the new state of the host.
                 *
                 * @param c The host connection.
                 * @param state The new state
                 */
                @Override
                public void onStateChanged(android.telecom.Connection c, int state) {
                    setState(state);
                }

                /**
                 * Disconnects the conference when its host connection disconnects.
                 *
                 * @param c The host connection.
                 * @param disconnectCause The host connection disconnect cause.
                 */
                @Override
                public void onDisconnected(android.telecom.Connection c,
                        DisconnectCause disconnectCause) {
                    setDisconnected(disconnectCause);
                }

                @Override
                public void onVideoStateChanged(android.telecom.Connection c, int videoState) {
                    Log.d(this, "onVideoStateChanged video state %d", videoState);
                    setVideoState(c, videoState);
                }

                @Override
                public void onVideoProviderChanged(android.telecom.Connection c,
                        Connection.VideoProvider videoProvider) {
                    Log.d(this, "onVideoProviderChanged: Connection: %s, VideoProvider: %s", c,
                            videoProvider);
                    setVideoProvider(c, videoProvider);
                }

                @Override
                public void onConnectionCapabilitiesChanged(Connection c,
                        int connectionCapabilities) {
                    Log.d(this, "onConnectionCapabilitiesChanged: Connection: %s,"
                            + " connectionCapabilities: %s", c, connectionCapabilities);
                    int capabilites = ImsConference.this.getConnectionCapabilities();
                    boolean isVideoConferencingSupported = mConferenceHost == null ? false :
                            mConferenceHost.isCarrierVideoConferencingSupported();
                    setConnectionCapabilities(
                            applyHostCapabilities(capabilites, connectionCapabilities,
                                    isVideoConferencingSupported));
                }

                @Override
                public void onConnectionPropertiesChanged(Connection c, int connectionProperties) {
                    Log.d(this, "onConnectionPropertiesChanged: Connection: %s,"
                            + " connectionProperties: %s", c, connectionProperties);
                    updateConnectionProperties(connectionProperties);
                }

                @Override
                public void onStatusHintsChanged(Connection c, StatusHints statusHints) {
                    Log.v(this, "onStatusHintsChanged");
                    updateStatusHints();
                }

                @Override
                public void onExtrasChanged(Connection c, Bundle extras) {
                    Log.v(this, "onExtrasChanged: c=" + c + " Extras=" + Rlog.pii(LOG_TAG, extras));
                    updateExtras(extras);
                }

                @Override
                public void onExtrasRemoved(Connection c, List<String> keys) {
                    Log.v(this, "onExtrasRemoved: c=" + c + " key=" + keys);
                    removeExtras(keys);
                }

                @Override
                public void onConnectionEvent(Connection c, String event, Bundle extras) {
                    if (Connection.EVENT_MERGE_START.equals(event)) {
                        // Do not pass a merge start event on the underlying host connection; only
                        // indicate a merge has started on the connections which are merged into a
                        // conference.
                        return;
                    }

                    sendConferenceEvent(event, extras);
                }

                @Override
                public void onOriginalConnectionConfigured(TelephonyConnection c) {
                    if (c == mConferenceHost) {
                        handleOriginalConnectionChange();
                    }
                }

                /**
                 * Handles changes to conference participant data as reported by the conference host
                 * connection.
                 *
                 * @param c The connection.
                 * @param participants The participant information.
                 */
                @Override
                public void onConferenceParticipantsChanged(android.telecom.Connection c,
                        List<ConferenceParticipant> participants) {

                    if (c == null || participants == null) {
                        return;
                    }
                    Log.v(this, "onConferenceParticipantsChanged: %d participants",
                            participants.size());
                    TelephonyConnection telephonyConnection = (TelephonyConnection) c;
                    handleConferenceParticipantsUpdate(telephonyConnection, participants);
                }

                /**
                 * Handles request to play a ringback tone.
                 *
                 * @param c The connection.
                 * @param ringback Whether the ringback tone is to be played.
                 */
                @Override
                public void onRingbackRequested(android.telecom.Connection c, boolean ringback) {
                    Log.d(this, "onRingbackRequested ringback %s", ringback ? "Y" : "N");
                    setRingbackRequested(ringback);
                }
            };

    /**
     * The telephony connection service; used to add new participant connections to Telecom.
     */
    private TelephonyConnectionServiceProxy mTelephonyConnectionService;

    /**
     * The connection to the conference server which is hosting the conference.
     */
    private TelephonyConnection mConferenceHost;

    /**
     * The PhoneAccountHandle of the conference host.
     */
    private PhoneAccountHandle mConferenceHostPhoneAccountHandle;

    /**
     * The address of the conference host.
     */
    private Uri[] mConferenceHostAddress;

    private TelecomAccountRegistry mTelecomAccountRegistry;

    /**
     * The participant with which Adhoc Conference call is getting formed.
     */
    private List<Uri> mParticipants;

    /**
     * The known conference participant connections.  The HashMap is keyed by a Pair containing
     * the handle and endpoint Uris.
     * Access to the hashmap is protected by the {@link #mUpdateSyncRoot}.
     */
    private final HashMap<Pair<Uri, Uri>, ConferenceParticipantConnection>
            mConferenceParticipantConnections = new HashMap<>();

    /**
     * Sychronization root used to ensure that updates to the
     * {@link #mConferenceParticipantConnections} happen atomically are are not interleaved across
     * threads.  There are some instances where the network will send conference event package
     * data closely spaced.  If that happens, it is possible that the interleaving of the update
     * will cause duplicate participant info to be added.
     */
    private final Object mUpdateSyncRoot = new Object();

    private boolean mIsHoldable;
    private boolean mCouldManageConference;
    private FeatureFlagProxy mFeatureFlagProxy;
    private final CarrierConfiguration mCarrierConfig;
    private boolean mIsUsingSimCallManager = false;

    /**
     * Where {@link #isMultiparty()} is {@code false}, contains the
     * {@link ConferenceParticipantConnection#getUserEntity()} and
     * {@link ConferenceParticipantConnection#getEndpoint()} of the single participant which this
     * conference pretends to be.
     */
    private Pair<Uri, Uri> mLoneParticipantIdentity = null;

    /**
     * The {@link ConferenceParticipantConnection#getUserEntity()} and
     * {@link ConferenceParticipantConnection#getEndpoint()} of the conference host as they appear
     * in the CEP.  This is determined when we scan the first conference event package.
     * It is possible that this will be {@code null} for carriers which do not include the host
     * in the CEP.
     */
    private Pair<Uri, Uri> mHostParticipantIdentity = null;

    public void updateConferenceParticipantsAfterCreation() {
        if (mConferenceHost != null) {
            Log.v(this, "updateConferenceStateAfterCreation :: process participant update");
            handleConferenceParticipantsUpdate(mConferenceHost,
                    mConferenceHost.getConferenceParticipants());
        } else {
            Log.v(this, "updateConferenceStateAfterCreation :: null mConferenceHost");
        }
    }

    /**
     * Initializes a new {@link ImsConference}.
     *  @param telephonyConnectionService The connection service responsible for adding new
     *                                   conferene participants.
     * @param conferenceHost The telephony connection hosting the conference.
     * @param phoneAccountHandle The phone account handle associated with the conference.
     * @param featureFlagProxy
     */
    public ImsConference(TelecomAccountRegistry telecomAccountRegistry,
            TelephonyConnectionServiceProxy telephonyConnectionService,
            TelephonyConnection conferenceHost, PhoneAccountHandle phoneAccountHandle,
            FeatureFlagProxy featureFlagProxy, CarrierConfiguration carrierConfig) {

        super(phoneAccountHandle);

        mTelecomAccountRegistry = telecomAccountRegistry;
        mFeatureFlagProxy = featureFlagProxy;
        mCarrierConfig = carrierConfig;

        // Specify the connection time of the conference to be the connection time of the original
        // connection.
        long connectTime = conferenceHost.getOriginalConnection().getConnectTime();
        long connectElapsedTime = conferenceHost.getOriginalConnection().getConnectTimeReal();
        setConnectionTime(connectTime);
        setConnectionStartElapsedRealtimeMillis(connectElapsedTime);
        // Set the connectTime in the connection as well.
        conferenceHost.setConnectTimeMillis(connectTime);
        conferenceHost.setConnectionStartElapsedRealtimeMillis(connectElapsedTime);

        mTelephonyConnectionService = telephonyConnectionService;
        setConferenceHost(conferenceHost);
        setVideoProvider(conferenceHost, conferenceHost.getVideoProvider());

        int capabilities = Connection.CAPABILITY_MUTE |
                Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN;
        if (mCarrierConfig.isHoldAllowed()) {
            capabilities |= Connection.CAPABILITY_SUPPORT_HOLD | Connection.CAPABILITY_HOLD;
            mIsHoldable = true;
        }
        capabilities = applyHostCapabilities(capabilities,
                mConferenceHost.getConnectionCapabilities(),
                mConferenceHost.isCarrierVideoConferencingSupported());
        setConnectionCapabilities(capabilities);
    }

    /**
     * Transfers capabilities from the conference host to the conference itself.
     *
     * @param conferenceCapabilities The current conference capabilities.
     * @param capabilities The new conference host capabilities.
     * @param isVideoConferencingSupported Whether video conferencing is supported.
     * @return The merged capabilities to be applied to the conference.
     */
    private int applyHostCapabilities(int conferenceCapabilities, int capabilities,
            boolean isVideoConferencingSupported) {

        conferenceCapabilities = changeBitmask(conferenceCapabilities,
                    Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL,
                (capabilities & Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL) != 0);

        if (isVideoConferencingSupported) {
            conferenceCapabilities = changeBitmask(conferenceCapabilities,
                    Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL,
                    (capabilities & Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL) != 0);
            conferenceCapabilities = changeBitmask(conferenceCapabilities,
                    Connection.CAPABILITY_CAN_UPGRADE_TO_VIDEO,
                    (capabilities & Connection.CAPABILITY_CAN_UPGRADE_TO_VIDEO) != 0);
        } else {
            // If video conferencing is not supported, explicitly turn off the remote video
            // capability and the ability to upgrade to video.
            Log.v(this, "applyHostCapabilities : video conferencing not supported");
            conferenceCapabilities = changeBitmask(conferenceCapabilities,
                    Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL, false);
            conferenceCapabilities = changeBitmask(conferenceCapabilities,
                    Connection.CAPABILITY_CAN_UPGRADE_TO_VIDEO, false);
        }

        conferenceCapabilities = changeBitmask(conferenceCapabilities,
                Connection.CAPABILITY_CANNOT_DOWNGRADE_VIDEO_TO_AUDIO,
                (capabilities & Connection.CAPABILITY_CANNOT_DOWNGRADE_VIDEO_TO_AUDIO) != 0);

        conferenceCapabilities = changeBitmask(conferenceCapabilities,
                Connection.CAPABILITY_CAN_PAUSE_VIDEO,
                mConferenceHost.getVideoPauseSupported() && isVideoCapable());

        conferenceCapabilities = changeBitmask(conferenceCapabilities,
                Connection.CAPABILITY_ADD_PARTICIPANT,
                (capabilities & Connection.CAPABILITY_ADD_PARTICIPANT) != 0);

        return conferenceCapabilities;
    }

    /**
     * Transfers properties from the conference host to the conference itself.
     *
     * @param conferenceProperties The current conference properties.
     * @param properties The new conference host properties.
     * @return The merged properties to be applied to the conference.
     */
    private int applyHostProperties(int conferenceProperties, int properties) {
        conferenceProperties = changeBitmask(conferenceProperties,
                Connection.PROPERTY_HIGH_DEF_AUDIO,
                (properties & Connection.PROPERTY_HIGH_DEF_AUDIO) != 0);

        conferenceProperties = changeBitmask(conferenceProperties,
                Connection.PROPERTY_WIFI,
                (properties & Connection.PROPERTY_WIFI) != 0);

        conferenceProperties = changeBitmask(conferenceProperties,
                Connection.PROPERTY_IS_EXTERNAL_CALL,
                (properties & Connection.PROPERTY_IS_EXTERNAL_CALL) != 0);

        conferenceProperties = changeBitmask(conferenceProperties,
                Connection.PROPERTY_REMOTELY_HOSTED, !isConferenceHost());

        conferenceProperties = changeBitmask(conferenceProperties,
                Connection.PROPERTY_IS_ADHOC_CONFERENCE,
                (properties & Connection.PROPERTY_IS_ADHOC_CONFERENCE) != 0);

        return conferenceProperties;
    }

    /**
     * Not used by the IMS conference controller.
     *
     * @return {@code Null}.
     */
    @Override
    public android.telecom.Connection getPrimaryConnection() {
        return null;
    }

    /**
     * Returns VideoProvider of the conference. This can be null.
     *
     * @hide
     */
    @Override
    public VideoProvider getVideoProvider() {
        if (mConferenceHost != null) {
            return mConferenceHost.getVideoProvider();
        }
        return null;
    }

    /**
     * Returns video state of conference
     *
     * @hide
     */
    @Override
    public int getVideoState() {
        if (mConferenceHost != null) {
            return mConferenceHost.getVideoState();
        }
        return VideoProfile.STATE_AUDIO_ONLY;
    }

    public Connection getConferenceHost() {
        return mConferenceHost;
    }

    /**
     * @return The address's to which this Connection is currently communicating.
     */
    public final List<Uri> getParticipants() {
        return mParticipants;
    }

    /**
     * Sets the value of the {@link #getParticipants()}.
     *
     * @param address The new address's.
     */
    public final void setParticipants(List<Uri> address) {
        mParticipants = address;
    }

    /**
     * Invoked when the Conference and all its {@link Connection}s should be disconnected.
     * <p>
     * Hangs up the call via the conference host connection.  When the host connection has been
     * successfully disconnected, the {@link #mTelephonyConnectionListener} listener receives an
     * {@code onDestroyed} event, which triggers the conference participant connections to be
     * disconnected.
     */
    @Override
    public void onDisconnect() {
        Log.v(this, "onDisconnect: hanging up conference host.");
        if (mConferenceHost == null) {
            return;
        }

        disconnectConferenceParticipants();

        Call call = mConferenceHost.getCall();
        if (call != null) {
            try {
                call.hangup();
            } catch (CallStateException e) {
                Log.e(this, e, "Exception thrown trying to hangup conference");
            }
        } else {
            Log.w(this, "onDisconnect - null call");
        }
    }

    /**
     * Invoked when the specified {@link android.telecom.Connection} should be separated from the
     * conference call.
     * <p>
     * IMS does not support separating connections from the conference.
     *
     * @param connection The connection to separate.
     */
    @Override
    public void onSeparate(android.telecom.Connection connection) {
        Log.wtf(this, "Cannot separate connections from an IMS conference.");
    }

    /**
     * Invoked when the specified {@link android.telecom.Connection} should be merged into the
     * conference call.
     *
     * @param connection The {@code Connection} to merge.
     */
    @Override
    public void onMerge(android.telecom.Connection connection) {
        try {
            Phone phone = mConferenceHost.getPhone();
            if (phone != null) {
                phone.conference();
            }
        } catch (CallStateException e) {
            Log.e(this, e, "Exception thrown trying to merge call into a conference");
        }
    }

    /**
     * Supports adding participants to an existing conference call
     *
     * @param participants that are pulled to existing conference call
     */
    @Override
    public void onAddConferenceParticipants(List<Uri> participants) {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.performAddConferenceParticipants(participants);
    }

    /**
     * Invoked when the conference is answered.
     */
    @Override
    public void onAnswer(int videoState) {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.performAnswer(videoState);
    }

    /**
     * Invoked when the conference is rejected.
     */
    @Override
    public void onReject() {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.performReject(android.telecom.Call.REJECT_REASON_DECLINED);
    }

    /**
     * Invoked when the conference should be put on hold.
     */
    @Override
    public void onHold() {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.performHold();
    }

    /**
     * Invoked when the conference should be moved from hold to active.
     */
    @Override
    public void onUnhold() {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.performUnhold();
    }

    /**
     * Invoked to play a DTMF tone.
     *
     * @param c A DTMF character.
     */
    @Override
    public void onPlayDtmfTone(char c) {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.onPlayDtmfTone(c);
    }

    /**
     * Invoked to stop playing a DTMF tone.
     */
    @Override
    public void onStopDtmfTone() {
        if (mConferenceHost == null) {
            return;
        }
        mConferenceHost.onStopDtmfTone();
    }

    /**
     * Handles the addition of connections to the {@link ImsConference}.  The
     * {@link ImsConferenceController} does not add connections to the conference.
     *
     * @param connection The newly added connection.
     */
    @Override
    public void onConnectionAdded(android.telecom.Connection connection) {
        // No-op
        Log.d(this, "connection added: " + connection
                + ", time: " + connection.getConnectTimeMillis());
    }

    @Override
    public void setHoldable(boolean isHoldable) {
        mIsHoldable = isHoldable;
        if (!mIsHoldable) {
            removeCapability(Connection.CAPABILITY_HOLD);
        } else {
            addCapability(Connection.CAPABILITY_HOLD);
        }
    }

    @Override
    public boolean isChildHoldable() {
        // The conference should not be a child of other conference.
        return false;
    }

    /**
     * Changes a bit-mask to add or remove a bit-field.
     *
     * @param bitmask The bit-mask.
     * @param bitfield The bit-field to change.
     * @param enabled Whether the bit-field should be set or removed.
     * @return The bit-mask with the bit-field changed.
     */
    private int changeBitmask(int bitmask, int bitfield, boolean enabled) {
        if (enabled) {
            return bitmask | bitfield;
        } else {
            return bitmask & ~bitfield;
        }
    }

    /**
     * Determines if this conference is hosted on the current device or the peer device.
     *
     * @return {@code true} if this conference is hosted on the current device, {@code false} if it
     *      is hosted on the peer device.
     */
    public boolean isConferenceHost() {
        if (mConferenceHost == null) {
            return false;
        }
        com.android.internal.telephony.Connection originalConnection =
                mConferenceHost.getOriginalConnection();

        return originalConnection != null && originalConnection.isMultiparty() &&
                originalConnection.isConferenceHost();
    }

    /**
     * Updates the manage conference capability of the conference.
     *
     * The following cases are handled:
     * <ul>
     *     <li>There is only a single participant in the conference -- manage conference is
     *     disabled.</li>
     *     <li>There is more than one participant in the conference -- manage conference is
     *     enabled.</li>
     *     <li>No conference event package data is available -- manage conference is disabled.</li>
     * </ul>
     * <p>
     * Note: We add and remove {@link Connection#CAPABILITY_CONFERENCE_HAS_NO_CHILDREN} to ensure
     * that the conference is represented appropriately on Bluetooth devices.
     */
    private void updateManageConference() {
        boolean couldManageConference =
                (getConnectionCapabilities() & Connection.CAPABILITY_MANAGE_CONFERENCE) != 0;
        boolean canManageConference = mFeatureFlagProxy.isUsingSinglePartyCallEmulation()
                && !isMultiparty()
                ? mConferenceParticipantConnections.size() > 1
                : mConferenceParticipantConnections.size() != 0;
        Log.v(this, "updateManageConference was :%s is:%s", couldManageConference ? "Y" : "N",
                canManageConference ? "Y" : "N");

        if (couldManageConference != canManageConference) {
            int capabilities = getConnectionCapabilities();

            if (canManageConference) {
                capabilities |= Connection.CAPABILITY_MANAGE_CONFERENCE;
                capabilities &= ~Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN;
            } else {
                capabilities &= ~Connection.CAPABILITY_MANAGE_CONFERENCE;
                capabilities |= Connection.CAPABILITY_CONFERENCE_HAS_NO_CHILDREN;
            }

            setConnectionCapabilities(capabilities);
        }
    }

    /**
     * Sets the connection hosting the conference and registers for callbacks.
     *
     * @param conferenceHost The connection hosting the conference.
     */
    private void setConferenceHost(TelephonyConnection conferenceHost) {
        Log.i(this, "setConferenceHost " + conferenceHost);

        mConferenceHost = conferenceHost;

        // Attempt to get the conference host's address (e.g. the host's own phone number).
        // We need to look at the default phone for the ImsPhone when creating the phone account
        // for the
        if (mConferenceHost.getPhone() != null &&
                mConferenceHost.getPhone().getPhoneType() == PhoneConstants.PHONE_TYPE_IMS) {
            // Look up the conference host's address; we need this later for filtering out the
            // conference host in conference event package data.
            Phone imsPhone = mConferenceHost.getPhone();
            mConferenceHostPhoneAccountHandle =
                    PhoneUtils.makePstnPhoneAccountHandle(imsPhone.getDefaultPhone());
            Uri hostAddress = mTelecomAccountRegistry.getAddress(mConferenceHostPhoneAccountHandle);

            ArrayList<Uri> hostAddresses = new ArrayList<>();

            // add address from TelecomAccountRegistry
            if (hostAddress != null) {
                hostAddresses.add(hostAddress);
            }

            // add addresses from phone
            if (imsPhone.getCurrentSubscriberUris() != null) {
                hostAddresses.addAll(
                        new ArrayList<>(Arrays.asList(imsPhone.getCurrentSubscriberUris())));
            }

            mConferenceHostAddress = new Uri[hostAddresses.size()];
            mConferenceHostAddress = hostAddresses.toArray(mConferenceHostAddress);

            Log.i(this, "setConferenceHost: hosts are "
                    + Arrays.stream(mConferenceHostAddress)
                    .map(Uri::getSchemeSpecificPart)
                    .map(ssp -> Rlog.pii(LOG_TAG, ssp))
                    .collect(Collectors.joining(", ")));

            mIsUsingSimCallManager = mTelecomAccountRegistry.isUsingSimCallManager(
                    mConferenceHostPhoneAccountHandle);
        }

        // If the conference is not hosted on this device copy over the address and presentation and
        // connect times so that we can log this appropriately in the call log.
        if (!isConferenceHost()) {
            setAddress(mConferenceHost.getAddress(), mConferenceHost.getAddressPresentation());
            setCallerDisplayName(mConferenceHost.getCallerDisplayName(),
                    mConferenceHost.getCallerDisplayNamePresentation());
            setConnectionStartElapsedRealtimeMillis(
                    mConferenceHost.getConnectionStartElapsedRealtimeMillis());
            setConnectionTime(mConferenceHost.getConnectTimeMillis());
        }

        mConferenceHost.addTelephonyConnectionListener(mTelephonyConnectionListener);
        setConnectionCapabilities(applyHostCapabilities(getConnectionCapabilities(),
                mConferenceHost.getConnectionCapabilities(),
                mConferenceHost.isCarrierVideoConferencingSupported()));
        setConnectionProperties(applyHostProperties(getConnectionProperties(),
                mConferenceHost.getConnectionProperties()));

        setState(mConferenceHost.getState());
        updateStatusHints();
        putExtras(mConferenceHost.getExtras());
    }

    /**
     * Handles state changes for conference participant(s).  The participants data passed in
     *
     * @param parent The connection which was notified of the conference participant.
     * @param participants The conference participant information.
     */
    @VisibleForTesting
    public void handleConferenceParticipantsUpdate(
            TelephonyConnection parent, List<ConferenceParticipant> participants) {

        if (participants == null) {
            return;
        }

        if (parent != null && !parent.isManageImsConferenceCallSupported()) {
            Log.i(this, "handleConferenceParticipantsUpdate: manage conference is disallowed");
            return;
        }

        Log.i(this, "handleConferenceParticipantsUpdate: size=%d", participants.size());

        // Perform the update in a synchronized manner.  It is possible for the IMS framework to
        // trigger two onConferenceParticipantsChanged callbacks in quick succession.  If the first
        // update adds new participants, and the second does something like update the status of one
        // of the participants, we can get into a situation where the participant is added twice.
        synchronized (mUpdateSyncRoot) {
            int oldParticipantCount = mConferenceParticipantConnections.size();
            boolean newParticipantsAdded = false;
            boolean oldParticipantsRemoved = false;
            ArrayList<ConferenceParticipant> newParticipants = new ArrayList<>(participants.size());
            HashSet<Pair<Uri,Uri>> participantUserEntities = new HashSet<>(participants.size());

            // Determine if the conference event package represents a single party conference.
            // A single party conference is one where there is no other participant other than the
            // conference host and one other participant.
            // We purposely exclude participants which have a disconnected state in the conference
            // event package; some carriers are known to keep a disconnected participant around in
            // subsequent CEP updates with a state of disconnected, even though its no longer part
            // of the conference.
            // Note: We consider 0 to still be a single party conference since some carriers will
            // send a conference event package with JUST the host in it when the conference is
            // disconnected.  We don't want to change back to conference mode prior to disconnection
            // or we will not log the call.
            boolean isSinglePartyConference = participants.stream()
                    .filter(p -> {
                        Pair<Uri, Uri> pIdent = new Pair<>(p.getHandle(), p.getEndpoint());
                        return !Objects.equals(mHostParticipantIdentity, pIdent)
                                && p.getState() != Connection.STATE_DISCONNECTED;
                    })
                    .count() <= 1;

            // We will only process the CEP data if:
            // 1. We're not emulating a single party call.
            // 2. We're emulating a single party call and the CEP contains more than just the
            //    single party
            if ((!isMultiparty() && !isSinglePartyConference)
                    || isMultiparty()) {
                // Add any new participants and update existing.
                for (ConferenceParticipant participant : participants) {
                    Pair<Uri, Uri> userEntity = new Pair<>(participant.getHandle(),
                            participant.getEndpoint());

                    // We will exclude disconnected participants from the hash set of tracked
                    // participants.  Some carriers are known to leave disconnected participants in
                    // the conference event package data which would cause them to be present in the
                    // conference even though they're disconnected.  Removing them from the hash set
                    // here means we'll clean them up below.
                    if (participant.getState() != Connection.STATE_DISCONNECTED) {
                        participantUserEntities.add(userEntity);
                    }
                    if (!mConferenceParticipantConnections.containsKey(userEntity)) {
                        // Some carriers will also include the conference host in the CEP.  We will
                        // filter that out here.
                        if (!isParticipantHost(mConferenceHostAddress, participant.getHandle())) {
                            createConferenceParticipantConnection(parent, participant);
                            newParticipants.add(participant);
                            newParticipantsAdded = true;
                        } else {
                            // Track the identity of the conference host; its useful to know when
                            // we look at the CEP in the future.
                            mHostParticipantIdentity = userEntity;
                        }
                    } else {
                        ConferenceParticipantConnection connection =
                                mConferenceParticipantConnections.get(userEntity);
                        Log.i(this,
                                "handleConferenceParticipantsUpdate: updateState, participant = %s",
                                participant);
                        connection.updateState(participant.getState());
                        if (participant.getState() == Connection.STATE_DISCONNECTED) {
                            /**
                             * Per {@link ConferenceParticipantConnection#updateState(int)}, we will
                             * destroy the connection when its disconnected.
                             */
                            handleConnectionDestruction(connection);
                        }
                        connection.setVideoState(parent.getVideoState());
                    }
                }

                // Set state of new participants.
                if (newParticipantsAdded) {
                    // Set the state of the new participants at once and add to the conference
                    for (ConferenceParticipant newParticipant : newParticipants) {
                        ConferenceParticipantConnection connection =
                                mConferenceParticipantConnections.get(new Pair<>(
                                        newParticipant.getHandle(),
                                        newParticipant.getEndpoint()));
                        connection.updateState(newParticipant.getState());
                        /**
                         * Per {@link ConferenceParticipantConnection#updateState(int)}, we will
                         * destroy the connection when its disconnected.
                         */
                        if (newParticipant.getState() == Connection.STATE_DISCONNECTED) {
                            handleConnectionDestruction(connection);
                        }
                        connection.setVideoState(parent.getVideoState());
                    }
                }

                // Finally, remove any participants from the conference that no longer exist in the
                // conference event package data.
                Iterator<Map.Entry<Pair<Uri, Uri>, ConferenceParticipantConnection>> entryIterator =
                        mConferenceParticipantConnections.entrySet().iterator();
                while (entryIterator.hasNext()) {
                    Map.Entry<Pair<Uri, Uri>, ConferenceParticipantConnection> entry =
                            entryIterator.next();

                    if (!participantUserEntities.contains(entry.getKey())) {
                        ConferenceParticipantConnection participant = entry.getValue();
                        participant.setDisconnected(new DisconnectCause(DisconnectCause.CANCELED));
                        removeTelephonyConnection(participant);
                        participant.destroy();
                        entryIterator.remove();
                        oldParticipantsRemoved = true;
                    }
                }
            }

            int newParticipantCount = mConferenceParticipantConnections.size();
            Log.v(this, "handleConferenceParticipantsUpdate: oldParticipantCount=%d, "
                            + "newParticipantcount=%d", oldParticipantCount, newParticipantCount);
            // If the single party call emulation fature flag is enabled, we can potentially treat
            // the conference as a single party call when there is just one participant.
            if (mFeatureFlagProxy.isUsingSinglePartyCallEmulation() &&
                    !mConferenceHost.isAdhocConferenceCall()) {
                if (oldParticipantCount != 1 && newParticipantCount == 1) {
                    // If number of participants goes to 1, emulate a single party call.
                    startEmulatingSinglePartyCall();
                } else if (!isMultiparty() && !isSinglePartyConference) {
                    // Number of participants increased, so stop emulating a single party call.
                    stopEmulatingSinglePartyCall();
                }
            }

            // If new participants were added or old ones were removed, we need to ensure the state
            // of the manage conference capability is updated.
            if (newParticipantsAdded || oldParticipantsRemoved) {
                updateManageConference();
            }

            // If the conference is empty and we're supposed to do a local disconnect, do so now.
            if (mCarrierConfig.shouldLocalDisconnectEmptyConference()
                    && oldParticipantCount > 0 && newParticipantCount == 0) {
                Log.i(this, "handleConferenceParticipantsUpdate: empty conference; "
                        + "local disconnect.");
                onDisconnect();
            }
        }
    }

    /**
     * Called after {@link #startEmulatingSinglePartyCall()} to cause the conference to appear as
     * if it is a conference again.
     * 1. Tell telecom we're a conference again.
     * 2. Restore {@link Connection#CAPABILITY_MANAGE_CONFERENCE} capability.
     * 3. Null out the name/address.
     *
     * Note: Single party call emulation is disabled if the conference is taking place via a
     * sim call manager.  Emulating a single party call requires properties of the conference to be
     * changed (connect time, address, conference state) which cannot be guaranteed to be relayed
     * correctly by the sim call manager to Telecom.
     */
    private void stopEmulatingSinglePartyCall() {
        if (mIsUsingSimCallManager) {
            Log.i(this, "stopEmulatingSinglePartyCall: using sim call manager; skip.");
            return;
        }

        Log.i(this, "stopEmulatingSinglePartyCall: conference now has more than one"
                + " participant; make it look conference-like again.");

        if (mCouldManageConference) {
            int currentCapabilities = getConnectionCapabilities();
            currentCapabilities |= Connection.CAPABILITY_MANAGE_CONFERENCE;
            setConnectionCapabilities(currentCapabilities);
        }

        // Null out the address/name so it doesn't look like a single party call
        setAddress(null, TelecomManager.PRESENTATION_UNKNOWN);
        setCallerDisplayName(null, TelecomManager.PRESENTATION_UNKNOWN);

        // Copy the conference connect time back to the previous lone participant.
        ConferenceParticipantConnection loneParticipant =
                mConferenceParticipantConnections.get(mLoneParticipantIdentity);
        if (loneParticipant != null) {
            Log.d(this,
                    "stopEmulatingSinglePartyCall: restored lone participant connect time");
            loneParticipant.setConnectTimeMillis(getConnectionTime());
            loneParticipant.setConnectionStartElapsedRealtimeMillis(
                    getConnectionStartElapsedRealtimeMillis());
        }

        // Tell Telecom its a conference again.
        setConferenceState(true);
    }

    /**
     * Called when a conference drops to a single participant. Causes this conference to present
     * itself to Telecom as if it was a single party call.
     * 1. Remove the participant from Telecom and from local tracking; when we get a new CEP in
     *    the future we'll just re-add the participant anyways.
     * 2. Tell telecom we're not a conference.
     * 3. Remove {@link Connection#CAPABILITY_MANAGE_CONFERENCE} capability.
     * 4. Set the name/address to that of the single participant.
     *
     * Note: Single party call emulation is disabled if the conference is taking place via a
     * sim call manager.  Emulating a single party call requires properties of the conference to be
     * changed (connect time, address, conference state) which cannot be guaranteed to be relayed
     * correctly by the sim call manager to Telecom.
     */
    private void startEmulatingSinglePartyCall() {
        if (mIsUsingSimCallManager) {
            Log.i(this, "startEmulatingSinglePartyCall: using sim call manager; skip.");
            return;
        }

        Log.i(this, "startEmulatingSinglePartyCall: conference has a single "
                + "participant; downgrade to single party call.");

        Iterator<ConferenceParticipantConnection> valueIterator =
                mConferenceParticipantConnections.values().iterator();
        if (valueIterator.hasNext()) {
            ConferenceParticipantConnection entry = valueIterator.next();

            // Set the conference name/number to that of the remaining participant.
            setAddress(entry.getAddress(), entry.getAddressPresentation());
            setCallerDisplayName(entry.getCallerDisplayName(),
                    entry.getCallerDisplayNamePresentation());
            setConnectionStartElapsedRealtimeMillis(
                    entry.getConnectionStartElapsedRealtimeMillis());
            setConnectionTime(entry.getConnectTimeMillis());
            setCallDirection(entry.getCallDirection());
            mLoneParticipantIdentity = new Pair<>(entry.getUserEntity(), entry.getEndpoint());

            // Remove the participant from Telecom.  It'll get picked up in a future CEP update
            // again anyways.
            entry.setDisconnected(new DisconnectCause(DisconnectCause.CANCELED,
                    DisconnectCause.REASON_EMULATING_SINGLE_CALL));
            removeTelephonyConnection(entry);
            entry.destroy();
            valueIterator.remove();
        }

        // Have Telecom pretend its not a conference.
        setConferenceState(false);

        // Remove manage conference capability.
        mCouldManageConference =
                (getConnectionCapabilities() & Connection.CAPABILITY_MANAGE_CONFERENCE) != 0;
        int currentCapabilities = getConnectionCapabilities();
        currentCapabilities &= ~Connection.CAPABILITY_MANAGE_CONFERENCE;
        setConnectionCapabilities(currentCapabilities);
    }

    /**
     * Creates a new {@link ConferenceParticipantConnection} to represent a
     * {@link ConferenceParticipant}.
     * <p>
     * The new connection is added to the conference controller and connection service.
     *
     * @param parent The connection which was notified of the participant change (e.g. the
     *                         parent connection).
     * @param participant The conference participant information.
     */
    private void createConferenceParticipantConnection(
            TelephonyConnection parent, ConferenceParticipant participant) {

        // Create and add the new connection in holding state so that it does not become the
        // active call.
        ConferenceParticipantConnection connection = new ConferenceParticipantConnection(
                parent.getOriginalConnection(), participant,
                !isConferenceHost() /* isRemotelyHosted */);
        if (participant.getConnectTime() == 0) {
            connection.setConnectTimeMillis(parent.getConnectTimeMillis());
            connection.setConnectionStartElapsedRealtimeMillis(
                    parent.getConnectionStartElapsedRealtimeMillis());
        } else {
            connection.setConnectTimeMillis(participant.getConnectTime());
            connection.setConnectionStartElapsedRealtimeMillis(participant.getConnectElapsedTime());
        }
        // Indicate whether this is an MT or MO call to Telecom; the participant has the cached
        // data from the time of merge.
        connection.setCallDirection(participant.getCallDirection());

        // Ensure important attributes of the parent get copied to child.
        connection.setConnectionProperties(applyHostPropertiesToChild(
                connection.getConnectionProperties(), parent.getConnectionProperties()));
        connection.setStatusHints(parent.getStatusHints());
        connection.setExtras(getChildExtrasFromHostBundle(parent.getExtras()));

        Log.i(this, "createConferenceParticipantConnection: participant=%s, connection=%s",
                participant, connection);

        synchronized(mUpdateSyncRoot) {
            mConferenceParticipantConnections.put(new Pair<>(participant.getHandle(),
                    participant.getEndpoint()), connection);
        }

        mTelephonyConnectionService.addExistingConnection(mConferenceHostPhoneAccountHandle,
                connection, this);
        addTelephonyConnection(connection);
    }

    /**
     * Removes a conference participant from the conference.
     *
     * @param participant The participant to remove.
     */
    private void removeConferenceParticipant(ConferenceParticipantConnection participant) {
        Log.i(this, "removeConferenceParticipant: %s", participant);

        synchronized(mUpdateSyncRoot) {
            mConferenceParticipantConnections.remove(new Pair<>(participant.getUserEntity(),
                    participant.getEndpoint()));
        }
        participant.destroy();
    }

    /**
     * Disconnects all conference participants from the conference.
     */
    private void disconnectConferenceParticipants() {
        Log.v(this, "disconnectConferenceParticipants");

        synchronized(mUpdateSyncRoot) {
            for (ConferenceParticipantConnection connection :
                    mConferenceParticipantConnections.values()) {

                // Mark disconnect cause as cancelled to ensure that the call is not logged in the
                // call log.
                connection.setDisconnected(new DisconnectCause(DisconnectCause.CANCELED));
                connection.destroy();
            }
            mConferenceParticipantConnections.clear();
            updateManageConference();
        }
    }

    /**
     * Extracts a phone number from a {@link Uri}.
     * <p>
     * Phone numbers can be represented either as a TEL URI or a SIP URI.
     * For conference event packages, RFC3261 specifies how participants can be identified using a
     * SIP URI.
     * A valid SIP uri has the format: sip:user:password@host:port;uri-parameters?headers
     * Per RFC3261, the "user" can be a telephone number.
     * For example: sip:1650555121;phone-context=blah.com@host.com
     * In this case, the phone number is in the user field of the URI, and the parameters can be
     * ignored.
     *
     * A SIP URI can also specify a phone number in a format similar to:
     * sip:+1-212-555-1212@something.com;user=phone
     * In this case, the phone number is again in user field and the parameters can be ignored.
     * We can get the user field in these instances by splitting the string on the @, ;, or :
     * and looking at the first found item.
     * @param handle The URI containing a SIP or TEL formatted phone number.
     * @return extracted phone number.
     */
    private static @NonNull String extractPhoneNumber(@NonNull Uri handle) {
        // Number is always in the scheme specific part, regardless of whether this is a TEL or SIP
        // URI.
        String number = handle.getSchemeSpecificPart();
        // Get anything before the @ for the SIP case.
        String[] numberParts = number.split("[@;:]");

        if (numberParts.length == 0) {
            Log.v(LOG_TAG, "extractPhoneNumber(N) : no number in handle");
            return "";
        }
        return numberParts[0];
    }

    /**
     * Determines if the passed in participant handle is the same as the conference host's handle.
     * Starts with a simple equality check.  However, the handles from a conference event package
     * will be a SIP uri, so we need to pull that apart to look for the participant's phone number.
     *
     * @param hostHandles The handle(s) of the connection hosting the conference, typically obtained
     *                    from P-Associated-Uri entries.
     * @param handle The handle of the conference participant.
     * @return {@code true} if the host's handle matches the participant's handle, {@code false}
     *      otherwise.
     */
    @VisibleForTesting
    public static boolean isParticipantHost(Uri[] hostHandles, Uri handle) {
        // If there is no host handle or no participant handle, bail early.
        if (hostHandles == null || hostHandles.length == 0 || handle == null) {
            Log.v(LOG_TAG, "isParticipantHost(N) : host or participant uri null");
            return false;
        }

        String number = extractPhoneNumber(handle);
        // If we couldn't extract the participant's number, then we can't determine if it is the
        // host or not.
        if (TextUtils.isEmpty(number)) {
            return false;
        }

        for (Uri hostHandle : hostHandles) {
            if (hostHandle == null) {
                continue;
            }
            // Similar to the CEP participant data, the host identity in the P-Associated-Uri could
            // be a SIP URI or a TEL URI.
            String hostNumber = extractPhoneNumber(hostHandle);

            // Use a loose comparison of the phone numbers.  This ensures that numbers that differ
            // by special characters are counted as equal.
            // E.g. +16505551212 would be the same as 16505551212
            boolean isHost = PhoneNumberUtils.compare(hostNumber, number);

            Log.v(LOG_TAG, "isParticipantHost(%s) : host: %s, participant %s", (isHost ? "Y" : "N"),
                    Rlog.pii(LOG_TAG, hostNumber), Rlog.pii(LOG_TAG, number));

            if (isHost) {
                return true;
            }
        }
        return false;
    }

    /**
     * Handles a change in the original connection backing the conference host connection.  This can
     * happen if an SRVCC event occurs on the original IMS connection, requiring a fallback to
     * GSM or CDMA.
     * <p>
     * If this happens, we will add the conference host connection to telecom and tear down the
     * conference.
     */
    private void handleOriginalConnectionChange() {
        if (mConferenceHost == null) {
            Log.w(this, "handleOriginalConnectionChange; conference host missing.");
            return;
        }

        com.android.internal.telephony.Connection originalConnection =
                mConferenceHost.getOriginalConnection();

        if (originalConnection != null &&
                originalConnection.getPhoneType() != PhoneConstants.PHONE_TYPE_IMS) {
            Log.i(this,
                    "handleOriginalConnectionChange : handover from IMS connection to " +
                            "new connection: %s", originalConnection);

            PhoneAccountHandle phoneAccountHandle = null;
            if (mConferenceHost.getPhone() != null) {
                if (mConferenceHost.getPhone().getPhoneType() == PhoneConstants.PHONE_TYPE_IMS) {
                    Phone imsPhone = mConferenceHost.getPhone();
                    // The phone account handle for an ImsPhone is based on the default phone (ie
                    // the base GSM or CDMA phone, not on the ImsPhone itself).
                    phoneAccountHandle =
                            PhoneUtils.makePstnPhoneAccountHandle(imsPhone.getDefaultPhone());
                } else {
                    // In the case of SRVCC, we still need a phone account, so use the top level
                    // phone to create a phone account.
                    phoneAccountHandle = PhoneUtils.makePstnPhoneAccountHandle(
                            mConferenceHost.getPhone());
                }
            }

            if (mConferenceHost.getPhone().getPhoneType() == PhoneConstants.PHONE_TYPE_GSM) {
                Log.i(this,"handleOriginalConnectionChange : SRVCC to GSM");
                GsmConnection c = new GsmConnection(originalConnection, getTelecomCallId(),
                        mConferenceHost.getCallDirection());
                // This is a newly created conference connection as a result of SRVCC
                c.setConferenceSupported(true);
                c.setTelephonyConnectionProperties(
                        c.getConnectionProperties() | Connection.PROPERTY_IS_DOWNGRADED_CONFERENCE);
                c.updateState();
                // Copy the connect time from the conferenceHost
                c.setConnectTimeMillis(mConferenceHost.getConnectTimeMillis());
                c.setConnectionStartElapsedRealtimeMillis(
                        mConferenceHost.getConnectionStartElapsedRealtimeMillis());
                mTelephonyConnectionService.addExistingConnection(phoneAccountHandle, c);
                mTelephonyConnectionService.addConnectionToConferenceController(c);
            } // CDMA case not applicable for SRVCC
            mConferenceHost.removeTelephonyConnectionListener(mTelephonyConnectionListener);
            mConferenceHost = null;
            setDisconnected(new DisconnectCause(DisconnectCause.OTHER));
            disconnectConferenceParticipants();
            destroyTelephonyConference();
        }

        updateStatusHints();
    }

    /**
     * Changes the state of the Ims conference.
     *
     * @param state the new state.
     */
    public void setState(int state) {
        Log.v(this, "setState %s", Connection.stateToString(state));

        switch (state) {
            case Connection.STATE_INITIALIZING:
            case Connection.STATE_NEW:
                // No-op -- not applicable.
                break;
            case Connection.STATE_RINGING:
                setConferenceOnRinging();
                break;
            case Connection.STATE_DIALING:
                setConferenceOnDialing();
                break;
            case Connection.STATE_DISCONNECTED:
                DisconnectCause disconnectCause;
                if (mConferenceHost == null) {
                    disconnectCause = new DisconnectCause(DisconnectCause.CANCELED);
                } else {
                    if (mConferenceHost.getPhone() != null) {
                        disconnectCause = DisconnectCauseUtil.toTelecomDisconnectCause(
                                mConferenceHost.getOriginalConnection().getDisconnectCause(),
                                null, mConferenceHost.getPhone().getPhoneId());
                    } else {
                        disconnectCause = DisconnectCauseUtil.toTelecomDisconnectCause(
                                mConferenceHost.getOriginalConnection().getDisconnectCause());
                    }
                }
                setDisconnected(disconnectCause);
                disconnectConferenceParticipants();
                destroyTelephonyConference();
                break;
            case Connection.STATE_ACTIVE:
                setConferenceOnActive();
                break;
            case Connection.STATE_HOLDING:
                setConferenceOnHold();
                break;
        }
    }

    /**
     * Determines if the host of this conference is capable of video calling.
     * @return {@code true} if video capable, {@code false} otherwise.
     */
    private boolean isVideoCapable() {
        int capabilities = mConferenceHost.getConnectionCapabilities();
        return (capabilities & Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL) != 0
                && (capabilities & Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL) != 0;
    }

    private void updateStatusHints() {
        if (mConferenceHost == null) {
            setStatusHints(null);
            return;
        }

        if (mConferenceHost.isWifi()) {
            Phone phone = mConferenceHost.getPhone();
            if (phone != null) {
                Context context = phone.getContext();
                StatusHints hints = new StatusHints(
                        context.getString(R.string.status_hint_label_wifi_call),
                        Icon.createWithResource(
                                context, R.drawable.ic_signal_wifi_4_bar_24dp),
                        null /* extras */);
                setStatusHints(hints);

                // Ensure the children know they're a WIFI call as well.
                for (Connection c : getConnections()) {
                    c.setStatusHints(hints);
                }
            }
        } else {
            setStatusHints(null);
        }
    }

    /**
     * Updates the conference's properties based on changes to the host.
     * Also ensures pertinent properties from the host such as the WIFI property are copied to the
     * children as well.
     * @param connectionProperties The new host properties.
     */
    private void updateConnectionProperties(int connectionProperties) {
        int properties = ImsConference.this.getConnectionProperties();
        setConnectionProperties(applyHostProperties(properties, connectionProperties));

        for (Connection c : getConnections()) {
            c.setConnectionProperties(applyHostPropertiesToChild(c.getConnectionProperties(),
                    connectionProperties));
        }
    }

    /**
     * Updates extras in the conference based on changes made in the parent.
     * Also copies select extras (e.g. EXTRA_CALL_NETWORK_TYPE) to the children as well.
     * @param extras The extras to copy.
     */
    private void updateExtras(Bundle extras) {
        putExtras(extras);

        if (extras == null) {
            return;
        }

        Bundle childBundle = getChildExtrasFromHostBundle(extras);
        for (Connection c : getConnections()) {
            c.putExtras(childBundle);
        }
    }

    /**
     * Given an extras bundle from the host, returns a new bundle containing those extras which are
     * releveant to the children.
     * @param extras The host extras.
     * @return The extras pertinent to the children.
     */
    private Bundle getChildExtrasFromHostBundle(Bundle extras) {
        Bundle extrasToCopy = new Bundle();
        if (extras != null && extras.containsKey(TelecomManager.EXTRA_CALL_NETWORK_TYPE)) {
            int networkType = extras.getInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE);
            extrasToCopy.putInt(TelecomManager.EXTRA_CALL_NETWORK_TYPE, networkType);
        }
        return extrasToCopy;
    }

    /**
     * Given the properties from a conference host applies and changes to the host's properties to
     * the child as well.
     * @param childProperties The existing child properties.
     * @param hostProperties The properties from the host.
     * @return The child properties with the applicable host bits set/unset.
     */
    private int applyHostPropertiesToChild(int childProperties, int hostProperties) {
        childProperties = changeBitmask(childProperties,
                Connection.PROPERTY_WIFI,
                (hostProperties & Connection.PROPERTY_WIFI) != 0);
        return childProperties;
    }

    /**
     * Builds a string representation of the {@link ImsConference}.
     *
     * @return String representing the conference.
     */
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("[ImsConference objId:");
        sb.append(System.identityHashCode(this));
        sb.append(" telecomCallID:");
        sb.append(getTelecomCallId());
        sb.append(" state:");
        sb.append(Connection.stateToString(getState()));
        sb.append(" hostConnection:");
        sb.append(mConferenceHost);
        sb.append(" participants:");
        sb.append(mConferenceParticipantConnections.size());
        sb.append("]");
        return sb.toString();
    }

    /**
     * @return The number of participants in the conference.
     */
    public int getNumberOfParticipants() {
        return mConferenceParticipantConnections.size();
    }

    /**
     * @return {@code True} if the carrier enforces a maximum conference size, and the number of
     *      participants in the conference has reached the limit, {@code false} otherwise.
     */
    public boolean isFullConference() {
        return mCarrierConfig.isMaximumConferenceSizeEnforced()
                && getNumberOfParticipants() >= mCarrierConfig.getMaximumConferenceSize();
    }

    /**
     * Handles destruction of a {@link ConferenceParticipantConnection}.
     * We remove the participant from the list of tracked participants in the conference and
     * update whether the conference can be managed.
     * @param participant the conference participant.
     */
    private void handleConnectionDestruction(ConferenceParticipantConnection participant) {
        removeConferenceParticipant(participant);
        updateManageConference();
    }
}
