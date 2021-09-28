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
 * limitations under the License.
 */

package com.android.server.telecom;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.os.ParcelFileDescriptor;
import android.os.Parcelable;
import android.os.RemoteException;
import android.os.SystemClock;
import android.os.Trace;
import android.os.UserHandle;
import android.provider.ContactsContract.Contacts;
import android.telecom.CallAudioState;
import android.telecom.CallerInfo;
import android.telecom.Conference;
import android.telecom.Connection;
import android.telecom.ConnectionService;
import android.telecom.DisconnectCause;
import android.telecom.GatewayInfo;
import android.telecom.Log;
import android.telecom.Logging.EventManager;
import android.telecom.ParcelableConference;
import android.telecom.ParcelableConnection;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.Response;
import android.telecom.StatusHints;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.PhoneNumberUtils;
import android.telephony.TelephonyManager;
import android.telephony.emergency.EmergencyNumber;
import android.text.TextUtils;
import android.widget.Toast;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telecom.IVideoProvider;
import com.android.internal.util.Preconditions;
import com.android.server.telecom.ui.ToastFactory;

import java.io.IOException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Date;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.ConcurrentHashMap;

/**
 *  Encapsulates all aspects of a given phone call throughout its lifecycle, starting
 *  from the time the call intent was received by Telecom (vs. the time the call was
 *  connected etc).
 */
@VisibleForTesting
public class Call implements CreateConnectionResponse, EventManager.Loggable,
        ConnectionServiceFocusManager.CallFocus {
    public final static String CALL_ID_UNKNOWN = "-1";
    public final static long DATA_USAGE_NOT_SET = -1;

    public static final int CALL_DIRECTION_UNDEFINED = 0;
    public static final int CALL_DIRECTION_OUTGOING = 1;
    public static final int CALL_DIRECTION_INCOMING = 2;
    public static final int CALL_DIRECTION_UNKNOWN = 3;

    /** Identifies extras changes which originated from a connection service. */
    public static final int SOURCE_CONNECTION_SERVICE = 1;
    /** Identifies extras changes which originated from an incall service. */
    public static final int SOURCE_INCALL_SERVICE = 2;

    private static final int RTT_PIPE_READ_SIDE_INDEX = 0;
    private static final int RTT_PIPE_WRITE_SIDE_INDEX = 1;

    private static final int INVALID_RTT_REQUEST_ID = -1;

    private static final char NO_DTMF_TONE = '\0';

    /**
     * Listener for events on the call.
     */
    @VisibleForTesting
    public interface Listener {
        void onSuccessfulOutgoingCall(Call call, int callState);
        void onFailedOutgoingCall(Call call, DisconnectCause disconnectCause);
        void onSuccessfulIncomingCall(Call call);
        void onFailedIncomingCall(Call call);
        void onSuccessfulUnknownCall(Call call, int callState);
        void onFailedUnknownCall(Call call);
        void onRingbackRequested(Call call, boolean ringbackRequested);
        void onPostDialWait(Call call, String remaining);
        void onPostDialChar(Call call, char nextChar);
        void onConnectionCapabilitiesChanged(Call call);
        void onConnectionPropertiesChanged(Call call, boolean didRttChange);
        void onParentChanged(Call call);
        void onChildrenChanged(Call call);
        void onCannedSmsResponsesLoaded(Call call);
        void onVideoCallProviderChanged(Call call);
        void onCallerInfoChanged(Call call);
        void onIsVoipAudioModeChanged(Call call);
        void onStatusHintsChanged(Call call);
        void onExtrasChanged(Call c, int source, Bundle extras);
        void onExtrasRemoved(Call c, int source, List<String> keys);
        void onHandleChanged(Call call);
        void onCallerDisplayNameChanged(Call call);
        void onCallDirectionChanged(Call call);
        void onVideoStateChanged(Call call, int previousVideoState, int newVideoState);
        void onTargetPhoneAccountChanged(Call call);
        void onConnectionManagerPhoneAccountChanged(Call call);
        void onPhoneAccountChanged(Call call);
        void onConferenceableCallsChanged(Call call);
        void onConferenceStateChanged(Call call, boolean isConference);
        void onCdmaConferenceSwap(Call call);
        boolean onCanceledViaNewOutgoingCallBroadcast(Call call, long disconnectionTimeout);
        void onHoldToneRequested(Call call);
        void onCallHoldFailed(Call call);
        void onCallSwitchFailed(Call call);
        void onConnectionEvent(Call call, String event, Bundle extras);
        void onExternalCallChanged(Call call, boolean isExternalCall);
        void onRttInitiationFailure(Call call, int reason);
        void onRemoteRttRequest(Call call, int requestId);
        void onHandoverRequested(Call call, PhoneAccountHandle handoverTo, int videoState,
                                 Bundle extras, boolean isLegacy);
        void onHandoverFailed(Call call, int error);
        void onHandoverComplete(Call call);
    }

    public abstract static class ListenerBase implements Listener {
        @Override
        public void onSuccessfulOutgoingCall(Call call, int callState) {}
        @Override
        public void onFailedOutgoingCall(Call call, DisconnectCause disconnectCause) {}
        @Override
        public void onSuccessfulIncomingCall(Call call) {}
        @Override
        public void onFailedIncomingCall(Call call) {}
        @Override
        public void onSuccessfulUnknownCall(Call call, int callState) {}
        @Override
        public void onFailedUnknownCall(Call call) {}
        @Override
        public void onRingbackRequested(Call call, boolean ringbackRequested) {}
        @Override
        public void onPostDialWait(Call call, String remaining) {}
        @Override
        public void onPostDialChar(Call call, char nextChar) {}
        @Override
        public void onConnectionCapabilitiesChanged(Call call) {}
        @Override
        public void onConnectionPropertiesChanged(Call call, boolean didRttChange) {}
        @Override
        public void onParentChanged(Call call) {}
        @Override
        public void onChildrenChanged(Call call) {}
        @Override
        public void onCannedSmsResponsesLoaded(Call call) {}
        @Override
        public void onVideoCallProviderChanged(Call call) {}
        @Override
        public void onCallerInfoChanged(Call call) {}
        @Override
        public void onIsVoipAudioModeChanged(Call call) {}
        @Override
        public void onStatusHintsChanged(Call call) {}
        @Override
        public void onExtrasChanged(Call c, int source, Bundle extras) {}
        @Override
        public void onExtrasRemoved(Call c, int source, List<String> keys) {}
        @Override
        public void onHandleChanged(Call call) {}
        @Override
        public void onCallerDisplayNameChanged(Call call) {}
        @Override
        public void onCallDirectionChanged(Call call) {}
        @Override
        public void onVideoStateChanged(Call call, int previousVideoState, int newVideoState) {}
        @Override
        public void onTargetPhoneAccountChanged(Call call) {}
        @Override
        public void onConnectionManagerPhoneAccountChanged(Call call) {}
        @Override
        public void onPhoneAccountChanged(Call call) {}
        @Override
        public void onConferenceableCallsChanged(Call call) {}
        @Override
        public void onConferenceStateChanged(Call call, boolean isConference) {}
        @Override
        public void onCdmaConferenceSwap(Call call) {}
        @Override
        public boolean onCanceledViaNewOutgoingCallBroadcast(Call call, long disconnectionTimeout) {
            return false;
        }
        @Override
        public void onHoldToneRequested(Call call) {}
        @Override
        public void onCallHoldFailed(Call call) {}
        @Override
        public void onCallSwitchFailed(Call call) {}
        @Override
        public void onConnectionEvent(Call call, String event, Bundle extras) {}
        @Override
        public void onExternalCallChanged(Call call, boolean isExternalCall) {}
        @Override
        public void onRttInitiationFailure(Call call, int reason) {}
        @Override
        public void onRemoteRttRequest(Call call, int requestId) {}
        @Override
        public void onHandoverRequested(Call call, PhoneAccountHandle handoverTo, int videoState,
                                        Bundle extras, boolean isLegacy) {}
        @Override
        public void onHandoverFailed(Call call, int error) {}
        @Override
        public void onHandoverComplete(Call call) {}
    }

    private final CallerInfoLookupHelper.OnQueryCompleteListener mCallerInfoQueryListener =
            new CallerInfoLookupHelper.OnQueryCompleteListener() {
                /** ${inheritDoc} */
                @Override
                public void onCallerInfoQueryComplete(Uri handle, CallerInfo callerInfo) {
                    synchronized (mLock) {
                        Call.this.setCallerInfo(handle, callerInfo);
                    }
                }

                @Override
                public void onContactPhotoQueryComplete(Uri handle, CallerInfo callerInfo) {
                    synchronized (mLock) {
                        Call.this.setCallerInfo(handle, callerInfo);
                    }
                }
            };

    /**
     * One of CALL_DIRECTION_INCOMING, CALL_DIRECTION_OUTGOING, or CALL_DIRECTION_UNKNOWN
     */
    private int mCallDirection;

    /**
     * The post-dial digits that were dialed after the network portion of the number
     */
    private String mPostDialDigits;

    /**
     * The secondary line number that an incoming call has been received on if the SIM subscription
     * has multiple associated numbers.
     */
    private String mViaNumber = "";

    /**
     * The wall clock time this call was created. Beyond logging and such, may also be used for
     * bookkeeping and specifically for marking certain call attempts as failed attempts.
     * Note: This timestamp should NOT be used for calculating call duration.
     */
    private long mCreationTimeMillis;

    /** The time this call was made active. */
    private long mConnectTimeMillis = 0;

    /**
     * The time, in millis, since boot when this call was connected.  This should ONLY be used when
     * calculating the duration of the call.
     *
     * The reason for this is that the {@link SystemClock#elapsedRealtime()} is based on the
     * elapsed time since the device was booted.  Changes to the system clock (e.g. due to NITZ
     * time sync, time zone changes user initiated clock changes) would cause a duration calculated
     * based on {@link #mConnectTimeMillis} to change based on the delta in the time.
     * Using the {@link SystemClock#elapsedRealtime()} ensures that changes to the wall clock do
     * not impact the call duration.
     */
    private long mConnectElapsedTimeMillis = 0;

    /** The wall clock time this call was disconnected. */
    private long mDisconnectTimeMillis = 0;

    /**
     * The elapsed time since boot when this call was disconnected.  Recorded as the
     * {@link SystemClock#elapsedRealtime()}.  This ensures that the call duration is not impacted
     * by changes in the wall time clock.
     */
    private long mDisconnectElapsedTimeMillis = 0;

    /** The gateway information associated with this call. This stores the original call handle
     * that the user is attempting to connect to via the gateway, the actual handle to dial in
     * order to connect the call via the gateway, as well as the package name of the gateway
     * service. */
    private GatewayInfo mGatewayInfo;

    private PhoneAccountHandle mConnectionManagerPhoneAccountHandle;

    private PhoneAccountHandle mTargetPhoneAccountHandle;

    private PhoneAccountHandle mRemotePhoneAccountHandle;

    private UserHandle mInitiatingUser;

    private final Handler mHandler = new Handler(Looper.getMainLooper());

    private final List<Call> mConferenceableCalls = new ArrayList<>();

    /** The state of the call. */
    private int mState;

    /** The handle with which to establish this call. */
    private Uri mHandle;

    /** The participants with which to establish adhoc conference call */
    private List<Uri> mParticipants;
    /**
     * The presentation requirements for the handle. See {@link TelecomManager} for valid values.
     */
    private int mHandlePresentation;

    /**
     * The verification status for an incoming call's number.
     */
    private @Connection.VerificationStatus int mCallerNumberVerificationStatus;

    /** The caller display name (CNAP) set by the connection service. */
    private String mCallerDisplayName;

    /**
     * The presentation requirements for the handle. See {@link TelecomManager} for valid values.
     */
    private int mCallerDisplayNamePresentation;

    /**
     * The connection service which is attempted or already connecting this call.
     */
    private ConnectionServiceWrapper mConnectionService;

    private boolean mIsEmergencyCall;

    // The Call is considered an emergency call for testing, but will not actually connect to
    // emergency services.
    private boolean mIsTestEmergencyCall;

    private boolean mSpeakerphoneOn;

    private boolean mIsDisconnectingChildCall = false;

    /**
     * Tracks the video states which were applicable over the duration of a call.
     * See {@link VideoProfile} for a list of valid video states.
     * <p>
     * Video state history is tracked when the call is active, and when a call is rejected or
     * missed.
     */
    private int mVideoStateHistory;

    private int mVideoState;

    /**
     * Disconnect cause for the call. Only valid if the state of the call is STATE_DISCONNECTED.
     * See {@link android.telecom.DisconnectCause}.
     */
    private DisconnectCause mDisconnectCause = new DisconnectCause(DisconnectCause.UNKNOWN);

    /**
     * Override the disconnect cause set by the connection service. Used for audio processing and
     * simulated ringing calls as well as the condition when an emergency call is ended due to
     * an emergency call being placed.
     */
    private DisconnectCause mOverrideDisconnectCause = new DisconnectCause(DisconnectCause.UNKNOWN);

    private Bundle mIntentExtras = new Bundle();

    /**
     * The {@link Intent} which originally created this call.  Only populated when we are putting a
     * call into a pending state and need to pick up initiation of the call later.
     */
    private Intent mOriginalCallIntent = null;

    /** Set of listeners on this call.
     *
     * ConcurrentHashMap constructor params: 8 is initial table size, 0.9f is
     * load factor before resizing, 1 means we only expect a single thread to
     * access the map so make only a single shard
     */
    private final Set<Listener> mListeners = Collections.newSetFromMap(
            new ConcurrentHashMap<Listener, Boolean>(8, 0.9f, 1));

    private CreateConnectionProcessor mCreateConnectionProcessor;

    /** Caller information retrieved from the latest contact query. */
    private CallerInfo mCallerInfo;

    /** The latest token used with a contact info query. */
    private int mQueryToken = 0;

    /** Whether this call is requesting that Telecom play the ringback tone on its behalf. */
    private boolean mRingbackRequested = false;

    /** Whether this call is requesting to be silently ringing. */
    private boolean mSilentRingingRequested = false;

    /** Whether direct-to-voicemail query is pending. */
    private boolean mDirectToVoicemailQueryPending;

    private int mConnectionCapabilities;

    private int mConnectionProperties;

    private int mSupportedAudioRoutes = CallAudioState.ROUTE_ALL;

    private boolean mIsConference = false;

    private boolean mHadChildren = false;

    private final boolean mShouldAttachToExistingConnection;

    private Call mParentCall = null;

    private List<Call> mChildCalls = new LinkedList<>();

    /** Set of text message responses allowed for this call, if applicable. */
    private List<String> mCannedSmsResponses = Collections.EMPTY_LIST;

    /** Whether an attempt has been made to load the text message responses. */
    private boolean mCannedSmsResponsesLoadingStarted = false;

    private IVideoProvider mVideoProvider;
    private VideoProviderProxy mVideoProviderProxy;

    private boolean mIsVoipAudioMode;
    private StatusHints mStatusHints;
    private Bundle mExtras;
    private final ConnectionServiceRepository mRepository;
    private final Context mContext;
    private final CallsManager mCallsManager;
    private final ClockProxy mClockProxy;
    private final ToastFactory mToastFactory;
    private final TelecomSystem.SyncRoot mLock;
    private final String mId;
    private String mConnectionId;
    private Analytics.CallInfo mAnalytics = new Analytics.CallInfo();
    private char mPlayingDtmfTone;

    private boolean mWasConferencePreviouslyMerged = false;
    private boolean mWasHighDefAudio = false;
    private boolean mWasWifi = false;
    private boolean mWasVolte = false;

    // For conferences which support merge/swap at their level, we retain a notion of an active
    // call. This is used for BluetoothPhoneService.  In order to support hold/merge, it must have
    // the notion of the current "active" call within the conference call. This maintains the
    // "active" call and switches every time the user hits "swap".
    private Call mConferenceLevelActiveCall = null;

    private boolean mIsLocallyDisconnecting = false;

    /**
     * Tracks the current call data usage as reported by the video provider.
     */
    private long mCallDataUsage = DATA_USAGE_NOT_SET;

    private boolean mIsWorkCall;

    /**
     * Tracks whether this {@link Call}'s {@link #getTargetPhoneAccount()} has
     * {@link PhoneAccount#EXTRA_PLAY_CALL_RECORDING_TONE} set.
     */
    private boolean mUseCallRecordingTone;

    // Set to true once the NewOutgoingCallIntentBroadcast comes back and is processed.
    private boolean mIsNewOutgoingCallIntentBroadcastDone = false;

    /**
     * Indicates whether the call is remotely held.  A call is considered remotely held when
     * {@link #onConnectionEvent(String)} receives the {@link Connection#EVENT_ON_HOLD_TONE_START}
     * event.
     */
    private boolean mIsRemotelyHeld = false;

    /**
     * Indicates whether the {@link PhoneAccount} associated with this call is self-managed.
     * See {@link PhoneAccount#CAPABILITY_SELF_MANAGED} for more information.
     */
    private boolean mIsSelfManaged = false;

    /**
     * Indicates whether the {@link PhoneAccount} associated with this call supports video calling.
     * {@code True} if the phone account supports video calling, {@code false} otherwise.
     */
    private boolean mIsVideoCallingSupportedByPhoneAccount = false;

    /**
     * Indicates whether or not this call can be pulled if it is an external call. If true, respect
     * the Connection Capability set by the ConnectionService. If false, override the capability
     * set and always remove the ability to pull this external call.
     *
     * See {@link #setIsPullExternalCallSupported(boolean)}
     */
    private boolean mIsPullExternalCallSupported = true;

    private PhoneNumberUtilsAdapter mPhoneNumberUtilsAdapter;

    /**
     * For {@link Connection}s or {@link android.telecom.Conference}s added via a ConnectionManager
     * using the {@link android.telecom.ConnectionService#addExistingConnection(PhoneAccountHandle,
     * Connection)} or {@link android.telecom.ConnectionService#addConference(Conference)},
     * indicates the ID of this call as it was referred to by the {@code ConnectionService} which
     * originally created it.
     *
     * See {@link Connection#EXTRA_ORIGINAL_CONNECTION_ID} for more information.
     */
    private String mOriginalConnectionId;

    /**
     * Two pairs of {@link android.os.ParcelFileDescriptor}s that handle RTT text communication
     * between the in-call app and the connection service. If both non-null, this call should be
     * treated as an RTT call.
     * Each array should be of size 2. First one is the read side and the second one is the write
     * side.
     */
    private ParcelFileDescriptor[] mInCallToConnectionServiceStreams;
    private ParcelFileDescriptor[] mConnectionServiceToInCallStreams;

    /**
     * True if we're supposed to start this call with RTT, either due to the master switch or due
     * to an extra.
     */
    private boolean mDidRequestToStartWithRtt = false;
    /**
     * Integer constant from {@link android.telecom.Call.RttCall}. Describes the current RTT mode.
     */
    private int mRttMode;
    /**
     * True if the call was ever an RTT call.
     */
    private boolean mWasEverRtt = false;

    /**
     * Integer indicating the remote RTT request ID that is pending a response from the user.
     */
    private int mPendingRttRequestId = INVALID_RTT_REQUEST_ID;

    /**
     * When a call handover has been initiated via {@link #requestHandover(PhoneAccountHandle,
     * int, Bundle, boolean)}, contains the call which this call is being handed over to.
     */
    private Call mHandoverDestinationCall = null;

    /**
     * When a call handover has been initiated via {@link #requestHandover(PhoneAccountHandle,
     * int, Bundle, boolean)}, contains the call which this call is being handed over from.
     */
    private Call mHandoverSourceCall = null;

    /**
     * The user-visible app name of the app that requested for this call to be put into the
     * AUDIO_PROCESSING state. Used to display a notification to the user.
     */
    private CharSequence mAudioProcessingRequestingApp = null;

    /**
     * Indicates the current state of this call if it is in the process of a handover.
     */
    private int mHandoverState = HandoverState.HANDOVER_NONE;

    /**
     * Indicates whether this call is using one of the
     * {@link com.android.server.telecom.callfiltering.IncomingCallFilter.CallFilter} modules.
     */
    private boolean mIsUsingCallFiltering = false;

    /**
     * Indicates whether or not this call has been active before. This is helpful in detecting
     * situations where we have moved into {@link CallState#SIMULATED_RINGING} or
     * {@link CallState#AUDIO_PROCESSING} again after being active. If a call has moved into one
     * of these states again after being active and the user dials an emergency call, we want to
     * log these calls normally instead of considering them MISSED. If the emergency call was
     * dialed during initial screening however, we want to treat those calls as MISSED (because the
     * user never got the chance to explicitly reject).
     */
    private boolean mHasGoneActiveBefore = false;

    /**
     * Indicates the package name of the {@link android.telecom.CallScreeningService} which should
     * be sent the {@link android.telecom.TelecomManager#ACTION_POST_CALL} intent upon disconnection
     * of a call.
     */
    private String mPostCallPackageName;

    /**
     * Persists the specified parameters and initializes the new instance.
     * @param context The context.
     * @param repository The connection service repository.
     * @param handle The handle to dial.
     * @param gatewayInfo Gateway information to use for the call.
     * @param connectionManagerPhoneAccountHandle Account to use for the service managing the call.
     *         This account must be one that was registered with the
     *           {@link PhoneAccount#CAPABILITY_CONNECTION_MANAGER} flag.
     * @param targetPhoneAccountHandle Account information to use for the call. This account must be
     *         one that was registered with the {@link PhoneAccount#CAPABILITY_CALL_PROVIDER} flag.
     * @param callDirection one of CALL_DIRECTION_INCOMING, CALL_DIRECTION_OUTGOING,
     *         or CALL_DIRECTION_UNKNOWN.
     * @param shouldAttachToExistingConnection Set to true to attach the call to an existing
     * @param clockProxy
     */
    public Call(
            String callId,
            Context context,
            CallsManager callsManager,
            TelecomSystem.SyncRoot lock,
            ConnectionServiceRepository repository,
            PhoneNumberUtilsAdapter phoneNumberUtilsAdapter,
            Uri handle,
            GatewayInfo gatewayInfo,
            PhoneAccountHandle connectionManagerPhoneAccountHandle,
            PhoneAccountHandle targetPhoneAccountHandle,
            int callDirection,
            boolean shouldAttachToExistingConnection,
            boolean isConference,
            ClockProxy clockProxy,
            ToastFactory toastFactory) {
        this(callId, context, callsManager, lock, repository, phoneNumberUtilsAdapter,
               handle, null, gatewayInfo, connectionManagerPhoneAccountHandle,
               targetPhoneAccountHandle, callDirection, shouldAttachToExistingConnection,
               isConference, clockProxy, toastFactory);

    }

    public Call(
            String callId,
            Context context,
            CallsManager callsManager,
            TelecomSystem.SyncRoot lock,
            ConnectionServiceRepository repository,
            PhoneNumberUtilsAdapter phoneNumberUtilsAdapter,
            Uri handle,
            List<Uri> participants,
            GatewayInfo gatewayInfo,
            PhoneAccountHandle connectionManagerPhoneAccountHandle,
            PhoneAccountHandle targetPhoneAccountHandle,
            int callDirection,
            boolean shouldAttachToExistingConnection,
            boolean isConference,
            ClockProxy clockProxy,
            ToastFactory toastFactory) {

        mId = callId;
        mConnectionId = callId;
        mState = (isConference && callDirection != CALL_DIRECTION_INCOMING &&
                callDirection != CALL_DIRECTION_OUTGOING) ?
                CallState.ACTIVE : CallState.NEW;
        mContext = context;
        mCallsManager = callsManager;
        mLock = lock;
        mRepository = repository;
        mPhoneNumberUtilsAdapter = phoneNumberUtilsAdapter;
        setHandle(handle);
        mParticipants = participants;
        mPostDialDigits = handle != null
                ? PhoneNumberUtils.extractPostDialPortion(handle.getSchemeSpecificPart()) : "";
        mGatewayInfo = gatewayInfo;
        setConnectionManagerPhoneAccount(connectionManagerPhoneAccountHandle);
        setTargetPhoneAccount(targetPhoneAccountHandle);
        mCallDirection = callDirection;
        mIsConference = isConference;
        mShouldAttachToExistingConnection = shouldAttachToExistingConnection
                || callDirection == CALL_DIRECTION_INCOMING;
        maybeLoadCannedSmsResponses();
        mClockProxy = clockProxy;
        mToastFactory = toastFactory;
        mCreationTimeMillis = mClockProxy.currentTimeMillis();
    }

    /**
     * Persists the specified parameters and initializes the new instance.
     * @param context The context.
     * @param repository The connection service repository.
     * @param handle The handle to dial.
     * @param gatewayInfo Gateway information to use for the call.
     * @param connectionManagerPhoneAccountHandle Account to use for the service managing the call.
     * This account must be one that was registered with the
     * {@link PhoneAccount#CAPABILITY_CONNECTION_MANAGER} flag.
     * @param targetPhoneAccountHandle Account information to use for the call. This account must be
     * one that was registered with the {@link PhoneAccount#CAPABILITY_CALL_PROVIDER} flag.
     * @param callDirection one of CALL_DIRECTION_INCOMING, CALL_DIRECTION_OUTGOING,
     * or CALL_DIRECTION_UNKNOWN
     * @param shouldAttachToExistingConnection Set to true to attach the call to an existing
     * connection, regardless of whether it's incoming or outgoing.
     * @param connectTimeMillis The connection time of the call.
     * @param clockProxy
     */
    Call(
            String callId,
            Context context,
            CallsManager callsManager,
            TelecomSystem.SyncRoot lock,
            ConnectionServiceRepository repository,
            PhoneNumberUtilsAdapter phoneNumberUtilsAdapter,
            Uri handle,
            GatewayInfo gatewayInfo,
            PhoneAccountHandle connectionManagerPhoneAccountHandle,
            PhoneAccountHandle targetPhoneAccountHandle,
            int callDirection,
            boolean shouldAttachToExistingConnection,
            boolean isConference,
            long connectTimeMillis,
            long connectElapsedTimeMillis,
            ClockProxy clockProxy,
            ToastFactory toastFactory) {
        this(callId, context, callsManager, lock, repository,
                phoneNumberUtilsAdapter, handle, gatewayInfo,
                connectionManagerPhoneAccountHandle, targetPhoneAccountHandle, callDirection,
                shouldAttachToExistingConnection, isConference, clockProxy, toastFactory);

        mConnectTimeMillis = connectTimeMillis;
        mConnectElapsedTimeMillis = connectElapsedTimeMillis;
        mAnalytics.setCallStartTime(connectTimeMillis);
    }

    public void addListener(Listener listener) {
        mListeners.add(listener);
    }

    public void removeListener(Listener listener) {
        if (listener != null) {
            mListeners.remove(listener);
        }
    }

    public void initAnalytics() {
        initAnalytics(null);
    }

    public void initAnalytics(String callingPackage) {
        int analyticsDirection;
        switch (mCallDirection) {
            case CALL_DIRECTION_OUTGOING:
                analyticsDirection = Analytics.OUTGOING_DIRECTION;
                break;
            case CALL_DIRECTION_INCOMING:
                analyticsDirection = Analytics.INCOMING_DIRECTION;
                break;
            case CALL_DIRECTION_UNKNOWN:
            case CALL_DIRECTION_UNDEFINED:
            default:
                analyticsDirection = Analytics.UNKNOWN_DIRECTION;
        }
        mAnalytics = Analytics.initiateCallAnalytics(mId, analyticsDirection);
        mAnalytics.setCallIsEmergency(mIsEmergencyCall);
        Log.addEvent(this, LogUtils.Events.CREATED, callingPackage);
    }

    public Analytics.CallInfo getAnalytics() {
        return mAnalytics;
    }

    public void destroy() {
        // We should not keep these bitmaps around because the Call objects may be held for logging
        // purposes.
        // TODO: Make a container object that only stores the information we care about for Logging.
        if (mCallerInfo != null) {
            mCallerInfo.cachedPhotoIcon = null;
            mCallerInfo.cachedPhoto = null;
        }
        closeRttStreams();

        Log.addEvent(this, LogUtils.Events.DESTROYED);
    }

    private void closeRttStreams() {
        if (mConnectionServiceToInCallStreams != null) {
            for (ParcelFileDescriptor fd : mConnectionServiceToInCallStreams) {
                if (fd != null) {
                    try {
                        fd.close();
                    } catch (IOException e) {
                        // ignore
                    }
                }
            }
        }
        if (mInCallToConnectionServiceStreams != null) {
            for (ParcelFileDescriptor fd : mInCallToConnectionServiceStreams) {
                if (fd != null) {
                    try {
                        fd.close();
                    } catch (IOException e) {
                        // ignore
                    }
                }
            }
        }
    }

    /** {@inheritDoc} */
    @Override
    public String toString() {
        return String.format(Locale.US, "[Call id=%s, state=%s, tpac=%s, cmgr=%s, handle=%s, "
                        + "vidst=%s, childs(%d), has_parent(%b), cap=%s, prop=%s]",
                mId,
                CallState.toString(getParcelableCallState()),
                getTargetPhoneAccount(),
                getConnectionManagerPhoneAccount(),
                Log.piiHandle(mHandle),
                getVideoStateDescription(getVideoState()),
                getChildCalls().size(),
                getParentCall() != null,
                Connection.capabilitiesToStringShort(getConnectionCapabilities()),
                Connection.propertiesToStringShort(getConnectionProperties()));
    }

    @Override
    public String getDescription() {
        StringBuilder s = new StringBuilder();
        if (isSelfManaged()) {
            s.append("SelfMgd Call");
        } else if (isExternalCall()) {
            s.append("External Call");
        } else {
            s.append("Call");
        }
        s.append(getId());
        s.append(" [");
        s.append(SimpleDateFormat.getDateTimeInstance().format(new Date(getCreationTimeMillis())));
        s.append("]");
        s.append(isIncoming() ? "(MT - incoming)" : "(MO - outgoing)");
        s.append("\n\t");

        PhoneAccountHandle targetPhoneAccountHandle = getTargetPhoneAccount();
        PhoneAccountHandle remotePhoneAccountHandle = getRemotePhoneAccountHandle();
        PhoneAccountHandle connectionMgrAccountHandle = getConnectionManagerPhoneAccount();
        PhoneAccountHandle delegatePhoneAccountHandle = getDelegatePhoneAccountHandle();
        boolean isTargetSameAsRemote = targetPhoneAccountHandle != null
                && targetPhoneAccountHandle.equals(remotePhoneAccountHandle);
        if (Objects.equals(delegatePhoneAccountHandle, targetPhoneAccountHandle)) {
            s.append(">>>");
        }
        s.append("Target");
        s.append(" PhoneAccount: ");
        if (targetPhoneAccountHandle != null) {
            s.append(targetPhoneAccountHandle);
            s.append(" (");
            s.append(getTargetPhoneAccountLabel());
            s.append(")");
            if (isTargetSameAsRemote) {
                s.append("(remote)");
            }
        } else {
            s.append("not set");
        }
        if (!isTargetSameAsRemote && remotePhoneAccountHandle != null) {
            // This is a RARE case and will likely not be seen in practice but it is possible.
            if (delegatePhoneAccountHandle.equals(remotePhoneAccountHandle)) {
                s.append("\n\t>>>Remote PhoneAccount: ");
            } else {
                s.append("\n\tRemote PhoneAccount: ");
            }
            s.append(remotePhoneAccountHandle);
        }
        if (connectionMgrAccountHandle != null) {
            if (delegatePhoneAccountHandle.equals(connectionMgrAccountHandle)) {
                s.append("\n\t>>>Conn mgr: ");
            } else {
                s.append("\n\tConn mgr: ");
            }
            s.append(connectionMgrAccountHandle);
        }

        s.append("\n\tTo address: ");
        s.append(Log.piiHandle(getHandle()));
        if (isIncoming()) {
            switch (mCallerNumberVerificationStatus) {
                case Connection.VERIFICATION_STATUS_FAILED:
                    s.append(" Verstat: fail");
                    break;
                case Connection.VERIFICATION_STATUS_NOT_VERIFIED:
                    s.append(" Verstat: not");
                    break;
                case Connection.VERIFICATION_STATUS_PASSED:
                    s.append(" Verstat: pass");
                    break;
            }
        }
        s.append(" Presentation: ");
        switch (getHandlePresentation()) {
            case TelecomManager.PRESENTATION_ALLOWED:
                s.append("Allowed");
                break;
            case TelecomManager.PRESENTATION_PAYPHONE:
                s.append("Payphone");
                break;
            case TelecomManager.PRESENTATION_RESTRICTED:
                s.append("Restricted");
                break;
            case TelecomManager.PRESENTATION_UNKNOWN:
                s.append("Unknown");
                break;
            default:
                s.append("<undefined>");
        }
        s.append("\n");
        return s.toString();
    }

    /**
     * Builds a debug-friendly description string for a video state.
     * <p>
     * A = audio active, T = video transmission active, R = video reception active, P = video
     * paused.
     *
     * @param videoState The video state.
     * @return A string indicating which bits are set in the video state.
     */
    private String getVideoStateDescription(int videoState) {
        StringBuilder sb = new StringBuilder();
        sb.append("A");

        if (VideoProfile.isTransmissionEnabled(videoState)) {
            sb.append("T");
        }

        if (VideoProfile.isReceptionEnabled(videoState)) {
            sb.append("R");
        }

        if (VideoProfile.isPaused(videoState)) {
            sb.append("P");
        }

        return sb.toString();
    }

    @Override
    public ConnectionServiceFocusManager.ConnectionServiceFocus getConnectionServiceWrapper() {
        return mConnectionService;
    }

    @VisibleForTesting
    public int getState() {
        return mState;
    }

    /**
     * Similar to {@link #getState()}, except will return {@link CallState#DISCONNECTING} if the
     * call is locally disconnecting.  This is the call state which is reported to the
     * {@link android.telecom.InCallService}s when a call is parcelled.
     * @return The parcelable call state.
     */
    public int getParcelableCallState() {
        if (isLocallyDisconnecting() &&
                (mState != android.telecom.Call.STATE_DISCONNECTED)) {
            return CallState.DISCONNECTING;
        }
        return mState;
    }

    /**
     * Determines if this {@link Call} can receive call focus via the
     * {@link ConnectionServiceFocusManager}.
     * Only top-level calls and non-external calls are eligible.
     * @return {@code true} if this call is focusable, {@code false} otherwise.
     */
    @Override
    public boolean isFocusable() {
        boolean isChild = getParentCall() != null;
        return !isChild && !isExternalCall();
    }

    private boolean shouldContinueProcessingAfterDisconnect() {
        // Stop processing once the call is active.
        if (!CreateConnectionTimeout.isCallBeingPlaced(this)) {
            return false;
        }

        // Only Redial a Call in the case of it being an Emergency Call.
        if(!isEmergencyCall()) {
            return false;
        }

        // Make sure that there are additional connection services to process.
        if (mCreateConnectionProcessor == null
            || !mCreateConnectionProcessor.isProcessingComplete()
            || !mCreateConnectionProcessor.hasMorePhoneAccounts()) {
            return false;
        }

        if (mDisconnectCause == null) {
            return false;
        }

        // Continue processing if the current attempt failed or timed out.
        return mDisconnectCause.getCode() == DisconnectCause.ERROR ||
            mCreateConnectionProcessor.isCallTimedOut();
    }

    /**
     * Returns the unique ID for this call as it exists in Telecom.
     * @return The call ID.
     */
    public String getId() {
        return mId;
    }

    /**
     * Returns the unique ID for this call (see {@link #getId}) along with an attempt indicator that
     * iterates based on attempts to establish a {@link Connection} using createConnectionProcessor.
     * @return The call ID with an appended attempt id.
     */
    public String getConnectionId() {
        if(mCreateConnectionProcessor != null) {
            mConnectionId = mId + "_" +
                    String.valueOf(mCreateConnectionProcessor.getConnectionAttempt());
            return mConnectionId;
        } else {
            return mConnectionId;
        }
    }

    /**
     * Sets the call state. Although there exists the notion of appropriate state transitions
     * (see {@link CallState}), in practice those expectations break down when cellular systems
     * misbehave and they do this very often. The result is that we do not enforce state transitions
     * and instead keep the code resilient to unexpected state changes.
     * @return true indicates if setState succeeded in setting the state to newState,
     * else it is failed, and the call is still in its original state.
     */
    public boolean setState(int newState, String tag) {
        if (mState != newState) {
            Log.v(this, "setState %s -> %s", CallState.toString(mState),
                    CallState.toString(newState));

            if (newState == CallState.DISCONNECTED && shouldContinueProcessingAfterDisconnect()) {
                Log.w(this, "continuing processing disconnected call with another service");
                mCreateConnectionProcessor.continueProcessingIfPossible(this, mDisconnectCause);
                return false;
            } else if (newState == CallState.ANSWERED && mState == CallState.ACTIVE) {
                Log.w(this, "setState %s -> %s; call already active.", CallState.toString(mState),
                        CallState.toString(newState));
                return false;
            }

            updateVideoHistoryViaState(mState, newState);

            mState = newState;
            maybeLoadCannedSmsResponses();

            if (mState == CallState.ACTIVE || mState == CallState.ON_HOLD) {
                if (mConnectTimeMillis == 0) {
                    // We check to see if mConnectTime is already set to prevent the
                    // call from resetting active time when it goes in and out of
                    // ACTIVE/ON_HOLD
                    mConnectTimeMillis = mClockProxy.currentTimeMillis();
                    mConnectElapsedTimeMillis = mClockProxy.elapsedRealtime();
                    mAnalytics.setCallStartTime(mConnectTimeMillis);
                }

                // We're clearly not disconnected, so reset the disconnected time.
                mDisconnectTimeMillis = 0;
                mDisconnectElapsedTimeMillis = 0;
                mHasGoneActiveBefore = true;
            } else if (mState == CallState.DISCONNECTED) {
                mDisconnectTimeMillis = mClockProxy.currentTimeMillis();
                mDisconnectElapsedTimeMillis = mClockProxy.elapsedRealtime();
                mAnalytics.setCallEndTime(mDisconnectTimeMillis);
                setLocallyDisconnecting(false);
                fixParentAfterDisconnect();
            }

            // Log the state transition event
            String event = null;
            Object data = null;
            switch (newState) {
                case CallState.ACTIVE:
                    event = LogUtils.Events.SET_ACTIVE;
                    break;
                case CallState.CONNECTING:
                    event = LogUtils.Events.SET_CONNECTING;
                    break;
                case CallState.DIALING:
                    event = LogUtils.Events.SET_DIALING;
                    break;
                case CallState.PULLING:
                    event = LogUtils.Events.SET_PULLING;
                    break;
                case CallState.DISCONNECTED:
                    event = LogUtils.Events.SET_DISCONNECTED;
                    data = getDisconnectCause();
                    break;
                case CallState.DISCONNECTING:
                    event = LogUtils.Events.SET_DISCONNECTING;
                    break;
                case CallState.ON_HOLD:
                    event = LogUtils.Events.SET_HOLD;
                    break;
                case CallState.SELECT_PHONE_ACCOUNT:
                    event = LogUtils.Events.SET_SELECT_PHONE_ACCOUNT;
                    break;
                case CallState.RINGING:
                    event = LogUtils.Events.SET_RINGING;
                    break;
                case CallState.ANSWERED:
                    event = LogUtils.Events.SET_ANSWERED;
                    break;
            }
            if (event != null) {
                // The string data should be just the tag.
                String stringData = tag;
                if (data != null) {
                    // If data exists, add it to tag.  If no tag, just use data.toString().
                    stringData = stringData == null ? data.toString() : stringData + "> " + data;
                }
                Log.addEvent(this, event, stringData);
            }
            int statsdDisconnectCause = (newState == CallState.DISCONNECTED) ?
                    getDisconnectCause().getCode() : DisconnectCause.UNKNOWN;
            TelecomStatsLog.write(TelecomStatsLog.CALL_STATE_CHANGED, newState,
                    statsdDisconnectCause, isSelfManaged(), isExternalCall());
        }
        return true;
    }

    void setRingbackRequested(boolean ringbackRequested) {
        mRingbackRequested = ringbackRequested;
        for (Listener l : mListeners) {
            l.onRingbackRequested(this, mRingbackRequested);
        }
    }

    boolean isRingbackRequested() {
        return mRingbackRequested;
    }

    public void setSilentRingingRequested(boolean silentRingingRequested) {
        mSilentRingingRequested = silentRingingRequested;
        Bundle bundle = new Bundle();
        bundle.putBoolean(android.telecom.Call.EXTRA_SILENT_RINGING_REQUESTED,
                silentRingingRequested);
        putExtras(SOURCE_CONNECTION_SERVICE, bundle);
    }

    public boolean isSilentRingingRequested() {
        return mSilentRingingRequested;
    }

    @VisibleForTesting
    public boolean isConference() {
        return mIsConference;
    }

    /**
     * @return {@code true} if this call had children at some point, {@code false} otherwise.
     */
    public boolean hadChildren() {
        return mHadChildren;
    }

    public Uri getHandle() {
        return mHandle;
    }

    public List<Uri> getParticipants() {
        return mParticipants;
    }

    public boolean isAdhocConferenceCall() {
        return mIsConference &&
                (mCallDirection == CALL_DIRECTION_OUTGOING ||
                mCallDirection == CALL_DIRECTION_INCOMING);
    }

    public String getPostDialDigits() {
        return mPostDialDigits;
    }

    public void clearPostDialDigits() {
        mPostDialDigits = null;
    }

    public String getViaNumber() {
        return mViaNumber;
    }

    public void setViaNumber(String viaNumber) {
        // If at any point the via number is not empty throughout the call, save that via number.
        if (!TextUtils.isEmpty(viaNumber)) {
            mViaNumber = viaNumber;
        }
    }

    public int getHandlePresentation() {
        return mHandlePresentation;
    }

    public void setCallerNumberVerificationStatus(
            @Connection.VerificationStatus int callerNumberVerificationStatus) {
        mCallerNumberVerificationStatus = callerNumberVerificationStatus;
    }

    public @Connection.VerificationStatus int getCallerNumberVerificationStatus() {
        return mCallerNumberVerificationStatus;
    }

    void setHandle(Uri handle) {
        setHandle(handle, TelecomManager.PRESENTATION_ALLOWED);
    }

    public void setHandle(Uri handle, int presentation) {
        if (!Objects.equals(handle, mHandle) || presentation != mHandlePresentation) {
            mHandlePresentation = presentation;
            if (mHandlePresentation == TelecomManager.PRESENTATION_RESTRICTED ||
                    mHandlePresentation == TelecomManager.PRESENTATION_UNKNOWN) {
                mHandle = null;
            } else {
                mHandle = handle;
                if (mHandle != null && !PhoneAccount.SCHEME_VOICEMAIL.equals(mHandle.getScheme())
                        && TextUtils.isEmpty(mHandle.getSchemeSpecificPart())) {
                    // If the number is actually empty, set it to null, unless this is a
                    // SCHEME_VOICEMAIL uri which always has an empty number.
                    mHandle = null;
                }
            }

            // Let's not allow resetting of the emergency flag. Once a call becomes an emergency
            // call, it will remain so for the rest of it's lifetime.
            if (!mIsEmergencyCall) {
                try {
                    mIsEmergencyCall = mHandle != null &&
                            getTelephonyManager().isEmergencyNumber(
                                    mHandle.getSchemeSpecificPart());
                } catch (IllegalStateException ise) {
                    Log.e(this, ise, "setHandle: can't determine if number is emergency");
                    mIsEmergencyCall = false;
                }
                mAnalytics.setCallIsEmergency(mIsEmergencyCall);
            }
            if (!mIsTestEmergencyCall) {
                mIsTestEmergencyCall = mHandle != null &&
                        isTestEmergencyCall(mHandle.getSchemeSpecificPart());
            }
            startCallerInfoLookup();
            for (Listener l : mListeners) {
                l.onHandleChanged(this);
            }
        }
    }

    private boolean isTestEmergencyCall(String number) {
        try {
            Map<Integer, List<EmergencyNumber>> eMap =
                    getTelephonyManager().getEmergencyNumberList();
            return eMap.values().stream().flatMap(Collection::stream)
                    .anyMatch(eNumber ->
                            eNumber.isFromSources(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_TEST) &&
                                    number.equals(eNumber.getNumber()));
        } catch (IllegalStateException ise) {
            return false;
        }
    }

    public String getCallerDisplayName() {
        return mCallerDisplayName;
    }

    public int getCallerDisplayNamePresentation() {
        return mCallerDisplayNamePresentation;
    }

    void setCallerDisplayName(String callerDisplayName, int presentation) {
        if (!TextUtils.equals(callerDisplayName, mCallerDisplayName) ||
                presentation != mCallerDisplayNamePresentation) {
            mCallerDisplayName = callerDisplayName;
            mCallerDisplayNamePresentation = presentation;
            for (Listener l : mListeners) {
                l.onCallerDisplayNameChanged(this);
            }
        }
    }

    public String getName() {
        return mCallerInfo == null ? null : mCallerInfo.getName();
    }

    public String getPhoneNumber() {
        return mCallerInfo == null ? null : mCallerInfo.getPhoneNumber();
    }

    public Bitmap getPhotoIcon() {
        return mCallerInfo == null ? null : mCallerInfo.cachedPhotoIcon;
    }

    public Drawable getPhoto() {
        return mCallerInfo == null ? null : mCallerInfo.cachedPhoto;
    }

    /**
     * @param cause The reason for the disconnection, represented by
     * {@link android.telecom.DisconnectCause}.
     */
    public void setDisconnectCause(DisconnectCause cause) {
        // TODO: Consider combining this method with a setDisconnected() method that is totally
        // separate from setState.

        if (mOverrideDisconnectCause.getCode() != DisconnectCause.UNKNOWN) {
            cause = new DisconnectCause(mOverrideDisconnectCause.getCode(),
                    TextUtils.isEmpty(mOverrideDisconnectCause.getLabel()) ?
                            cause.getLabel() : mOverrideDisconnectCause.getLabel(),
                    (mOverrideDisconnectCause.getDescription() == null) ?
                            cause.getDescription() :mOverrideDisconnectCause.getDescription(),
                    TextUtils.isEmpty(mOverrideDisconnectCause.getReason()) ?
                            cause.getReason() : mOverrideDisconnectCause.getReason(),
                    (mOverrideDisconnectCause.getTone() == 0) ?
                            cause.getTone() : mOverrideDisconnectCause.getTone());
        }
        mAnalytics.setCallDisconnectCause(cause);
        mDisconnectCause = cause;
    }

    public void setOverrideDisconnectCauseCode(DisconnectCause overrideDisconnectCause) {
        mOverrideDisconnectCause = overrideDisconnectCause;
    }


    public DisconnectCause getDisconnectCause() {
        return mDisconnectCause;
    }

    /**
     * @return {@code true} if this is an outgoing call to emergency services. An outgoing call is
     * identified as an emergency call by the dialer phone number.
     */
    @VisibleForTesting
    public boolean isEmergencyCall() {
        return mIsEmergencyCall;
    }

    /**
     * @return {@code true} if this an outgoing call to a test emergency number (and NOT to
     * emergency services). Used for testing purposes to differentiate between a real and fake
     * emergency call for safety reasons during testing.
     */
    public boolean isTestEmergencyCall() {
        return mIsTestEmergencyCall;
    }

    /**
     * @return {@code true} if the network has identified this call as an emergency call.
     */
    public boolean isNetworkIdentifiedEmergencyCall() {
        return hasProperty(Connection.PROPERTY_NETWORK_IDENTIFIED_EMERGENCY_CALL);
    }

    /**
     * @return The original handle this call is associated with. In-call services should use this
     * handle when indicating in their UI the handle that is being called.
     */
    public Uri getOriginalHandle() {
        if (mGatewayInfo != null && !mGatewayInfo.isEmpty()) {
            return mGatewayInfo.getOriginalAddress();
        }
        return getHandle();
    }

    @VisibleForTesting
    public GatewayInfo getGatewayInfo() {
        return mGatewayInfo;
    }

    void setGatewayInfo(GatewayInfo gatewayInfo) {
        mGatewayInfo = gatewayInfo;
    }

    @VisibleForTesting
    public PhoneAccountHandle getConnectionManagerPhoneAccount() {
        return mConnectionManagerPhoneAccountHandle;
    }

    @VisibleForTesting
    public void setConnectionManagerPhoneAccount(PhoneAccountHandle accountHandle) {
        if (!Objects.equals(mConnectionManagerPhoneAccountHandle, accountHandle)) {
            mConnectionManagerPhoneAccountHandle = accountHandle;
            for (Listener l : mListeners) {
                l.onConnectionManagerPhoneAccountChanged(this);
            }
        }
        checkIfRttCapable();
    }

    /**
     * @return the {@link PhoneAccountHandle} of the remote connection service which placing this
     * call was delegated to, or {@code null} if a remote connection service was not used.
     */
    public @Nullable PhoneAccountHandle getRemotePhoneAccountHandle() {
        return mRemotePhoneAccountHandle;
    }

    /**
     * Sets the {@link PhoneAccountHandle} of the remote connection service which placing this
     * call was delegated to.
     * @param accountHandle The phone account handle.
     */
    public void setRemotePhoneAccountHandle(PhoneAccountHandle accountHandle) {
        mRemotePhoneAccountHandle = accountHandle;
    }

    /**
     * Determines which {@link PhoneAccountHandle} is actually placing a call.
     * Where {@link #getRemotePhoneAccountHandle()} is non-null, the connection manager is placing
     * the call via a remote connection service, so the remote connection service's phone account
     * is the source.
     * Where {@link #getConnectionManagerPhoneAccount()} is non-null and
     * {@link #getRemotePhoneAccountHandle()} is null, the connection manager is placing the call
     * itself (even if the target specifies something else).
     * Finally, if neither of the above cases apply, the target phone account is the one actually
     * placing the call.
     * @return The {@link PhoneAccountHandle} which is actually placing a call.
     */
    public @NonNull PhoneAccountHandle getDelegatePhoneAccountHandle() {
        if (mRemotePhoneAccountHandle != null) {
            return mRemotePhoneAccountHandle;
        }
        if (mConnectionManagerPhoneAccountHandle != null) {
            return mConnectionManagerPhoneAccountHandle;
        }
        return mTargetPhoneAccountHandle;
    }

    @VisibleForTesting
    public PhoneAccountHandle getTargetPhoneAccount() {
        return mTargetPhoneAccountHandle;
    }

    @VisibleForTesting
    public void setTargetPhoneAccount(PhoneAccountHandle accountHandle) {
        if (!Objects.equals(mTargetPhoneAccountHandle, accountHandle)) {
            mTargetPhoneAccountHandle = accountHandle;
            for (Listener l : mListeners) {
                l.onTargetPhoneAccountChanged(this);
            }
            configureCallAttributes();
        }
        checkIfVideoCapable();
        checkIfRttCapable();
    }

    public CharSequence getTargetPhoneAccountLabel() {
        if (getTargetPhoneAccount() == null) {
            return null;
        }
        PhoneAccount phoneAccount = mCallsManager.getPhoneAccountRegistrar()
                .getPhoneAccountUnchecked(getTargetPhoneAccount());

        if (phoneAccount == null) {
            return null;
        }

        return phoneAccount.getLabel();
    }

    /**
     * Determines if this Call should be written to the call log.
     * @return {@code true} for managed calls or for self-managed calls which have the
     * {@link PhoneAccount#EXTRA_LOG_SELF_MANAGED_CALLS} extra set.
     */
    public boolean isLoggedSelfManaged() {
        if (!isSelfManaged()) {
            // Managed calls are always logged.
            return true;
        }
        if (getTargetPhoneAccount() == null) {
            return false;
        }
        PhoneAccount phoneAccount = mCallsManager.getPhoneAccountRegistrar()
                .getPhoneAccountUnchecked(getTargetPhoneAccount());

        if (phoneAccount == null) {
            return false;
        }

        if (getHandle() == null) {
            // No point in logging a null-handle call. Some self-managed calls will have this.
            return false;
        }

        if (!PhoneAccount.SCHEME_SIP.equals(getHandle().getScheme()) &&
                !PhoneAccount.SCHEME_TEL.equals(getHandle().getScheme())) {
            // Can't log schemes other than SIP or TEL for now.
            return false;
        }

        return phoneAccount.getExtras() != null && phoneAccount.getExtras().getBoolean(
                PhoneAccount.EXTRA_LOG_SELF_MANAGED_CALLS, false);
    }

    @VisibleForTesting
    public boolean isIncoming() {
        return mCallDirection == CALL_DIRECTION_INCOMING;
    }

    public boolean isExternalCall() {
        return (getConnectionProperties() & Connection.PROPERTY_IS_EXTERNAL_CALL) ==
                Connection.PROPERTY_IS_EXTERNAL_CALL;
    }

    public boolean isWorkCall() {
        return mIsWorkCall;
    }

    public boolean isUsingCallRecordingTone() {
        return mUseCallRecordingTone;
    }

    /**
     * @return {@code true} if the {@link Call}'s {@link #getTargetPhoneAccount()} supports video.
     */
    public boolean isVideoCallingSupportedByPhoneAccount() {
        return mIsVideoCallingSupportedByPhoneAccount;
    }

    /**
     * Sets whether video calling is supported by the current phone account. Since video support
     * can change during a call, this method facilitates updating call video state.
     * @param isVideoCallingSupported Sets whether video calling is supported.
     */
    public void setVideoCallingSupportedByPhoneAccount(boolean isVideoCallingSupported) {
        if (mIsVideoCallingSupportedByPhoneAccount == isVideoCallingSupported) {
            return;
        }
        Log.i(this, "setVideoCallingSupportedByPhoneAccount: isSupp=%b", isVideoCallingSupported);
        mIsVideoCallingSupportedByPhoneAccount = isVideoCallingSupported;

        // Force an update of the connection capabilities so that the dialer is informed of the new
        // video capabilities based on the phone account's support for video.
        setConnectionCapabilities(getConnectionCapabilities(), true /* force */);
    }

    /**
     * Determines if pulling this external call is supported. If it is supported, we will allow the
     * {@link Connection#CAPABILITY_CAN_PULL_CALL} capability to be added to this call's
     * capabilities. If it is not supported, we will strip this capability before sending this
     * call's capabilities to the InCallService.
     * @param isPullExternalCallSupported true, if pulling this external call is supported, false
     *                                    otherwise.
     */
    public void setIsPullExternalCallSupported(boolean isPullExternalCallSupported) {
        if (!isExternalCall()) return;
        if (isPullExternalCallSupported == mIsPullExternalCallSupported) return;

        Log.i(this, "setCanPullExternalCall: canPull=%b", isPullExternalCallSupported);

        mIsPullExternalCallSupported = isPullExternalCallSupported;

        // Use mConnectionCapabilities here to get the unstripped capabilities.
        setConnectionCapabilities(mConnectionCapabilities, true /* force */);
    }

    /**
     * @return {@code true} if the {@link Call} locally supports video.
     */
    public boolean isLocallyVideoCapable() {
        return (getConnectionCapabilities() & Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL)
                == Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL;
    }

    public boolean isSelfManaged() {
        return mIsSelfManaged;
    }

    public void setIsSelfManaged(boolean isSelfManaged) {
        mIsSelfManaged = isSelfManaged;

        // Connection properties will add/remove the PROPERTY_SELF_MANAGED.
        setConnectionProperties(getConnectionProperties());
    }

    public void markFinishedHandoverStateAndCleanup(int handoverState) {
        if (mHandoverSourceCall != null) {
            mHandoverSourceCall.setHandoverState(handoverState);
        } else if (mHandoverDestinationCall != null) {
            mHandoverDestinationCall.setHandoverState(handoverState);
        }
        setHandoverState(handoverState);
        maybeCleanupHandover();
    }

    public void maybeCleanupHandover() {
        if (mHandoverSourceCall != null) {
            mHandoverSourceCall.setHandoverSourceCall(null);
            mHandoverSourceCall.setHandoverDestinationCall(null);
            mHandoverSourceCall = null;
        } else if (mHandoverDestinationCall != null) {
            mHandoverDestinationCall.setHandoverSourceCall(null);
            mHandoverDestinationCall.setHandoverDestinationCall(null);
            mHandoverDestinationCall = null;
        }
    }

    public boolean isHandoverInProgress() {
        return mHandoverSourceCall != null || mHandoverDestinationCall != null;
    }

    public Call getHandoverDestinationCall() {
        return mHandoverDestinationCall;
    }

    public void setHandoverDestinationCall(Call call) {
        mHandoverDestinationCall = call;
    }

    public Call getHandoverSourceCall() {
        return mHandoverSourceCall;
    }

    public void setHandoverSourceCall(Call call) {
        mHandoverSourceCall = call;
    }

    public void setHandoverState(int handoverState) {
        Log.d(this, "setHandoverState: callId=%s, handoverState=%s", getId(),
                HandoverState.stateToString(handoverState));
        mHandoverState = handoverState;
    }

    public int getHandoverState() {
        return mHandoverState;
    }

    private void configureCallAttributes() {
        PhoneAccountRegistrar phoneAccountRegistrar = mCallsManager.getPhoneAccountRegistrar();
        boolean isWorkCall = false;
        boolean isCallRecordingToneSupported = false;
        PhoneAccount phoneAccount =
                phoneAccountRegistrar.getPhoneAccountUnchecked(mTargetPhoneAccountHandle);
        if (phoneAccount != null) {
            final UserHandle userHandle;
            if (phoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_MULTI_USER)) {
                userHandle = mInitiatingUser;
            } else {
                userHandle = mTargetPhoneAccountHandle.getUserHandle();
            }
            if (userHandle != null) {
                isWorkCall = UserUtil.isManagedProfile(mContext, userHandle);
            }

            isCallRecordingToneSupported = (phoneAccount.hasCapabilities(
                    PhoneAccount.CAPABILITY_SIM_SUBSCRIPTION) && phoneAccount.getExtras() != null
                    && phoneAccount.getExtras().getBoolean(
                    PhoneAccount.EXTRA_PLAY_CALL_RECORDING_TONE, false));
        }
        mIsWorkCall = isWorkCall;
        mUseCallRecordingTone = isCallRecordingToneSupported;
    }

    /**
     * Caches the state of the {@link PhoneAccount#CAPABILITY_VIDEO_CALLING} {@link PhoneAccount}
     * capability and ensures that the video state is updated if the phone account does not support
     * video calling.
     */
    private void checkIfVideoCapable() {
        PhoneAccountRegistrar phoneAccountRegistrar = mCallsManager.getPhoneAccountRegistrar();
        if (mTargetPhoneAccountHandle == null) {
            // If no target phone account handle is specified, assume we can potentially perform a
            // video call; once the phone account is set, we can confirm that it is video capable.
            mIsVideoCallingSupportedByPhoneAccount = true;
            Log.d(this, "checkIfVideoCapable: no phone account selected; assume video capable.");
            return;
        }
        PhoneAccount phoneAccount =
                phoneAccountRegistrar.getPhoneAccountUnchecked(mTargetPhoneAccountHandle);
        mIsVideoCallingSupportedByPhoneAccount = phoneAccount != null &&
                phoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_VIDEO_CALLING);

        if (!mIsVideoCallingSupportedByPhoneAccount && VideoProfile.isVideo(getVideoState())) {
            // The PhoneAccount for the Call was set to one which does not support video calling,
            // and the current call is configured to be a video call; downgrade to audio-only.
            setVideoState(VideoProfile.STATE_AUDIO_ONLY);
            Log.d(this, "checkIfVideoCapable: selected phone account doesn't support video.");
        }
    }

    private void checkIfRttCapable() {
        PhoneAccountRegistrar phoneAccountRegistrar = mCallsManager.getPhoneAccountRegistrar();
        if (mTargetPhoneAccountHandle == null) {
            return;
        }

        // Check both the target phone account and the connection manager phone account -- if
        // either support RTT, just set the streams and have them set/unset the RTT property as
        // needed.
        PhoneAccount phoneAccount =
                phoneAccountRegistrar.getPhoneAccountUnchecked(mTargetPhoneAccountHandle);
        PhoneAccount connectionManagerPhoneAccount = phoneAccountRegistrar.getPhoneAccountUnchecked(
                        mConnectionManagerPhoneAccountHandle);
        boolean isRttSupported = phoneAccount != null && phoneAccount.hasCapabilities(
                PhoneAccount.CAPABILITY_RTT);
        boolean isConnectionManagerRttSupported = connectionManagerPhoneAccount != null
                && connectionManagerPhoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_RTT);

        if ((isConnectionManagerRttSupported || isRttSupported)
                && mDidRequestToStartWithRtt && !areRttStreamsInitialized()) {
            // If the phone account got set to an RTT capable one and we haven't set the streams
            // yet, do so now.
            createRttStreams();
            Log.i(this, "Setting RTT streams after target phone account selected");
        }
    }

    boolean shouldAttachToExistingConnection() {
        return mShouldAttachToExistingConnection;
    }

    /**
     * Note: This method relies on {@link #mConnectElapsedTimeMillis} and
     * {@link #mDisconnectElapsedTimeMillis} which are independent of the wall clock (which could
     * change due to clock changes).
     * @return The "age" of this call object in milliseconds, which typically also represents the
     *     period since this call was added to the set pending outgoing calls.
     */
    @VisibleForTesting
    public long getAgeMillis() {
        if (mState == CallState.DISCONNECTED &&
                (mDisconnectCause.getCode() == DisconnectCause.REJECTED ||
                 mDisconnectCause.getCode() == DisconnectCause.MISSED)) {
            // Rejected and missed calls have no age. They're immortal!!
            return 0;
        } else if (mConnectElapsedTimeMillis == 0) {
            // Age is measured in the amount of time the call was active. A zero connect time
            // indicates that we never went active, so return 0 for the age.
            return 0;
        } else if (mDisconnectElapsedTimeMillis == 0) {
            // We connected, but have not yet disconnected
            return mClockProxy.elapsedRealtime() - mConnectElapsedTimeMillis;
        }

        return mDisconnectElapsedTimeMillis - mConnectElapsedTimeMillis;
    }

    /**
     * @return The time when this call object was created and added to the set of pending outgoing
     *     calls.
     */
    public long getCreationTimeMillis() {
        return mCreationTimeMillis;
    }

    public void setCreationTimeMillis(long time) {
        mCreationTimeMillis = time;
    }

    public long getConnectTimeMillis() {
        return mConnectTimeMillis;
    }

    public void setConnectTimeMillis(long connectTimeMillis) {
        mConnectTimeMillis = connectTimeMillis;
    }

    public void setConnectElapsedTimeMillis(long connectElapsedTimeMillis) {
        mConnectElapsedTimeMillis = connectElapsedTimeMillis;
    }

    public int getConnectionCapabilities() {
        return stripUnsupportedCapabilities(mConnectionCapabilities);
    }

    int getConnectionProperties() {
        return mConnectionProperties;
    }

    public void setConnectionCapabilities(int connectionCapabilities) {
        setConnectionCapabilities(connectionCapabilities, false /* forceUpdate */);
    }

    void setConnectionCapabilities(int connectionCapabilities, boolean forceUpdate) {
        Log.v(this, "setConnectionCapabilities: %s", Connection.capabilitiesToString(
                connectionCapabilities));
        if (forceUpdate || mConnectionCapabilities != connectionCapabilities) {
            int previousCapabilities = mConnectionCapabilities;
            mConnectionCapabilities = connectionCapabilities;
            for (Listener l : mListeners) {
                l.onConnectionCapabilitiesChanged(this);
            }

            int strippedCaps = getConnectionCapabilities();
            int xorCaps = previousCapabilities ^ strippedCaps;
            Log.addEvent(this, LogUtils.Events.CAPABILITY_CHANGE,
                    "Current: [%s], Removed [%s], Added [%s]",
                    Connection.capabilitiesToStringShort(strippedCaps),
                    Connection.capabilitiesToStringShort(previousCapabilities & xorCaps),
                    Connection.capabilitiesToStringShort(strippedCaps & xorCaps));
        }
    }

    /**
     * For some states of Telecom, we need to modify this connection's capabilities:
     * - A user should not be able to pull an external call during an emergency call, so
     *   CAPABILITY_CAN_PULL_CALL should be removed until the emergency call ends.
     * @param capabilities The original capabilities.
     * @return The stripped capabilities.
     */
    private int stripUnsupportedCapabilities(int capabilities) {
        if (!mIsPullExternalCallSupported) {
            if ((capabilities |= Connection.CAPABILITY_CAN_PULL_CALL) > 0) {
                capabilities &= ~Connection.CAPABILITY_CAN_PULL_CALL;
                Log.i(this, "stripCapabilitiesBasedOnState: CAPABILITY_CAN_PULL_CALL removed.");
            }
        }
        return capabilities;
    }

    public void setConnectionProperties(int connectionProperties) {
        Log.v(this, "setConnectionProperties: %s", Connection.propertiesToString(
                connectionProperties));

        // Ensure the ConnectionService can't change the state of the self-managed property.
        if (isSelfManaged()) {
            connectionProperties |= Connection.PROPERTY_SELF_MANAGED;
        } else {
            connectionProperties &= ~Connection.PROPERTY_SELF_MANAGED;
        }

        int changedProperties = mConnectionProperties ^ connectionProperties;

        if (changedProperties != 0) {
            int previousProperties = mConnectionProperties;
            mConnectionProperties = connectionProperties;
            boolean didRttChange =
                    (changedProperties & Connection.PROPERTY_IS_RTT) == Connection.PROPERTY_IS_RTT;
            if (didRttChange) {
                if ((mConnectionProperties & Connection.PROPERTY_IS_RTT) ==
                        Connection.PROPERTY_IS_RTT) {
                    createRttStreams();
                    // Call startRtt to pass the RTT pipes down to the connection service.
                    // They already turned on the RTT property so no request should be sent.
                    mConnectionService.startRtt(this,
                            getInCallToCsRttPipeForCs(), getCsToInCallRttPipeForCs());
                    mWasEverRtt = true;
                    if (isEmergencyCall()) {
                        mCallsManager.mute(false);
                    }
                } else {
                    closeRttStreams();
                    mInCallToConnectionServiceStreams = null;
                    mConnectionServiceToInCallStreams = null;
                }
            }
            mWasHighDefAudio = (connectionProperties & Connection.PROPERTY_HIGH_DEF_AUDIO) ==
                    Connection.PROPERTY_HIGH_DEF_AUDIO;
            mWasWifi = (connectionProperties & Connection.PROPERTY_WIFI) > 0;
            for (Listener l : mListeners) {
                l.onConnectionPropertiesChanged(this, didRttChange);
            }

            boolean wasExternal = (previousProperties & Connection.PROPERTY_IS_EXTERNAL_CALL)
                    == Connection.PROPERTY_IS_EXTERNAL_CALL;
            boolean isExternal = (connectionProperties & Connection.PROPERTY_IS_EXTERNAL_CALL)
                    == Connection.PROPERTY_IS_EXTERNAL_CALL;
            if (wasExternal != isExternal) {
                Log.v(this, "setConnectionProperties: external call changed isExternal = %b",
                        isExternal);
                Log.addEvent(this, LogUtils.Events.IS_EXTERNAL, isExternal);
                if (isExternal) {
                    // If there is an ongoing emergency call, remove the ability for this call to
                    // be pulled.
                    boolean isInEmergencyCall = mCallsManager.isInEmergencyCall();
                    setIsPullExternalCallSupported(!isInEmergencyCall);
                }
                for (Listener l : mListeners) {
                    l.onExternalCallChanged(this, isExternal);
                }
            }

            mAnalytics.addCallProperties(mConnectionProperties);

            int xorProps = previousProperties ^ mConnectionProperties;
            Log.addEvent(this, LogUtils.Events.PROPERTY_CHANGE,
                    "Current: [%s], Removed [%s], Added [%s]",
                    Connection.propertiesToStringShort(mConnectionProperties),
                    Connection.propertiesToStringShort(previousProperties & xorProps),
                    Connection.propertiesToStringShort(mConnectionProperties & xorProps));
        }
    }

    public int getSupportedAudioRoutes() {
        return mSupportedAudioRoutes;
    }

    void setSupportedAudioRoutes(int audioRoutes) {
        if (mSupportedAudioRoutes != audioRoutes) {
            mSupportedAudioRoutes = audioRoutes;
        }
    }

    @VisibleForTesting
    public Call getParentCall() {
        return mParentCall;
    }

    @VisibleForTesting
    public List<Call> getChildCalls() {
        return mChildCalls;
    }

    @VisibleForTesting
    public boolean wasConferencePreviouslyMerged() {
        return mWasConferencePreviouslyMerged;
    }

    public boolean isDisconnectingChildCall() {
        return mIsDisconnectingChildCall;
    }

    /**
     * Sets whether this call is a child call.
     */
    private void maybeSetCallAsDisconnectingChild() {
        if (mParentCall != null) {
            mIsDisconnectingChildCall = true;
        }
    }

    @VisibleForTesting
    public Call getConferenceLevelActiveCall() {
        return mConferenceLevelActiveCall;
    }

    @VisibleForTesting
    public ConnectionServiceWrapper getConnectionService() {
        return mConnectionService;
    }

    /**
     * Retrieves the {@link Context} for the call.
     *
     * @return The {@link Context}.
     */
    public Context getContext() {
        return mContext;
    }

    @VisibleForTesting
    public void setConnectionService(ConnectionServiceWrapper service) {
        Preconditions.checkNotNull(service);

        clearConnectionService();

        service.incrementAssociatedCallCount();
        mConnectionService = service;
        mAnalytics.setCallConnectionService(service.getComponentName().flattenToShortString());
        mConnectionService.addCall(this);
    }

    /**
     * Perform an in-place replacement of the {@link ConnectionServiceWrapper} for this Call.
     * Removes the call from its former {@link ConnectionServiceWrapper}, ensuring that the
     * ConnectionService is NOT unbound if the call count hits zero.
     * This is used by the {@link ConnectionServiceWrapper} when handling {@link Connection} and
     * {@link Conference} additions via a ConnectionManager.
     * The original {@link android.telecom.ConnectionService} will directly add external calls and
     * conferences to Telecom as well as the ConnectionManager, which will add to Telecom.  In these
     * cases since its first added to via the original CS, we want to change the CS responsible for
     * the call to the ConnectionManager rather than adding it again as another call/conference.
     *
     * @param service The new {@link ConnectionServiceWrapper}.
     */
    public void replaceConnectionService(ConnectionServiceWrapper service) {
        Preconditions.checkNotNull(service);

        if (mConnectionService != null) {
            ConnectionServiceWrapper serviceTemp = mConnectionService;
            mConnectionService = null;
            serviceTemp.removeCall(this);
            serviceTemp.decrementAssociatedCallCount(true /*isSuppressingUnbind*/);
        }

        service.incrementAssociatedCallCount();
        mConnectionService = service;
        mAnalytics.setCallConnectionService(service.getComponentName().flattenToShortString());
    }

    /**
     * Clears the associated connection service.
     */
    void clearConnectionService() {
        if (mConnectionService != null) {
            ConnectionServiceWrapper serviceTemp = mConnectionService;
            mConnectionService = null;
            serviceTemp.removeCall(this);

            // Decrementing the count can cause the service to unbind, which itself can trigger the
            // service-death code.  Since the service death code tries to clean up any associated
            // calls, we need to make sure to remove that information (e.g., removeCall()) before
            // we decrement. Technically, invoking removeCall() prior to decrementing is all that is
            // necessary, but cleaning up mConnectionService prior to triggering an unbind is good
            // to do.
            decrementAssociatedCallCount(serviceTemp);
        }
    }

    /**
     * Starts the create connection sequence. Upon completion, there should exist an active
     * connection through a connection service (or the call will have failed).
     *
     * @param phoneAccountRegistrar The phone account registrar.
     */
    void startCreateConnection(PhoneAccountRegistrar phoneAccountRegistrar) {
        if (mCreateConnectionProcessor != null) {
            Log.w(this, "mCreateConnectionProcessor in startCreateConnection is not null. This is" +
                    " due to a race between NewOutgoingCallIntentBroadcaster and " +
                    "phoneAccountSelected, but is harmlessly resolved by ignoring the second " +
                    "invocation.");
            return;
        }
        mCreateConnectionProcessor = new CreateConnectionProcessor(this, mRepository, this,
                phoneAccountRegistrar, mContext);
        mCreateConnectionProcessor.process();
    }

    @Override
    public void handleCreateConferenceSuccess(
            CallIdMapper idMapper,
            ParcelableConference conference) {
        Log.v(this, "handleCreateConferenceSuccessful %s", conference);
        setTargetPhoneAccount(conference.getPhoneAccount());
        setHandle(conference.getHandle(), conference.getHandlePresentation());

        setConnectionCapabilities(conference.getConnectionCapabilities());
        setConnectionProperties(conference.getConnectionProperties());
        setVideoProvider(conference.getVideoProvider());
        setVideoState(conference.getVideoState());
        setRingbackRequested(conference.isRingbackRequested());
        setStatusHints(conference.getStatusHints());
        putExtras(SOURCE_CONNECTION_SERVICE, conference.getExtras());

        switch (mCallDirection) {
            case CALL_DIRECTION_INCOMING:
                // Listeners (just CallsManager for now) will be responsible for checking whether
                // the call should be blocked.
                for (Listener l : mListeners) {
                    l.onSuccessfulIncomingCall(this);
                }
                break;
            case CALL_DIRECTION_OUTGOING:
                for (Listener l : mListeners) {
                    l.onSuccessfulOutgoingCall(this,
                            getStateFromConnectionState(conference.getState()));
                }
                break;
        }
    }

    @Override
    public void handleCreateConnectionSuccess(
            CallIdMapper idMapper,
            ParcelableConnection connection) {
        Log.v(this, "handleCreateConnectionSuccessful %s", connection);
        setTargetPhoneAccount(connection.getPhoneAccount());
        setHandle(connection.getHandle(), connection.getHandlePresentation());
        setCallerDisplayName(
                connection.getCallerDisplayName(), connection.getCallerDisplayNamePresentation());

        setConnectionCapabilities(connection.getConnectionCapabilities());
        setConnectionProperties(connection.getConnectionProperties());
        setIsVoipAudioMode(connection.getIsVoipAudioMode());
        setSupportedAudioRoutes(connection.getSupportedAudioRoutes());
        setVideoProvider(connection.getVideoProvider());
        setVideoState(connection.getVideoState());
        setRingbackRequested(connection.isRingbackRequested());
        setStatusHints(connection.getStatusHints());
        putExtras(SOURCE_CONNECTION_SERVICE, connection.getExtras());

        mConferenceableCalls.clear();
        for (String id : connection.getConferenceableConnectionIds()) {
            mConferenceableCalls.add(idMapper.getCall(id));
        }

        switch (mCallDirection) {
            case CALL_DIRECTION_INCOMING:
                setCallerNumberVerificationStatus(connection.getCallerNumberVerificationStatus());

                // Listeners (just CallsManager for now) will be responsible for checking whether
                // the call should be blocked.
                for (Listener l : mListeners) {
                    l.onSuccessfulIncomingCall(this);
                }
                break;
            case CALL_DIRECTION_OUTGOING:
                for (Listener l : mListeners) {
                    l.onSuccessfulOutgoingCall(this,
                            getStateFromConnectionState(connection.getState()));
                }
                break;
            case CALL_DIRECTION_UNKNOWN:
                for (Listener l : mListeners) {
                    l.onSuccessfulUnknownCall(this, getStateFromConnectionState(connection
                            .getState()));
                }
                break;
        }
    }

    @Override
    public void handleCreateConferenceFailure(DisconnectCause disconnectCause) {
        clearConnectionService();
        setDisconnectCause(disconnectCause);
        mCallsManager.markCallAsDisconnected(this, disconnectCause);

        switch (mCallDirection) {
            case CALL_DIRECTION_INCOMING:
                for (Listener listener : mListeners) {
                    listener.onFailedIncomingCall(this);
                }
                break;
            case CALL_DIRECTION_OUTGOING:
                for (Listener listener : mListeners) {
                    listener.onFailedOutgoingCall(this, disconnectCause);
                }
                break;
        }
    }

    @Override
    public void handleCreateConnectionFailure(DisconnectCause disconnectCause) {
        clearConnectionService();
        setDisconnectCause(disconnectCause);
        mCallsManager.markCallAsDisconnected(this, disconnectCause);

        switch (mCallDirection) {
            case CALL_DIRECTION_INCOMING:
                for (Listener listener : mListeners) {
                    listener.onFailedIncomingCall(this);
                }
                break;
            case CALL_DIRECTION_OUTGOING:
                for (Listener listener : mListeners) {
                    listener.onFailedOutgoingCall(this, disconnectCause);
                }
                break;
            case CALL_DIRECTION_UNKNOWN:
                for (Listener listener : mListeners) {
                    listener.onFailedUnknownCall(this);
                }
                break;
        }
    }

    /**
     * Plays the specified DTMF tone.
     */
    @VisibleForTesting
    public void playDtmfTone(char digit) {
        if (mConnectionService == null) {
            Log.w(this, "playDtmfTone() request on a call without a connection service.");
        } else {
            Log.i(this, "Send playDtmfTone to connection service for call %s", this);
            mConnectionService.playDtmfTone(this, digit);
            Log.addEvent(this, LogUtils.Events.START_DTMF, Log.pii(digit));
        }
        mPlayingDtmfTone = digit;
    }

    /**
     * Stops playing any currently playing DTMF tone.
     */
    @VisibleForTesting
    public void stopDtmfTone() {
        if (mConnectionService == null) {
            Log.w(this, "stopDtmfTone() request on a call without a connection service.");
        } else {
            Log.i(this, "Send stopDtmfTone to connection service for call %s", this);
            Log.addEvent(this, LogUtils.Events.STOP_DTMF);
            mConnectionService.stopDtmfTone(this);
        }
        mPlayingDtmfTone = NO_DTMF_TONE;
    }

    /**
     * @return {@code true} if a DTMF tone has been started via {@link #playDtmfTone(char)} but has
     * not been stopped via {@link #stopDtmfTone()}, {@code false} otherwise.
     */
    boolean isDtmfTonePlaying() {
        return mPlayingDtmfTone != NO_DTMF_TONE;
    }

    /**
     * Silences the ringer.
     */
    void silence() {
        if (mConnectionService == null) {
            Log.w(this, "silence() request on a call without a connection service.");
        } else {
            Log.i(this, "Send silence to connection service for call %s", this);
            Log.addEvent(this, LogUtils.Events.SILENCE);
            mConnectionService.silence(this);
        }
    }

    @VisibleForTesting
    public void disconnect() {
        disconnect(0);
    }

    @VisibleForTesting
    public void disconnect(String reason) {
        disconnect(0, reason);
    }

    /**
     * Attempts to disconnect the call through the connection service.
     */
    @VisibleForTesting
    public void disconnect(long disconnectionTimeout) {
        disconnect(disconnectionTimeout, "internal" /** reason */);
    }

    /**
     * Attempts to disconnect the call through the connection service.
     * @param reason the reason for the disconnect; used for logging purposes only.  In some cases
     *               this can be a package name if the disconnect was initiated through an API such
     *               as TelecomManager.
     */
    @VisibleForTesting
    public void disconnect(long disconnectionTimeout, String reason) {
        Log.addEvent(this, LogUtils.Events.REQUEST_DISCONNECT, reason);

        // Track that the call is now locally disconnecting.
        setLocallyDisconnecting(true);
        maybeSetCallAsDisconnectingChild();

        if (mState == CallState.NEW || mState == CallState.SELECT_PHONE_ACCOUNT ||
                mState == CallState.CONNECTING) {
            Log.v(this, "Aborting call %s", this);
            abort(disconnectionTimeout);
        } else if (mState != CallState.ABORTED && mState != CallState.DISCONNECTED) {
            if (mState == CallState.AUDIO_PROCESSING && !hasGoneActiveBefore()) {
                setOverrideDisconnectCauseCode(new DisconnectCause(DisconnectCause.REJECTED));
            } else if (mState == CallState.SIMULATED_RINGING) {
                // This is the case where the dialer calls disconnect() because the call timed out
                // or an emergency call was dialed while in this state.
                // Override the disconnect cause to MISSED
                setOverrideDisconnectCauseCode(new DisconnectCause(DisconnectCause.MISSED));
            }
            if (mConnectionService == null) {
                Log.e(this, new Exception(), "disconnect() request on a call without a"
                        + " connection service.");
            } else {
                Log.i(this, "Send disconnect to connection service for call: %s", this);
                // The call isn't officially disconnected until the connection service
                // confirms that the call was actually disconnected. Only then is the
                // association between call and connection service severed, see
                // {@link CallsManager#markCallAsDisconnected}.
                mConnectionService.disconnect(this);
            }
        }
    }

    void abort(long disconnectionTimeout) {
        if (mCreateConnectionProcessor != null &&
                !mCreateConnectionProcessor.isProcessingComplete()) {
            mCreateConnectionProcessor.abort();
        } else if (mState == CallState.NEW || mState == CallState.SELECT_PHONE_ACCOUNT
                || mState == CallState.CONNECTING) {
            if (disconnectionTimeout > 0) {
                // If the cancelation was from NEW_OUTGOING_CALL with a timeout of > 0
                // milliseconds, do not destroy the call.
                // Instead, we announce the cancellation and CallsManager handles
                // it through a timer. Since apps often cancel calls through NEW_OUTGOING_CALL and
                // then re-dial them quickly using a gateway, allowing the first call to end
                // causes jank. This timeout allows CallsManager to transition the first call into
                // the second call so that in-call only ever sees a single call...eliminating the
                // jank altogether. The app will also be able to set the timeout via an extra on
                // the ordered broadcast.
                for (Listener listener : mListeners) {
                    if (listener.onCanceledViaNewOutgoingCallBroadcast(
                            this, disconnectionTimeout)) {
                        // The first listener to handle this wins. A return value of true means that
                        // the listener will handle the disconnection process later and so we
                        // should not continue it here.
                        setLocallyDisconnecting(false);
                        return;
                    }
                }
            }

            handleCreateConnectionFailure(new DisconnectCause(DisconnectCause.CANCELED));
        } else {
            Log.v(this, "Cannot abort a call which is neither SELECT_PHONE_ACCOUNT or CONNECTING");
        }
    }

    /**
     * Answers the call if it is ringing.
     *
     * @param videoState The video state in which to answer the call.
     */
    @VisibleForTesting
    public void answer(int videoState) {
        // Check to verify that the call is still in the ringing state. A call can change states
        // between the time the user hits 'answer' and Telecom receives the command.
        if (isRinging("answer")) {
            if (!isVideoCallingSupportedByPhoneAccount() && VideoProfile.isVideo(videoState)) {
                // Video calling is not supported, yet the InCallService is attempting to answer as
                // video.  We will simply answer as audio-only.
                videoState = VideoProfile.STATE_AUDIO_ONLY;
            }
            // At this point, we are asking the connection service to answer but we don't assume
            // that it will work. Instead, we wait until confirmation from the connectino service
            // that the call is in a non-STATE_RINGING state before changing the UI. See
            // {@link ConnectionServiceAdapter#setActive} and other set* methods.
            if (mConnectionService != null) {
                mConnectionService.answer(this, videoState);
            } else {
                Log.e(this, new NullPointerException(),
                        "answer call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_ACCEPT);
        }
    }

    /**
     * Answers the call on the connectionservice side in order to start audio processing.
     *
     * This pathway keeps the call in the ANSWERED state until the connection service confirms the
     * answer, at which point we'll set it to AUDIO_PROCESSING. However, to prevent any other
     * components from seeing the churn between RINGING -> ANSWERED -> AUDIO_PROCESSING, we'll
     * refrain from tracking this call in CallsManager until we've stabilized in AUDIO_PROCESSING
     */
    public void answerForAudioProcessing() {
        if (mState != CallState.RINGING) {
            Log.w(this, "Trying to audio-process a non-ringing call: id=%s", mId);
            return;
        }

        if (mConnectionService != null) {
            mConnectionService.answer(this, VideoProfile.STATE_AUDIO_ONLY);
        } else {
            Log.e(this, new NullPointerException(),
                    "answer call (audio processing) failed due to null CS callId=%s", getId());
        }

        Log.addEvent(this, LogUtils.Events.REQUEST_PICKUP_FOR_AUDIO_PROCESSING);
    }

    public void setAudioProcessingRequestingApp(CharSequence appName) {
        mAudioProcessingRequestingApp = appName;
    }

    public CharSequence getAudioProcessingRequestingApp() {
        return mAudioProcessingRequestingApp;
    }

    /**
     * Deflects the call if it is ringing.
     *
     * @param address address to be deflected to.
     */
    @VisibleForTesting
    public void deflect(Uri address) {
        // Check to verify that the call is still in the ringing state. A call can change states
        // between the time the user hits 'deflect' and Telecomm receives the command.
        if (isRinging("deflect")) {
            // At this point, we are asking the connection service to deflect but we don't assume
            // that it will work. Instead, we wait until confirmation from the connection service
            // that the call is in a non-STATE_RINGING state before changing the UI. See
            // {@link ConnectionServiceAdapter#setActive} and other set* methods.
            mVideoStateHistory |= mVideoState;
            if (mConnectionService != null) {
                mConnectionService.deflect(this, address);
            } else {
                Log.e(this, new NullPointerException(),
                        "deflect call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_DEFLECT, Log.pii(address));
        }
    }

    /**
     * Rejects the call if it is ringing.
     *
     * @param rejectWithMessage Whether to send a text message as part of the call rejection.
     * @param textMessage An optional text message to send as part of the rejection.
     */
    @VisibleForTesting
    public void reject(boolean rejectWithMessage, String textMessage) {
        reject(rejectWithMessage, textMessage, "internal" /** reason */);
    }

    /**
     * Rejects the call if it is ringing.
     *
     * @param rejectWithMessage Whether to send a text message as part of the call rejection.
     * @param textMessage An optional text message to send as part of the rejection.
     * @param reason The reason for the reject; used for logging purposes.  May be a package name
     *               if the reject is initiated from an API such as TelecomManager.
     */
    @VisibleForTesting
    public void reject(boolean rejectWithMessage, String textMessage, String reason) {
        if (mState == CallState.SIMULATED_RINGING) {
            // This handles the case where the user manually rejects a call that's in simulated
            // ringing. Since the call is already active on the connectionservice side, we want to
            // hangup, not reject.
            setOverrideDisconnectCauseCode(new DisconnectCause(DisconnectCause.REJECTED));
            if (mConnectionService != null) {
                mConnectionService.disconnect(this);
            } else {
                Log.e(this, new NullPointerException(),
                        "reject call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_REJECT, reason);
        } else if (isRinging("reject")) {
            // Ensure video state history tracks video state at time of rejection.
            mVideoStateHistory |= mVideoState;

            if (mConnectionService != null) {
                mConnectionService.reject(this, rejectWithMessage, textMessage);
            } else {
                Log.e(this, new NullPointerException(),
                        "reject call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_REJECT, reason);
        }
    }

    /**
     * Reject this Telecom call with the user-indicated reason.
     * @param rejectReason The user-indicated reason fore rejecting the call.
     */
    public void reject(@android.telecom.Call.RejectReason int rejectReason) {
        if (mState == CallState.SIMULATED_RINGING) {
            // This handles the case where the user manually rejects a call that's in simulated
            // ringing. Since the call is already active on the connectionservice side, we want to
            // hangup, not reject.
            // Since its simulated reason we can't pass along the reject reason.
            setOverrideDisconnectCauseCode(new DisconnectCause(DisconnectCause.REJECTED));
            if (mConnectionService != null) {
                mConnectionService.disconnect(this);
            } else {
                Log.e(this, new NullPointerException(),
                        "reject call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_REJECT);
        } else if (isRinging("reject")) {
            // Ensure video state history tracks video state at time of rejection.
            mVideoStateHistory |= mVideoState;

            if (mConnectionService != null) {
                mConnectionService.rejectWithReason(this, rejectReason);
            } else {
                Log.e(this, new NullPointerException(),
                        "reject call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_REJECT, rejectReason);
        }
    }

    /**
     * Transfers the call if it is active or held.
     *
     * @param number number to be transferred to.
     * @param isConfirmationRequired whether for blind or assured transfer.
     */
    @VisibleForTesting
    public void transfer(Uri number, boolean isConfirmationRequired) {
        if (mState == CallState.ACTIVE || mState == CallState.ON_HOLD) {
            if (mConnectionService != null) {
                mConnectionService.transfer(this, number, isConfirmationRequired);
            } else {
                Log.e(this, new NullPointerException(),
                        "transfer call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_TRANSFER, Log.pii(number));
        }
    }

    /**
     * Transfers the call when this call is active and the other call is held.
     * This is for Consultative call transfer.
     *
     * @param otherCall The other {@link Call} to which this call will be transferred.
     */
    @VisibleForTesting
    public void transfer(Call otherCall) {
        if (mState == CallState.ACTIVE &&
                (otherCall != null && otherCall.getState() == CallState.ON_HOLD)) {
            if (mConnectionService != null) {
                mConnectionService.transfer(this, otherCall);
            } else {
                Log.e(this, new NullPointerException(),
                        "transfer call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_CONSULTATIVE_TRANSFER, otherCall);
        }
    }

    /**
     * Puts the call on hold if it is currently active.
     */
    @VisibleForTesting
    public void hold() {
        hold(null /* reason */);
    }

    public void hold(String reason) {
        if (mState == CallState.ACTIVE) {
            if (mConnectionService != null) {
                mConnectionService.hold(this);
            } else {
                Log.e(this, new NullPointerException(),
                        "hold call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_HOLD, reason);
        }
    }

    /**
     * Releases the call from hold if it is currently active.
     */
    @VisibleForTesting
    public void unhold() {
        unhold(null /* reason */);
    }

    public void unhold(String reason) {
        if (mState == CallState.ON_HOLD) {
            if (mConnectionService != null) {
                mConnectionService.unhold(this);
            } else {
                Log.e(this, new NullPointerException(),
                        "unhold call failed due to null CS callId=%s", getId());
            }
            Log.addEvent(this, LogUtils.Events.REQUEST_UNHOLD, reason);
        }
    }

    /** Checks if this is a live call or not. */
    @VisibleForTesting
    public boolean isAlive() {
        switch (mState) {
            case CallState.NEW:
            case CallState.RINGING:
            case CallState.ANSWERED:
            case CallState.DISCONNECTED:
            case CallState.ABORTED:
                return false;
            default:
                return true;
        }
    }

    @VisibleForTesting
    public boolean isActive() {
        return mState == CallState.ACTIVE;
    }

    Bundle getExtras() {
        return mExtras;
    }

    /**
     * Adds extras to the extras bundle associated with this {@link Call}.
     *
     * Note: this method needs to know the source of the extras change (see
     * {@link #SOURCE_CONNECTION_SERVICE}, {@link #SOURCE_INCALL_SERVICE}).  Extras changes which
     * originate from a connection service will only be notified to incall services.  Likewise,
     * changes originating from the incall services will only notify the connection service of the
     * change.
     *
     * @param source The source of the extras addition.
     * @param extras The extras.
     */
    public void putExtras(int source, Bundle extras) {
        if (extras == null) {
            return;
        }
        if (mExtras == null) {
            mExtras = new Bundle();
        }
        mExtras.putAll(extras);

        for (Listener l : mListeners) {
            l.onExtrasChanged(this, source, extras);
        }

        // If mExtra shows that the call using Volte, record it with mWasVolte
        if (mExtras.containsKey(TelecomManager.EXTRA_CALL_NETWORK_TYPE) &&
            mExtras.get(TelecomManager.EXTRA_CALL_NETWORK_TYPE)
                    .equals(TelephonyManager.NETWORK_TYPE_LTE)) {
            mWasVolte = true;
        }

        if (extras.containsKey(Connection.EXTRA_ORIGINAL_CONNECTION_ID)) {
            setOriginalConnectionId(extras.getString(Connection.EXTRA_ORIGINAL_CONNECTION_ID));
        }

        // The remote connection service API can track the phone account which was originally
        // requested to create a connection via the remote connection service API; we store that so
        // we have some visibility into how a call was actually placed.
        if (mExtras.containsKey(Connection.EXTRA_REMOTE_PHONE_ACCOUNT_HANDLE)) {
            setRemotePhoneAccountHandle(extras.getParcelable(
                    Connection.EXTRA_REMOTE_PHONE_ACCOUNT_HANDLE));
        }

        // If the change originated from an InCallService, notify the connection service.
        if (source == SOURCE_INCALL_SERVICE) {
            if (mConnectionService != null) {
                mConnectionService.onExtrasChanged(this, mExtras);
            } else {
                Log.e(this, new NullPointerException(),
                        "putExtras failed due to null CS callId=%s", getId());
            }
        }
    }

    /**
     * Removes extras from the extras bundle associated with this {@link Call}.
     *
     * Note: this method needs to know the source of the extras change (see
     * {@link #SOURCE_CONNECTION_SERVICE}, {@link #SOURCE_INCALL_SERVICE}).  Extras changes which
     * originate from a connection service will only be notified to incall services.  Likewise,
     * changes originating from the incall services will only notify the connection service of the
     * change.
     *
     * @param source The source of the extras removal.
     * @param keys The extra keys to remove.
     */
    void removeExtras(int source, List<String> keys) {
        if (mExtras == null) {
            return;
        }
        for (String key : keys) {
            mExtras.remove(key);
        }

        for (Listener l : mListeners) {
            l.onExtrasRemoved(this, source, keys);
        }

        // If the change originated from an InCallService, notify the connection service.
        if (source == SOURCE_INCALL_SERVICE) {
            if (mConnectionService != null) {
                mConnectionService.onExtrasChanged(this, mExtras);
            } else {
                Log.e(this, new NullPointerException(),
                        "removeExtras failed due to null CS callId=%s", getId());
            }
        }
    }

    @VisibleForTesting
    public Bundle getIntentExtras() {
        return mIntentExtras;
    }

    void setIntentExtras(Bundle extras) {
        mIntentExtras = extras;
    }

    public Intent getOriginalCallIntent() {
        return mOriginalCallIntent;
    }

    public void setOriginalCallIntent(Intent intent) {
        mOriginalCallIntent = intent;
    }

    /**
     * @return the uri of the contact associated with this call.
     */
    @VisibleForTesting
    public Uri getContactUri() {
        if (mCallerInfo == null || !mCallerInfo.contactExists) {
            return getHandle();
        }
        return Contacts.getLookupUri(mCallerInfo.getContactId(), mCallerInfo.lookupKey);
    }

    Uri getRingtone() {
        return mCallerInfo == null ? null : mCallerInfo.contactRingtoneUri;
    }

    void onPostDialWait(String remaining) {
        for (Listener l : mListeners) {
            l.onPostDialWait(this, remaining);
        }
    }

    void onPostDialChar(char nextChar) {
        for (Listener l : mListeners) {
            l.onPostDialChar(this, nextChar);
        }
    }

    void postDialContinue(boolean proceed) {
        if (mConnectionService != null) {
            mConnectionService.onPostDialContinue(this, proceed);
        } else {
            Log.e(this, new NullPointerException(),
                    "postDialContinue failed due to null CS callId=%s", getId());
        }
    }

    void conferenceWith(Call otherCall) {
        if (mConnectionService == null) {
            Log.w(this, "conference requested on a call without a connection service.");
        } else {
            Log.addEvent(this, LogUtils.Events.CONFERENCE_WITH, otherCall);
            mConnectionService.conference(this, otherCall);
        }
    }

    void splitFromConference() {
        if (mConnectionService == null) {
            Log.w(this, "splitting from conference call without a connection service");
        } else {
            Log.addEvent(this, LogUtils.Events.SPLIT_FROM_CONFERENCE);
            mConnectionService.splitFromConference(this);
        }
    }

    @VisibleForTesting
    public void mergeConference() {
        if (mConnectionService == null) {
            Log.w(this, "merging conference calls without a connection service.");
        } else if (can(Connection.CAPABILITY_MERGE_CONFERENCE)) {
            Log.addEvent(this, LogUtils.Events.CONFERENCE_WITH);
            mConnectionService.mergeConference(this);
            mWasConferencePreviouslyMerged = true;
        }
    }

    @VisibleForTesting
    public void swapConference() {
        if (mConnectionService == null) {
            Log.w(this, "swapping conference calls without a connection service.");
        } else if (can(Connection.CAPABILITY_SWAP_CONFERENCE)) {
            Log.addEvent(this, LogUtils.Events.SWAP);
            mConnectionService.swapConference(this);
            switch (mChildCalls.size()) {
                case 1:
                    mConferenceLevelActiveCall = mChildCalls.get(0);
                    break;
                case 2:
                    // swap
                    mConferenceLevelActiveCall = mChildCalls.get(0) == mConferenceLevelActiveCall ?
                            mChildCalls.get(1) : mChildCalls.get(0);
                    break;
                default:
                    // For anything else 0, or 3+, set it to null since it is impossible to tell.
                    mConferenceLevelActiveCall = null;
                    break;
            }
            for (Listener l : mListeners) {
                l.onCdmaConferenceSwap(this);
            }
        }
    }

    public void addConferenceParticipants(List<Uri> participants) {
        if (mConnectionService == null) {
            Log.w(this, "adding conference participants without a connection service.");
        } else if (can(Connection.CAPABILITY_ADD_PARTICIPANT)) {
            Log.addEvent(this, LogUtils.Events.ADD_PARTICIPANT);
            mConnectionService.addConferenceParticipants(this, participants);
        }
    }

    /**
     * Initiates a request to the connection service to pull this call.
     * <p>
     * This method can only be used for calls that have the
     * {@link android.telecom.Connection#CAPABILITY_CAN_PULL_CALL} capability and
     * {@link android.telecom.Connection#PROPERTY_IS_EXTERNAL_CALL} property set.
     * <p>
     * An external call is a representation of a call which is taking place on another device
     * associated with a PhoneAccount on this device.  Issuing a request to pull the external call
     * tells the {@link android.telecom.ConnectionService} that it should move the call from the
     * other device to this one.  An example of this is the IMS multi-endpoint functionality.  A
     * user may have two phones with the same phone number.  If the user is engaged in an active
     * call on their first device, the network will inform the second device of that ongoing call in
     * the form of an external call.  The user may wish to continue their conversation on the second
     * device, so will issue a request to pull the call to the second device.
     * <p>
     * Requests to pull a call which is not external, or a call which is not pullable are ignored.
     * If there is an ongoing emergency call, pull requests are also ignored.
     */
    public void pullExternalCall() {
        if (mConnectionService == null) {
            Log.w(this, "pulling a call without a connection service.");
        }

        if (!hasProperty(Connection.PROPERTY_IS_EXTERNAL_CALL)) {
            Log.w(this, "pullExternalCall - call %s is not an external call.", mId);
            return;
        }

        if (!can(Connection.CAPABILITY_CAN_PULL_CALL)) {
            Log.w(this, "pullExternalCall - call %s is external but cannot be pulled.", mId);
            return;
        }

        if (mCallsManager.isInEmergencyCall()) {
            Log.w(this, "pullExternalCall = pullExternalCall - call %s is external but can not be"
                    + " pulled while an emergency call is in progress.", mId);
            mToastFactory.makeText(mContext, R.string.toast_emergency_can_not_pull_call,
                    Toast.LENGTH_LONG).show();
            return;
        }

        Log.addEvent(this, LogUtils.Events.REQUEST_PULL);
        mConnectionService.pullExternalCall(this);
    }

    /**
     * Sends a call event to the {@link ConnectionService} for this call. This function is
     * called for event other than {@link Call#EVENT_REQUEST_HANDOVER}
     *
     * @param event The call event.
     * @param extras Associated extras.
     */
    public void sendCallEvent(String event, Bundle extras) {
        sendCallEvent(event, 0/*For Event != EVENT_REQUEST_HANDOVER*/, extras);
    }

    /**
     * Sends a call event to the {@link ConnectionService} for this call.
     *
     * See {@link Call#sendCallEvent(String, Bundle)}.
     *
     * @param event The call event.
     * @param targetSdkVer SDK version of the app calling this api
     * @param extras Associated extras.
     */
    public void sendCallEvent(String event, int targetSdkVer, Bundle extras) {
        if (mConnectionService != null) {
            if (android.telecom.Call.EVENT_REQUEST_HANDOVER.equals(event)) {
                if (targetSdkVer > Build.VERSION_CODES.P) {
                    Log.e(this, new Exception(), "sendCallEvent failed. Use public api handoverTo" +
                            " for API > 28(P)");
                    // Event-based Handover APIs are deprecated, so inform the user.
                    mHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            mToastFactory.makeText(mContext,
                                    "WARNING: Event-based handover APIs are deprecated and will no"
                                            + " longer function in Android Q.",
                                    Toast.LENGTH_LONG).show();
                        }
                    });

                    // Uncomment and remove toast at feature complete: return;
                }

                // Handover requests are targeted at Telecom, not the ConnectionService.
                if (extras == null) {
                    Log.w(this, "sendCallEvent: %s event received with null extras.",
                            android.telecom.Call.EVENT_REQUEST_HANDOVER);
                    mConnectionService.sendCallEvent(this,
                            android.telecom.Call.EVENT_HANDOVER_FAILED, null);
                    return;
                }
                Parcelable parcelable = extras.getParcelable(
                        android.telecom.Call.EXTRA_HANDOVER_PHONE_ACCOUNT_HANDLE);
                if (!(parcelable instanceof PhoneAccountHandle) || parcelable == null) {
                    Log.w(this, "sendCallEvent: %s event received with invalid handover acct.",
                            android.telecom.Call.EVENT_REQUEST_HANDOVER);
                    mConnectionService.sendCallEvent(this,
                            android.telecom.Call.EVENT_HANDOVER_FAILED, null);
                    return;
                }
                PhoneAccountHandle phoneAccountHandle = (PhoneAccountHandle) parcelable;
                int videoState = extras.getInt(android.telecom.Call.EXTRA_HANDOVER_VIDEO_STATE,
                        VideoProfile.STATE_AUDIO_ONLY);
                Parcelable handoverExtras = extras.getParcelable(
                        android.telecom.Call.EXTRA_HANDOVER_EXTRAS);
                Bundle handoverExtrasBundle = null;
                if (handoverExtras instanceof Bundle) {
                    handoverExtrasBundle = (Bundle) handoverExtras;
                }
                requestHandover(phoneAccountHandle, videoState, handoverExtrasBundle, true);
            } else {
                Log.addEvent(this, LogUtils.Events.CALL_EVENT, event);
                mConnectionService.sendCallEvent(this, event, extras);
            }
        } else {
            Log.e(this, new NullPointerException(),
                    "sendCallEvent failed due to null CS callId=%s", getId());
        }
    }

    /**
     * Initiates a handover of this Call to the {@link ConnectionService} identified
     * by destAcct.
     * @param destAcct ConnectionService to which the call should be handed over.
     * @param videoState The video state desired after the handover.
     * @param extras Extra information to be passed to ConnectionService
     */
    public void handoverTo(PhoneAccountHandle destAcct, int videoState, Bundle extras) {
        requestHandover(destAcct, videoState, extras, false);
    }

    /**
     * Sets this {@link Call} to has the specified {@code parentCall}.  Also sets the parent to
     * have this call as a child.
     * @param parentCall
     */
    void setParentAndChildCall(Call parentCall) {
        boolean isParentChanging = (mParentCall != parentCall);
        setParentCall(parentCall);
        setChildOf(parentCall);
        if (isParentChanging) {
            notifyParentChanged(parentCall);
        }
    }

    /**
     * Notifies listeners when the parent call changes.
     * Used by {@link #setParentAndChildCall(Call)}, and in {@link CallsManager}.
     * @param parentCall The new parent call for this call.
     */
    void notifyParentChanged(Call parentCall) {
        Log.addEvent(this, LogUtils.Events.SET_PARENT, parentCall);
        for (Listener l : mListeners) {
            l.onParentChanged(this);
        }
    }

    /**
     * Unlike {@link #setParentAndChildCall(Call)}, only sets the parent call but does NOT set
     * the child.
     * TODO: This is only required when adding existing connections as a workaround so that we
     * can avoid sending the "onParentChanged" callback until later.
     * @param parentCall The new parent call.
     */
    void setParentCall(Call parentCall) {
        if (parentCall == this) {
            Log.e(this, new Exception(), "setting the parent to self");
            return;
        }
        if (parentCall == mParentCall) {
            // nothing to do
            return;
        }
        if (mParentCall != null) {
            mParentCall.removeChildCall(this);
        }
        mParentCall = parentCall;
    }

    /**
     * To be called after {@link #setParentCall(Call)} to complete setting the parent by adding
     * this call as a child of another call.
     * <p>
     * Note: if using this method alone, the caller must call {@link #notifyParentChanged(Call)} to
     * ensure the InCall UI is updated with the change in parent.
     * @param parentCall The new parent for this call.
     */
    public void setChildOf(Call parentCall) {
        if (parentCall != null && !parentCall.getChildCalls().contains(this)) {
            parentCall.addChildCall(this);
        }
    }

    void setConferenceableCalls(List<Call> conferenceableCalls) {
        mConferenceableCalls.clear();
        mConferenceableCalls.addAll(conferenceableCalls);

        for (Listener l : mListeners) {
            l.onConferenceableCallsChanged(this);
        }
    }

    @VisibleForTesting
    public List<Call> getConferenceableCalls() {
        return mConferenceableCalls;
    }

    @VisibleForTesting
    public boolean can(int capability) {
        return (getConnectionCapabilities() & capability) == capability;
    }

    @VisibleForTesting
    public boolean hasProperty(int property) {
        return (mConnectionProperties & property) == property;
    }

    private void addChildCall(Call call) {
        if (!mChildCalls.contains(call)) {
            mHadChildren = true;
            // Set the pseudo-active call to the latest child added to the conference.
            // See definition of mConferenceLevelActiveCall for more detail.
            mConferenceLevelActiveCall = call;
            mChildCalls.add(call);

            // When adding a child, we will potentially adjust the various times from the calls
            // based on the children being added.  This ensures the parent of the conference has a
            // connect time reflective of all the children added.
            maybeAdjustConnectTime(call);

            Log.addEvent(this, LogUtils.Events.ADD_CHILD, call);

            for (Listener l : mListeners) {
                l.onChildrenChanged(this);
            }
        }
    }

    /**
     * Potentially adjust the connect and creation time of this call based on another one.
     * Ensures that if the other call has an earlier connect time that we adjust the connect time of
     * this call to match.
     * <p>
     * This is important for conference calls; as we add children to the conference we need to
     * ensure that earlier connect time is reflected on the conference.  In the past this
     * was just done in {@link ParcelableCallUtils} when parceling the calls to the UI, but that
     * approach would not reflect the right time on the parent as children disconnect.
     *
     * @param call the call to potentially use to adjust connect time.
     */
    private void maybeAdjustConnectTime(@NonNull Call call) {
        long childConnectTimeMillis = call.getConnectTimeMillis();
        long currentConnectTimeMillis = getConnectTimeMillis();
        // Conference calls typically have a 0 connect time, so we will replace the current connect
        // time if its zero also.
        if (childConnectTimeMillis != 0
                && (currentConnectTimeMillis == 0
                || childConnectTimeMillis < getConnectTimeMillis())) {
            setConnectTimeMillis(childConnectTimeMillis);
        }
    }

    private void removeChildCall(Call call) {
        if (mChildCalls.remove(call)) {
            Log.addEvent(this, LogUtils.Events.REMOVE_CHILD, call);
            for (Listener l : mListeners) {
                l.onChildrenChanged(this);
            }
        }
    }

    /**
     * Return whether the user can respond to this {@code Call} via an SMS message.
     *
     * @return true if the "Respond via SMS" feature should be enabled
     * for this incoming call.
     *
     * The general rule is that we *do* allow "Respond via SMS" except for
     * the few (relatively rare) cases where we know for sure it won't
     * work, namely:
     *   - a bogus or blank incoming number
     *   - a call from a SIP address
     *   - a "call presentation" that doesn't allow the number to be revealed
     *
     * In all other cases, we allow the user to respond via SMS.
     *
     * Note that this behavior isn't perfect; for example we have no way
     * to detect whether the incoming call is from a landline (with most
     * networks at least), so we still enable this feature even though
     * SMSes to that number will silently fail.
     */
    public boolean isRespondViaSmsCapable() {
        if (mState != CallState.RINGING) {
            return false;
        }

        if (getHandle() == null) {
            // No incoming number known or call presentation is "PRESENTATION_RESTRICTED", in
            // other words, the user should not be able to see the incoming phone number.
            return false;
        }

        if (mPhoneNumberUtilsAdapter.isUriNumber(getHandle().toString())) {
            // The incoming number is actually a URI (i.e. a SIP address),
            // not a regular PSTN phone number, and we can't send SMSes to
            // SIP addresses.
            // (TODO: That might still be possible eventually, though. Is
            // there some SIP-specific equivalent to sending a text message?)
            return false;
        }

        // Is there a valid SMS application on the phone?
        if (mContext.getSystemService(TelephonyManager.class)
                .getAndUpdateDefaultRespondViaMessageApplication() == null) {
            return false;
        }

        // TODO: with some carriers (in certain countries) you *can* actually
        // tell whether a given number is a mobile phone or not. So in that
        // case we could potentially return false here if the incoming call is
        // from a land line.

        // If none of the above special cases apply, it's OK to enable the
        // "Respond via SMS" feature.
        return true;
    }

    List<String> getCannedSmsResponses() {
        return mCannedSmsResponses;
    }

    /**
     * We need to make sure that before we move a call to the disconnected state, it no
     * longer has any parent/child relationships.  We want to do this to ensure that the InCall
     * Service always has the right data in the right order.  We also want to do it in telecom so
     * that the insurance policy lives in the framework side of things.
     */
    private void fixParentAfterDisconnect() {
        setParentAndChildCall(null);
    }

    /**
     * @return True if the call is ringing, else logs the action name.
     */
    private boolean isRinging(String actionName) {
        if (mState == CallState.RINGING || mState == CallState.ANSWERED) {
            return true;
        }

        Log.i(this, "Request to %s a non-ringing call %s", actionName, this);
        return false;
    }

    @SuppressWarnings("rawtypes")
    private void decrementAssociatedCallCount(ServiceBinder binder) {
        if (binder != null) {
            binder.decrementAssociatedCallCount();
        }
    }

    /**
     * Looks up contact information based on the current handle.
     */
    private void startCallerInfoLookup() {
        mCallerInfo = null;
        mCallsManager.getCallerInfoLookupHelper().startLookup(mHandle, mCallerInfoQueryListener);
    }

    /**
     * Saves the specified caller info if the specified token matches that of the last query
     * that was made.
     *
     * @param callerInfo The new caller information to set.
     */
    private void setCallerInfo(Uri handle, CallerInfo callerInfo) {
        Trace.beginSection("setCallerInfo");
        if (callerInfo == null) {
            Log.i(this, "CallerInfo lookup returned null, skipping update");
            return;
        }

        if ((handle != null) && !handle.equals(mHandle)) {
            Log.i(this, "setCallerInfo received stale caller info for an old handle. Ignoring.");
            return;
        }

        mCallerInfo = callerInfo;
        Log.i(this, "CallerInfo received for %s: %s", Log.piiHandle(mHandle), callerInfo);

        if (mCallerInfo.getContactDisplayPhotoUri() == null ||
                mCallerInfo.cachedPhotoIcon != null || mCallerInfo.cachedPhoto != null) {
            for (Listener l : mListeners) {
                l.onCallerInfoChanged(this);
            }
        }

        Trace.endSection();
    }

    public CallerInfo getCallerInfo() {
        return mCallerInfo;
    }

    private void maybeLoadCannedSmsResponses() {
        if (mCallDirection == CALL_DIRECTION_INCOMING
                && isRespondViaSmsCapable()
                && !mCannedSmsResponsesLoadingStarted) {
            Log.d(this, "maybeLoadCannedSmsResponses: starting task to load messages");
            mCannedSmsResponsesLoadingStarted = true;
            mCallsManager.getRespondViaSmsManager().loadCannedTextMessages(
                    new Response<Void, List<String>>() {
                        @Override
                        public void onResult(Void request, List<String>... result) {
                            if (result.length > 0) {
                                Log.d(this, "maybeLoadCannedSmsResponses: got %s", result[0]);
                                mCannedSmsResponses = result[0];
                                for (Listener l : mListeners) {
                                    l.onCannedSmsResponsesLoaded(Call.this);
                                }
                            }
                        }

                        @Override
                        public void onError(Void request, int code, String msg) {
                            Log.w(Call.this, "Error obtaining canned SMS responses: %d %s", code,
                                    msg);
                        }
                    },
                    mContext
            );
        } else {
            Log.d(this, "maybeLoadCannedSmsResponses: doing nothing");
        }
    }

    /**
     * Sets speakerphone option on when call begins.
     */
    public void setStartWithSpeakerphoneOn(boolean startWithSpeakerphone) {
        mSpeakerphoneOn = startWithSpeakerphone;
    }

    /**
     * Returns speakerphone option.
     *
     * @return Whether or not speakerphone should be set automatically when call begins.
     */
    public boolean getStartWithSpeakerphoneOn() {
        return mSpeakerphoneOn;
    }

    public void setRequestedToStartWithRtt() {
        mDidRequestToStartWithRtt = true;
    }

    public void stopRtt() {
        if (mConnectionService != null) {
            mConnectionService.stopRtt(this);
        } else {
            // If this gets called by the in-call app before the connection service is set, we'll
            // just ignore it since it's really not supposed to happen.
            Log.w(this, "stopRtt() called before connection service is set.");
        }
    }

    public void sendRttRequest() {
        createRttStreams();
        mConnectionService.startRtt(this, getInCallToCsRttPipeForCs(), getCsToInCallRttPipeForCs());
    }

    private boolean areRttStreamsInitialized() {
        return mInCallToConnectionServiceStreams != null
                && mConnectionServiceToInCallStreams != null;
    }

    public void createRttStreams() {
        if (!areRttStreamsInitialized()) {
            Log.i(this, "Initializing RTT streams");
            try {
                mInCallToConnectionServiceStreams = ParcelFileDescriptor.createReliablePipe();
                mConnectionServiceToInCallStreams = ParcelFileDescriptor.createReliablePipe();
            } catch (IOException e) {
                Log.e(this, e, "Failed to create pipes for RTT call.");
            }
        }
    }

    public void onRttConnectionFailure(int reason) {
        Log.i(this, "Got RTT initiation failure with reason %d", reason);
        for (Listener l : mListeners) {
            l.onRttInitiationFailure(this, reason);
        }
    }

    public void onRemoteRttRequest() {
        if (isRttCall()) {
            Log.w(this, "Remote RTT request on a call that's already RTT");
            return;
        }

        mPendingRttRequestId = mCallsManager.getNextRttRequestId();
        for (Listener l : mListeners) {
            l.onRemoteRttRequest(this, mPendingRttRequestId);
        }
    }

    public void handleRttRequestResponse(int id, boolean accept) {
        if (mPendingRttRequestId == INVALID_RTT_REQUEST_ID) {
            Log.w(this, "Response received to a nonexistent RTT request: %d", id);
            return;
        }
        if (id != mPendingRttRequestId) {
            Log.w(this, "Response ID %d does not match expected %d", id, mPendingRttRequestId);
            return;
        }
        if (accept) {
            createRttStreams();
            Log.i(this, "RTT request %d accepted.", id);
            mConnectionService.respondToRttRequest(
                    this, getInCallToCsRttPipeForCs(), getCsToInCallRttPipeForCs());
        } else {
            Log.i(this, "RTT request %d rejected.", id);
            mConnectionService.respondToRttRequest(this, null, null);
        }
    }

    public boolean isRttCall() {
        return (mConnectionProperties & Connection.PROPERTY_IS_RTT) == Connection.PROPERTY_IS_RTT;
    }

    public boolean wasEverRttCall() {
        return mWasEverRtt;
    }

    public ParcelFileDescriptor getCsToInCallRttPipeForCs() {
        return mConnectionServiceToInCallStreams == null ? null
                : mConnectionServiceToInCallStreams[RTT_PIPE_WRITE_SIDE_INDEX];
    }

    public ParcelFileDescriptor getInCallToCsRttPipeForCs() {
        return mInCallToConnectionServiceStreams == null ? null
                : mInCallToConnectionServiceStreams[RTT_PIPE_READ_SIDE_INDEX];
    }

    public ParcelFileDescriptor getCsToInCallRttPipeForInCall() {
        return mConnectionServiceToInCallStreams == null ? null
                : mConnectionServiceToInCallStreams[RTT_PIPE_READ_SIDE_INDEX];
    }

    public ParcelFileDescriptor getInCallToCsRttPipeForInCall() {
        return mInCallToConnectionServiceStreams == null ? null
                : mInCallToConnectionServiceStreams[RTT_PIPE_WRITE_SIDE_INDEX];
    }

    public int getRttMode() {
        return mRttMode;
    }

    /**
     * Sets a video call provider for the call.
     */
    public void setVideoProvider(IVideoProvider videoProvider) {
        Log.v(this, "setVideoProvider");

        if (mVideoProviderProxy != null) {
            mVideoProviderProxy.clearVideoCallback();
            mVideoProviderProxy = null;
        }

        if (videoProvider != null ) {
            try {
                mVideoProviderProxy = new VideoProviderProxy(mLock, videoProvider, this,
                        mCallsManager);
            } catch (RemoteException ignored) {
                // Ignore RemoteException.
            }
        }

        mVideoProvider = videoProvider;

        for (Listener l : mListeners) {
            l.onVideoCallProviderChanged(Call.this);
        }
    }

    /**
     * @return The {@link Connection.VideoProvider} binder.
     */
    public IVideoProvider getVideoProvider() {
        if (mVideoProviderProxy == null) {
            return null;
        }

        return mVideoProviderProxy.getInterface();
    }

    /**
     * @return The {@link VideoProviderProxy} for this call.
     */
    public VideoProviderProxy getVideoProviderProxy() {
        return mVideoProviderProxy;
    }

    /**
     * The current video state for the call.
     * See {@link VideoProfile} for a list of valid video states.
     */
    public int getVideoState() {
        return mVideoState;
    }

    /**
     * Returns the video states which were applicable over the duration of a call.
     * See {@link VideoProfile} for a list of valid video states.
     *
     * @return The video states applicable over the duration of the call.
     */
    public int getVideoStateHistory() {
        return mVideoStateHistory;
    }

    /**
     * Determines the current video state for the call.
     * For an outgoing call determines the desired video state for the call.
     * Valid values: see {@link VideoProfile}
     *
     * @param videoState The video state for the call.
     */
    public void setVideoState(int videoState) {
        // If the phone account associated with this call does not support video calling, then we
        // will automatically set the video state to audio-only.
        if (!isVideoCallingSupportedByPhoneAccount()) {
            Log.d(this, "setVideoState: videoState=%s defaulted to audio (video not supported)",
                    VideoProfile.videoStateToString(videoState));
            videoState = VideoProfile.STATE_AUDIO_ONLY;
        }

        // Track Video State history during the duration of the call.
        // Only update the history when the call is active or disconnected. This ensures we do
        // not include the video state history when:
        // - Call is incoming (but not answered).
        // - Call it outgoing (but not answered).
        // We include the video state when disconnected to ensure that rejected calls reflect the
        // appropriate video state.
        // For all other times we add to the video state history, see #setState.
        if (isActive() || getState() == CallState.DISCONNECTED) {
            mVideoStateHistory = mVideoStateHistory | videoState;
        }

        int previousVideoState = mVideoState;
        mVideoState = videoState;
        if (mVideoState != previousVideoState) {
            Log.addEvent(this, LogUtils.Events.VIDEO_STATE_CHANGED,
                    VideoProfile.videoStateToString(videoState));
            for (Listener l : mListeners) {
                l.onVideoStateChanged(this, previousVideoState, mVideoState);
            }
        }

        if (VideoProfile.isVideo(videoState)) {
            mAnalytics.setCallIsVideo(true);
        }
    }

    public boolean getIsVoipAudioMode() {
        return mIsVoipAudioMode;
    }

    public void setIsVoipAudioMode(boolean audioModeIsVoip) {
        mIsVoipAudioMode = audioModeIsVoip;
        for (Listener l : mListeners) {
            l.onIsVoipAudioModeChanged(this);
        }
    }

    public StatusHints getStatusHints() {
        return mStatusHints;
    }

    public void setStatusHints(StatusHints statusHints) {
        mStatusHints = statusHints;
        for (Listener l : mListeners) {
            l.onStatusHintsChanged(this);
        }
    }

    public boolean isUnknown() {
        return mCallDirection == CALL_DIRECTION_UNKNOWN;
    }

    /**
     * Determines if this call is in a disconnecting state.
     *
     * @return {@code true} if this call is locally disconnecting.
     */
    public boolean isLocallyDisconnecting() {
        return mIsLocallyDisconnecting;
    }

    /**
     * Sets whether this call is in a disconnecting state.
     *
     * @param isLocallyDisconnecting {@code true} if this call is locally disconnecting.
     */
    private void setLocallyDisconnecting(boolean isLocallyDisconnecting) {
        mIsLocallyDisconnecting = isLocallyDisconnecting;
    }

    /**
     * @return user handle of user initiating the outgoing call.
     */
    public UserHandle getInitiatingUser() {
        return mInitiatingUser;
    }

    /**
     * Set the user handle of user initiating the outgoing call.
     * @param initiatingUser
     */
    public void setInitiatingUser(UserHandle initiatingUser) {
        Preconditions.checkNotNull(initiatingUser);
        mInitiatingUser = initiatingUser;
    }

    static int getStateFromConnectionState(int state) {
        switch (state) {
            case Connection.STATE_INITIALIZING:
                return CallState.CONNECTING;
            case Connection.STATE_ACTIVE:
                return CallState.ACTIVE;
            case Connection.STATE_DIALING:
                return CallState.DIALING;
            case Connection.STATE_PULLING_CALL:
                return CallState.PULLING;
            case Connection.STATE_DISCONNECTED:
                return CallState.DISCONNECTED;
            case Connection.STATE_HOLDING:
                return CallState.ON_HOLD;
            case Connection.STATE_NEW:
                return CallState.NEW;
            case Connection.STATE_RINGING:
                return CallState.RINGING;
        }
        return CallState.DISCONNECTED;
    }

    /**
     * Determines if this call is in disconnected state and waiting to be destroyed.
     *
     * @return {@code true} if this call is disconected.
     */
    public boolean isDisconnected() {
        return (getState() == CallState.DISCONNECTED || getState() == CallState.ABORTED);
    }

    /**
     * Determines if this call has just been created and has not been configured properly yet.
     *
     * @return {@code true} if this call is new.
     */
    public boolean isNew() {
        return getState() == CallState.NEW;
    }

    /**
     * Sets the call data usage for the call.
     *
     * @param callDataUsage The new call data usage (in bytes).
     */
    public void setCallDataUsage(long callDataUsage) {
        mCallDataUsage = callDataUsage;
    }

    /**
     * Returns the call data usage for the call.
     *
     * @return The call data usage (in bytes).
     */
    public long getCallDataUsage() {
        return mCallDataUsage;
    }

    public void setRttMode(int mode) {
        mRttMode = mode;
        // TODO: hook this up to CallAudioManager
    }

    /**
     * Returns true if the call is outgoing and the NEW_OUTGOING_CALL ordered broadcast intent
     * has come back to telecom and was processed.
     */
    public boolean isNewOutgoingCallIntentBroadcastDone() {
        return mIsNewOutgoingCallIntentBroadcastDone;
    }

    public void setNewOutgoingCallIntentBroadcastIsDone() {
        mIsNewOutgoingCallIntentBroadcastDone = true;
    }

    /**
     * Determines if the call has been held by the remote party.
     *
     * @return {@code true} if the call is remotely held, {@code false} otherwise.
     */
    public boolean isRemotelyHeld() {
        return mIsRemotelyHeld;
    }

    /**
     * Handles Connection events received from a {@link ConnectionService}.
     *
     * @param event The event.
     * @param extras The extras.
     */
    public void onConnectionEvent(String event, Bundle extras) {
        Log.addEvent(this, LogUtils.Events.CONNECTION_EVENT, event);
        if (Connection.EVENT_ON_HOLD_TONE_START.equals(event)) {
            mIsRemotelyHeld = true;
            Log.addEvent(this, LogUtils.Events.REMOTELY_HELD);
            // Inform listeners of the fact that a call hold tone was received.  This will trigger
            // the CallAudioManager to play a tone via the InCallTonePlayer.
            for (Listener l : mListeners) {
                l.onHoldToneRequested(this);
            }
        } else if (Connection.EVENT_ON_HOLD_TONE_END.equals(event)) {
            mIsRemotelyHeld = false;
            Log.addEvent(this, LogUtils.Events.REMOTELY_UNHELD);
            for (Listener l : mListeners) {
                l.onHoldToneRequested(this);
            }
        } else if (Connection.EVENT_CALL_HOLD_FAILED.equals(event)) {
            for (Listener l : mListeners) {
                l.onCallHoldFailed(this);
            }
        } else if (Connection.EVENT_CALL_SWITCH_FAILED.equals(event)) {
            for (Listener l : mListeners) {
                l.onCallSwitchFailed(this);
            }
        } else {
            for (Listener l : mListeners) {
                l.onConnectionEvent(this, event, extras);
            }
        }
    }

    /**
     * Notifies interested parties that the handover has completed.
     * Notifies:
     * 1. {@link InCallController} which communicates this to the
     * {@link android.telecom.InCallService} via {@link Listener#onHandoverComplete()}.
     * 2. {@link ConnectionServiceWrapper} which informs the {@link android.telecom.Connection} of
     * the successful handover.
     */
    public void onHandoverComplete() {
        Log.i(this, "onHandoverComplete; callId=%s", getId());
        if (mConnectionService != null) {
            mConnectionService.handoverComplete(this);
        }
        for (Listener l : mListeners) {
            l.onHandoverComplete(this);
        }
    }

    public void onHandoverFailed(int handoverError) {
        Log.i(this, "onHandoverFailed; callId=%s, handoverError=%d", getId(), handoverError);
        for (Listener l : mListeners) {
            l.onHandoverFailed(this, handoverError);
        }
    }

    public void setOriginalConnectionId(String originalConnectionId) {
        mOriginalConnectionId = originalConnectionId;
    }

    /**
     * For calls added via a ConnectionManager using the
     * {@link android.telecom.ConnectionService#addExistingConnection(PhoneAccountHandle,
     * Connection)}, or {@link android.telecom.ConnectionService#addConference(Conference)} APIS,
     * indicates the ID of this call as it was referred to by the {@code ConnectionService} which
     * originally created it.
     *
     * See {@link Connection#EXTRA_ORIGINAL_CONNECTION_ID}.
     * @return The original connection ID.
     */
    public String getOriginalConnectionId() {
        return mOriginalConnectionId;
    }

    public ConnectionServiceFocusManager getConnectionServiceFocusManager() {
        return mCallsManager.getConnectionServiceFocusManager();
    }

    /**
     * Determines if a {@link Call}'s capabilities bitmask indicates that video is supported either
     * remotely or locally.
     *
     * @param capabilities The {@link Connection} capabilities for the call.
     * @return {@code true} if video is supported, {@code false} otherwise.
     */
    private boolean doesCallSupportVideo(int capabilities) {
        return (capabilities & Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL) != 0 ||
                (capabilities & Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL) != 0;
    }

    /**
     * Remove any video capabilities set on a {@link Connection} capabilities bitmask.
     *
     * @param capabilities The capabilities.
     * @return The bitmask with video capabilities removed.
     */
    private int removeVideoCapabilities(int capabilities) {
        return capabilities & ~(Connection.CAPABILITY_SUPPORTS_VT_LOCAL_BIDIRECTIONAL |
                Connection.CAPABILITY_SUPPORTS_VT_REMOTE_BIDIRECTIONAL);
    }

    /**
     * Initiates a handover of this {@link Call} to another {@link PhoneAccount}.
     * @param handoverToHandle The {@link PhoneAccountHandle} to handover to.
     * @param videoState The video state of the call when handed over.
     * @param extras Optional extras {@link Bundle} provided by the initiating
     *      {@link android.telecom.InCallService}.
     */
    private void requestHandover(PhoneAccountHandle handoverToHandle, int videoState,
                                 Bundle extras, boolean isLegacy) {
        for (Listener l : mListeners) {
            l.onHandoverRequested(this, handoverToHandle, videoState, extras, isLegacy);
        }
    }

    private TelephonyManager getTelephonyManager() {
        return mContext.getSystemService(TelephonyManager.class);
    }

    /**
     * Sets whether this {@link Call} is a conference or not.
     * @param isConference
     */
    public void setConferenceState(boolean isConference) {
        mIsConference = isConference;
        Log.addEvent(this, LogUtils.Events.CONF_STATE_CHANGED, "isConference=" + isConference);
        // Ultimately CallsManager needs to know so it can update the "add call" state and inform
        // the UI to update itself.
        for (Listener l : mListeners) {
            l.onConferenceStateChanged(this, isConference);
        }
    }

    /**
     * Change the call direction. This is useful if it was not previously defined (for example in
     * single caller emulation mode).
     * @param callDirection The new direction of this call.
     */
    // Make sure the callDirection has been mapped to the Call definition correctly!
    public void setCallDirection(int callDirection) {
        if (mCallDirection != callDirection) {
            Log.addEvent(this, LogUtils.Events.CALL_DIRECTION_CHANGED, "callDirection="
                    + callDirection);
            mCallDirection = callDirection;
            for (Listener l : mListeners) {
                // Update InCallService directly, do not notify CallsManager.
                l.onCallDirectionChanged(this);
            }
        }
    }

    /**
     * Sets the video history based on the state and state transitions of the call. Always add the
     * current video state to the video state history during a call transition except for the
     * transitions DIALING->ACTIVE and RINGING->ANSWERED. In these cases, clear the history. If a
     * call starts dialing/ringing as a VT call and gets downgraded to audio, we need to record
     * the history as an audio call.
     */
    private void updateVideoHistoryViaState(int oldState, int newState) {
        if ((oldState == CallState.DIALING && newState == CallState.ACTIVE)
                || (oldState == CallState.RINGING && newState == CallState.ANSWERED)) {
            mVideoStateHistory = mVideoState;
        }

        mVideoStateHistory |= mVideoState;
    }

    /**
     * Returns whether or not high definition audio was used.
     *
     * @return true if high definition audio was used during this call.
     */
    boolean wasHighDefAudio() {
        return mWasHighDefAudio;
    }

    /**
     * Returns whether or not Wifi call was used.
     *
     * @return true if wifi call was used during this call.
     */
    boolean wasWifi() {
        return mWasWifi;
    }

    public void setIsUsingCallFiltering(boolean isUsingCallFiltering) {
        mIsUsingCallFiltering = isUsingCallFiltering;
    }

    /**
     * Returns whether or not Volte call was used.
     *
     * @return true if Volte call was used during this call.
     */
    public boolean wasVolte() {
        return mWasVolte;
    }

    /**
     * In some cases, we need to know if this call has ever gone active (for example, the case
     * when the call was put into the {@link CallState#AUDIO_PROCESSING} state after being active)
     * for call logging purposes.
     *
     * @return {@code true} if this call has gone active before (even if it isn't now), false if it
     * has never gone active.
     */
    public boolean hasGoneActiveBefore() {
        return mHasGoneActiveBefore;
    }

    /**
     * When upgrading a call to video via
     * {@link VideoProviderProxy#onSendSessionModifyRequest(VideoProfile, VideoProfile)}, if the
     * upgrade is from audio to video, potentially auto-engage the speakerphone.
     * @param newVideoState The proposed new video state for the call.
     */
    public void maybeEnableSpeakerForVideoUpgrade(@VideoProfile.VideoState int newVideoState) {
        if (mCallsManager.isSpeakerphoneAutoEnabledForVideoCalls(newVideoState)) {
            Log.i(this, "maybeEnableSpeakerForVideoCall; callId=%s, auto-enable speaker for call"
                            + " upgraded to video.");
            mCallsManager.setAudioRoute(CallAudioState.ROUTE_SPEAKER, null);
        }
    }

    /**
     * Remaps the call direction as indicated by an {@link android.telecom.Call.Details} direction
     * constant to the constants (e.g. {@link #CALL_DIRECTION_INCOMING}) used in this call class.
     * @param direction The android.telecom.Call direction.
     * @return The direction using the constants in this class.
     */
    public static int getRemappedCallDirection(
            @android.telecom.Call.Details.CallDirection int direction) {
        switch(direction) {
            case android.telecom.Call.Details.DIRECTION_INCOMING:
                return CALL_DIRECTION_INCOMING;
            case android.telecom.Call.Details.DIRECTION_OUTGOING:
                return CALL_DIRECTION_OUTGOING;
            case android.telecom.Call.Details.DIRECTION_UNKNOWN:
                return CALL_DIRECTION_UNDEFINED;
        }
        return CALL_DIRECTION_UNDEFINED;
    }

    /**
     * Set the package name of the {@link android.telecom.CallScreeningService} which should be sent
     * the {@link android.telecom.TelecomManager#ACTION_POST_CALL} upon disconnection of a call.
     * @param packageName post call screen service package name.
     */
    public void setPostCallPackageName(String packageName) {
        mPostCallPackageName = packageName;
    }

    /**
     * Return the package name of the {@link android.telecom.CallScreeningService} which should be
     * sent the {@link android.telecom.TelecomManager#ACTION_POST_CALL} upon disconnection of a
     * call.
     * @return post call screen service package name.
     */
    public String getPostCallPackageName() {
        return mPostCallPackageName;
    }
}
