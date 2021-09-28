/*
 * Copyright (C) 2013 The Android Open Source Project
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

import static android.telecom.TelecomManager.ACTION_POST_CALL;
import static android.telecom.TelecomManager.DURATION_LONG;
import static android.telecom.TelecomManager.DURATION_MEDIUM;
import static android.telecom.TelecomManager.DURATION_SHORT;
import static android.telecom.TelecomManager.DURATION_VERY_SHORT;
import static android.telecom.TelecomManager.EXTRA_CALL_DURATION;
import static android.telecom.TelecomManager.EXTRA_DISCONNECT_CAUSE;
import static android.telecom.TelecomManager.EXTRA_HANDLE;
import static android.telecom.TelecomManager.MEDIUM_CALL_TIME_MS;
import static android.telecom.TelecomManager.SHORT_CALL_TIME_MS;
import static android.telecom.TelecomManager.VERY_SHORT_CALL_TIME_MS;

import android.Manifest;
import android.annotation.NonNull;
import android.app.ActivityManager;
import android.app.AlertDialog;
import android.app.KeyguardManager;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageManager;
import android.content.pm.UserInfo;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.media.AudioManager;
import android.media.AudioSystem;
import android.media.MediaPlayer;
import android.media.ToneGenerator;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.os.PersistableBundle;
import android.os.Process;
import android.os.SystemClock;
import android.os.SystemVibrator;
import android.os.Trace;
import android.os.UserHandle;
import android.os.UserManager;
import android.provider.BlockedNumberContract;
import android.provider.BlockedNumberContract.SystemContract;
import android.provider.CallLog.Calls;
import android.provider.Settings;
import android.sysprop.TelephonyProperties;
import android.telecom.CallAudioState;
import android.telecom.CallerInfo;
import android.telecom.Conference;
import android.telecom.Connection;
import android.telecom.DisconnectCause;
import android.telecom.GatewayInfo;
import android.telecom.Log;
import android.telecom.Logging.Runnable;
import android.telecom.Logging.Session;
import android.telecom.ParcelableConference;
import android.telecom.ParcelableConnection;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.PhoneAccountSuggestion;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.TelephonyManager;
import android.text.TextUtils;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.WindowManager;
import android.widget.Button;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.util.IndentingPrintWriter;
import com.android.server.telecom.bluetooth.BluetoothRouteManager;
import com.android.server.telecom.bluetooth.BluetoothStateReceiver;
import com.android.server.telecom.callfiltering.BlockCheckerAdapter;
import com.android.server.telecom.callfiltering.BlockCheckerFilter;
import com.android.server.telecom.callfiltering.CallFilterResultCallback;
import com.android.server.telecom.callfiltering.CallFilteringResult;
import com.android.server.telecom.callfiltering.CallFilteringResult.Builder;
import com.android.server.telecom.callfiltering.CallScreeningServiceFilter;
import com.android.server.telecom.callfiltering.DirectToVoicemailFilter;
import com.android.server.telecom.callfiltering.IncomingCallFilter;
import com.android.server.telecom.callfiltering.IncomingCallFilterGraph;
import com.android.server.telecom.callredirection.CallRedirectionProcessor;
import com.android.server.telecom.components.ErrorDialogActivity;
import com.android.server.telecom.components.TelecomBroadcastReceiver;
import com.android.server.telecom.settings.BlockedNumbersUtil;
import com.android.server.telecom.ui.AudioProcessingNotification;
import com.android.server.telecom.ui.CallRedirectionTimeoutDialogActivity;
import com.android.server.telecom.ui.ConfirmCallDialogActivity;
import com.android.server.telecom.ui.DisconnectedCallNotifier;
import com.android.server.telecom.ui.IncomingCallNotifier;
import com.android.server.telecom.ui.ToastFactory;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.ConcurrentHashMap;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;
import java.util.stream.IntStream;
import java.util.stream.Stream;

/**
 * Singleton.
 *
 * NOTE: by design most APIs are package private, use the relevant adapter/s to allow
 * access from other packages specifically refraining from passing the CallsManager instance
 * beyond the com.android.server.telecom package boundary.
 */
@VisibleForTesting
public class CallsManager extends Call.ListenerBase
        implements VideoProviderProxy.Listener, CallFilterResultCallback, CurrentUserProxy {

    // TODO: Consider renaming this CallsManagerPlugin.
    @VisibleForTesting
    public interface CallsManagerListener {
        void onCallAdded(Call call);
        void onCallRemoved(Call call);
        void onCallStateChanged(Call call, int oldState, int newState);
        void onConnectionServiceChanged(
                Call call,
                ConnectionServiceWrapper oldService,
                ConnectionServiceWrapper newService);
        void onIncomingCallAnswered(Call call);
        void onIncomingCallRejected(Call call, boolean rejectWithMessage, String textMessage);
        void onCallAudioStateChanged(CallAudioState oldAudioState, CallAudioState newAudioState);
        void onRingbackRequested(Call call, boolean ringback);
        void onIsConferencedChanged(Call call);
        void onIsVoipAudioModeChanged(Call call);
        void onVideoStateChanged(Call call, int previousVideoState, int newVideoState);
        void onCanAddCallChanged(boolean canAddCall);
        void onSessionModifyRequestReceived(Call call, VideoProfile videoProfile);
        void onHoldToneRequested(Call call);
        void onExternalCallChanged(Call call, boolean isExternalCall);
        void onDisconnectedTonePlaying(boolean isTonePlaying);
        void onConnectionTimeChanged(Call call);
        void onConferenceStateChanged(Call call, boolean isConference);
        void onCdmaConferenceSwap(Call call);
        void onSetCamera(Call call, String cameraId);
    }

    /** Interface used to define the action which is executed delay under some condition. */
    interface PendingAction {
        void performAction();
    }

    private static final String TAG = "CallsManager";

    /**
     * Call filter specifier used with
     * {@link #getNumCallsWithState(int, Call, PhoneAccountHandle, int...)} to indicate only
     * self-managed calls should be included.
     */
    private static final int CALL_FILTER_SELF_MANAGED = 1;

    /**
     * Call filter specifier used with
     * {@link #getNumCallsWithState(int, Call, PhoneAccountHandle, int...)} to indicate only
     * managed calls should be included.
     */
    private static final int CALL_FILTER_MANAGED = 2;

    /**
     * Call filter specifier used with
     * {@link #getNumCallsWithState(int, Call, PhoneAccountHandle, int...)} to indicate both managed
     * and self-managed calls should be included.
     */
    private static final int CALL_FILTER_ALL = 3;

    private static final String PERMISSION_PROCESS_PHONE_ACCOUNT_REGISTRATION =
            "android.permission.PROCESS_PHONE_ACCOUNT_REGISTRATION";

    private static final int HANDLER_WAIT_TIMEOUT = 10000;
    private static final int MAXIMUM_LIVE_CALLS = 1;
    private static final int MAXIMUM_HOLD_CALLS = 1;
    private static final int MAXIMUM_RINGING_CALLS = 1;
    private static final int MAXIMUM_DIALING_CALLS = 1;
    private static final int MAXIMUM_OUTGOING_CALLS = 1;
    private static final int MAXIMUM_TOP_LEVEL_CALLS = 2;
    private static final int MAXIMUM_SELF_MANAGED_CALLS = 10;

    private static final int[] OUTGOING_CALL_STATES =
            {CallState.CONNECTING, CallState.SELECT_PHONE_ACCOUNT, CallState.DIALING,
                    CallState.PULLING};

    /**
     * These states are used by {@link #makeRoomForOutgoingCall(Call, boolean)} to determine which
     * call should be ended first to make room for a new outgoing call.
     */
    private static final int[] LIVE_CALL_STATES =
            {CallState.CONNECTING, CallState.SELECT_PHONE_ACCOUNT, CallState.DIALING,
                    CallState.PULLING, CallState.ACTIVE, CallState.AUDIO_PROCESSING};

    /**
     * These states determine which calls will cause {@link TelecomManager#isInCall()} or
     * {@link TelecomManager#isInManagedCall()} to return true.
     *
     * See also {@link PhoneStateBroadcaster}, which considers a similar set of states as being
     * off-hook.
     */
    public static final int[] ONGOING_CALL_STATES =
            {CallState.SELECT_PHONE_ACCOUNT, CallState.DIALING, CallState.PULLING, CallState.ACTIVE,
                    CallState.ON_HOLD, CallState.RINGING,  CallState.SIMULATED_RINGING,
                    CallState.ANSWERED, CallState.AUDIO_PROCESSING};

    private static final int[] ANY_CALL_STATE =
            {CallState.NEW, CallState.CONNECTING, CallState.SELECT_PHONE_ACCOUNT, CallState.DIALING,
                    CallState.RINGING, CallState.SIMULATED_RINGING, CallState.ACTIVE,
                    CallState.ON_HOLD, CallState.DISCONNECTED, CallState.ABORTED,
                    CallState.DISCONNECTING, CallState.PULLING, CallState.ANSWERED,
                    CallState.AUDIO_PROCESSING};

    public static final String TELECOM_CALL_ID_PREFIX = "TC@";

    // Maps call technologies in TelephonyManager to those in Analytics.
    private static final Map<Integer, Integer> sAnalyticsTechnologyMap;
    static {
        sAnalyticsTechnologyMap = new HashMap<>(5);
        sAnalyticsTechnologyMap.put(TelephonyManager.PHONE_TYPE_CDMA, Analytics.CDMA_PHONE);
        sAnalyticsTechnologyMap.put(TelephonyManager.PHONE_TYPE_GSM, Analytics.GSM_PHONE);
        sAnalyticsTechnologyMap.put(TelephonyManager.PHONE_TYPE_IMS, Analytics.IMS_PHONE);
        sAnalyticsTechnologyMap.put(TelephonyManager.PHONE_TYPE_SIP, Analytics.SIP_PHONE);
        sAnalyticsTechnologyMap.put(TelephonyManager.PHONE_TYPE_THIRD_PARTY,
                Analytics.THIRD_PARTY_PHONE);
    }

    /**
     * The main call repository. Keeps an instance of all live calls. New incoming and outgoing
     * calls are added to the map and removed when the calls move to the disconnected state.
     *
     * ConcurrentHashMap constructor params: 8 is initial table size, 0.9f is
     * load factor before resizing, 1 means we only expect a single thread to
     * access the map so make only a single shard
     */
    private final Set<Call> mCalls = Collections.newSetFromMap(
            new ConcurrentHashMap<Call, Boolean>(8, 0.9f, 1));

    /**
     * A pending call is one which requires user-intervention in order to be placed.
     * Used by {@link #startCallConfirmation}.
     */
    private Call mPendingCall;
    /**
     * Cached latest pending redirected call which requires user-intervention in order to be placed.
     * Used by {@link #onCallRedirectionComplete}.
     */
    private Call mPendingRedirectedOutgoingCall;

    /**
     * Cached call that's been answered but will be added to mCalls pending confirmation of active
     * status from the connection service.
     */
    private Call mPendingAudioProcessingCall;

    /**
     * Cached latest pending redirected call information which require user-intervention in order
     * to be placed. Used by {@link #onCallRedirectionComplete}.
     */
    private final Map<String, Runnable> mPendingRedirectedOutgoingCallInfo =
            new ConcurrentHashMap<>();
    /**
     * Cached latest pending Unredirected call information which require user-intervention in order
     * to be placed. Used by {@link #onCallRedirectionComplete}.
     */
    private final Map<String, Runnable> mPendingUnredirectedOutgoingCallInfo =
            new ConcurrentHashMap<>();

    private CompletableFuture<Call> mPendingCallConfirm;
    private CompletableFuture<Pair<Call, PhoneAccountHandle>> mPendingAccountSelection;

    // Instance variables for testing -- we keep the latest copy of the outgoing call futures
    // here so that we can wait on them in tests
    private CompletableFuture<Call> mLatestPostSelectionProcessingFuture;
    private CompletableFuture<Pair<Call, List<PhoneAccountSuggestion>>>
            mLatestPreAccountSelectionFuture;

    /**
     * The current telecom call ID.  Used when creating new instances of {@link Call}.  Should
     * only be accessed using the {@link #getNextCallId()} method which synchronizes on the
     * {@link #mLock} sync root.
     */
    private int mCallId = 0;

    private int mRttRequestId = 0;
    /**
     * Stores the current foreground user.
     */
    private UserHandle mCurrentUserHandle = UserHandle.of(ActivityManager.getCurrentUser());

    private final ConnectionServiceRepository mConnectionServiceRepository;
    private final DtmfLocalTonePlayer mDtmfLocalTonePlayer;
    private final InCallController mInCallController;
    private final CallAudioManager mCallAudioManager;
    private final CallRecordingTonePlayer mCallRecordingTonePlayer;
    private RespondViaSmsManager mRespondViaSmsManager;
    private final Ringer mRinger;
    private final InCallWakeLockController mInCallWakeLockController;
    // For this set initial table size to 16 because we add 13 listeners in
    // the CallsManager constructor.
    private final Set<CallsManagerListener> mListeners = Collections.newSetFromMap(
            new ConcurrentHashMap<CallsManagerListener, Boolean>(16, 0.9f, 1));
    private final HeadsetMediaButton mHeadsetMediaButton;
    private final WiredHeadsetManager mWiredHeadsetManager;
    private final SystemStateHelper mSystemStateHelper;
    private final BluetoothRouteManager mBluetoothRouteManager;
    private final DockManager mDockManager;
    private final TtyManager mTtyManager;
    private final ProximitySensorManager mProximitySensorManager;
    private final PhoneStateBroadcaster mPhoneStateBroadcaster;
    private final CallLogManager mCallLogManager;
    private final Context mContext;
    private final TelecomSystem.SyncRoot mLock;
    private final PhoneAccountRegistrar mPhoneAccountRegistrar;
    private final MissedCallNotifier mMissedCallNotifier;
    private final DisconnectedCallNotifier mDisconnectedCallNotifier;
    private IncomingCallNotifier mIncomingCallNotifier;
    private final CallerInfoLookupHelper mCallerInfoLookupHelper;
    private final IncomingCallFilter.Factory mIncomingCallFilterFactory;
    private final DefaultDialerCache mDefaultDialerCache;
    private final Timeouts.Adapter mTimeoutsAdapter;
    private final PhoneNumberUtilsAdapter mPhoneNumberUtilsAdapter;
    private final ClockProxy mClockProxy;
    private final ToastFactory mToastFactory;
    private final Set<Call> mLocallyDisconnectingCalls = new HashSet<>();
    private final Set<Call> mPendingCallsToDisconnect = new HashSet<>();
    private final ConnectionServiceFocusManager mConnectionSvrFocusMgr;
    /* Handler tied to thread in which CallManager was initialized. */
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final EmergencyCallHelper mEmergencyCallHelper;
    private final RoleManagerAdapter mRoleManagerAdapter;

    private final ConnectionServiceFocusManager.CallsManagerRequester mRequester =
            new ConnectionServiceFocusManager.CallsManagerRequester() {
                @Override
                public void releaseConnectionService(
                        ConnectionServiceFocusManager.ConnectionServiceFocus connectionService) {
                    mCalls.stream()
                            .filter(c -> c.getConnectionServiceWrapper().equals(connectionService))
                            .forEach(c -> c.disconnect("release " +
                                    connectionService.getComponentName().getPackageName()));
                }

                @Override
                public void setCallsManagerListener(CallsManagerListener listener) {
                    mListeners.add(listener);
                }
            };

    private boolean mCanAddCall = true;

    private int mMaxNumberOfSimultaneouslyActiveSims = -1;

    private Runnable mStopTone;

    private LinkedList<HandlerThread> mGraphHandlerThreads;

    private boolean mHasActiveRttCall = false;

    /**
     * Listener to PhoneAccountRegistrar events.
     */
    private PhoneAccountRegistrar.Listener mPhoneAccountListener =
            new PhoneAccountRegistrar.Listener() {
        public void onPhoneAccountRegistered(PhoneAccountRegistrar registrar,
                                             PhoneAccountHandle handle) {
            broadcastRegisterIntent(handle);
        }
        public void onPhoneAccountUnRegistered(PhoneAccountRegistrar registrar,
                                               PhoneAccountHandle handle) {
            broadcastUnregisterIntent(handle);
        }

        @Override
        public void onPhoneAccountChanged(PhoneAccountRegistrar registrar,
                PhoneAccount phoneAccount) {
            handlePhoneAccountChanged(registrar, phoneAccount);
        }
    };

    /**
     * Receiver for enhanced call blocking feature to update the emergency call notification
     * in below cases:
     *  1) Carrier config changed.
     *  2) Blocking suppression state changed.
     */
    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            Log.startSession("CM.CCCR");
            String action = intent.getAction();
            if (CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED.equals(action)
                    || SystemContract.ACTION_BLOCK_SUPPRESSION_STATE_CHANGED.equals(action)) {
                new UpdateEmergencyCallNotificationTask().doInBackground(
                        Pair.create(context, Log.createSubsession()));
            }
        }
    };

    private static class UpdateEmergencyCallNotificationTask
            extends AsyncTask<Pair<Context, Session>, Void, Void> {
        @SafeVarargs
        @Override
        protected final Void doInBackground(Pair<Context, Session>... args) {
            if (args == null || args.length != 1 || args[0] == null) {
                Log.e(this, new IllegalArgumentException(), "Incorrect invocation");
                return null;
            }
            Log.continueSession(args[0].second, "CM.UECNT");
            Context context = args[0].first;
            BlockedNumbersUtil.updateEmergencyCallNotification(context,
                    SystemContract.shouldShowEmergencyCallNotification(context));
            Log.endSession();
            return null;
        }
    }

    /**
     * Initializes the required Telecom components.
     */
    @VisibleForTesting
    public CallsManager(
            Context context,
            TelecomSystem.SyncRoot lock,
            CallerInfoLookupHelper callerInfoLookupHelper,
            MissedCallNotifier missedCallNotifier,
            DisconnectedCallNotifier.Factory disconnectedCallNotifierFactory,
            PhoneAccountRegistrar phoneAccountRegistrar,
            HeadsetMediaButtonFactory headsetMediaButtonFactory,
            ProximitySensorManagerFactory proximitySensorManagerFactory,
            InCallWakeLockControllerFactory inCallWakeLockControllerFactory,
            ConnectionServiceFocusManager.ConnectionServiceFocusManagerFactory
                    connectionServiceFocusManagerFactory,
            CallAudioManager.AudioServiceFactory audioServiceFactory,
            BluetoothRouteManager bluetoothManager,
            WiredHeadsetManager wiredHeadsetManager,
            SystemStateHelper systemStateHelper,
            DefaultDialerCache defaultDialerCache,
            Timeouts.Adapter timeoutsAdapter,
            AsyncRingtonePlayer asyncRingtonePlayer,
            PhoneNumberUtilsAdapter phoneNumberUtilsAdapter,
            EmergencyCallHelper emergencyCallHelper,
            InCallTonePlayer.ToneGeneratorFactory toneGeneratorFactory,
            ClockProxy clockProxy,
            AudioProcessingNotification audioProcessingNotification,
            BluetoothStateReceiver bluetoothStateReceiver,
            CallAudioRouteStateMachine.Factory callAudioRouteStateMachineFactory,
            CallAudioModeStateMachine.Factory callAudioModeStateMachineFactory,
            InCallControllerFactory inCallControllerFactory,
            RoleManagerAdapter roleManagerAdapter,
            IncomingCallFilter.Factory incomingCallFilterFactory,
            ToastFactory toastFactory) {
        mContext = context;
        mLock = lock;
        mPhoneNumberUtilsAdapter = phoneNumberUtilsAdapter;
        mPhoneAccountRegistrar = phoneAccountRegistrar;
        mPhoneAccountRegistrar.addListener(mPhoneAccountListener);
        mMissedCallNotifier = missedCallNotifier;
        mDisconnectedCallNotifier = disconnectedCallNotifierFactory.create(mContext, this);
        StatusBarNotifier statusBarNotifier = new StatusBarNotifier(context, this);
        mWiredHeadsetManager = wiredHeadsetManager;
        mSystemStateHelper = systemStateHelper;
        mDefaultDialerCache = defaultDialerCache;
        mBluetoothRouteManager = bluetoothManager;
        mDockManager = new DockManager(context);
        mTimeoutsAdapter = timeoutsAdapter;
        mEmergencyCallHelper = emergencyCallHelper;
        mCallerInfoLookupHelper = callerInfoLookupHelper;
        mIncomingCallFilterFactory = incomingCallFilterFactory;

        mDtmfLocalTonePlayer =
                new DtmfLocalTonePlayer(new DtmfLocalTonePlayer.ToneGeneratorProxy());
        CallAudioRouteStateMachine callAudioRouteStateMachine =
                callAudioRouteStateMachineFactory.create(
                        context,
                        this,
                        bluetoothManager,
                        wiredHeadsetManager,
                        statusBarNotifier,
                        audioServiceFactory,
                        CallAudioRouteStateMachine.EARPIECE_AUTO_DETECT
                );
        callAudioRouteStateMachine.initialize();

        CallAudioRoutePeripheralAdapter callAudioRoutePeripheralAdapter =
                new CallAudioRoutePeripheralAdapter(
                        callAudioRouteStateMachine,
                        bluetoothManager,
                        wiredHeadsetManager,
                        mDockManager);

        AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        InCallTonePlayer.MediaPlayerFactory mediaPlayerFactory =
                (resourceId, attributes) ->
                        new InCallTonePlayer.MediaPlayerAdapterImpl(
                                MediaPlayer.create(mContext, resourceId, attributes,
                                        audioManager.generateAudioSessionId()));
        InCallTonePlayer.Factory playerFactory = new InCallTonePlayer.Factory(
                callAudioRoutePeripheralAdapter, lock, toneGeneratorFactory, mediaPlayerFactory,
                () -> audioManager.getStreamVolume(AudioManager.STREAM_RING) > 0);

        SystemSettingsUtil systemSettingsUtil = new SystemSettingsUtil();
        RingtoneFactory ringtoneFactory = new RingtoneFactory(this, context);
        SystemVibrator systemVibrator = new SystemVibrator(context);
        mInCallController = inCallControllerFactory.create(context, mLock, this,
                systemStateHelper, defaultDialerCache, mTimeoutsAdapter,
                emergencyCallHelper);
        mRinger = new Ringer(playerFactory, context, systemSettingsUtil, asyncRingtonePlayer,
                ringtoneFactory, systemVibrator,
                new Ringer.VibrationEffectProxy(), mInCallController);
        mCallRecordingTonePlayer = new CallRecordingTonePlayer(mContext, audioManager,
                mTimeoutsAdapter, mLock);
        mCallAudioManager = new CallAudioManager(callAudioRouteStateMachine,
                this, callAudioModeStateMachineFactory.create(systemStateHelper,
                (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE)),
                playerFactory, mRinger, new RingbackPlayer(playerFactory),
                bluetoothStateReceiver, mDtmfLocalTonePlayer);

        mConnectionSvrFocusMgr = connectionServiceFocusManagerFactory.create(mRequester);
        mHeadsetMediaButton = headsetMediaButtonFactory.create(context, this, mLock);
        mTtyManager = new TtyManager(context, mWiredHeadsetManager);
        mProximitySensorManager = proximitySensorManagerFactory.create(context, this);
        mPhoneStateBroadcaster = new PhoneStateBroadcaster(this);
        mCallLogManager = new CallLogManager(context, phoneAccountRegistrar, mMissedCallNotifier);
        mConnectionServiceRepository =
                new ConnectionServiceRepository(mPhoneAccountRegistrar, mContext, mLock, this);
        mInCallWakeLockController = inCallWakeLockControllerFactory.create(context, this);
        mClockProxy = clockProxy;
        mToastFactory = toastFactory;
        mRoleManagerAdapter = roleManagerAdapter;

        mListeners.add(mInCallWakeLockController);
        mListeners.add(statusBarNotifier);
        mListeners.add(mCallLogManager);
        mListeners.add(mPhoneStateBroadcaster);
        mListeners.add(mInCallController);
        mListeners.add(mCallAudioManager);
        mListeners.add(mCallRecordingTonePlayer);
        mListeners.add(missedCallNotifier);
        mListeners.add(mDisconnectedCallNotifier);
        mListeners.add(mHeadsetMediaButton);
        mListeners.add(mProximitySensorManager);
        mListeners.add(audioProcessingNotification);

        // There is no USER_SWITCHED broadcast for user 0, handle it here explicitly.
        final UserManager userManager = UserManager.get(mContext);
        // Don't load missed call if it is run in split user model.
        if (userManager.isPrimaryUser()) {
            onUserSwitch(Process.myUserHandle());
        }
        // Register BroadcastReceiver to handle enhanced call blocking feature related event.
        IntentFilter intentFilter = new IntentFilter(
                CarrierConfigManager.ACTION_CARRIER_CONFIG_CHANGED);
        intentFilter.addAction(SystemContract.ACTION_BLOCK_SUPPRESSION_STATE_CHANGED);
        context.registerReceiver(mReceiver, intentFilter);
        mGraphHandlerThreads = new LinkedList<>();
    }

    public void setIncomingCallNotifier(IncomingCallNotifier incomingCallNotifier) {
        if (mIncomingCallNotifier != null) {
            mListeners.remove(mIncomingCallNotifier);
        }
        mIncomingCallNotifier = incomingCallNotifier;
        mListeners.add(mIncomingCallNotifier);
    }

    public void setRespondViaSmsManager(RespondViaSmsManager respondViaSmsManager) {
        if (mRespondViaSmsManager != null) {
            mListeners.remove(mRespondViaSmsManager);
        }
        mRespondViaSmsManager = respondViaSmsManager;
        mListeners.add(respondViaSmsManager);
    }

    public RespondViaSmsManager getRespondViaSmsManager() {
        return mRespondViaSmsManager;
    }

    public CallerInfoLookupHelper getCallerInfoLookupHelper() {
        return mCallerInfoLookupHelper;
    }

    public RoleManagerAdapter getRoleManagerAdapter() {
        return mRoleManagerAdapter;
    }

    @Override
    public void onSuccessfulOutgoingCall(Call call, int callState) {
        Log.v(this, "onSuccessfulOutgoingCall, %s", call);
        call.setPostCallPackageName(getRoleManagerAdapter().getDefaultCallScreeningApp());

        setCallState(call, callState, "successful outgoing call");
        if (!mCalls.contains(call)) {
            // Call was not added previously in startOutgoingCall due to it being a potential MMI
            // code, so add it now.
            addCall(call);
        }

        // The call's ConnectionService has been updated.
        for (CallsManagerListener listener : mListeners) {
            listener.onConnectionServiceChanged(call, null, call.getConnectionService());
        }

        markCallAsDialing(call);
    }

    @Override
    public void onFailedOutgoingCall(Call call, DisconnectCause disconnectCause) {
        Log.v(this, "onFailedOutgoingCall, call: %s", call);

        markCallAsRemoved(call);
    }

    @Override
    public void onSuccessfulIncomingCall(Call incomingCall) {
        Log.d(this, "onSuccessfulIncomingCall");
        PhoneAccount phoneAccount = mPhoneAccountRegistrar.getPhoneAccountUnchecked(
                incomingCall.getTargetPhoneAccount());
        Bundle extras =
            phoneAccount == null || phoneAccount.getExtras() == null
                ? new Bundle()
                : phoneAccount.getExtras();
        if (incomingCall.hasProperty(Connection.PROPERTY_EMERGENCY_CALLBACK_MODE) ||
                incomingCall.isSelfManaged() ||
                extras.getBoolean(PhoneAccount.EXTRA_SKIP_CALL_FILTERING)) {
            Log.i(this, "Skipping call filtering for %s (ecm=%b, selfMgd=%b, skipExtra=%b)",
                    incomingCall.getId(),
                    incomingCall.hasProperty(Connection.PROPERTY_EMERGENCY_CALLBACK_MODE),
                    incomingCall.isSelfManaged(),
                    extras.getBoolean(PhoneAccount.EXTRA_SKIP_CALL_FILTERING));
            onCallFilteringComplete(incomingCall, new Builder()
                    .setShouldAllowCall(true)
                    .setShouldReject(false)
                    .setShouldAddToCallLog(true)
                    .setShouldShowNotification(true)
                    .build());
            incomingCall.setIsUsingCallFiltering(false);
            return;
        }

        IncomingCallFilterGraph graph = setUpCallFilterGraph(incomingCall);
        graph.performFiltering();
    }

    private IncomingCallFilterGraph setUpCallFilterGraph(Call incomingCall) {
        incomingCall.setIsUsingCallFiltering(true);
        String carrierPackageName = getCarrierPackageName();
        String defaultDialerPackageName = TelecomManager.from(mContext).getDefaultDialerPackage();
        String userChosenPackageName = getRoleManagerAdapter().getDefaultCallScreeningApp();
        AppLabelProxy appLabelProxy = packageName -> AppLabelProxy.Util.getAppLabel(
                mContext.getPackageManager(), packageName);
        ParcelableCallUtils.Converter converter = new ParcelableCallUtils.Converter();

        IncomingCallFilterGraph graph = new IncomingCallFilterGraph(incomingCall,
                this::onCallFilteringComplete, mContext, mTimeoutsAdapter, mLock);
        DirectToVoicemailFilter voicemailFilter = new DirectToVoicemailFilter(incomingCall,
                mCallerInfoLookupHelper);
        BlockCheckerFilter blockCheckerFilter = new BlockCheckerFilter(mContext, incomingCall,
                mCallerInfoLookupHelper, new BlockCheckerAdapter());
        CallScreeningServiceFilter carrierCallScreeningServiceFilter =
                new CallScreeningServiceFilter(incomingCall, carrierPackageName,
                        CallScreeningServiceFilter.PACKAGE_TYPE_CARRIER, mContext, this,
                        appLabelProxy, converter);
        CallScreeningServiceFilter callScreeningServiceFilter;
        if ((userChosenPackageName != null)
                && (!userChosenPackageName.equals(defaultDialerPackageName))) {
            callScreeningServiceFilter = new CallScreeningServiceFilter(incomingCall,
                    userChosenPackageName, CallScreeningServiceFilter.PACKAGE_TYPE_USER_CHOSEN,
                    mContext, this, appLabelProxy, converter);
        } else {
            callScreeningServiceFilter = new CallScreeningServiceFilter(incomingCall,
                    defaultDialerPackageName,
                    CallScreeningServiceFilter.PACKAGE_TYPE_DEFAULT_DIALER,
                    mContext, this, appLabelProxy, converter);
        }
        graph.addFilter(voicemailFilter);
        graph.addFilter(blockCheckerFilter);
        graph.addFilter(carrierCallScreeningServiceFilter);
        graph.addFilter(callScreeningServiceFilter);
        IncomingCallFilterGraph.addEdge(voicemailFilter, carrierCallScreeningServiceFilter);
        IncomingCallFilterGraph.addEdge(blockCheckerFilter, carrierCallScreeningServiceFilter);
        IncomingCallFilterGraph.addEdge(carrierCallScreeningServiceFilter,
                callScreeningServiceFilter);
        mGraphHandlerThreads.add(graph.getHandlerThread());
        return graph;
    }

    private String getCarrierPackageName() {
        ComponentName componentName = null;
        CarrierConfigManager configManager = (CarrierConfigManager) mContext.getSystemService
                (Context.CARRIER_CONFIG_SERVICE);
        PersistableBundle configBundle = configManager.getConfig();
        if (configBundle != null) {
            componentName = ComponentName.unflattenFromString(configBundle.getString
                    (CarrierConfigManager.KEY_CARRIER_CALL_SCREENING_APP_STRING, ""));
        }

        return componentName != null ? componentName.getPackageName() : null;
    }

    @Override
    public void onCallFilteringComplete(Call incomingCall, CallFilteringResult result) {
        // Only set the incoming call as ringing if it isn't already disconnected. It is possible
        // that the connection service disconnected the call before it was even added to Telecom, in
        // which case it makes no sense to set it back to a ringing state.
        mGraphHandlerThreads.clear();

        if (incomingCall.getState() != CallState.DISCONNECTED &&
                incomingCall.getState() != CallState.DISCONNECTING) {
            setCallState(incomingCall, CallState.RINGING,
                    result.shouldAllowCall ? "successful incoming call" : "blocking call");
        } else {
            Log.i(this, "onCallFilteringCompleted: call already disconnected.");
            return;
        }

        if (result.shouldAllowCall) {
            incomingCall.setPostCallPackageName(
                    getRoleManagerAdapter().getDefaultCallScreeningApp());

            if (hasMaximumManagedRingingCalls(incomingCall)) {
                if (shouldSilenceInsteadOfReject(incomingCall)) {
                    incomingCall.silence();
                } else {
                    Log.i(this, "onCallFilteringCompleted: Call rejected! " +
                            "Exceeds maximum number of ringing calls.");
                    rejectCallAndLog(incomingCall, result);
                }
            } else if (hasMaximumManagedDialingCalls(incomingCall)) {
                if (shouldSilenceInsteadOfReject(incomingCall)) {
                    incomingCall.silence();
                } else {

                    Log.i(this, "onCallFilteringCompleted: Call rejected! Exceeds maximum number of " +
                            "dialing calls.");
                    rejectCallAndLog(incomingCall, result);
                }
            } else if (result.shouldScreenViaAudio) {
                Log.i(this, "onCallFilteringCompleted: starting background audio processing");
                answerCallForAudioProcessing(incomingCall);
                incomingCall.setAudioProcessingRequestingApp(result.mCallScreeningAppName);
            } else if (result.shouldSilence) {
                Log.i(this, "onCallFilteringCompleted: setting the call to silent ringing state");
                incomingCall.setSilentRingingRequested(true);
                addCall(incomingCall);
            } else {
                addCall(incomingCall);
            }
        } else {
            if (result.shouldReject) {
                Log.i(this, "onCallFilteringCompleted: blocked call, rejecting.");
                incomingCall.reject(false, null);
            }
            if (result.shouldAddToCallLog) {
                Log.i(this, "onCallScreeningCompleted: blocked call, adding to call log.");
                if (result.shouldShowNotification) {
                    Log.w(this, "onCallScreeningCompleted: blocked call, showing notification.");
                }
                mCallLogManager.logCall(incomingCall, Calls.BLOCKED_TYPE,
                        result.shouldShowNotification, result);
            } else if (result.shouldShowNotification) {
                Log.i(this, "onCallScreeningCompleted: blocked call, showing notification.");
                mMissedCallNotifier.showMissedCallNotification(
                        new MissedCallNotifier.CallInfo(incomingCall));
            }
        }
    }

    /**
     * In the event that the maximum supported calls of a given type is reached, the
     * default behavior is to reject any additional calls of that type.  This checks
     * if the device is configured to silence instead of reject the call, provided
     * that the incoming call is from a different source (connection service).
     */
    private boolean shouldSilenceInsteadOfReject(Call incomingCall) {
        if (!mContext.getResources().getBoolean(
                R.bool.silence_incoming_when_different_service_and_maximum_ringing)) {
            return false;
        }

        for (Call call : mCalls) {
            // Only operate on top-level calls
            if (call.getParentCall() != null) {
                continue;
            }

            if (call.isExternalCall()) {
                continue;
            }

            if (call.getConnectionService() == incomingCall.getConnectionService()) {
                return false;
            }
        }

        return true;
    }

    @Override
    public void onFailedIncomingCall(Call call) {
        setCallState(call, CallState.DISCONNECTED, "failed incoming call");
        call.removeListener(this);
    }

    @Override
    public void onSuccessfulUnknownCall(Call call, int callState) {
        setCallState(call, callState, "successful unknown call");
        Log.i(this, "onSuccessfulUnknownCall for call %s", call);
        addCall(call);
    }

    @Override
    public void onFailedUnknownCall(Call call) {
        Log.i(this, "onFailedUnknownCall for call %s", call);
        setCallState(call, CallState.DISCONNECTED, "failed unknown call");
        call.removeListener(this);
    }

    @Override
    public void onRingbackRequested(Call call, boolean ringback) {
        for (CallsManagerListener listener : mListeners) {
            listener.onRingbackRequested(call, ringback);
        }
    }

    @Override
    public void onPostDialWait(Call call, String remaining) {
        mInCallController.onPostDialWait(call, remaining);
    }

    @Override
    public void onPostDialChar(final Call call, char nextChar) {
        if (PhoneNumberUtils.is12Key(nextChar)) {
            // Play tone if it is one of the dialpad digits, canceling out the previously queued
            // up stopTone runnable since playing a new tone automatically stops the previous tone.
            if (mStopTone != null) {
                mHandler.removeCallbacks(mStopTone.getRunnableToCancel());
                mStopTone.cancel();
            }

            mDtmfLocalTonePlayer.playTone(call, nextChar);

            mStopTone = new Runnable("CM.oPDC", mLock) {
                @Override
                public void loggedRun() {
                    // Set a timeout to stop the tone in case there isn't another tone to
                    // follow.
                    mDtmfLocalTonePlayer.stopTone(call);
                }
            };
            mHandler.postDelayed(mStopTone.prepare(),
                    Timeouts.getDelayBetweenDtmfTonesMillis(mContext.getContentResolver()));
        } else if (nextChar == 0 || nextChar == TelecomManager.DTMF_CHARACTER_WAIT ||
                nextChar == TelecomManager.DTMF_CHARACTER_PAUSE) {
            // Stop the tone if a tone is playing, removing any other stopTone callbacks since
            // the previous tone is being stopped anyway.
            if (mStopTone != null) {
                mHandler.removeCallbacks(mStopTone.getRunnableToCancel());
                mStopTone.cancel();
            }
            mDtmfLocalTonePlayer.stopTone(call);
        } else {
            Log.w(this, "onPostDialChar: invalid value %d", nextChar);
        }
    }

    @Override
    public void onConnectionPropertiesChanged(Call call, boolean didRttChange) {
        if (didRttChange) {
            updateHasActiveRttCall();
        }
    }

    @Override
    public void onParentChanged(Call call) {
        // parent-child relationship affects which call should be foreground, so do an update.
        updateCanAddCall();
        for (CallsManagerListener listener : mListeners) {
            listener.onIsConferencedChanged(call);
        }
    }

    @Override
    public void onChildrenChanged(Call call) {
        // parent-child relationship affects which call should be foreground, so do an update.
        updateCanAddCall();
        for (CallsManagerListener listener : mListeners) {
            listener.onIsConferencedChanged(call);
        }
    }

    @Override
    public void onConferenceStateChanged(Call call, boolean isConference) {
        // Conference changed whether it is treated as a conference or not.
        updateCanAddCall();
        for (CallsManagerListener listener : mListeners) {
            listener.onConferenceStateChanged(call, isConference);
        }
    }

    @Override
    public void onCdmaConferenceSwap(Call call) {
        // SWAP was executed on a CDMA conference
        for (CallsManagerListener listener : mListeners) {
            listener.onCdmaConferenceSwap(call);
        }
    }

    @Override
    public void onIsVoipAudioModeChanged(Call call) {
        for (CallsManagerListener listener : mListeners) {
            listener.onIsVoipAudioModeChanged(call);
        }
    }

    @Override
    public void onVideoStateChanged(Call call, int previousVideoState, int newVideoState) {
        for (CallsManagerListener listener : mListeners) {
            listener.onVideoStateChanged(call, previousVideoState, newVideoState);
        }
    }

    @Override
    public boolean onCanceledViaNewOutgoingCallBroadcast(final Call call,
            long disconnectionTimeout) {
        mPendingCallsToDisconnect.add(call);
        mHandler.postDelayed(new Runnable("CM.oCVNOCB", mLock) {
            @Override
            public void loggedRun() {
                if (mPendingCallsToDisconnect.remove(call)) {
                    Log.i(this, "Delayed disconnection of call: %s", call);
                    call.disconnect();
                }
            }
        }.prepare(), disconnectionTimeout);

        return true;
    }

    /**
     * Handles changes to the {@link Connection.VideoProvider} for a call.  Adds the
     * {@link CallsManager} as a listener for the {@link VideoProviderProxy} which is created
     * in {@link Call#setVideoProvider(IVideoProvider)}.  This allows the {@link CallsManager} to
     * respond to callbacks from the {@link VideoProviderProxy}.
     *
     * @param call The call.
     */
    @Override
    public void onVideoCallProviderChanged(Call call) {
        VideoProviderProxy videoProviderProxy = call.getVideoProviderProxy();

        if (videoProviderProxy == null) {
            return;
        }

        videoProviderProxy.addListener(this);
    }

    /**
     * Handles session modification requests received via the {@link TelecomVideoCallCallback} for
     * a call.  Notifies listeners of the {@link CallsManager.CallsManagerListener} of the session
     * modification request.
     *
     * @param call The call.
     * @param videoProfile The {@link VideoProfile}.
     */
    @Override
    public void onSessionModifyRequestReceived(Call call, VideoProfile videoProfile) {
        int videoState = videoProfile != null ? videoProfile.getVideoState() :
                VideoProfile.STATE_AUDIO_ONLY;
        Log.v(TAG, "onSessionModifyRequestReceived : videoProfile = " + VideoProfile
                .videoStateToString(videoState));

        for (CallsManagerListener listener : mListeners) {
            listener.onSessionModifyRequestReceived(call, videoProfile);
        }
    }

    /**
     * Handles a change to the currently active camera for a call by notifying listeners.
     * @param call The call.
     * @param cameraId The ID of the camera in use, or {@code null} if no camera is in use.
     */
    @Override
    public void onSetCamera(Call call, String cameraId) {
        for (CallsManagerListener listener : mListeners) {
            listener.onSetCamera(call, cameraId);
        }
    }

    public Collection<Call> getCalls() {
        return Collections.unmodifiableCollection(mCalls);
    }

    /**
     * Play or stop a call hold tone for a call.  Triggered via
     * {@link Connection#sendConnectionEvent(String)} when the
     * {@link Connection#EVENT_ON_HOLD_TONE_START} event or
     * {@link Connection#EVENT_ON_HOLD_TONE_STOP} event is passed through to the
     *
     * @param call The call which requested the hold tone.
     */
    @Override
    public void onHoldToneRequested(Call call) {
        for (CallsManagerListener listener : mListeners) {
            listener.onHoldToneRequested(call);
        }
    }

    /**
     * A {@link Call} managed by the {@link CallsManager} has requested a handover to another
     * {@link PhoneAccount}.
     * @param call The call.
     * @param handoverTo The {@link PhoneAccountHandle} to handover the call to.
     * @param videoState The desired video state of the call after handover.
     * @param extras
     */
    @Override
    public void onHandoverRequested(Call call, PhoneAccountHandle handoverTo, int videoState,
                                    Bundle extras, boolean isLegacy) {
        if (isLegacy) {
            requestHandoverViaEvents(call, handoverTo, videoState, extras);
        } else {
            requestHandover(call, handoverTo, videoState, extras);
        }
    }

    @VisibleForTesting
    public Call getForegroundCall() {
        if (mCallAudioManager == null) {
            // Happens when getForegroundCall is called before full initialization.
            return null;
        }
        return mCallAudioManager.getForegroundCall();
    }

    @Override
    public void onCallHoldFailed(Call call) {
        markAllAnsweredCallAsRinging(call, "hold");
    }

    @Override
    public void onCallSwitchFailed(Call call) {
        markAllAnsweredCallAsRinging(call, "switch");
    }

    private void markAllAnsweredCallAsRinging(Call call, String actionName) {
        // Normally, we don't care whether a call hold or switch has failed.
        // However, if a call was held or switched in order to answer an incoming call, that
        // incoming call needs to be brought out of the ANSWERED state so that the user can
        // try the operation again.
        for (Call call1 : mCalls) {
            if (call1 != call && call1.getState() == CallState.ANSWERED) {
                setCallState(call1, CallState.RINGING, actionName + " failed on other call");
            }
        }
    }

    @Override
    public UserHandle getCurrentUserHandle() {
        return mCurrentUserHandle;
    }

    public CallAudioManager getCallAudioManager() {
        return mCallAudioManager;
    }

    InCallController getInCallController() {
        return mInCallController;
    }

    EmergencyCallHelper getEmergencyCallHelper() {
        return mEmergencyCallHelper;
    }

    public DefaultDialerCache getDefaultDialerCache() {
        return mDefaultDialerCache;
    }

    @VisibleForTesting
    public PhoneAccountRegistrar.Listener getPhoneAccountListener() {
        return mPhoneAccountListener;
    }

    public boolean hasEmergencyRttCall() {
        for (Call call : mCalls) {
            if (call.isEmergencyCall() && call.isRttCall()) {
                return true;
            }
        }
        return false;
    }

    @VisibleForTesting
    public boolean hasOnlyDisconnectedCalls() {
        if (mCalls.size() == 0) {
            return false;
        }
        for (Call call : mCalls) {
            if (!call.isDisconnected()) {
                return false;
            }
        }
        return true;
    }

    public boolean hasVideoCall() {
        for (Call call : mCalls) {
            if (VideoProfile.isVideo(call.getVideoState())) {
                return true;
            }
        }
        return false;
    }

    @VisibleForTesting
    public CallAudioState getAudioState() {
        return mCallAudioManager.getCallAudioState();
    }

    boolean isTtySupported() {
        return mTtyManager.isTtySupported();
    }

    int getCurrentTtyMode() {
        return mTtyManager.getCurrentTtyMode();
    }

    @VisibleForTesting
    public void addListener(CallsManagerListener listener) {
        mListeners.add(listener);
    }

    @VisibleForTesting
    public void removeListener(CallsManagerListener listener) {
        mListeners.remove(listener);
    }

    void processIncomingConference(PhoneAccountHandle phoneAccountHandle, Bundle extras) {
        Log.d(this, "processIncomingCallConference");
        processIncomingCallIntent(phoneAccountHandle, extras, true);
    }

    /**
     * Starts the process to attach the call to a connection service.
     *
     * @param phoneAccountHandle The phone account which contains the component name of the
     *        connection service to use for this call.
     * @param extras The optional extras Bundle passed with the intent used for the incoming call.
     */
    void processIncomingCallIntent(PhoneAccountHandle phoneAccountHandle, Bundle extras) {
        processIncomingCallIntent(phoneAccountHandle, extras, false);
    }

    void processIncomingCallIntent(PhoneAccountHandle phoneAccountHandle, Bundle extras,
        boolean isConference) {
        Log.d(this, "processIncomingCallIntent");
        boolean isHandover = extras.getBoolean(TelecomManager.EXTRA_IS_HANDOVER);
        Uri handle = extras.getParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS);
        if (handle == null) {
            // Required for backwards compatibility
            handle = extras.getParcelable(TelephonyManager.EXTRA_INCOMING_NUMBER);
        }
        Call call = new Call(
                getNextCallId(),
                mContext,
                this,
                mLock,
                mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                handle,
                null /* gatewayInfo */,
                null /* connectionManagerPhoneAccount */,
                phoneAccountHandle,
                Call.CALL_DIRECTION_INCOMING /* callDirection */,
                false /* forceAttachToExistingConnection */,
                isConference, /* isConference */
                mClockProxy,
                mToastFactory);

        // Ensure new calls related to self-managed calls/connections are set as such.  This will
        // be overridden when the actual connection is returned in startCreateConnection, however
        // doing this now ensures the logs and any other logic will treat this call as self-managed
        // from the moment it is created.
        PhoneAccount phoneAccount = mPhoneAccountRegistrar.getPhoneAccountUnchecked(
                phoneAccountHandle);
        if (phoneAccount != null) {
            call.setIsSelfManaged(phoneAccount.isSelfManaged());
            if (call.isSelfManaged()) {
                // Self managed calls will always be voip audio mode.
                call.setIsVoipAudioMode(true);
            } else {
                // Incoming call is managed, the active call is self-managed and can't be held.
                // We need to set extras on it to indicate whether answering will cause a 
                // active self-managed call to drop.
                Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
                if (activeCall != null && !canHold(activeCall) && activeCall.isSelfManaged()) {
                    Bundle dropCallExtras = new Bundle();
                    dropCallExtras.putBoolean(Connection.EXTRA_ANSWERING_DROPS_FG_CALL, true);

                    // Include the name of the app which will drop the call.
                    CharSequence droppedApp = activeCall.getTargetPhoneAccountLabel();
                    dropCallExtras.putCharSequence(
                            Connection.EXTRA_ANSWERING_DROPS_FG_CALL_APP_NAME, droppedApp);
                    Log.i(this, "Incoming managed call will drop %s call.", droppedApp);
                    call.putExtras(Call.SOURCE_CONNECTION_SERVICE, dropCallExtras);
                }
            }

            Bundle phoneAccountExtras = phoneAccount.getExtras();
            if (phoneAccountExtras != null
                    && phoneAccountExtras.getBoolean(
                            PhoneAccount.EXTRA_ALWAYS_USE_VOIP_AUDIO_MODE)) {
                Log.d(this, "processIncomingCallIntent: defaulting to voip mode for call %s",
                        call.getId());
                call.setIsVoipAudioMode(true);
            }
        }

        boolean isRttSettingOn = isRttSettingOn(phoneAccountHandle);
        if (isRttSettingOn ||
                extras.getBoolean(TelecomManager.EXTRA_START_CALL_WITH_RTT, false)) {
            Log.i(this, "Incoming call requesting RTT, rtt setting is %b", isRttSettingOn);
            call.createRttStreams();
            // Even if the phone account doesn't support RTT yet, the connection manager might
            // change that. Set this to check it later.
            call.setRequestedToStartWithRtt();
        }
        // If the extras specifies a video state, set it on the call if the PhoneAccount supports
        // video.
        int videoState = VideoProfile.STATE_AUDIO_ONLY;
        if (extras.containsKey(TelecomManager.EXTRA_INCOMING_VIDEO_STATE) &&
                phoneAccount != null && phoneAccount.hasCapabilities(
                        PhoneAccount.CAPABILITY_VIDEO_CALLING)) {
            videoState = extras.getInt(TelecomManager.EXTRA_INCOMING_VIDEO_STATE);
            call.setVideoState(videoState);
        }

        call.initAnalytics();
        if (getForegroundCall() != null) {
            getForegroundCall().getAnalytics().setCallIsInterrupted(true);
            call.getAnalytics().setCallIsAdditional(true);
        }
        setIntentExtrasAndStartTime(call, extras);
        // TODO: Move this to be a part of addCall()
        call.addListener(this);

        if (extras.containsKey(TelecomManager.EXTRA_CALL_DISCONNECT_MESSAGE)) {
          String disconnectMessage = extras.getString(TelecomManager.EXTRA_CALL_DISCONNECT_MESSAGE);
          Log.i(this, "processIncomingCallIntent Disconnect message " + disconnectMessage);
        }

        boolean isHandoverAllowed = true;
        if (isHandover) {
            if (!isHandoverInProgress() &&
                    isHandoverToPhoneAccountSupported(phoneAccountHandle)) {
                final String handleScheme = handle.getSchemeSpecificPart();
                Call fromCall = mCalls.stream()
                        .filter((c) -> mPhoneNumberUtilsAdapter.isSamePhoneNumber(
                                (c.getHandle() == null
                                        ? null : c.getHandle().getSchemeSpecificPart()),
                                handleScheme))
                        .findFirst()
                        .orElse(null);
                if (fromCall != null) {
                    if (!isHandoverFromPhoneAccountSupported(fromCall.getTargetPhoneAccount())) {
                        Log.w(this, "processIncomingCallIntent: From account doesn't support " +
                                "handover.");
                        isHandoverAllowed = false;
                    }
                } else {
                    Log.w(this, "processIncomingCallIntent: handover fail; can't find from call.");
                    isHandoverAllowed = false;
                }

                if (isHandoverAllowed) {
                    // Link the calls so we know we're handing over.
                    fromCall.setHandoverDestinationCall(call);
                    call.setHandoverSourceCall(fromCall);
                    call.setHandoverState(HandoverState.HANDOVER_TO_STARTED);
                    fromCall.setHandoverState(HandoverState.HANDOVER_FROM_STARTED);
                    Log.addEvent(fromCall, LogUtils.Events.START_HANDOVER,
                            "handOverFrom=%s, handOverTo=%s", fromCall.getId(), call.getId());
                    Log.addEvent(call, LogUtils.Events.START_HANDOVER,
                            "handOverFrom=%s, handOverTo=%s", fromCall.getId(), call.getId());
                    if (isSpeakerEnabledForVideoCalls() && VideoProfile.isVideo(videoState)) {
                        // Ensure when the call goes active that it will go to speakerphone if the
                        // handover to call is a video call.
                        call.setStartWithSpeakerphoneOn(true);
                    }
                }
            } else {
                Log.w(this, "processIncomingCallIntent: To account doesn't support handover.");
            }
        }

        if (!isHandoverAllowed || (call.isSelfManaged() && !isIncomingCallPermitted(call,
                call.getTargetPhoneAccount()))) {
            if (isConference) {
                notifyCreateConferenceFailed(phoneAccountHandle, call);
            } else {
                notifyCreateConnectionFailed(phoneAccountHandle, call);
            }
        } else if (isInEmergencyCall()) {
            // The incoming call is implicitly being rejected so the user does not get any incoming
            // call UI during an emergency call. In this case, log the call as missed instead of
            // rejected since the user did not explicitly reject.
            mCallLogManager.logCall(call, Calls.MISSED_TYPE,
                    true /*showNotificationForMissedCall*/, null /*CallFilteringResult*/);
            if (isConference) {
                notifyCreateConferenceFailed(phoneAccountHandle, call);
            } else {
                notifyCreateConnectionFailed(phoneAccountHandle, call);
            }
        } else {
            call.startCreateConnection(mPhoneAccountRegistrar);
        }
    }

    void addNewUnknownCall(PhoneAccountHandle phoneAccountHandle, Bundle extras) {
        Uri handle = extras.getParcelable(TelecomManager.EXTRA_UNKNOWN_CALL_HANDLE);
        Log.i(this, "addNewUnknownCall with handle: %s", Log.pii(handle));
        Call call = new Call(
                getNextCallId(),
                mContext,
                this,
                mLock,
                mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                handle,
                null /* gatewayInfo */,
                null /* connectionManagerPhoneAccount */,
                phoneAccountHandle,
                Call.CALL_DIRECTION_UNKNOWN /* callDirection */,
                // Use onCreateIncomingConnection in TelephonyConnectionService, so that we attach
                // to the existing connection instead of trying to create a new one.
                true /* forceAttachToExistingConnection */,
                false, /* isConference */
                mClockProxy,
                mToastFactory);
        call.initAnalytics();

        setIntentExtrasAndStartTime(call, extras);
        call.addListener(this);
        call.startCreateConnection(mPhoneAccountRegistrar);
    }

    private boolean areHandlesEqual(Uri handle1, Uri handle2) {
        if (handle1 == null || handle2 == null) {
            return handle1 == handle2;
        }

        if (!TextUtils.equals(handle1.getScheme(), handle2.getScheme())) {
            return false;
        }

        final String number1 = PhoneNumberUtils.normalizeNumber(handle1.getSchemeSpecificPart());
        final String number2 = PhoneNumberUtils.normalizeNumber(handle2.getSchemeSpecificPart());
        return TextUtils.equals(number1, number2);
    }

    private Call reuseOutgoingCall(Uri handle) {
        // Check to see if we can reuse any of the calls that are waiting to disconnect.
        // See {@link Call#abort} and {@link #onCanceledViaNewOutgoingCall} for more information.
        Call reusedCall = null;
        for (Iterator<Call> callIter = mPendingCallsToDisconnect.iterator(); callIter.hasNext();) {
            Call pendingCall = callIter.next();
            if (reusedCall == null && areHandlesEqual(pendingCall.getHandle(), handle)) {
                callIter.remove();
                Log.i(this, "Reusing disconnected call %s", pendingCall);
                reusedCall = pendingCall;
            } else {
                Log.i(this, "Not reusing disconnected call %s", pendingCall);
                callIter.remove();
                pendingCall.disconnect();
            }
        }

        return reusedCall;
    }

    /**
     * Kicks off the first steps to creating an outgoing call.
     *
     * For managed connections, this is the first step to launching the Incall UI.
     * For self-managed connections, we don't expect the Incall UI to launch, but this is still a
     * first step in getting the self-managed ConnectionService to create the connection.
     * @param handle Handle to connect the call with.
     * @param requestedAccountHandle The phone account which contains the component name of the
     *        connection service to use for this call.
     * @param extras The optional extras Bundle passed with the intent used for the incoming call.
     * @param initiatingUser {@link UserHandle} of user that place the outgoing call.
     * @param originalIntent
     * @param callingPackage the package name of the app which initiated the outgoing call.
     */
    @VisibleForTesting
    public @NonNull
    CompletableFuture<Call> startOutgoingCall(Uri handle,
            PhoneAccountHandle requestedAccountHandle,
            Bundle extras, UserHandle initiatingUser, Intent originalIntent,
            String callingPackage) {
        final List<Uri> callee = new ArrayList<>();
        callee.add(handle);
        return startOutgoingCall(callee, requestedAccountHandle, extras, initiatingUser,
                originalIntent, callingPackage, false);
    }

    private CompletableFuture<Call> startOutgoingCall(List<Uri> participants,
            PhoneAccountHandle requestedAccountHandle,
            Bundle extras, UserHandle initiatingUser, Intent originalIntent,
            String callingPackage, boolean isConference) {
        boolean isReusedCall;
        Uri handle = isConference ? Uri.parse("tel:conf-factory") : participants.get(0);
        Call call = reuseOutgoingCall(handle);

        PhoneAccount account =
                mPhoneAccountRegistrar.getPhoneAccount(requestedAccountHandle, initiatingUser);
        boolean isSelfManaged = account != null && account.isSelfManaged();

        // Create a call with original handle. The handle may be changed when the call is attached
        // to a connection service, but in most cases will remain the same.
        if (call == null) {
            call = new Call(getNextCallId(), mContext,
                    this,
                    mLock,
                    mConnectionServiceRepository,
                    mPhoneNumberUtilsAdapter,
                    handle,
                    isConference ? participants : null,
                    null /* gatewayInfo */,
                    null /* connectionManagerPhoneAccount */,
                    null /* requestedAccountHandle */,
                    Call.CALL_DIRECTION_OUTGOING /* callDirection */,
                    false /* forceAttachToExistingConnection */,
                    isConference, /* isConference */
                    mClockProxy,
                    mToastFactory);
            call.initAnalytics(callingPackage);

            // Ensure new calls related to self-managed calls/connections are set as such.  This
            // will be overridden when the actual connection is returned in startCreateConnection,
            // however doing this now ensures the logs and any other logic will treat this call as
            // self-managed from the moment it is created.
            call.setIsSelfManaged(isSelfManaged);
            if (isSelfManaged) {
                // Self-managed calls will ALWAYS use voip audio mode.
                call.setIsVoipAudioMode(true);
            }
            call.setInitiatingUser(initiatingUser);
            isReusedCall = false;
        } else {
            isReusedCall = true;
        }

        int videoState = VideoProfile.STATE_AUDIO_ONLY;
        if (extras != null) {
            // Set the video state on the call early so that when it is added to the InCall UI the
            // UI knows to configure itself as a video call immediately.
            videoState = extras.getInt(TelecomManager.EXTRA_START_CALL_WITH_VIDEO_STATE,
                    VideoProfile.STATE_AUDIO_ONLY);

            // If this is an emergency video call, we need to check if the phone account supports
            // emergency video calling.
            // Also, ensure we don't try to place an outgoing call with video if video is not
            // supported.
            if (VideoProfile.isVideo(videoState)) {
                if (call.isEmergencyCall() && account != null &&
                        !account.hasCapabilities(PhoneAccount.CAPABILITY_EMERGENCY_VIDEO_CALLING)) {
                    // Phone account doesn't support emergency video calling, so fallback to
                    // audio-only now to prevent the InCall UI from setting up video surfaces
                    // needlessly.
                    Log.i(this, "startOutgoingCall - emergency video calls not supported; " +
                            "falling back to audio-only");
                    videoState = VideoProfile.STATE_AUDIO_ONLY;
                } else if (account != null &&
                        !account.hasCapabilities(PhoneAccount.CAPABILITY_VIDEO_CALLING)) {
                    // Phone account doesn't support video calling, so fallback to audio-only.
                    Log.i(this, "startOutgoingCall - video calls not supported; fallback to " +
                            "audio-only.");
                    videoState = VideoProfile.STATE_AUDIO_ONLY;
                }
            }

            call.setVideoState(videoState);
        }

        final int finalVideoState = videoState;
        final Call finalCall = call;
        Handler outgoingCallHandler = new Handler(Looper.getMainLooper());
        // Create a empty CompletableFuture and compose it with findOutgoingPhoneAccount to get
        // a first guess at the list of suitable outgoing PhoneAccounts.
        // findOutgoingPhoneAccount returns a CompletableFuture which is either already complete
        // (in the case where we don't need to do the per-contact lookup) or a CompletableFuture
        // that completes once the contact lookup via CallerInfoLookupHelper is complete.
        CompletableFuture<List<PhoneAccountHandle>> accountsForCall =
                CompletableFuture.completedFuture((Void) null).thenComposeAsync((x) ->
                                findOutgoingCallPhoneAccount(requestedAccountHandle, handle,
                                        VideoProfile.isVideo(finalVideoState),
                                        finalCall.isEmergencyCall(), initiatingUser,
                                        isConference),
                        new LoggedHandlerExecutor(outgoingCallHandler, "CM.fOCP", mLock));

        // This is a block of code that executes after the list of potential phone accts has been
        // retrieved.
        CompletableFuture<List<PhoneAccountHandle>> setAccountHandle =
                accountsForCall.whenCompleteAsync((potentialPhoneAccounts, exception) -> {
                    Log.i(CallsManager.this, "set outgoing call phone acct stage");
                    PhoneAccountHandle phoneAccountHandle;
                    if (potentialPhoneAccounts.size() == 1) {
                        phoneAccountHandle = potentialPhoneAccounts.get(0);
                    } else {
                        phoneAccountHandle = null;
                    }
                    finalCall.setTargetPhoneAccount(phoneAccountHandle);
                }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.sOCPA", mLock));


        // This composes the future containing the potential phone accounts with code that queries
        // the suggestion service if necessary (i.e. if the list is longer than 1).
        // If the suggestion service is queried, the inner lambda will return a future that
        // completes when the suggestion service calls the callback.
        CompletableFuture<List<PhoneAccountSuggestion>> suggestionFuture = accountsForCall.
                thenComposeAsync(potentialPhoneAccounts -> {
                    Log.i(CallsManager.this, "call outgoing call suggestion service stage");
                    if (potentialPhoneAccounts.size() == 1) {
                        PhoneAccountSuggestion suggestion =
                                new PhoneAccountSuggestion(potentialPhoneAccounts.get(0),
                                        PhoneAccountSuggestion.REASON_NONE, true);
                        return CompletableFuture.completedFuture(
                                Collections.singletonList(suggestion));
                    }
                    return PhoneAccountSuggestionHelper.bindAndGetSuggestions(mContext,
                            finalCall.getHandle(), potentialPhoneAccounts);
                }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.cOCSS", mLock));


        // This future checks the status of existing calls and attempts to make room for the
        // outgoing call. The future returned by the inner method will usually be pre-completed --
        // we only pause here if user interaction is required to disconnect a self-managed call.
        // It runs after the account handle is set, independently of the phone account suggestion
        // future.
        CompletableFuture<Call> makeRoomForCall = setAccountHandle.thenComposeAsync(
                potentialPhoneAccounts -> {
                    Log.i(CallsManager.this, "make room for outgoing call stage");
                    if (isPotentialInCallMMICode(handle) && !isSelfManaged) {
                        return CompletableFuture.completedFuture(finalCall);
                    }
                    // If a call is being reused, then it has already passed the
                    // makeRoomForOutgoingCall check once and will fail the second time due to the
                    // call transitioning into the CONNECTING state.
                    if (isReusedCall) {
                        return CompletableFuture.completedFuture(finalCall);
                    }

                    // If we can not supportany more active calls, our options are to move a call
                    // to hold, disconnect a call, or cancel this call altogether.
                    boolean isRoomForCall = finalCall.isEmergencyCall() ?
                            makeRoomForOutgoingEmergencyCall(finalCall) :
                            makeRoomForOutgoingCall(finalCall);
                    if (!isRoomForCall) {
                        Call foregroundCall = getForegroundCall();
                        Log.d(CallsManager.this, "No more room for outgoing call %s ", finalCall);
                        if (foregroundCall.isSelfManaged()) {
                            // If the ongoing call is a self-managed call, then prompt the user to
                            // ask if they'd like to disconnect their ongoing call and place the
                            // outgoing call.
                            Log.i(CallsManager.this, "Prompting user to disconnect "
                                    + "self-managed call");
                            finalCall.setOriginalCallIntent(originalIntent);
                            CompletableFuture<Call> completionFuture = new CompletableFuture<>();
                            startCallConfirmation(finalCall, completionFuture);
                            return completionFuture;
                        } else {
                            // If the ongoing call is a managed call, we will prevent the outgoing
                            // call from dialing.
                            if (isConference) {
                                notifyCreateConferenceFailed(finalCall.getTargetPhoneAccount(),
                                    finalCall);
                            } else {
                                notifyCreateConnectionFailed(
                                        finalCall.getTargetPhoneAccount(), finalCall);
                            }
                        }
                        Log.i(CallsManager.this, "Aborting call since there's no room");
                        return CompletableFuture.completedFuture(null);
                    }
                    return CompletableFuture.completedFuture(finalCall);
        }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.dSMCP", mLock));

        // The outgoing call can be placed, go forward. This future glues together the results of
        // the account suggestion stage and the make room for call stage.
        CompletableFuture<Pair<Call, List<PhoneAccountSuggestion>>> preSelectStage =
                makeRoomForCall.thenCombine(suggestionFuture, Pair::create);
        mLatestPreAccountSelectionFuture = preSelectStage;

        // This future takes the list of suggested accounts and the call and determines if more
        // user interaction in the form of a phone account selection screen is needed. If so, it
        // will set the call to SELECT_PHONE_ACCOUNT, add it to our internal list/send it to dialer,
        // and then execution will pause pending the dialer calling phoneAccountSelected.
        CompletableFuture<Pair<Call, PhoneAccountHandle>> dialerSelectPhoneAccountFuture =
                preSelectStage.thenComposeAsync(
                        (args) -> {
                            Log.i(CallsManager.this, "dialer phone acct select stage");
                            Call callToPlace = args.first;
                            List<PhoneAccountSuggestion> accountSuggestions = args.second;
                            if (callToPlace == null) {
                                return CompletableFuture.completedFuture(null);
                            }
                            if (accountSuggestions == null || accountSuggestions.isEmpty()) {
                                Log.i(CallsManager.this, "Aborting call since there are no"
                                        + " available accounts.");
                                showErrorMessage(R.string.cant_call_due_to_no_supported_service);
                                return CompletableFuture.completedFuture(null);
                            }
                            boolean needsAccountSelection = accountSuggestions.size() > 1
                                    && !callToPlace.isEmergencyCall() && !isSelfManaged;
                            if (!needsAccountSelection) {
                                return CompletableFuture.completedFuture(Pair.create(callToPlace,
                                        accountSuggestions.get(0).getPhoneAccountHandle()));
                            }
                            // This is the state where the user is expected to select an account
                            callToPlace.setState(CallState.SELECT_PHONE_ACCOUNT,
                                    "needs account selection");
                            // Create our own instance to modify (since extras may be Bundle.EMPTY)
                            Bundle newExtras = new Bundle(extras);
                            List<PhoneAccountHandle> accountsFromSuggestions = accountSuggestions
                                    .stream()
                                    .map(PhoneAccountSuggestion::getPhoneAccountHandle)
                                    .collect(Collectors.toList());
                            newExtras.putParcelableList(
                                    android.telecom.Call.AVAILABLE_PHONE_ACCOUNTS,
                                    accountsFromSuggestions);
                            newExtras.putParcelableList(
                                    android.telecom.Call.EXTRA_SUGGESTED_PHONE_ACCOUNTS,
                                    accountSuggestions);
                            // Set a future in place so that we can proceed once the dialer replies.
                            mPendingAccountSelection = new CompletableFuture<>();
                            callToPlace.setIntentExtras(newExtras);

                            addCall(callToPlace);
                            return mPendingAccountSelection;
                        }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.dSPA", mLock));

        // Potentially perform call identification for dialed TEL scheme numbers.
        if (PhoneAccount.SCHEME_TEL.equals(handle.getScheme())) {
            // Perform an asynchronous contacts lookup in this stage; ensure post-dial digits are
            // not included.
            CompletableFuture<Pair<Uri, CallerInfo>> contactLookupFuture =
                    mCallerInfoLookupHelper.startLookup(Uri.fromParts(handle.getScheme(),
                            PhoneNumberUtils.extractNetworkPortion(handle.getSchemeSpecificPart()),
                            null));

            // Once the phone account selection stage has completed, we can handle the results from
            // that with the contacts lookup in order to determine if we should lookup bind to the
            // CallScreeningService in order for it to potentially provide caller ID.
            dialerSelectPhoneAccountFuture.thenAcceptBothAsync(contactLookupFuture,
                    (callPhoneAccountHandlePair, uriCallerInfoPair) -> {
                        Call theCall = callPhoneAccountHandlePair.first;
                        boolean isInContacts = uriCallerInfoPair.second != null
                                && uriCallerInfoPair.second.contactExists;
                        Log.d(CallsManager.this, "outgoingCallIdStage: isInContacts=%s",
                                isInContacts);

                        // We only want to provide a CallScreeningService with a call if its not in
                        // contacts or the package has READ_CONTACT permission.
                        PackageManager packageManager = mContext.getPackageManager();
                        int permission = packageManager.checkPermission(
                                Manifest.permission.READ_CONTACTS,
                                mRoleManagerAdapter.getDefaultCallScreeningApp());
                        Log.d(CallsManager.this,
                                "default call screening service package %s has permissions=%s",
                                mRoleManagerAdapter.getDefaultCallScreeningApp(),
                                permission == PackageManager.PERMISSION_GRANTED);
                        if ((!isInContacts) || (permission == PackageManager.PERMISSION_GRANTED)) {
                            bindForOutgoingCallerId(theCall);
                        }
            }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.pCSB", mLock));
        }

        // Finally, after all user interaction is complete, we execute this code to finish setting
        // up the outgoing call. The inner method always returns a completed future containing the
        // call that we've finished setting up.
        mLatestPostSelectionProcessingFuture = dialerSelectPhoneAccountFuture
                .thenComposeAsync(args -> {
                    if (args == null) {
                        return CompletableFuture.completedFuture(null);
                    }
                    Log.i(CallsManager.this, "post acct selection stage");
                    Call callToUse = args.first;
                    PhoneAccountHandle phoneAccountHandle = args.second;
                    PhoneAccount accountToUse = mPhoneAccountRegistrar
                            .getPhoneAccount(phoneAccountHandle, initiatingUser);
                    callToUse.setTargetPhoneAccount(phoneAccountHandle);
                    if (accountToUse != null && accountToUse.getExtras() != null) {
                        if (accountToUse.getExtras()
                                .getBoolean(PhoneAccount.EXTRA_ALWAYS_USE_VOIP_AUDIO_MODE)) {
                            Log.d(this, "startOutgoingCall: defaulting to voip mode for call %s",
                                    callToUse.getId());
                            callToUse.setIsVoipAudioMode(true);
                        }
                    }

                    callToUse.setState(
                            CallState.CONNECTING,
                            phoneAccountHandle == null ? "no-handle"
                                    : phoneAccountHandle.toString());

                    boolean isVoicemail = isVoicemail(callToUse.getHandle(), accountToUse);

                    boolean isRttSettingOn = isRttSettingOn(phoneAccountHandle);
                    if (!isVoicemail && (isRttSettingOn || (extras != null
                            && extras.getBoolean(TelecomManager.EXTRA_START_CALL_WITH_RTT,
                            false)))) {
                        Log.d(this, "Outgoing call requesting RTT, rtt setting is %b",
                                isRttSettingOn);
                        if (callToUse.isEmergencyCall() || (accountToUse != null
                                && accountToUse.hasCapabilities(PhoneAccount.CAPABILITY_RTT))) {
                            // If the call requested RTT and it's an emergency call, ignore the
                            // capability and hope that the modem will deal with it somehow.
                            callToUse.createRttStreams();
                        }
                        // Even if the phone account doesn't support RTT yet,
                        // the connection manager might change that. Set this to check it later.
                        callToUse.setRequestedToStartWithRtt();
                    }

                    setIntentExtrasAndStartTime(callToUse, extras);
                    setCallSourceToAnalytics(callToUse, originalIntent);

                    if (isPotentialMMICode(handle) && !isSelfManaged) {
                        // Do not add the call if it is a potential MMI code.
                        callToUse.addListener(this);
                    } else if (!mCalls.contains(callToUse)) {
                        // We check if mCalls already contains the call because we could
                        // potentially be reusing
                        // a call which was previously added (See {@link #reuseOutgoingCall}).
                        addCall(callToUse);
                    }
                    return CompletableFuture.completedFuture(callToUse);
                }, new LoggedHandlerExecutor(outgoingCallHandler, "CM.pASP", mLock));
        return mLatestPostSelectionProcessingFuture;
    }

    public void startConference(List<Uri> participants, Bundle clientExtras, String callingPackage,
            UserHandle initiatingUser) {

         if (clientExtras == null) {
             clientExtras = new Bundle();
         }

         PhoneAccountHandle phoneAccountHandle = clientExtras.getParcelable(
                 TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE);
         CompletableFuture<Call> callFuture = startOutgoingCall(participants, phoneAccountHandle,
                 clientExtras, initiatingUser, null/* originalIntent */, callingPackage,
                 true/* isconference*/);

         final boolean speakerphoneOn = clientExtras.getBoolean(
                 TelecomManager.EXTRA_START_CALL_WITH_SPEAKERPHONE);
         final int videoState = clientExtras.getInt(
                 TelecomManager.EXTRA_START_CALL_WITH_VIDEO_STATE);

         final Session logSubsession = Log.createSubsession();
         callFuture.thenAccept((call) -> {
             if (call != null) {
                 Log.continueSession(logSubsession, "CM.pOGC");
                 try {
                     placeOutgoingCall(call, call.getHandle(), null/* gatewayInfo */,
                             speakerphoneOn, videoState);
                 } finally {
                     Log.endSession();
                 }
             }
         });
    }

    /**
     * Performs call identification for an outgoing phone call.
     * @param theCall The outgoing call to perform identification.
     */
    private void bindForOutgoingCallerId(Call theCall) {
        // Find the user chosen call screening app.
        String callScreeningApp =
                mRoleManagerAdapter.getDefaultCallScreeningApp();

        CompletableFuture future =
                new CallScreeningServiceHelper(mContext,
                mLock,
                callScreeningApp,
                new ParcelableCallUtils.Converter(),
                mCurrentUserHandle,
                theCall,
                new AppLabelProxy() {
                    @Override
                    public CharSequence getAppLabel(String packageName) {
                        return Util.getAppLabel(mContext.getPackageManager(), packageName);
                    }
                }).process();
        future.thenApply( v -> {
            Log.i(this, "Outgoing caller ID complete");
            return null;
        });
    }

    /**
     * Finds the {@link PhoneAccountHandle}(s) which could potentially be used to place an outgoing
     * call.  Takes into account the following:
     * 1. Any pre-chosen {@link PhoneAccountHandle} which was specified on the
     * {@link Intent#ACTION_CALL} intent.  If one was chosen it will be used if possible.
     * 2. Whether the call is a video call.  If the call being placed is a video call, an attempt is
     * first made to consider video capable phone accounts.  If no video capable phone accounts are
     * found, the usual non-video capable phone accounts will be considered.
     * 3. Whether there is a user-chosen default phone account; that one will be used if possible.
     *
     * @param targetPhoneAccountHandle The pre-chosen {@link PhoneAccountHandle} passed in when the
     *                                 call was placed.  Will be {@code null} if the
     *                                 {@link Intent#ACTION_CALL} intent did not specify a target
     *                                 phone account.
     * @param handle The handle of the outgoing call; used to determine the SIP scheme when matching
     *               phone accounts.
     * @param isVideo {@code true} if the call is a video call, {@code false} otherwise.
     * @param isEmergency {@code true} if the call is an emergency call.
     * @param initiatingUser The {@link UserHandle} the call is placed on.
     * @return
     */
    @VisibleForTesting
    public CompletableFuture<List<PhoneAccountHandle>> findOutgoingCallPhoneAccount(
            PhoneAccountHandle targetPhoneAccountHandle, Uri handle, boolean isVideo,
            boolean isEmergency, UserHandle initiatingUser) {
       return findOutgoingCallPhoneAccount(targetPhoneAccountHandle, handle, isVideo,
               isEmergency, initiatingUser, false/* isConference */);
    }

    public CompletableFuture<List<PhoneAccountHandle>> findOutgoingCallPhoneAccount(
            PhoneAccountHandle targetPhoneAccountHandle, Uri handle, boolean isVideo,
            boolean isEmergency, UserHandle initiatingUser, boolean isConference) {

        if (isSelfManaged(targetPhoneAccountHandle, initiatingUser)) {
            return CompletableFuture.completedFuture(Arrays.asList(targetPhoneAccountHandle));
        }

        List<PhoneAccountHandle> accounts;
        // Try to find a potential phone account, taking into account whether this is a video
        // call.
        accounts = constructPossiblePhoneAccounts(handle, initiatingUser, isVideo, isEmergency,
                isConference);
        if (isVideo && accounts.size() == 0) {
            // Placing a video call but no video capable accounts were found, so consider any
            // call capable accounts (we can fallback to audio).
            accounts = constructPossiblePhoneAccounts(handle, initiatingUser,
                    false /* isVideo */, isEmergency /* isEmergency */, isConference);
        }
        Log.v(this, "findOutgoingCallPhoneAccount: accounts = " + accounts);

        // Only dial with the requested phoneAccount if it is still valid. Otherwise treat this
        // call as if a phoneAccount was not specified (does the default behavior instead).
        // Note: We will not attempt to dial with a requested phoneAccount if it is disabled.
        if (targetPhoneAccountHandle != null) {
            if (accounts.contains(targetPhoneAccountHandle)) {
                // The target phone account is valid and was found.
                return CompletableFuture.completedFuture(Arrays.asList(targetPhoneAccountHandle));
            }
        }
        if (accounts.isEmpty() || accounts.size() == 1) {
            return CompletableFuture.completedFuture(accounts);
        }

        // Do the query for whether there's a preferred contact
        final CompletableFuture<PhoneAccountHandle> userPreferredAccountForContact =
                new CompletableFuture<>();
        final List<PhoneAccountHandle> possibleAccounts = accounts;
        mCallerInfoLookupHelper.startLookup(handle,
                new CallerInfoLookupHelper.OnQueryCompleteListener() {
                    @Override
                    public void onCallerInfoQueryComplete(Uri handle, CallerInfo info) {
                        if (info != null &&
                                info.preferredPhoneAccountComponent != null &&
                                info.preferredPhoneAccountId != null &&
                                !info.preferredPhoneAccountId.isEmpty()) {
                            PhoneAccountHandle contactDefaultHandle = new PhoneAccountHandle(
                                    info.preferredPhoneAccountComponent,
                                    info.preferredPhoneAccountId,
                                    initiatingUser);
                            userPreferredAccountForContact.complete(contactDefaultHandle);
                        } else {
                            userPreferredAccountForContact.complete(null);
                        }
                    }

                    @Override
                    public void onContactPhotoQueryComplete(Uri handle, CallerInfo info) {
                        // ignore this
                    }
                });

        return userPreferredAccountForContact.thenApply(phoneAccountHandle -> {
            if (phoneAccountHandle != null) {
                return Collections.singletonList(phoneAccountHandle);
            }
            // No preset account, check if default exists that supports the URI scheme for the
            // handle and verify it can be used.
            PhoneAccountHandle defaultPhoneAccountHandle =
                    mPhoneAccountRegistrar.getOutgoingPhoneAccountForScheme(
                            handle.getScheme(), initiatingUser);
            if (defaultPhoneAccountHandle != null &&
                    possibleAccounts.contains(defaultPhoneAccountHandle)) {
                return Collections.singletonList(defaultPhoneAccountHandle);
            }
            return possibleAccounts;
        });
    }

    /**
     * Determines if a {@link PhoneAccountHandle} is for a self-managed ConnectionService.
     * @param targetPhoneAccountHandle The phone account to check.
     * @param initiatingUser The user associated with the account.
     * @return {@code true} if the phone account is self-managed, {@code false} otherwise.
     */
    public boolean isSelfManaged(PhoneAccountHandle targetPhoneAccountHandle,
            UserHandle initiatingUser) {
        PhoneAccount targetPhoneAccount = mPhoneAccountRegistrar.getPhoneAccount(
                targetPhoneAccountHandle, initiatingUser);
        return targetPhoneAccount != null && targetPhoneAccount.isSelfManaged();
    }

    public void onCallRedirectionComplete(Call call, Uri handle,
                                          PhoneAccountHandle phoneAccountHandle,
                                          GatewayInfo gatewayInfo, boolean speakerphoneOn,
                                          int videoState, boolean shouldCancelCall,
                                          String uiAction) {
        Log.i(this, "onCallRedirectionComplete for Call %s with handle %s" +
                " and phoneAccountHandle %s", call, handle, phoneAccountHandle);

        boolean endEarly = false;
        String disconnectReason = "";
        String callRedirectionApp = mRoleManagerAdapter.getDefaultCallRedirectionApp();

        boolean isPotentialEmergencyNumber;
        try {
            isPotentialEmergencyNumber =
                    handle != null && getTelephonyManager().isPotentialEmergencyNumber(
                            handle.getSchemeSpecificPart());
        } catch (IllegalStateException ise) {
            isPotentialEmergencyNumber = false;
        }

        if (shouldCancelCall) {
            Log.w(this, "onCallRedirectionComplete: call is canceled");
            endEarly = true;
            disconnectReason = "Canceled from Call Redirection Service";

            // Show UX when user-defined call redirection service does not response; the UX
            // is not needed to show if the call is disconnected (e.g. by the user)
            if (uiAction.equals(CallRedirectionProcessor.UI_TYPE_USER_DEFINED_TIMEOUT)
                    && !call.isDisconnected()) {
                Intent timeoutIntent = new Intent(mContext,
                        CallRedirectionTimeoutDialogActivity.class);
                timeoutIntent.putExtra(
                        CallRedirectionTimeoutDialogActivity.EXTRA_REDIRECTION_APP_NAME,
                        mRoleManagerAdapter.getApplicationLabelForPackageName(callRedirectionApp));
                timeoutIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mContext.startActivityAsUser(timeoutIntent, UserHandle.CURRENT);
            }
        } else if (handle == null) {
            Log.w(this, "onCallRedirectionComplete: handle is null");
            endEarly = true;
            disconnectReason = "Null handle from Call Redirection Service";
        } else if (phoneAccountHandle == null) {
            Log.w(this, "onCallRedirectionComplete: phoneAccountHandle is null");
            endEarly = true;
            disconnectReason = "Null phoneAccountHandle from Call Redirection Service";
        } else if (isPotentialEmergencyNumber) {
            Log.w(this, "onCallRedirectionComplete: emergency number %s is redirected from Call"
                    + " Redirection Service", handle.getSchemeSpecificPart());
            endEarly = true;
            disconnectReason = "Emergency number is redirected from Call Redirection Service";
        }
        if (endEarly) {
            if (call != null) {
                call.disconnect(disconnectReason);
            }
            return;
        }

        // If this call is already disconnected then we have nothing more to do.
        if (call.isDisconnected()) {
            Log.w(this, "onCallRedirectionComplete: Call has already been disconnected,"
                    + " ignore the call redirection %s", call);
            return;
        }

        if (uiAction.equals(CallRedirectionProcessor.UI_TYPE_USER_DEFINED_ASK_FOR_CONFIRM)) {
            Log.addEvent(call, LogUtils.Events.REDIRECTION_USER_CONFIRMATION);
            mPendingRedirectedOutgoingCall = call;

            mPendingRedirectedOutgoingCallInfo.put(call.getId(),
                    new Runnable("CM.oCRC", mLock) {
                        @Override
                        public void loggedRun() {
                            Log.addEvent(call, LogUtils.Events.REDIRECTION_USER_CONFIRMED);
                            call.setTargetPhoneAccount(phoneAccountHandle);
                            placeOutgoingCall(call, handle, gatewayInfo, speakerphoneOn,
                                    videoState);
                        }
                    });

            mPendingUnredirectedOutgoingCallInfo.put(call.getId(),
                    new Runnable("CM.oCRC", mLock) {
                        @Override
                        public void loggedRun() {
                            call.setTargetPhoneAccount(phoneAccountHandle);
                            placeOutgoingCall(call, handle, null, speakerphoneOn,
                                    videoState);
                        }
                    });

            Log.i(this, "onCallRedirectionComplete: UI_TYPE_USER_DEFINED_ASK_FOR_CONFIRM "
                            + "callId=%s, callRedirectionAppName=%s",
                    call.getId(), callRedirectionApp);

            showRedirectionDialog(call.getId(),
                    mRoleManagerAdapter.getApplicationLabelForPackageName(callRedirectionApp));
        } else {
            call.setTargetPhoneAccount(phoneAccountHandle);
            placeOutgoingCall(call, handle, gatewayInfo, speakerphoneOn, videoState);
        }
    }

    /**
     * Shows the call redirection confirmation dialog.  This is explicitly done here instead of in
     * an activity class such as {@link ConfirmCallDialogActivity}.  This was originally done with
     * an activity class, however due to the fact that the InCall UI is being spun up at the same
     * time as the dialog activity, there is a potential race condition where the InCall UI will
     * often be shown instead of the dialog.  Activity manager chooses not to show the redirection
     * dialog in that case since the new top activity from dialer is going to show.
     * By showing the dialog here we're able to set the dialog's window type to
     * {@link WindowManager.LayoutParams#TYPE_SYSTEM_ALERT} which guarantees it shows above other
     * content on the screen.
     * @param callId The ID of the call to show the redirection dialog for.
     */
    private void showRedirectionDialog(@NonNull String callId, @NonNull CharSequence appName) {
        AlertDialog confirmDialog = new AlertDialog.Builder(mContext).create();
        LayoutInflater layoutInflater = LayoutInflater.from(mContext);
        View dialogView = layoutInflater.inflate(R.layout.call_redirection_confirm_dialog, null);

        Button buttonFirstLine = (Button) dialogView.findViewById(R.id.buttonFirstLine);
        buttonFirstLine.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent proceedWithoutRedirectedCall = new Intent(
                        TelecomBroadcastIntentProcessor.ACTION_PLACE_UNREDIRECTED_CALL,
                        null, mContext,
                        TelecomBroadcastReceiver.class);
                proceedWithoutRedirectedCall.putExtra(
                        TelecomBroadcastIntentProcessor.EXTRA_REDIRECTION_OUTGOING_CALL_ID,
                        callId);
                mContext.sendBroadcast(proceedWithoutRedirectedCall);
                confirmDialog.dismiss();
            }
        });

        Button buttonSecondLine = (Button) dialogView.findViewById(R.id.buttonSecondLine);
        buttonSecondLine.setText(mContext.getString(
                R.string.alert_place_outgoing_call_with_redirection, appName));
        buttonSecondLine.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent proceedWithRedirectedCall = new Intent(
                        TelecomBroadcastIntentProcessor.ACTION_PLACE_REDIRECTED_CALL, null,
                        mContext,
                        TelecomBroadcastReceiver.class);
                proceedWithRedirectedCall.putExtra(
                        TelecomBroadcastIntentProcessor.EXTRA_REDIRECTION_OUTGOING_CALL_ID,
                        callId);
                mContext.sendBroadcast(proceedWithRedirectedCall);
                confirmDialog.dismiss();
            }
        });

        Button buttonThirdLine = (Button) dialogView.findViewById(R.id.buttonThirdLine);
        buttonThirdLine.setOnClickListener(new View.OnClickListener() {
            public void onClick(View v) {
                cancelRedirection(callId);
                confirmDialog.dismiss();
            }
        });

        confirmDialog.setOnCancelListener(new DialogInterface.OnCancelListener() {
            @Override
            public void onCancel(DialogInterface dialog) {
                cancelRedirection(callId);
                confirmDialog.dismiss();
            }
        });

        confirmDialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        confirmDialog.getWindow().setType(WindowManager.LayoutParams.TYPE_SYSTEM_ALERT);

        confirmDialog.setCancelable(false);
        confirmDialog.setCanceledOnTouchOutside(false);
        confirmDialog.setView(dialogView);

        confirmDialog.show();
    }

    /**
     * Signals to Telecom that redirection of the call is to be cancelled.
     */
    private void cancelRedirection(String callId) {
        Intent cancelRedirectedCall = new Intent(
                TelecomBroadcastIntentProcessor.ACTION_CANCEL_REDIRECTED_CALL,
                null, mContext,
                TelecomBroadcastReceiver.class);
        cancelRedirectedCall.putExtra(
                TelecomBroadcastIntentProcessor.EXTRA_REDIRECTION_OUTGOING_CALL_ID, callId);
        mContext.sendBroadcastAsUser(cancelRedirectedCall, UserHandle.CURRENT);
    }

    public void processRedirectedOutgoingCallAfterUserInteraction(String callId, String action) {
        Log.i(this, "processRedirectedOutgoingCallAfterUserInteraction for Call ID %s, action=%s",
                callId, action);
        if (mPendingRedirectedOutgoingCall != null && mPendingRedirectedOutgoingCall.getId()
                .equals(callId)) {
            if (action.equals(TelecomBroadcastIntentProcessor.ACTION_PLACE_REDIRECTED_CALL)) {
                mHandler.post(mPendingRedirectedOutgoingCallInfo.get(callId).prepare());
            } else if (action.equals(
                    TelecomBroadcastIntentProcessor.ACTION_PLACE_UNREDIRECTED_CALL)) {
                mHandler.post(mPendingUnredirectedOutgoingCallInfo.get(callId).prepare());
            } else if (action.equals(
                    TelecomBroadcastIntentProcessor.ACTION_CANCEL_REDIRECTED_CALL)) {
                Log.addEvent(mPendingRedirectedOutgoingCall,
                        LogUtils.Events.REDIRECTION_USER_CANCELLED);
                mPendingRedirectedOutgoingCall.disconnect("User canceled the redirected call.");
            }
            mPendingRedirectedOutgoingCall = null;
            mPendingRedirectedOutgoingCallInfo.remove(callId);
            mPendingUnredirectedOutgoingCallInfo.remove(callId);
        } else {
            Log.w(this, "processRedirectedOutgoingCallAfterUserInteraction for non-matched Call ID"
                    + " %s", callId);
        }
    }

    /**
     * Attempts to issue/connect the specified call.
     *
     * @param handle Handle to connect the call with.
     * @param gatewayInfo Optional gateway information that can be used to route the call to the
     *        actual dialed handle via a gateway provider. May be null.
     * @param speakerphoneOn Whether or not to turn the speakerphone on once the call connects.
     * @param videoState The desired video state for the outgoing call.
     */
    @VisibleForTesting
    public void placeOutgoingCall(Call call, Uri handle, GatewayInfo gatewayInfo,
            boolean speakerphoneOn, int videoState) {
        if (call == null) {
            // don't do anything if the call no longer exists
            Log.i(this, "Canceling unknown call.");
            return;
        }

        final Uri uriHandle = (gatewayInfo == null) ? handle : gatewayInfo.getGatewayAddress();

        if (gatewayInfo == null) {
            Log.i(this, "Creating a new outgoing call with handle: %s", Log.piiHandle(uriHandle));
        } else {
            Log.i(this, "Creating a new outgoing call with gateway handle: %s, original handle: %s",
                    Log.pii(uriHandle), Log.pii(handle));
        }

        call.setHandle(uriHandle);
        call.setGatewayInfo(gatewayInfo);

        final boolean useSpeakerWhenDocked = mContext.getResources().getBoolean(
                R.bool.use_speaker_when_docked);
        final boolean useSpeakerForDock = isSpeakerphoneEnabledForDock();
        final boolean useSpeakerForVideoCall = isSpeakerphoneAutoEnabledForVideoCalls(videoState);

        // Auto-enable speakerphone if the originating intent specified to do so, if the call
        // is a video call, of if using speaker when docked
        PhoneAccount account = mPhoneAccountRegistrar.getPhoneAccount(
                call.getTargetPhoneAccount(), call.getInitiatingUser());
        boolean allowVideo = false;
        if (account != null) {
            allowVideo = account.hasCapabilities(PhoneAccount.CAPABILITY_VIDEO_CALLING);
        }
        call.setStartWithSpeakerphoneOn(speakerphoneOn || (useSpeakerForVideoCall && allowVideo)
                || (useSpeakerWhenDocked && useSpeakerForDock));
        call.setVideoState(videoState);

        if (speakerphoneOn) {
            Log.i(this, "%s Starting with speakerphone as requested", call);
        } else if (useSpeakerWhenDocked && useSpeakerForDock) {
            Log.i(this, "%s Starting with speakerphone because car is docked.", call);
        } else if (useSpeakerForVideoCall) {
            Log.i(this, "%s Starting with speakerphone because its a video call.", call);
        }

        if (call.isEmergencyCall()) {
            Executors.defaultThreadFactory().newThread(() ->
                    BlockedNumberContract.SystemContract.notifyEmergencyContact(mContext))
                    .start();
        }

        final boolean requireCallCapableAccountByHandle = mContext.getResources().getBoolean(
                com.android.internal.R.bool.config_requireCallCapableAccountForHandle);
        final boolean isOutgoingCallPermitted = isOutgoingCallPermitted(call,
                call.getTargetPhoneAccount());
        final String callHandleScheme =
                call.getHandle() == null ? null : call.getHandle().getScheme();
        if (call.getTargetPhoneAccount() != null || call.isEmergencyCall()) {
            // If the account has been set, proceed to place the outgoing call.
            // Otherwise the connection will be initiated when the account is set by the user.
            if (call.isSelfManaged() && !isOutgoingCallPermitted) {
                if (call.isAdhocConferenceCall()) {
                    notifyCreateConferenceFailed(call.getTargetPhoneAccount(), call);
                } else {
                    notifyCreateConnectionFailed(call.getTargetPhoneAccount(), call);
                }
            } else {
                if (call.isEmergencyCall()) {
                    // Drop any ongoing self-managed calls to make way for an emergency call.
                    disconnectSelfManagedCalls("place emerg call" /* reason */);
                }

                call.startCreateConnection(mPhoneAccountRegistrar);
            }
        } else if (mPhoneAccountRegistrar.getCallCapablePhoneAccounts(
                requireCallCapableAccountByHandle ? callHandleScheme : null, false,
                call.getInitiatingUser()).isEmpty()) {
            // If there are no call capable accounts, disconnect the call.
            markCallAsDisconnected(call, new DisconnectCause(DisconnectCause.CANCELED,
                    "No registered PhoneAccounts"));
            markCallAsRemoved(call);
        }
    }

    /**
     * Attempts to start a conference call for the specified call.
     *
     * @param call The call to conference.
     * @param otherCall The other call to conference with.
     */
    @VisibleForTesting
    public void conference(Call call, Call otherCall) {
        call.conferenceWith(otherCall);
    }

    /**
     * Instructs Telecom to answer the specified call. Intended to be invoked by the in-call
     * app through {@link InCallAdapter} after Telecom notifies it of an incoming call followed by
     * the user opting to answer said call.
     *
     * @param call The call to answer.
     * @param videoState The video state in which to answer the call.
     */
    @VisibleForTesting
    public void answerCall(Call call, int videoState) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to answer a non-existent call %s", call);
        } else {
            // Hold or disconnect the active call and request call focus for the incoming call.
            Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
            Log.d(this, "answerCall: Incoming call = %s Ongoing call %s", call, activeCall);
            holdActiveCallForNewCall(call);
            mConnectionSvrFocusMgr.requestFocus(
                    call,
                    new RequestCallback(new ActionAnswerCall(call, videoState)));
        }
    }

    private void answerCallForAudioProcessing(Call call) {
        // We don't check whether the call has been added to the internal lists yet -- it's optional
        // until the call is actually in the AUDIO_PROCESSING state.
        Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
        if (activeCall != null && activeCall != call) {
            Log.w(this, "answerCallForAudioProcessing: another active call already exists. "
                    + "Ignoring request for audio processing and letting the incoming call "
                    + "through.");
            // The call should already be in the RINGING state, so all we have to do is add the
            // call to the internal tracker.
            addCall(call);
            return;
        }
        Log.d(this, "answerCallForAudioProcessing: Incoming call = %s", call);
        mConnectionSvrFocusMgr.requestFocus(
                call,
                new RequestCallback(() -> {
                    synchronized (mLock) {
                        Log.d(this, "answering call %s for audio processing with cs focus", call);
                        call.answerForAudioProcessing();
                        // Skip setting the call state to ANSWERED -- that's only for calls that
                        // were answered by user intervention.
                        mPendingAudioProcessingCall = call;
                    }
                }));

    }

    /**
     * Instructs Telecom to bring a call into the AUDIO_PROCESSING state.
     *
     * Used by the background audio call screener (also the default dialer) to signal that
     * they want to manually enter the AUDIO_PROCESSING state. The user will be aware that there is
     * an ongoing call at this time.
     *
     * @param call The call to manipulate
     */
    public void enterBackgroundAudioProcessing(Call call, String requestingPackageName) {
        if (!mCalls.contains(call)) {
            Log.w(this, "Trying to exit audio processing on an untracked call");
            return;
        }

        Call activeCall = getActiveCall();
        if (activeCall != null && activeCall != call) {
            Log.w(this, "Ignoring enter audio processing because there's already a call active");
            return;
        }

        CharSequence requestingAppName = AppLabelProxy.Util.getAppLabel(
                mContext.getPackageManager(), requestingPackageName);
        if (requestingAppName == null) {
            requestingAppName = requestingPackageName;
        }

        // We only want this to work on active or ringing calls
        if (call.getState() == CallState.RINGING) {
            // After the connection service sets up the call with the other end, it'll set the call
            // state to AUDIO_PROCESSING
            answerCallForAudioProcessing(call);
            call.setAudioProcessingRequestingApp(requestingAppName);
        } else if (call.getState() == CallState.ACTIVE) {
            setCallState(call, CallState.AUDIO_PROCESSING,
                    "audio processing set by dialer request");
            call.setAudioProcessingRequestingApp(requestingAppName);
        }
    }

    /**
     * Instructs Telecom to bring a call out of the AUDIO_PROCESSING state.
     *
     * Used by the background audio call screener (also the default dialer) to signal that it's
     * finished doing its thing and the user should be made aware of the call.
     *
     * @param call The call to manipulate
     * @param shouldRing if true, puts the call into SIMULATED_RINGING. Otherwise, makes the call
     *                   active.
     */
    public void exitBackgroundAudioProcessing(Call call, boolean shouldRing) {
        if (!mCalls.contains(call)) {
            Log.w(this, "Trying to exit audio processing on an untracked call");
            return;
        }

        Call activeCall = getActiveCall();
        if (activeCall != null) {
            Log.w(this, "Ignoring exit audio processing because there's already a call active");
        }

        if (shouldRing) {
            setCallState(call, CallState.SIMULATED_RINGING, "exitBackgroundAudioProcessing");
        } else {
            setCallState(call, CallState.ACTIVE, "exitBackgroundAudioProcessing");
        }
    }

    /**
     * Instructs Telecom to deflect the specified call. Intended to be invoked by the in-call
     * app through {@link InCallAdapter} after Telecom notifies it of an incoming call followed by
     * the user opting to deflect said call.
     */
    @VisibleForTesting
    public void deflectCall(Call call, Uri address) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to deflect a non-existent call %s", call);
        } else {
            call.deflect(address);
        }
    }

    /**
     * Determines if the speakerphone should be automatically enabled for the call.  Speakerphone
     * should be enabled if the call is a video call and bluetooth or the wired headset are not in
     * use.
     *
     * @param videoState The video state of the call.
     * @return {@code true} if the speakerphone should be enabled.
     */
    public boolean isSpeakerphoneAutoEnabledForVideoCalls(int videoState) {
        return VideoProfile.isVideo(videoState) &&
            !mWiredHeadsetManager.isPluggedIn() &&
            !mBluetoothRouteManager.isBluetoothAvailable() &&
            isSpeakerEnabledForVideoCalls();
    }

    /**
     * Determines if the speakerphone should be enabled for when docked.  Speakerphone
     * should be enabled if the device is docked and bluetooth or the wired headset are
     * not in use.
     *
     * @return {@code true} if the speakerphone should be enabled for the dock.
     */
    private boolean isSpeakerphoneEnabledForDock() {
        return mDockManager.isDocked() &&
            !mWiredHeadsetManager.isPluggedIn() &&
            !mBluetoothRouteManager.isBluetoothAvailable();
    }

    /**
     * Determines if the speakerphone should be automatically enabled for video calls.
     *
     * @return {@code true} if the speakerphone should automatically be enabled.
     */
    private static boolean isSpeakerEnabledForVideoCalls() {
        return TelephonyProperties.videocall_audio_output()
                .orElse(TelecomManager.AUDIO_OUTPUT_DEFAULT)
                == TelecomManager.AUDIO_OUTPUT_ENABLE_SPEAKER;
    }

    /**
     * Instructs Telecom to reject the specified call. Intended to be invoked by the in-call
     * app through {@link InCallAdapter} after Telecom notifies it of an incoming call followed by
     * the user opting to reject said call.
     */
    @VisibleForTesting
    public void rejectCall(Call call, boolean rejectWithMessage, String textMessage) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to reject a non-existent call %s", call);
        } else {
            for (CallsManagerListener listener : mListeners) {
                listener.onIncomingCallRejected(call, rejectWithMessage, textMessage);
            }
            call.reject(rejectWithMessage, textMessage);
        }
    }

    /**
     * Instructs Telecom to reject the specified call. Intended to be invoked by the in-call
     * app through {@link InCallAdapter} after Telecom notifies it of an incoming call followed by
     * the user opting to reject said call.
     */
    @VisibleForTesting
    public void rejectCall(Call call, @android.telecom.Call.RejectReason int rejectReason) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to reject a non-existent call %s", call);
        } else {
            for (CallsManagerListener listener : mListeners) {
                listener.onIncomingCallRejected(call, false /* rejectWithMessage */,
                        null /* textMessage */);
            }
            call.reject(rejectReason);
        }
    }

    /**
     * Instructs Telecom to transfer the specified call. Intended to be invoked by the in-call
     * app through {@link InCallAdapter} after the user opts to transfer the said call.
     */
    @VisibleForTesting
    public void transferCall(Call call, Uri number, boolean isConfirmationRequired) {
        if (!mCalls.contains(call)) {
            Log.i(this, "transferCall - Request to transfer a non-existent call %s", call);
        } else {
            call.transfer(number, isConfirmationRequired);
        }
    }

    /**
     * Instructs Telecom to transfer the specified call to another ongoing call.
     * Intended to be invoked by the in-call app through {@link InCallAdapter} after the user opts
     * to transfer the said call (consultative transfer).
     */
    @VisibleForTesting
    public void transferCall(Call call, Call otherCall) {
        if (!mCalls.contains(call) || !mCalls.contains(otherCall)) {
            Log.i(this, "transferCall - Non-existent call %s or %s", call, otherCall);
        } else {
            call.transfer(otherCall);
        }
    }

    /**
     * Instructs Telecom to play the specified DTMF tone within the specified call.
     *
     * @param digit The DTMF digit to play.
     */
    @VisibleForTesting
    public void playDtmfTone(Call call, char digit) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to play DTMF in a non-existent call %s", call);
        } else {
            if (call.getState() != CallState.ON_HOLD) {
                call.playDtmfTone(digit);
                mDtmfLocalTonePlayer.playTone(call, digit);
            } else {
                Log.i(this, "Request to play DTMF tone for held call %s", call.getId());
            }
        }
    }

    /**
     * Instructs Telecom to stop the currently playing DTMF tone, if any.
     */
    @VisibleForTesting
    public void stopDtmfTone(Call call) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to stop DTMF in a non-existent call %s", call);
        } else {
            call.stopDtmfTone();
            mDtmfLocalTonePlayer.stopTone(call);
        }
    }

    /**
     * Instructs Telecom to continue (or not) the current post-dial DTMF string, if any.
     */
    void postDialContinue(Call call, boolean proceed) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Request to continue post-dial string in a non-existent call %s", call);
        } else {
            call.postDialContinue(proceed);
        }
    }

    /**
     * Instructs Telecom to disconnect the specified call. Intended to be invoked by the
     * in-call app through {@link InCallAdapter} for an ongoing call. This is usually triggered by
     * the user hitting the end-call button.
     */
    @VisibleForTesting
    public void disconnectCall(Call call) {
        Log.v(this, "disconnectCall %s", call);

        if (!mCalls.contains(call)) {
            Log.w(this, "Unknown call (%s) asked to disconnect", call);
        } else {
            mLocallyDisconnectingCalls.add(call);
            int previousState = call.getState();
            call.disconnect();
            for (CallsManagerListener listener : mListeners) {
                listener.onCallStateChanged(call, previousState, call.getState());
            }
            // Cancel any of the outgoing call futures if they're still around.
            if (mPendingCallConfirm != null && !mPendingCallConfirm.isDone()) {
                mPendingCallConfirm.complete(null);
                mPendingCallConfirm = null;
            }
            if (mPendingAccountSelection != null && !mPendingAccountSelection.isDone()) {
                mPendingAccountSelection.complete(null);
                mPendingAccountSelection = null;
            }
        }
    }

    /**
     * Instructs Telecom to disconnect all calls.
     */
    void disconnectAllCalls() {
        Log.v(this, "disconnectAllCalls");

        for (Call call : mCalls) {
            disconnectCall(call);
        }
    }

    /**
     * Disconnects calls for any other {@link PhoneAccountHandle} but the one specified.
     * Note: As a protective measure, will NEVER disconnect an emergency call.  Although that
     * situation should never arise, its a good safeguard.
     * @param phoneAccountHandle Calls owned by {@link PhoneAccountHandle}s other than this one will
     *                          be disconnected.
     */
    private void disconnectOtherCalls(PhoneAccountHandle phoneAccountHandle) {
        mCalls.stream()
                .filter(c -> !c.isEmergencyCall() &&
                        !c.getTargetPhoneAccount().equals(phoneAccountHandle))
                .forEach(c -> disconnectCall(c));
    }

    /**
     * Instructs Telecom to put the specified call on hold. Intended to be invoked by the
     * in-call app through {@link InCallAdapter} for an ongoing call. This is usually triggered by
     * the user hitting the hold button during an active call.
     */
    @VisibleForTesting
    public void holdCall(Call call) {
        if (!mCalls.contains(call)) {
            Log.w(this, "Unknown call (%s) asked to be put on hold", call);
        } else {
            Log.d(this, "Putting call on hold: (%s)", call);
            call.hold();
        }
    }

    /**
     * Instructs Telecom to release the specified call from hold. Intended to be invoked by
     * the in-call app through {@link InCallAdapter} for an ongoing call. This is usually triggered
     * by the user hitting the hold button during a held call.
     */
    @VisibleForTesting
    public void unholdCall(Call call) {
        if (!mCalls.contains(call)) {
            Log.w(this, "Unknown call (%s) asked to be removed from hold", call);
        } else {
            if (getOutgoingCall() != null) {
                Log.w(this, "There is an outgoing call, so it is unable to unhold this call %s",
                        call);
                return;
            }
            Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
            String activeCallId = null;
            if (activeCall != null && !activeCall.isLocallyDisconnecting()) {
                activeCallId = activeCall.getId();
                if (canHold(activeCall)) {
                    activeCall.hold("Swap to " + call.getId());
                    Log.addEvent(activeCall, LogUtils.Events.SWAP, "To " + call.getId());
                    Log.addEvent(call, LogUtils.Events.SWAP, "From " + activeCall.getId());
                } else {
                    // This call does not support hold. If it is from a different connection
                    // service or connection manager, then disconnect it, otherwise invoke
                    // call.hold() and allow the connection service or connection manager to handle
                    // the situation.
                    if (!areFromSameSource(activeCall, call)) {
                        if (!activeCall.isEmergencyCall()) {
                            activeCall.disconnect("Swap to " + call.getId());
                        } else {
                            Log.w(this, "unholdCall: % is an emergency call, aborting swap to %s",
                                    activeCall.getId(), call.getId());
                            // Don't unhold the call as requested; we don't want to drop an
                            // emergency call.
                            return;
                        }
                    } else {
                        activeCall.hold("Swap to " + call.getId());
                    }
                }
            }
            mConnectionSvrFocusMgr.requestFocus(
                    call,
                    new RequestCallback(new ActionUnHoldCall(call, activeCallId)));
        }
    }

    @Override
    public void onExtrasRemoved(Call c, int source, List<String> keys) {
        if (source != Call.SOURCE_CONNECTION_SERVICE) {
            return;
        }
        updateCanAddCall();
    }

    @Override
    public void onExtrasChanged(Call c, int source, Bundle extras) {
        if (source != Call.SOURCE_CONNECTION_SERVICE) {
            return;
        }
        handleCallTechnologyChange(c);
        handleChildAddressChange(c);
        updateCanAddCall();
    }

    // Construct the list of possible PhoneAccounts that the outgoing call can use based on the
    // active calls in CallsManager. If any of the active calls are on a SIM based PhoneAccount,
    // then include only that SIM based PhoneAccount and any non-SIM PhoneAccounts, such as SIP.
    @VisibleForTesting
    public List<PhoneAccountHandle> constructPossiblePhoneAccounts(Uri handle, UserHandle user,
            boolean isVideo, boolean isEmergency) {
        return constructPossiblePhoneAccounts(handle, user, isVideo, isEmergency, false);
    }

    public List<PhoneAccountHandle> constructPossiblePhoneAccounts(Uri handle, UserHandle user,
            boolean isVideo, boolean isEmergency, boolean isConference) {

        if (handle == null) {
            return Collections.emptyList();
        }
        // If we're specifically looking for video capable accounts, then include that capability,
        // otherwise specify no additional capability constraints. When handling the emergency call,
        // it also needs to find the phone accounts excluded by CAPABILITY_EMERGENCY_CALLS_ONLY.
        int capabilities = isVideo ? PhoneAccount.CAPABILITY_VIDEO_CALLING : 0;
        capabilities |= isConference ? PhoneAccount.CAPABILITY_ADHOC_CONFERENCE_CALLING : 0;
        List<PhoneAccountHandle> allAccounts =
                mPhoneAccountRegistrar.getCallCapablePhoneAccounts(handle.getScheme(), false, user,
                        capabilities,
                        isEmergency ? 0 : PhoneAccount.CAPABILITY_EMERGENCY_CALLS_ONLY);
        if (mMaxNumberOfSimultaneouslyActiveSims < 0) {
            mMaxNumberOfSimultaneouslyActiveSims =
                    getTelephonyManager().getMaxNumberOfSimultaneouslyActiveSims();
        }
        // Only one SIM PhoneAccount can be active at one time for DSDS. Only that SIM PhoneAccount
        // should be available if a call is already active on the SIM account.
        if (mMaxNumberOfSimultaneouslyActiveSims == 1) {
            List<PhoneAccountHandle> simAccounts =
                    mPhoneAccountRegistrar.getSimPhoneAccountsOfCurrentUser();
            PhoneAccountHandle ongoingCallAccount = null;
            for (Call c : mCalls) {
                if (!c.isDisconnected() && !c.isNew() && simAccounts.contains(
                        c.getTargetPhoneAccount())) {
                    ongoingCallAccount = c.getTargetPhoneAccount();
                    break;
                }
            }
            if (ongoingCallAccount != null) {
                // Remove all SIM accounts that are not the active SIM from the list.
                simAccounts.remove(ongoingCallAccount);
                allAccounts.removeAll(simAccounts);
            }
        }
        return allAccounts;
    }

    private TelephonyManager getTelephonyManager() {
        return mContext.getSystemService(TelephonyManager.class);
    }

    /**
     * Informs listeners (notably {@link CallAudioManager} of a change to the call's external
     * property.
     * .
     * @param call The call whose external property changed.
     * @param isExternalCall {@code True} if the call is now external, {@code false} otherwise.
     */
    @Override
    public void onExternalCallChanged(Call call, boolean isExternalCall) {
        Log.v(this, "onConnectionPropertiesChanged: %b", isExternalCall);
        for (CallsManagerListener listener : mListeners) {
            listener.onExternalCallChanged(call, isExternalCall);
        }
    }

    private void handleCallTechnologyChange(Call call) {
        if (call.getExtras() != null
                && call.getExtras().containsKey(TelecomManager.EXTRA_CALL_TECHNOLOGY_TYPE)) {

            Integer analyticsCallTechnology = sAnalyticsTechnologyMap.get(
                    call.getExtras().getInt(TelecomManager.EXTRA_CALL_TECHNOLOGY_TYPE));
            if (analyticsCallTechnology == null) {
                analyticsCallTechnology = Analytics.THIRD_PARTY_PHONE;
            }
            call.getAnalytics().addCallTechnology(analyticsCallTechnology);
        }
    }

    public void handleChildAddressChange(Call call) {
        if (call.getExtras() != null
                && call.getExtras().containsKey(Connection.EXTRA_CHILD_ADDRESS)) {

            String viaNumber = call.getExtras().getString(Connection.EXTRA_CHILD_ADDRESS);
            call.setViaNumber(viaNumber);
        }
    }

    /** Called by the in-call UI to change the mute state. */
    void mute(boolean shouldMute) {
        if (isInEmergencyCall() && shouldMute) {
            Log.i(this, "Refusing to turn on mute because we're in an emergency call");
            shouldMute = false;
        }
        mCallAudioManager.mute(shouldMute);
    }

    /**
      * Called by the in-call UI to change the audio route, for example to change from earpiece to
      * speaker phone.
      */
    void setAudioRoute(int route, String bluetoothAddress) {
        mCallAudioManager.setAudioRoute(route, bluetoothAddress);
    }

    /** Called by the in-call UI to turn the proximity sensor on. */
    void turnOnProximitySensor() {
        mProximitySensorManager.turnOn();
    }

    /**
     * Called by the in-call UI to turn the proximity sensor off.
     * @param screenOnImmediately If true, the screen will be turned on immediately. Otherwise,
     *        the screen will be kept off until the proximity sensor goes negative.
     */
    void turnOffProximitySensor(boolean screenOnImmediately) {
        mProximitySensorManager.turnOff(screenOnImmediately);
    }

    private boolean isRttSettingOn(PhoneAccountHandle handle) {
        boolean isRttModeSettingOn = Settings.Secure.getInt(mContext.getContentResolver(),
                Settings.Secure.RTT_CALLING_MODE, 0) != 0;
        // If the carrier config says that we should ignore the RTT mode setting from the user,
        // assume that it's off (i.e. only make an RTT call if it's requested through the extra).
        boolean shouldIgnoreRttModeSetting = getCarrierConfigForPhoneAccount(handle)
                .getBoolean(CarrierConfigManager.KEY_IGNORE_RTT_MODE_SETTING_BOOL, false);
        return isRttModeSettingOn && !shouldIgnoreRttModeSetting;
    }

    private PersistableBundle getCarrierConfigForPhoneAccount(PhoneAccountHandle handle) {
        int subscriptionId = mPhoneAccountRegistrar.getSubscriptionIdForPhoneAccount(handle);
        CarrierConfigManager carrierConfigManager =
                mContext.getSystemService(CarrierConfigManager.class);
        PersistableBundle result = carrierConfigManager.getConfigForSubId(subscriptionId);
        return result == null ? new PersistableBundle() : result;
    }

    void phoneAccountSelected(Call call, PhoneAccountHandle account, boolean setDefault) {
        if (!mCalls.contains(call)) {
            Log.i(this, "Attempted to add account to unknown call %s", call);
        } else {
            if (setDefault) {
                mPhoneAccountRegistrar
                        .setUserSelectedOutgoingPhoneAccount(account, call.getInitiatingUser());
            }

            if (mPendingAccountSelection != null) {
                mPendingAccountSelection.complete(Pair.create(call, account));
                mPendingAccountSelection = null;
            }
        }
    }

    /** Called when the audio state changes. */
    @VisibleForTesting
    public void onCallAudioStateChanged(CallAudioState oldAudioState, CallAudioState
            newAudioState) {
        Log.v(this, "onAudioStateChanged, audioState: %s -> %s", oldAudioState, newAudioState);
        for (CallsManagerListener listener : mListeners) {
            listener.onCallAudioStateChanged(oldAudioState, newAudioState);
        }
    }

    /**
     * Called when disconnect tone is started or stopped, including any InCallTone
     * after disconnected call.
     *
     * @param isTonePlaying true if the disconnected tone is started, otherwise the disconnected
     * tone is stopped.
     */
    @VisibleForTesting
    public void onDisconnectedTonePlaying(boolean isTonePlaying) {
        Log.v(this, "onDisconnectedTonePlaying, %s", isTonePlaying ? "started" : "stopped");
        for (CallsManagerListener listener : mListeners) {
            listener.onDisconnectedTonePlaying(isTonePlaying);
        }
    }

    void markCallAsRinging(Call call) {
        setCallState(call, CallState.RINGING, "ringing set explicitly");
    }

    void markCallAsDialing(Call call) {
        setCallState(call, CallState.DIALING, "dialing set explicitly");
        maybeMoveToSpeakerPhone(call);
        maybeTurnOffMute(call);
        ensureCallAudible();
    }

    void markCallAsPulling(Call call) {
        setCallState(call, CallState.PULLING, "pulling set explicitly");
        maybeMoveToSpeakerPhone(call);
    }

    /**
     * Returns true if the active call is held.
     */
    boolean holdActiveCallForNewCall(Call call) {
        Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
        if (activeCall != null && activeCall != call) {
            if (canHold(activeCall)) {
                activeCall.hold();
                return true;
            } else if (supportsHold(activeCall)
                    && areFromSameSource(activeCall, call)) {

                // Handle the case where the active call and the new call are from the same CS or
                // connection manager, and the currently active call supports hold but cannot
                // currently be held.
                // In this case we'll look for the other held call for this connectionService and
                // disconnect it prior to holding the active call.
                // E.g.
                // Call A - Held   (Supports hold, can't hold)
                // Call B - Active (Supports hold, can't hold)
                // Call C - Incoming
                // Here we need to disconnect A prior to holding B so that C can be answered.
                // This case is driven by telephony requirements ultimately.
                Call heldCall = getHeldCallByConnectionService(call.getTargetPhoneAccount());
                if (heldCall != null) {
                    heldCall.disconnect();
                    Log.i(this, "holdActiveCallForNewCall: Disconnect held call %s before "
                                    + "holding active call %s.",
                            heldCall.getId(), activeCall.getId());
                }
                Log.i(this, "holdActiveCallForNewCall: Holding active %s before making %s active.",
                        activeCall.getId(), call.getId());
                activeCall.hold();
                return true;
            } else {
                // This call does not support hold. If it is from a different connection
                // service or connection manager, then disconnect it, otherwise allow the connection
                // service or connection manager to figure out the right states.
                if (!areFromSameSource(activeCall, call)) {
                    Log.i(this, "holdActiveCallForNewCall: disconnecting %s so that %s can be "
                            + "made active.", activeCall.getId(), call.getId());
                    if (!activeCall.isEmergencyCall()) {
                        activeCall.disconnect();
                    } else {
                        // It's not possible to hold the active call, and its an emergency call so
                        // we will silently reject the incoming call instead of answering it.
                        Log.w(this, "holdActiveCallForNewCall: rejecting incoming call %s as "
                                + "the active call is an emergency call and it cannot be held.",
                                call.getId());
                        call.reject(false /* rejectWithMessage */, "" /* message */,
                                "active emergency call can't be held");
                    }
                }
            }
        }
        return false;
    }

    @VisibleForTesting
    public void markCallAsActive(Call call) {
        if (call.isSelfManaged()) {
            // backward compatibility, the self-managed connection service will set the call state
            // to active directly. We should hold or disconnect the current active call based on the
            // holdability, and request the call focus for the self-managed call before the state
            // change.
            holdActiveCallForNewCall(call);
            mConnectionSvrFocusMgr.requestFocus(
                    call,
                    new RequestCallback(new ActionSetCallState(
                            call,
                            CallState.ACTIVE,
                            "active set explicitly for self-managed")));
        } else {
            if (mPendingAudioProcessingCall == call) {
                if (mCalls.contains(call)) {
                    setCallState(call, CallState.AUDIO_PROCESSING, "active set explicitly");
                } else {
                    call.setState(CallState.AUDIO_PROCESSING, "active set explicitly and adding");
                    addCall(call);
                }
                // Clear mPendingAudioProcessingCall so that future attempts to mark the call as
                // active (e.g. coming off of hold) don't put the call into audio processing instead
                mPendingAudioProcessingCall = null;
                return;
            }
            setCallState(call, CallState.ACTIVE, "active set explicitly");
            maybeMoveToSpeakerPhone(call);
            ensureCallAudible();
        }
    }

    @VisibleForTesting
    public void markCallAsOnHold(Call call) {
        setCallState(call, CallState.ON_HOLD, "on-hold set explicitly");
    }

    /**
     * Marks the specified call as STATE_DISCONNECTED and notifies the in-call app. If this was the
     * last live call, then also disconnect from the in-call controller.
     *
     * @param disconnectCause The disconnect cause, see {@link android.telecom.DisconnectCause}.
     */
    void markCallAsDisconnected(Call call, DisconnectCause disconnectCause) {
      int oldState = call.getState();
      if (call.getState() == CallState.SIMULATED_RINGING
                && disconnectCause.getCode() == DisconnectCause.REMOTE) {
            // If the remote end hangs up while in SIMULATED_RINGING, the call should
            // be marked as missed.
            call.setOverrideDisconnectCauseCode(new DisconnectCause(DisconnectCause.MISSED));
        }
        call.setDisconnectCause(disconnectCause);
        setCallState(call, CallState.DISCONNECTED, "disconnected set explicitly");

        if(oldState == CallState.NEW && disconnectCause.getCode() == DisconnectCause.MISSED) {
            Log.i(this, "markCallAsDisconnected: logging missed call ");
            mCallLogManager.logCall(call, Calls.MISSED_TYPE, true, null);
        }

    }

    /**
     * Removes an existing disconnected call, and notifies the in-call app.
     */
    void markCallAsRemoved(Call call) {
        mInCallController.getBindingFuture().thenRunAsync(() -> {
            call.maybeCleanupHandover();
            removeCall(call);
            Call foregroundCall = mCallAudioManager.getPossiblyHeldForegroundCall();
            if (mLocallyDisconnectingCalls.contains(call)) {
                boolean isDisconnectingChildCall = call.isDisconnectingChildCall();
                Log.v(this, "markCallAsRemoved: isDisconnectingChildCall = "
                        + isDisconnectingChildCall + "call -> %s", call);
                mLocallyDisconnectingCalls.remove(call);
                // Auto-unhold the foreground call due to a locally disconnected call, except if the
                // call which was disconnected is a member of a conference (don't want to auto
                // un-hold the conference if we remove a member of the conference).
                if (!isDisconnectingChildCall && foregroundCall != null
                        && foregroundCall.getState() == CallState.ON_HOLD) {
                    foregroundCall.unhold();
                }
            } else if (foregroundCall != null &&
                    !foregroundCall.can(Connection.CAPABILITY_SUPPORT_HOLD) &&
                    foregroundCall.getState() == CallState.ON_HOLD) {

                // The new foreground call is on hold, however the carrier does not display the hold
                // button in the UI.  Therefore, we need to auto unhold the held call since the user
                // has no means of unholding it themselves.
                Log.i(this, "Auto-unholding held foreground call (call doesn't support hold)");
                foregroundCall.unhold();
            }
        }, new LoggedHandlerExecutor(mHandler, "CM.mCAR", mLock));
    }

    /**
     * Given a call, marks the call as disconnected and removes it.  Set the error message to
     * indicate to the user that the call cannot me placed due to an ongoing call in another app.
     *
     * Used when there are ongoing self-managed calls and the user tries to make an outgoing managed
     * call.  Called by {@link #startCallConfirmation} when the user is already confirming an
     * outgoing call.  Realistically this should almost never be called since in practice the user
     * won't make multiple outgoing calls at the same time.
     *
     * @param call The call to mark as disconnected.
     */
    void markCallDisconnectedDueToSelfManagedCall(Call call) {
        Call activeCall = getActiveCall();
        CharSequence errorMessage;
        if (activeCall == null) {
            // Realistically this shouldn't happen, but best to handle gracefully
            errorMessage = mContext.getText(R.string.cant_call_due_to_ongoing_unknown_call);
        } else {
            errorMessage = mContext.getString(R.string.cant_call_due_to_ongoing_call,
                    activeCall.getTargetPhoneAccountLabel());
        }
        // Call is managed and there are ongoing self-managed calls.
        markCallAsDisconnected(call, new DisconnectCause(DisconnectCause.ERROR,
                errorMessage, errorMessage, "Ongoing call in another app."));
        markCallAsRemoved(call);
    }

    /**
     * Cleans up any calls currently associated with the specified connection service when the
     * service binder disconnects unexpectedly.
     *
     * @param service The connection service that disconnected.
     */
    void handleConnectionServiceDeath(ConnectionServiceWrapper service) {
        if (service != null) {
            Log.i(this, "handleConnectionServiceDeath: service %s died", service);
            for (Call call : mCalls) {
                if (call.getConnectionService() == service) {
                    if (call.getState() != CallState.DISCONNECTED) {
                        markCallAsDisconnected(call, new DisconnectCause(DisconnectCause.ERROR,
                                null /* message */, null /* description */, "CS_DEATH",
                                ToneGenerator.TONE_PROP_PROMPT));
                    }
                    markCallAsRemoved(call);
                }
            }
        }
    }

    /**
     * Determines if the {@link CallsManager} has any non-external calls.
     *
     * @return {@code True} if there are any non-external calls, {@code false} otherwise.
     */
    boolean hasAnyCalls() {
        if (mCalls.isEmpty()) {
            return false;
        }

        for (Call call : mCalls) {
            if (!call.isExternalCall()) {
                return true;
            }
        }
        return false;
    }

    boolean hasActiveOrHoldingCall() {
        return getFirstCallWithState(CallState.ACTIVE, CallState.ON_HOLD) != null;
    }

    boolean hasRingingCall() {
        return getFirstCallWithState(CallState.RINGING, CallState.ANSWERED) != null;
    }

    boolean hasRingingOrSimulatedRingingCall() {
        return getFirstCallWithState(
                CallState.SIMULATED_RINGING, CallState.RINGING, CallState.ANSWERED) != null;
    }

    @VisibleForTesting
    public boolean onMediaButton(int type) {
        if (hasAnyCalls()) {
            Call ringingCall = getFirstCallWithState(CallState.RINGING,
                    CallState.SIMULATED_RINGING);
            if (HeadsetMediaButton.SHORT_PRESS == type) {
                if (ringingCall == null) {
                    Call activeCall = getFirstCallWithState(CallState.ACTIVE);
                    Call onHoldCall = getFirstCallWithState(CallState.ON_HOLD);
                    if (activeCall != null && onHoldCall != null) {
                        // Two calls, short-press -> switch calls
                        Log.addEvent(onHoldCall, LogUtils.Events.INFO,
                                "two calls, media btn short press - switch call.");
                        unholdCall(onHoldCall);
                        return true;
                    }

                    Call callToHangup = getFirstCallWithState(CallState.RINGING, CallState.DIALING,
                            CallState.PULLING, CallState.ACTIVE, CallState.ON_HOLD);
                    Log.addEvent(callToHangup, LogUtils.Events.INFO,
                            "media btn short press - end call.");
                    if (callToHangup != null) {
                        disconnectCall(callToHangup);
                        return true;
                    }
                } else {
                    answerCall(ringingCall, VideoProfile.STATE_AUDIO_ONLY);
                    return true;
                }
            } else if (HeadsetMediaButton.LONG_PRESS == type) {
                if (ringingCall != null) {
                    Log.addEvent(getForegroundCall(),
                            LogUtils.Events.INFO, "media btn long press - reject");
                    ringingCall.reject(false, null);
                } else {
                    Call activeCall = getFirstCallWithState(CallState.ACTIVE);
                    Call onHoldCall = getFirstCallWithState(CallState.ON_HOLD);
                    if (activeCall != null && onHoldCall != null) {
                        // Two calls, long-press -> end current call
                        Log.addEvent(activeCall, LogUtils.Events.INFO,
                                "two calls, media btn long press - end current call.");
                        disconnectCall(activeCall);
                        return true;
                    }

                    Log.addEvent(getForegroundCall(), LogUtils.Events.INFO,
                            "media btn long press - mute");
                    mCallAudioManager.toggleMute();
                }
                return true;
            }
        }
        return false;
    }

    /**
     * Returns true if telecom supports adding another top-level call.
     */
    @VisibleForTesting
    public boolean canAddCall() {
        boolean isDeviceProvisioned = Settings.Global.getInt(mContext.getContentResolver(),
                Settings.Global.DEVICE_PROVISIONED, 0) != 0;
        if (!isDeviceProvisioned) {
            Log.d(TAG, "Device not provisioned, canAddCall is false.");
            return false;
        }

        if (getFirstCallWithState(OUTGOING_CALL_STATES) != null) {
            return false;
        }

        int count = 0;
        for (Call call : mCalls) {
            if (call.isEmergencyCall()) {
                // We never support add call if one of the calls is an emergency call.
                return false;
            } else if (call.isExternalCall()) {
                // External calls don't count.
                continue;
            } else if (call.getParentCall() == null) {
                count++;
            }
            Bundle extras = call.getExtras();
            if (extras != null) {
                if (extras.getBoolean(Connection.EXTRA_DISABLE_ADD_CALL, false)) {
                    return false;
                }
            }

            // We do not check states for canAddCall. We treat disconnected calls the same
            // and wait until they are removed instead. If we didn't count disconnected calls,
            // we could put InCallServices into a state where they are showing two calls but
            // also support add-call. Technically it's right, but overall looks better (UI-wise)
            // and acts better if we wait until the call is removed.
            if (count >= MAXIMUM_TOP_LEVEL_CALLS) {
                return false;
            }
        }

        return true;
    }

    @VisibleForTesting
    public Call getRingingOrSimulatedRingingCall() {
        return getFirstCallWithState(CallState.RINGING,
                CallState.ANSWERED, CallState.SIMULATED_RINGING);
    }

    public Call getActiveCall() {
        return getFirstCallWithState(CallState.ACTIVE);
    }

    Call getDialingCall() {
        return getFirstCallWithState(CallState.DIALING);
    }

    @VisibleForTesting
    public Call getHeldCall() {
        return getFirstCallWithState(CallState.ON_HOLD);
    }

    public Call getHeldCallByConnectionService(PhoneAccountHandle targetPhoneAccount) {
        Optional<Call> heldCall = mCalls.stream()
                .filter(call -> PhoneAccountHandle.areFromSamePackage(call.getTargetPhoneAccount(),
                        targetPhoneAccount)
                        && call.getParentCall() == null
                        && call.getState() == CallState.ON_HOLD)
                .findFirst();
        return heldCall.isPresent() ? heldCall.get() : null;
    }

    @VisibleForTesting
    public int getNumHeldCalls() {
        int count = 0;
        for (Call call : mCalls) {
            if (call.getParentCall() == null && call.getState() == CallState.ON_HOLD) {
                count++;
            }
        }
        return count;
    }

    @VisibleForTesting
    public Call getOutgoingCall() {
        return getFirstCallWithState(OUTGOING_CALL_STATES);
    }

    @VisibleForTesting
    public Call getFirstCallWithState(int... states) {
        return getFirstCallWithState(null, states);
    }

    @VisibleForTesting
    public PhoneNumberUtilsAdapter getPhoneNumberUtilsAdapter() {
        return mPhoneNumberUtilsAdapter;
    }

    @VisibleForTesting
    public CompletableFuture<Call> getLatestPostSelectionProcessingFuture() {
        return mLatestPostSelectionProcessingFuture;
    }

    @VisibleForTesting
    public CompletableFuture getLatestPreAccountSelectionFuture() {
        return mLatestPreAccountSelectionFuture;
    }

    /**
     * Returns the first call that it finds with the given states. The states are treated as having
     * priority order so that any call with the first state will be returned before any call with
     * states listed later in the parameter list.
     *
     * @param callToSkip Call that this method should skip while searching
     */
    Call getFirstCallWithState(Call callToSkip, int... states) {
        for (int currentState : states) {
            // check the foreground first
            Call foregroundCall = getForegroundCall();
            if (foregroundCall != null && foregroundCall.getState() == currentState) {
                return foregroundCall;
            }

            for (Call call : mCalls) {
                if (Objects.equals(callToSkip, call)) {
                    continue;
                }

                // Only operate on top-level calls
                if (call.getParentCall() != null) {
                    continue;
                }

                if (call.isExternalCall()) {
                    continue;
                }

                if (currentState == call.getState()) {
                    return call;
                }
            }
        }
        return null;
    }

    Call createConferenceCall(
            String callId,
            PhoneAccountHandle phoneAccount,
            ParcelableConference parcelableConference) {

        // If the parceled conference specifies a connect time, use it; otherwise default to 0,
        // which is the default value for new Calls.
        long connectTime =
                parcelableConference.getConnectTimeMillis() ==
                        Conference.CONNECT_TIME_NOT_SPECIFIED ? 0 :
                        parcelableConference.getConnectTimeMillis();
        long connectElapsedTime =
                parcelableConference.getConnectElapsedTimeMillis() ==
                        Conference.CONNECT_TIME_NOT_SPECIFIED ? 0 :
                        parcelableConference.getConnectElapsedTimeMillis();

        int callDirection = Call.getRemappedCallDirection(parcelableConference.getCallDirection());

        PhoneAccountHandle connectionMgr =
                    mPhoneAccountRegistrar.getSimCallManagerFromHandle(phoneAccount,
                            mCurrentUserHandle);
        Call call = new Call(
                callId,
                mContext,
                this,
                mLock,
                mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                null /* handle */,
                null /* gatewayInfo */,
                connectionMgr,
                phoneAccount,
                callDirection,
                false /* forceAttachToExistingConnection */,
                true /* isConference */,
                connectTime,
                connectElapsedTime,
                mClockProxy,
                mToastFactory);

        setCallState(call, Call.getStateFromConnectionState(parcelableConference.getState()),
                "new conference call");
        call.setHandle(parcelableConference.getHandle(),
                parcelableConference.getHandlePresentation());
        call.setConnectionCapabilities(parcelableConference.getConnectionCapabilities());
        call.setConnectionProperties(parcelableConference.getConnectionProperties());
        call.setVideoState(parcelableConference.getVideoState());
        call.setVideoProvider(parcelableConference.getVideoProvider());
        call.setStatusHints(parcelableConference.getStatusHints());
        call.putExtras(Call.SOURCE_CONNECTION_SERVICE, parcelableConference.getExtras());
        // In case this Conference was added via a ConnectionManager, keep track of the original
        // Connection ID as created by the originating ConnectionService.
        Bundle extras = parcelableConference.getExtras();
        if (extras != null && extras.containsKey(Connection.EXTRA_ORIGINAL_CONNECTION_ID)) {
            call.setOriginalConnectionId(extras.getString(Connection.EXTRA_ORIGINAL_CONNECTION_ID));
        }

        // TODO: Move this to be a part of addCall()
        call.addListener(this);
        addCall(call);
        return call;
    }

    /**
     * @return the call state currently tracked by {@link PhoneStateBroadcaster}
     */
    int getCallState() {
        return mPhoneStateBroadcaster.getCallState();
    }

    /**
     * Retrieves the {@link PhoneAccountRegistrar}.
     *
     * @return The {@link PhoneAccountRegistrar}.
     */
    @VisibleForTesting
    public PhoneAccountRegistrar getPhoneAccountRegistrar() {
        return mPhoneAccountRegistrar;
    }

    /**
     * Retrieves the {@link DisconnectedCallNotifier}
     * @return The {@link DisconnectedCallNotifier}.
     */
    DisconnectedCallNotifier getDisconnectedCallNotifier() {
        return mDisconnectedCallNotifier;
    }

    /**
     * Retrieves the {@link MissedCallNotifier}
     * @return The {@link MissedCallNotifier}.
     */
    MissedCallNotifier getMissedCallNotifier() {
        return mMissedCallNotifier;
    }

    /**
     * Retrieves the {@link IncomingCallNotifier}.
     * @return The {@link IncomingCallNotifier}.
     */
    IncomingCallNotifier getIncomingCallNotifier() {
        return mIncomingCallNotifier;
    }

    /**
     * Reject an incoming call and manually add it to the Call Log.
     * @param incomingCall Incoming call that has been rejected
     */
    private void rejectCallAndLog(Call incomingCall, CallFilteringResult result) {
        if (incomingCall.getConnectionService() != null) {
            // Only reject the call if it has not already been destroyed.  If a call ends while
            // incoming call filtering is taking place, it is possible that the call has already
            // been destroyed, and as such it will be impossible to send the reject to the
            // associated ConnectionService.
            incomingCall.reject(false, null);
        } else {
            Log.i(this, "rejectCallAndLog - call already destroyed.");
        }

        // Since the call was not added to the list of calls, we have to call the missed
        // call notifier and the call logger manually.
        // Do we need missed call notification for direct to Voicemail calls?
        mCallLogManager.logCall(incomingCall, Calls.MISSED_TYPE,
                true /*showNotificationForMissedCall*/, result);
    }

    /**
     * Adds the specified call to the main list of live calls.
     *
     * @param call The call to add.
     */
    @VisibleForTesting
    public void addCall(Call call) {
        Trace.beginSection("addCall");
        Log.v(this, "addCall(%s)", call);
        call.addListener(this);
        mCalls.add(call);

        // Specifies the time telecom finished routing the call. This is used by the dialer for
        // analytics.
        Bundle extras = call.getIntentExtras();
        extras.putLong(TelecomManager.EXTRA_CALL_TELECOM_ROUTING_END_TIME_MILLIS,
                SystemClock.elapsedRealtime());

        updateCanAddCall();
        updateHasActiveRttCall();
        updateExternalCallCanPullSupport();
        // onCallAdded for calls which immediately take the foreground (like the first call).
        for (CallsManagerListener listener : mListeners) {
            if (LogUtils.SYSTRACE_DEBUG) {
                Trace.beginSection(listener.getClass().toString() + " addCall");
            }
            listener.onCallAdded(call);
            if (LogUtils.SYSTRACE_DEBUG) {
                Trace.endSection();
            }
        }
        Trace.endSection();
    }

    @VisibleForTesting
    public void removeCall(Call call) {
        Trace.beginSection("removeCall");
        Log.v(this, "removeCall(%s)", call);

        call.setParentAndChildCall(null);  // clean up parent relationship before destroying.
        call.removeListener(this);
        call.clearConnectionService();
        // TODO: clean up RTT pipes

        boolean shouldNotify = false;
        if (mCalls.contains(call)) {
            mCalls.remove(call);
            shouldNotify = true;
        }

        call.destroy();
        updateExternalCallCanPullSupport();
        // Only broadcast changes for calls that are being tracked.
        if (shouldNotify) {
            updateCanAddCall();
            updateHasActiveRttCall();
            for (CallsManagerListener listener : mListeners) {
                if (LogUtils.SYSTRACE_DEBUG) {
                    Trace.beginSection(listener.getClass().toString() + " onCallRemoved");
                }
                listener.onCallRemoved(call);
                if (LogUtils.SYSTRACE_DEBUG) {
                    Trace.endSection();
                }
            }
        }
        Trace.endSection();
    }

    private void updateHasActiveRttCall() {
        boolean hasActiveRttCall = hasActiveRttCall();
        if (hasActiveRttCall != mHasActiveRttCall) {
            Log.i(this, "updateHasActiveRttCall %s -> %s", mHasActiveRttCall, hasActiveRttCall);
            AudioManager.setRttEnabled(hasActiveRttCall);
            mHasActiveRttCall = hasActiveRttCall;
        }
    }

    private boolean hasActiveRttCall() {
        for (Call call : mCalls) {
            if (call.isActive() && call.isRttCall()) {
                return true;
            }
        }
        return false;
    }

    /**
     * Sets the specified state on the specified call.
     *
     * @param call The call.
     * @param newState The new state of the call.
     */
    private void setCallState(Call call, int newState, String tag) {
        if (call == null) {
            return;
        }
        int oldState = call.getState();
        Log.i(this, "setCallState %s -> %s, call: %s",
                CallState.toString(call.getParcelableCallState()),
                CallState.toString(newState), call);
        if (newState != oldState) {
            // If the call switches to held state while a DTMF tone is playing, stop the tone to
            // ensure that the tone generator stops playing the tone.
            if (newState == CallState.ON_HOLD && call.isDtmfTonePlaying()) {
                stopDtmfTone(call);
            }

            // Unfortunately, in the telephony world the radio is king. So if the call notifies
            // us that the call is in a particular state, we allow it even if it doesn't make
            // sense (e.g., STATE_ACTIVE -> STATE_RINGING).
            // TODO: Consider putting a stop to the above and turning CallState
            // into a well-defined state machine.
            // TODO: Define expected state transitions here, and log when an
            // unexpected transition occurs.
            if (call.setState(newState, tag)) {
                if ((oldState != CallState.AUDIO_PROCESSING) &&
                        (newState == CallState.DISCONNECTED)) {
                    maybeSendPostCallScreenIntent(call);
                }
                maybeShowErrorDialogOnDisconnect(call);

                Trace.beginSection("onCallStateChanged");

                maybeHandleHandover(call, newState);

                // Only broadcast state change for calls that are being tracked.
                if (mCalls.contains(call)) {
                    updateCanAddCall();
                    updateHasActiveRttCall();
                    for (CallsManagerListener listener : mListeners) {
                        if (LogUtils.SYSTRACE_DEBUG) {
                            Trace.beginSection(listener.getClass().toString() +
                                    " onCallStateChanged");
                        }
                        listener.onCallStateChanged(call, oldState, newState);
                        if (LogUtils.SYSTRACE_DEBUG) {
                            Trace.endSection();
                        }
                    }
                }
                Trace.endSection();
            } else {
                Log.i(this, "failed in setting the state to new state");
            }
        }
    }

    /**
     * Identifies call state transitions for a call which trigger handover events.
     * - If this call has a handover to it which just started and this call goes active, treat
     * this as if the user accepted the handover.
     * - If this call has a handover to it which just started and this call is disconnected, treat
     * this as if the user rejected the handover.
     * - If this call has a handover from it which just started and this call is disconnected, do
     * nothing as the call prematurely disconnected before the user accepted the handover.
     * - If this call has a handover from it which was already accepted by the user and this call is
     * disconnected, mark the handover as complete.
     *
     * @param call A call whose state is changing.
     * @param newState The new state of the call.
     */
    private void maybeHandleHandover(Call call, int newState) {
        if (call.getHandoverSourceCall() != null) {
            // We are handing over another call to this one.
            if (call.getHandoverState() == HandoverState.HANDOVER_TO_STARTED) {
                // A handover to this call has just been initiated.
                if (newState == CallState.ACTIVE) {
                    // This call went active, so the user has accepted the handover.
                    Log.i(this, "setCallState: handover to accepted");
                    acceptHandoverTo(call);
                } else if (newState == CallState.DISCONNECTED) {
                    // The call was disconnected, so the user has rejected the handover.
                    Log.i(this, "setCallState: handover to rejected");
                    rejectHandoverTo(call);
                }
            }
        // If this call was disconnected because it was handed over TO another call, report the
        // handover as complete.
        } else if (call.getHandoverDestinationCall() != null
                && newState == CallState.DISCONNECTED) {
            int handoverState = call.getHandoverState();
            if (handoverState == HandoverState.HANDOVER_FROM_STARTED) {
                // Disconnect before handover was accepted.
                Log.i(this, "setCallState: disconnect before handover accepted");
                // Let the handover destination know that the source has disconnected prior to
                // completion of the handover.
                call.getHandoverDestinationCall().sendCallEvent(
                        android.telecom.Call.EVENT_HANDOVER_SOURCE_DISCONNECTED, null);
            } else if (handoverState == HandoverState.HANDOVER_ACCEPTED) {
                Log.i(this, "setCallState: handover from complete");
                completeHandoverFrom(call);
            }
        }
    }

    private void completeHandoverFrom(Call call) {
        Call handoverTo = call.getHandoverDestinationCall();
        Log.addEvent(handoverTo, LogUtils.Events.HANDOVER_COMPLETE, "from=%s, to=%s",
                call.getId(), handoverTo.getId());
        Log.addEvent(call, LogUtils.Events.HANDOVER_COMPLETE, "from=%s, to=%s",
                call.getId(), handoverTo.getId());

        // Inform the "from" Call (ie the source call) that the handover from it has
        // completed; this allows the InCallService to be notified that a handover it
        // initiated completed.
        call.onConnectionEvent(Connection.EVENT_HANDOVER_COMPLETE, null);
        call.onHandoverComplete();

        // Inform the "to" ConnectionService that handover to it has completed.
        handoverTo.sendCallEvent(android.telecom.Call.EVENT_HANDOVER_COMPLETE, null);
        handoverTo.onHandoverComplete();
        answerCall(handoverTo, handoverTo.getVideoState());
        call.markFinishedHandoverStateAndCleanup(HandoverState.HANDOVER_COMPLETE);

        // If the call we handed over to is self-managed, we need to disconnect the calls for other
        // ConnectionServices.
        if (handoverTo.isSelfManaged()) {
            disconnectOtherCalls(handoverTo.getTargetPhoneAccount());
        }
    }

    private void rejectHandoverTo(Call handoverTo) {
        Call handoverFrom = handoverTo.getHandoverSourceCall();
        Log.i(this, "rejectHandoverTo: from=%s, to=%s", handoverFrom.getId(), handoverTo.getId());
        Log.addEvent(handoverFrom, LogUtils.Events.HANDOVER_FAILED, "from=%s, to=%s, rejected",
                handoverTo.getId(), handoverFrom.getId());
        Log.addEvent(handoverTo, LogUtils.Events.HANDOVER_FAILED, "from=%s, to=%s, rejected",
                handoverTo.getId(), handoverFrom.getId());

        // Inform the "from" Call (ie the source call) that the handover from it has
        // failed; this allows the InCallService to be notified that a handover it
        // initiated failed.
        handoverFrom.onConnectionEvent(Connection.EVENT_HANDOVER_FAILED, null);
        handoverFrom.onHandoverFailed(android.telecom.Call.Callback.HANDOVER_FAILURE_USER_REJECTED);

        // Inform the "to" ConnectionService that handover to it has failed.  This
        // allows the ConnectionService the call was being handed over
        if (handoverTo.getConnectionService() != null) {
            // Only attempt if the call has a bound ConnectionService if handover failed
            // early on in the handover process, the CS will be unbound and we won't be
            // able to send the call event.
            handoverTo.sendCallEvent(android.telecom.Call.EVENT_HANDOVER_FAILED, null);
            handoverTo.getConnectionService().handoverFailed(handoverTo,
                    android.telecom.Call.Callback.HANDOVER_FAILURE_USER_REJECTED);
        }
        handoverTo.markFinishedHandoverStateAndCleanup(HandoverState.HANDOVER_FAILED);
    }

    private void acceptHandoverTo(Call handoverTo) {
        Call handoverFrom = handoverTo.getHandoverSourceCall();
        Log.i(this, "acceptHandoverTo: from=%s, to=%s", handoverFrom.getId(), handoverTo.getId());
        handoverTo.setHandoverState(HandoverState.HANDOVER_ACCEPTED);
        handoverTo.onHandoverComplete();
        handoverFrom.setHandoverState(HandoverState.HANDOVER_ACCEPTED);
        handoverFrom.onHandoverComplete();

        Log.addEvent(handoverTo, LogUtils.Events.ACCEPT_HANDOVER, "from=%s, to=%s",
                handoverFrom.getId(), handoverTo.getId());
        Log.addEvent(handoverFrom, LogUtils.Events.ACCEPT_HANDOVER, "from=%s, to=%s",
                handoverFrom.getId(), handoverTo.getId());

        // Disconnect the call we handed over from.
        disconnectCall(handoverFrom);
        // If we handed over to a self-managed ConnectionService, we need to disconnect calls for
        // other ConnectionServices.
        if (handoverTo.isSelfManaged()) {
            disconnectOtherCalls(handoverTo.getTargetPhoneAccount());
        }
    }

    private void updateCanAddCall() {
        boolean newCanAddCall = canAddCall();
        if (newCanAddCall != mCanAddCall) {
            mCanAddCall = newCanAddCall;
            for (CallsManagerListener listener : mListeners) {
                if (LogUtils.SYSTRACE_DEBUG) {
                    Trace.beginSection(listener.getClass().toString() + " updateCanAddCall");
                }
                listener.onCanAddCallChanged(mCanAddCall);
                if (LogUtils.SYSTRACE_DEBUG) {
                    Trace.endSection();
                }
            }
        }
    }

    private boolean isPotentialMMICode(Uri handle) {
        return (handle != null && handle.getSchemeSpecificPart() != null
                && handle.getSchemeSpecificPart().contains("#"));
    }

    /**
     * Determines if a dialed number is potentially an In-Call MMI code.  In-Call MMI codes are
     * MMI codes which can be dialed when one or more calls are in progress.
     * <P>
     * Checks for numbers formatted similar to the MMI codes defined in:
     * {@link com.android.internal.telephony.Phone#handleInCallMmiCommands(String)}
     *
     * @param handle The URI to call.
     * @return {@code True} if the URI represents a number which could be an in-call MMI code.
     */
    private boolean isPotentialInCallMMICode(Uri handle) {
        if (handle != null && handle.getSchemeSpecificPart() != null &&
                handle.getScheme() != null &&
                handle.getScheme().equals(PhoneAccount.SCHEME_TEL)) {

            String dialedNumber = handle.getSchemeSpecificPart();
            return (dialedNumber.equals("0") ||
                    (dialedNumber.startsWith("1") && dialedNumber.length() <= 2) ||
                    (dialedNumber.startsWith("2") && dialedNumber.length() <= 2) ||
                    dialedNumber.equals("3") ||
                    dialedNumber.equals("4") ||
                    dialedNumber.equals("5"));
        }
        return false;
    }

    @VisibleForTesting
    public int getNumCallsWithState(final boolean isSelfManaged, Call excludeCall,
                                    PhoneAccountHandle phoneAccountHandle, int... states) {
        return getNumCallsWithState(isSelfManaged ? CALL_FILTER_SELF_MANAGED : CALL_FILTER_MANAGED,
                excludeCall, phoneAccountHandle, states);
    }

    /**
     * Determines the number of calls matching the specified criteria.
     * @param callFilter indicates whether to include just managed calls
     *                   ({@link #CALL_FILTER_MANAGED}), self-managed calls
     *                   ({@link #CALL_FILTER_SELF_MANAGED}), or all calls
     *                   ({@link #CALL_FILTER_ALL}).
     * @param excludeCall Where {@code non-null}, this call is excluded from the count.
     * @param phoneAccountHandle Where {@code non-null}, calls for this {@link PhoneAccountHandle}
     *                           are excluded from the count.
     * @param states The list of {@link CallState}s to include in the count.
     * @return Count of calls matching criteria.
     */
    @VisibleForTesting
    public int getNumCallsWithState(final int callFilter, Call excludeCall,
                                    PhoneAccountHandle phoneAccountHandle, int... states) {

        Set<Integer> desiredStates = IntStream.of(states).boxed().collect(Collectors.toSet());

        Stream<Call> callsStream = mCalls.stream()
                .filter(call -> desiredStates.contains(call.getState()) &&
                        call.getParentCall() == null && !call.isExternalCall());

        if (callFilter == CALL_FILTER_MANAGED) {
            callsStream = callsStream.filter(call -> !call.isSelfManaged());
        } else if (callFilter == CALL_FILTER_SELF_MANAGED) {
            callsStream = callsStream.filter(call -> call.isSelfManaged());
        }

        // If a call to exclude was specified, filter it out.
        if (excludeCall != null) {
            callsStream = callsStream.filter(call -> call != excludeCall);
        }

        // If a phone account handle was specified, only consider calls for that phone account.
        if (phoneAccountHandle != null) {
            callsStream = callsStream.filter(
                    call -> phoneAccountHandle.equals(call.getTargetPhoneAccount()));
        }

        return (int) callsStream.count();
    }

    private boolean hasMaximumLiveCalls(Call exceptCall) {
        return MAXIMUM_LIVE_CALLS <= getNumCallsWithState(CALL_FILTER_ALL,
                exceptCall, null /* phoneAccountHandle*/, LIVE_CALL_STATES);
    }

    private boolean hasMaximumManagedLiveCalls(Call exceptCall) {
        return MAXIMUM_LIVE_CALLS <= getNumCallsWithState(false /* isSelfManaged */,
                exceptCall, null /* phoneAccountHandle */, LIVE_CALL_STATES);
    }

    private boolean hasMaximumSelfManagedCalls(Call exceptCall,
                                                   PhoneAccountHandle phoneAccountHandle) {
        return MAXIMUM_SELF_MANAGED_CALLS <= getNumCallsWithState(true /* isSelfManaged */,
                exceptCall, phoneAccountHandle, ANY_CALL_STATE);
    }

    private boolean hasMaximumManagedHoldingCalls(Call exceptCall) {
        return MAXIMUM_HOLD_CALLS <= getNumCallsWithState(false /* isSelfManaged */, exceptCall,
                null /* phoneAccountHandle */, CallState.ON_HOLD);
    }

    private boolean hasMaximumManagedRingingCalls(Call exceptCall) {
        return MAXIMUM_RINGING_CALLS <= getNumCallsWithState(false /* isSelfManaged */, exceptCall,
                null /* phoneAccountHandle */, CallState.RINGING, CallState.ANSWERED);
    }

    private boolean hasMaximumSelfManagedRingingCalls(Call exceptCall,
                                                      PhoneAccountHandle phoneAccountHandle) {
        return MAXIMUM_RINGING_CALLS <= getNumCallsWithState(true /* isSelfManaged */, exceptCall,
                phoneAccountHandle, CallState.RINGING, CallState.ANSWERED);
    }

    private boolean hasMaximumOutgoingCalls(Call exceptCall) {
        return MAXIMUM_LIVE_CALLS <= getNumCallsWithState(CALL_FILTER_ALL,
                exceptCall, null /* phoneAccountHandle */, OUTGOING_CALL_STATES);
    }

    private boolean hasMaximumManagedOutgoingCalls(Call exceptCall) {
        return MAXIMUM_OUTGOING_CALLS <= getNumCallsWithState(false /* isSelfManaged */, exceptCall,
                null /* phoneAccountHandle */, OUTGOING_CALL_STATES);
    }

    private boolean hasMaximumManagedDialingCalls(Call exceptCall) {
        return MAXIMUM_DIALING_CALLS <= getNumCallsWithState(false /* isSelfManaged */, exceptCall,
                null /* phoneAccountHandle */, CallState.DIALING, CallState.PULLING);
    }

    /**
     * Given a {@link PhoneAccountHandle} determines if there are other unholdable calls owned by
     * another connection service.
     * @param phoneAccountHandle The {@link PhoneAccountHandle} to check.
     * @return {@code true} if there are other unholdable calls, {@code false} otherwise.
     */
    public boolean hasUnholdableCallsForOtherConnectionService(
            PhoneAccountHandle phoneAccountHandle) {
        return getNumUnholdableCallsForOtherConnectionService(phoneAccountHandle) > 0;
    }

    /**
     * Determines the number of unholdable calls present in a connection service other than the one
     * the passed phone account belonds to.
     * @param phoneAccountHandle The handle of the PhoneAccount.
     * @return Number of unholdable calls owned by other connection service.
     */
    public int getNumUnholdableCallsForOtherConnectionService(
            PhoneAccountHandle phoneAccountHandle) {
        return (int) mCalls.stream().filter(call ->
                !phoneAccountHandle.getComponentName().equals(
                        call.getTargetPhoneAccount().getComponentName())
                        && call.getParentCall() == null
                        && !call.isExternalCall()
                        && !canHold(call)).count();
    }

    /**
     * Determines if there are any managed calls.
     * @return {@code true} if there are managed calls, {@code false} otherwise.
     */
    public boolean hasManagedCalls() {
        return mCalls.stream().filter(call -> !call.isSelfManaged() &&
                !call.isExternalCall()).count() > 0;
    }

    /**
     * Determines if there are any self-managed calls.
     * @return {@code true} if there are self-managed calls, {@code false} otherwise.
     */
    public boolean hasSelfManagedCalls() {
        return mCalls.stream().filter(call -> call.isSelfManaged()).count() > 0;
    }

    /**
     * Determines if there are any ongoing managed or self-managed calls.
     * Note: The {@link #ONGOING_CALL_STATES} are
     * @return {@code true} if there are ongoing managed or self-managed calls, {@code false}
     *      otherwise.
     */
    public boolean hasOngoingCalls() {
        return getNumCallsWithState(
                CALL_FILTER_ALL, null /* excludeCall */,
                null /* phoneAccountHandle */,
                ONGOING_CALL_STATES) > 0;
    }

    /**
     * Determines if there are any ongoing managed calls.
     * @return {@code true} if there are ongoing managed calls, {@code false} otherwise.
     */
    public boolean hasOngoingManagedCalls() {
        return getNumCallsWithState(
                CALL_FILTER_MANAGED, null /* excludeCall */,
                null /* phoneAccountHandle */,
                ONGOING_CALL_STATES) > 0;
    }

    /**
     * Determines if the system incoming call UI should be shown.
     * The system incoming call UI will be shown if the new incoming call is self-managed, and there
     * are ongoing calls for another PhoneAccount.
     * @param incomingCall The incoming call.
     * @return {@code true} if the system incoming call UI should be shown, {@code false} otherwise.
     */
    public boolean shouldShowSystemIncomingCallUi(Call incomingCall) {
        return incomingCall.isIncoming() && incomingCall.isSelfManaged()
                && hasUnholdableCallsForOtherConnectionService(incomingCall.getTargetPhoneAccount())
                && incomingCall.getHandoverSourceCall() == null;
    }

    @VisibleForTesting
    public boolean makeRoomForOutgoingEmergencyCall(Call emergencyCall) {
        // Always disconnect any ringing/incoming calls when an emergency call is placed to minimize
        // distraction. This does not affect live call count.
        if (hasRingingOrSimulatedRingingCall()) {
            Call ringingCall = getRingingOrSimulatedRingingCall();
            ringingCall.getAnalytics().setCallIsAdditional(true);
            ringingCall.getAnalytics().setCallIsInterrupted(true);
            if (ringingCall.getState() == CallState.SIMULATED_RINGING) {
                if (!ringingCall.hasGoneActiveBefore()) {
                    // If this is an incoming call that is currently in SIMULATED_RINGING only
                    // after a call screen, disconnect to make room and mark as missed, since
                    // the user didn't get a chance to accept/reject.
                    ringingCall.disconnect("emergency call dialed during simulated ringing "
                            + "after screen.");
                } else {
                    // If this is a simulated ringing call after being active and put in
                    // AUDIO_PROCESSING state again, disconnect normally.
                    ringingCall.reject(false, null, "emergency call dialed during simulated "
                            + "ringing.");
                }
            } else { // normal incoming ringing call.
                // Hang up the ringing call to make room for the emergency call and mark as missed,
                // since the user did not reject.
                ringingCall.setOverrideDisconnectCauseCode(
                        new DisconnectCause(DisconnectCause.MISSED));
                ringingCall.reject(false, null, "emergency call dialed during ringing.");
            }
        }

        // There is already room!
        if (!hasMaximumLiveCalls(emergencyCall)) return true;

        Call liveCall = getFirstCallWithState(LIVE_CALL_STATES);
        Log.i(this, "makeRoomForOutgoingEmergencyCall call = " + emergencyCall
                + " livecall = " + liveCall);

        if (emergencyCall == liveCall) {
            // Not likely, but a good sanity check.
            return true;
        }

        if (hasMaximumOutgoingCalls(emergencyCall)) {
            Call outgoingCall = getFirstCallWithState(OUTGOING_CALL_STATES);
            if (!outgoingCall.isEmergencyCall()) {
                emergencyCall.getAnalytics().setCallIsAdditional(true);
                outgoingCall.getAnalytics().setCallIsInterrupted(true);
                outgoingCall.disconnect("Disconnecting dialing call in favor of new dialing"
                        + " emergency call.");
                return true;
            }
            if (outgoingCall.getState() == CallState.SELECT_PHONE_ACCOUNT) {
                // Sanity check: if there is an orphaned emergency call in the
                // {@link CallState#SELECT_PHONE_ACCOUNT} state, just disconnect it since the user
                // has explicitly started a new call.
                emergencyCall.getAnalytics().setCallIsAdditional(true);
                outgoingCall.getAnalytics().setCallIsInterrupted(true);
                outgoingCall.disconnect("Disconnecting call in SELECT_PHONE_ACCOUNT in favor"
                        + " of new outgoing call.");
                return true;
            }
            //  If the user tries to make two outgoing calls to different emergency call numbers,
            //  we will try to connect the first outgoing call and reject the second.
            return false;
        }

        if (liveCall.getState() == CallState.AUDIO_PROCESSING) {
            emergencyCall.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            liveCall.disconnect("disconnecting audio processing call for emergency");
            return true;
        }

        // If we have the max number of held managed calls and we're placing an emergency call,
        // we'll disconnect the ongoing call if it cannot be held.
        if (hasMaximumManagedHoldingCalls(emergencyCall) && !canHold(liveCall)) {
            emergencyCall.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            // Disconnect the active call instead of the holding call because it is historically
            // easier to do, rather than disconnect a held call.
            liveCall.disconnect("disconnecting to make room for emergency call "
                    + emergencyCall.getId());
            return true;
        }

        // TODO: Remove once b/23035408 has been corrected.
        // If the live call is a conference, it will not have a target phone account set.  This
        // means the check to see if the live call has the same target phone account as the new
        // call will not cause us to bail early.  As a result, we'll end up holding the
        // ongoing conference call.  However, the ConnectionService is already doing that.  This
        // has caused problems with some carriers.  As a workaround until b/23035408 is
        // corrected, we will try and get the target phone account for one of the conference's
        // children and use that instead.
        PhoneAccountHandle liveCallPhoneAccount = liveCall.getTargetPhoneAccount();
        if (liveCallPhoneAccount == null && liveCall.isConference() &&
                !liveCall.getChildCalls().isEmpty()) {
            liveCallPhoneAccount = getFirstChildPhoneAccount(liveCall);
            Log.i(this, "makeRoomForOutgoingEmergencyCall: using child call PhoneAccount = " +
                    liveCallPhoneAccount);
        }

        // We may not know which PhoneAccount the emergency call will be placed on yet, but if
        // the liveCall PhoneAccount does not support placing emergency calls, then we know it
        // will not be that one and we do not want multiple PhoneAccounts active during an
        // emergency call if possible. Disconnect the active call in favor of the emergency call
        // instead of trying to hold.
        if (liveCall.getTargetPhoneAccount() != null) {
            PhoneAccount pa = mPhoneAccountRegistrar.getPhoneAccountUnchecked(
                    liveCall.getTargetPhoneAccount());
            if((pa.getCapabilities() & PhoneAccount.CAPABILITY_PLACE_EMERGENCY_CALLS) == 0) {
                liveCall.setOverrideDisconnectCauseCode(new DisconnectCause(
                        DisconnectCause.LOCAL, DisconnectCause.REASON_EMERGENCY_CALL_PLACED));
                liveCall.disconnect("outgoing call does not support emergency calls, "
                        + "disconnecting.");
            }
            return true;
        }

        // First thing, if we are trying to make an emergency call with the same package name as
        // the live call, then allow it so that the connection service can make its own decision
        // about how to handle the new call relative to the current one.
        // By default, for telephony, it will try to hold the existing call before placing the new
        // emergency call except for if the carrier does not support holding calls for emergency.
        // In this case, telephony will disconnect the call.
        if (PhoneAccountHandle.areFromSamePackage(liveCallPhoneAccount,
                emergencyCall.getTargetPhoneAccount())) {
            Log.i(this, "makeRoomForOutgoingEmergencyCall: phoneAccount matches.");
            emergencyCall.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            return true;
        } else if (emergencyCall.getTargetPhoneAccount() == null) {
            // Without a phone account, we can't say reliably that the call will fail.
            // If the user chooses the same phone account as the live call, then it's
            // still possible that the call can be made (like with CDMA calls not supporting
            // hold but they still support adding a call by going immediately into conference
            // mode). Return true here and we'll run this code again after user chooses an
            // account.
            return true;
        }

        // Hold the live call if possible before attempting the new outgoing emergency call.
        if (canHold(liveCall)) {
            Log.i(this, "makeRoomForOutgoingEmergencyCall: holding live call.");
            emergencyCall.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            liveCall.hold("calling " + emergencyCall.getId());
            return true;
        }

        // The live call cannot be held so we're out of luck here.  There's no room.
        return false;
    }

    private boolean makeRoomForOutgoingCall(Call call) {
        // Already room!
        if (!hasMaximumLiveCalls(call)) return true;

        // NOTE: If the amount of live calls changes beyond 1, this logic will probably
        // have to change.
        Call liveCall = getFirstCallWithState(LIVE_CALL_STATES);
        Log.i(this, "makeRoomForOutgoingCall call = " + call + " livecall = " +
               liveCall);

        if (call == liveCall) {
            // If the call is already the foreground call, then we are golden.
            // This can happen after the user selects an account in the SELECT_PHONE_ACCOUNT
            // state since the call was already populated into the list.
            return true;
        }

        if (hasMaximumOutgoingCalls(call)) {
            Call outgoingCall = getFirstCallWithState(OUTGOING_CALL_STATES);
            if (outgoingCall.getState() == CallState.SELECT_PHONE_ACCOUNT) {
                // If there is an orphaned call in the {@link CallState#SELECT_PHONE_ACCOUNT}
                // state, just disconnect it since the user has explicitly started a new call.
                call.getAnalytics().setCallIsAdditional(true);
                outgoingCall.getAnalytics().setCallIsInterrupted(true);
                outgoingCall.disconnect("Disconnecting call in SELECT_PHONE_ACCOUNT in favor"
                        + " of new outgoing call.");
                return true;
            }
            return false;
        }

        // TODO: Remove once b/23035408 has been corrected.
        // If the live call is a conference, it will not have a target phone account set.  This
        // means the check to see if the live call has the same target phone account as the new
        // call will not cause us to bail early.  As a result, we'll end up holding the
        // ongoing conference call.  However, the ConnectionService is already doing that.  This
        // has caused problems with some carriers.  As a workaround until b/23035408 is
        // corrected, we will try and get the target phone account for one of the conference's
        // children and use that instead.
        PhoneAccountHandle liveCallPhoneAccount = liveCall.getTargetPhoneAccount();
        if (liveCallPhoneAccount == null && liveCall.isConference() &&
                !liveCall.getChildCalls().isEmpty()) {
            liveCallPhoneAccount = getFirstChildPhoneAccount(liveCall);
            Log.i(this, "makeRoomForOutgoingCall: using child call PhoneAccount = " +
                    liveCallPhoneAccount);
        }

        // First thing, if we are trying to make a call with the same phone account as the live
        // call, then allow it so that the connection service can make its own decision about
        // how to handle the new call relative to the current one.
        if (PhoneAccountHandle.areFromSamePackage(liveCallPhoneAccount,
                call.getTargetPhoneAccount())) {
            Log.i(this, "makeRoomForOutgoingCall: phoneAccount matches.");
            call.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            return true;
        } else if (call.getTargetPhoneAccount() == null) {
            // Without a phone account, we can't say reliably that the call will fail.
            // If the user chooses the same phone account as the live call, then it's
            // still possible that the call can be made (like with CDMA calls not supporting
            // hold but they still support adding a call by going immediately into conference
            // mode). Return true here and we'll run this code again after user chooses an
            // account.
            return true;
        }

        // Try to hold the live call before attempting the new outgoing call.
        if (canHold(liveCall)) {
            Log.i(this, "makeRoomForOutgoingCall: holding live call.");
            call.getAnalytics().setCallIsAdditional(true);
            liveCall.getAnalytics().setCallIsInterrupted(true);
            liveCall.hold("calling " + call.getId());
            return true;
        }

        // The live call cannot be held so we're out of luck here.  There's no room.
        return false;
    }

    /**
     * Given a call, find the first non-null phone account handle of its children.
     *
     * @param parentCall The parent call.
     * @return The first non-null phone account handle of the children, or {@code null} if none.
     */
    private PhoneAccountHandle getFirstChildPhoneAccount(Call parentCall) {
        for (Call childCall : parentCall.getChildCalls()) {
            PhoneAccountHandle childPhoneAccount = childCall.getTargetPhoneAccount();
            if (childPhoneAccount != null) {
                return childPhoneAccount;
            }
        }
        return null;
    }

    /**
     * Checks to see if the call should be on speakerphone and if so, set it.
     */
    private void maybeMoveToSpeakerPhone(Call call) {
        if (call.isHandoverInProgress() && call.getState() == CallState.DIALING) {
            // When a new outgoing call is initiated for the purpose of handing over, do not engage
            // speaker automatically until the call goes active.
            return;
        }
        if (call.getStartWithSpeakerphoneOn()) {
            setAudioRoute(CallAudioState.ROUTE_SPEAKER, null);
            call.setStartWithSpeakerphoneOn(false);
        }
    }

    /**
     * Checks to see if the call is an emergency call and if so, turn off mute.
     */
    private void maybeTurnOffMute(Call call) {
        if (call.isEmergencyCall()) {
            mute(false);
        }
    }

    private void ensureCallAudible() {
        AudioManager am = mContext.getSystemService(AudioManager.class);
        if (am == null) {
            Log.w(this, "ensureCallAudible: audio manager is null");
            return;
        }
        if (am.getStreamVolume(AudioManager.STREAM_VOICE_CALL) == 0) {
            Log.i(this, "ensureCallAudible: voice call stream has volume 0. Adjusting to default.");
            am.setStreamVolume(AudioManager.STREAM_VOICE_CALL,
                    AudioSystem.getDefaultStreamVolume(AudioManager.STREAM_VOICE_CALL), 0);
        }
    }

    /**
     * Creates a new call for an existing connection.
     *
     * @param callId The id of the new call.
     * @param connection The connection information.
     * @return The new call.
     */
    Call createCallForExistingConnection(String callId, ParcelableConnection connection) {
        boolean isDowngradedConference = (connection.getConnectionProperties()
                & Connection.PROPERTY_IS_DOWNGRADED_CONFERENCE) != 0;

        PhoneAccountHandle connectionMgr =
                mPhoneAccountRegistrar.getSimCallManagerFromHandle(connection.getPhoneAccount(),
                        mCurrentUserHandle);
        Call call = new Call(
                callId,
                mContext,
                this,
                mLock,
                mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                connection.getHandle() /* handle */,
                null /* gatewayInfo */,
                connectionMgr,
                connection.getPhoneAccount(), /* targetPhoneAccountHandle */
                Call.getRemappedCallDirection(connection.getCallDirection()) /* callDirection */,
                false /* forceAttachToExistingConnection */,
                isDowngradedConference /* isConference */,
                connection.getConnectTimeMillis() /* connectTimeMillis */,
                connection.getConnectElapsedTimeMillis(), /* connectElapsedTimeMillis */
                mClockProxy,
                mToastFactory);

        call.initAnalytics();
        call.getAnalytics().setCreatedFromExistingConnection(true);

        setCallState(call, Call.getStateFromConnectionState(connection.getState()),
                "existing connection");
        call.setVideoState(connection.getVideoState());
        call.setConnectionCapabilities(connection.getConnectionCapabilities());
        call.setConnectionProperties(connection.getConnectionProperties());
        call.setHandle(connection.getHandle(), connection.getHandlePresentation());
        call.setCallerDisplayName(connection.getCallerDisplayName(),
                connection.getCallerDisplayNamePresentation());
        call.addListener(this);
        call.putExtras(Call.SOURCE_CONNECTION_SERVICE, connection.getExtras());

        Log.i(this, "createCallForExistingConnection: %s", connection);
        Call parentCall = null;
        if (!TextUtils.isEmpty(connection.getParentCallId())) {
            String parentId = connection.getParentCallId();
            parentCall = mCalls
                    .stream()
                    .filter(c -> c.getId().equals(parentId))
                    .findFirst()
                    .orElse(null);
            if (parentCall != null) {
                Log.i(this, "createCallForExistingConnection: %s added as child of %s.",
                        call.getId(),
                        parentCall.getId());
                // Set JUST the parent property, which won't send an update to the Incall UI.
                call.setParentCall(parentCall);
            }
        }
        addCall(call);
        if (parentCall != null) {
            // Now, set the call as a child of the parent since it has been added to Telecom.  This
            // is where we will inform InCall.
            call.setChildOf(parentCall);
            call.notifyParentChanged(parentCall);
        }

        return call;
    }

    /**
     * Determines whether Telecom already knows about a Connection added via the
     * {@link android.telecom.ConnectionService#addExistingConnection(PhoneAccountHandle,
     * Connection)} API via a ConnectionManager.
     *
     * See {@link Connection#EXTRA_ORIGINAL_CONNECTION_ID}.
     * @param originalConnectionId The new connection ID to check.
     * @return {@code true} if this connection is already known by Telecom.
     */
    Call getAlreadyAddedConnection(String originalConnectionId) {
        Optional<Call> existingCall = mCalls.stream()
                .filter(call -> originalConnectionId.equals(call.getOriginalConnectionId()) ||
                            originalConnectionId.equals(call.getId()))
                .findFirst();

        if (existingCall.isPresent()) {
            Log.i(this, "isExistingConnectionAlreadyAdded - call %s already added with id %s",
                    originalConnectionId, existingCall.get().getId());
            return existingCall.get();
        }

        return null;
    }

    /**
     * @return A new unique telecom call Id.
     */
    private String getNextCallId() {
        synchronized(mLock) {
            return TELECOM_CALL_ID_PREFIX + (++mCallId);
        }
    }

    public int getNextRttRequestId() {
        synchronized (mLock) {
            return (++mRttRequestId);
        }
    }

    /**
     * Callback when foreground user is switched. We will reload missed call in all profiles
     * including the user itself. There may be chances that profiles are not started yet.
     */
    @VisibleForTesting
    public void onUserSwitch(UserHandle userHandle) {
        mCurrentUserHandle = userHandle;
        mMissedCallNotifier.setCurrentUserHandle(userHandle);
        mRoleManagerAdapter.setCurrentUserHandle(userHandle);
        final UserManager userManager = UserManager.get(mContext);
        List<UserInfo> profiles = userManager.getEnabledProfiles(userHandle.getIdentifier());
        for (UserInfo profile : profiles) {
            reloadMissedCallsOfUser(profile.getUserHandle());
        }
    }

    /**
     * Because there may be chances that profiles are not started yet though its parent user is
     * switched, we reload missed calls of profile that are just started here.
     */
    void onUserStarting(UserHandle userHandle) {
        if (UserUtil.isProfile(mContext, userHandle)) {
            reloadMissedCallsOfUser(userHandle);
        }
    }

    public TelecomSystem.SyncRoot getLock() {
        return mLock;
    }

    public Timeouts.Adapter getTimeoutsAdapter() {
        return mTimeoutsAdapter;
    }

    public SystemStateHelper getSystemStateHelper() {
        return mSystemStateHelper;
    }

    private void reloadMissedCallsOfUser(UserHandle userHandle) {
        mMissedCallNotifier.reloadFromDatabase(mCallerInfoLookupHelper,
                new MissedCallNotifier.CallInfoFactory(), userHandle);
    }

    public void onBootCompleted() {
        mMissedCallNotifier.reloadAfterBootComplete(mCallerInfoLookupHelper,
                new MissedCallNotifier.CallInfoFactory());
    }

    public boolean isIncomingCallPermitted(PhoneAccountHandle phoneAccountHandle) {
        return isIncomingCallPermitted(null /* excludeCall */, phoneAccountHandle);
    }

    public boolean isIncomingCallPermitted(Call excludeCall,
                                           PhoneAccountHandle phoneAccountHandle) {
        if (phoneAccountHandle == null) {
            return false;
        }
        PhoneAccount phoneAccount =
                mPhoneAccountRegistrar.getPhoneAccountUnchecked(phoneAccountHandle);
        if (phoneAccount == null) {
            return false;
        }
        if (isInEmergencyCall()) return false;

        if (!phoneAccount.isSelfManaged()) {
            return !hasMaximumManagedRingingCalls(excludeCall) &&
                    !hasMaximumManagedHoldingCalls(excludeCall);
        } else {
            return !hasMaximumSelfManagedRingingCalls(excludeCall, phoneAccountHandle) &&
                    !hasMaximumSelfManagedCalls(excludeCall, phoneAccountHandle);
        }
    }

    public boolean isOutgoingCallPermitted(PhoneAccountHandle phoneAccountHandle) {
        return isOutgoingCallPermitted(null /* excludeCall */, phoneAccountHandle);
    }

    public boolean isOutgoingCallPermitted(Call excludeCall,
                                           PhoneAccountHandle phoneAccountHandle) {
        if (phoneAccountHandle == null) {
            return false;
        }
        PhoneAccount phoneAccount =
                mPhoneAccountRegistrar.getPhoneAccountUnchecked(phoneAccountHandle);
        if (phoneAccount == null) {
            return false;
        }

        if (!phoneAccount.isSelfManaged()) {
            return !hasMaximumManagedOutgoingCalls(excludeCall) &&
                    !hasMaximumManagedDialingCalls(excludeCall) &&
                    !hasMaximumManagedLiveCalls(excludeCall) &&
                    !hasMaximumManagedHoldingCalls(excludeCall);
        } else {
            // Only permit self-managed outgoing calls if
            // 1. there is no emergency ongoing call
            // 2. The outgoing call is an handover call or it not hit the self-managed call limit
            // and the current active call can be held.
            Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
            return !isInEmergencyCall() &&
                    ((excludeCall != null && excludeCall.getHandoverSourceCall() != null) ||
                            (!hasMaximumSelfManagedCalls(excludeCall, phoneAccountHandle) &&
                                    (activeCall == null || canHold(activeCall))));
        }
    }

    public boolean isReplyWithSmsAllowed(int uid) {
        UserHandle callingUser = UserHandle.of(UserHandle.getUserId(uid));
        UserManager userManager = mContext.getSystemService(UserManager.class);
        KeyguardManager keyguardManager = mContext.getSystemService(KeyguardManager.class);

        boolean isUserRestricted = userManager != null
                && userManager.hasUserRestriction(UserManager.DISALLOW_SMS, callingUser);
        boolean isLockscreenRestricted = keyguardManager != null
                && keyguardManager.isDeviceLocked();
        Log.d(this, "isReplyWithSmsAllowed: isUserRestricted: %s, isLockscreenRestricted: %s",
                isUserRestricted, isLockscreenRestricted);

        // TODO(hallliu): actually check the lockscreen once b/77731473 is fixed
        return !isUserRestricted;
    }
    /**
     * Blocks execution until all Telecom handlers have completed their current work.
     */
    public void waitOnHandlers() {
        CountDownLatch mainHandlerLatch = new CountDownLatch(3);
        mHandler.post(() -> {
            mainHandlerLatch.countDown();
        });
        mCallAudioManager.getCallAudioModeStateMachine().getHandler().post(() -> {
            mainHandlerLatch.countDown();
        });
        mCallAudioManager.getCallAudioRouteStateMachine().getHandler().post(() -> {
            mainHandlerLatch.countDown();
        });

        try {
            mainHandlerLatch.await(HANDLER_WAIT_TIMEOUT, TimeUnit.MILLISECONDS);
        } catch (InterruptedException e) {
            Log.w(this, "waitOnHandlers: interrupted %s", e);
        }
    }

    /**
     * Used to confirm creation of an outgoing call which was marked as pending confirmation in
     * {@link #startOutgoingCall(Uri, PhoneAccountHandle, Bundle, UserHandle, Intent, String)}.
     * Called via {@link TelecomBroadcastIntentProcessor} for a call which was confirmed via
     * {@link ConfirmCallDialogActivity}.
     * @param callId The call ID of the call to confirm.
     */
    public void confirmPendingCall(String callId) {
        Log.i(this, "confirmPendingCall: callId=%s", callId);
        if (mPendingCall != null && mPendingCall.getId().equals(callId)) {
            Log.addEvent(mPendingCall, LogUtils.Events.USER_CONFIRMED);

            // We are going to place the new outgoing call, so disconnect any ongoing self-managed
            // calls which are ongoing at this time.
            disconnectSelfManagedCalls("outgoing call " + callId);

            mPendingCallConfirm.complete(mPendingCall);
            mPendingCallConfirm = null;
            mPendingCall = null;
        }
    }

    /**
     * Used to cancel an outgoing call which was marked as pending confirmation in
     * {@link #startOutgoingCall(Uri, PhoneAccountHandle, Bundle, UserHandle, Intent, String)}.
     * Called via {@link TelecomBroadcastIntentProcessor} for a call which was confirmed via
     * {@link ConfirmCallDialogActivity}.
     * @param callId The call ID of the call to cancel.
     */
    public void cancelPendingCall(String callId) {
        Log.i(this, "cancelPendingCall: callId=%s", callId);
        if (mPendingCall != null && mPendingCall.getId().equals(callId)) {
            Log.addEvent(mPendingCall, LogUtils.Events.USER_CANCELLED);
            markCallAsDisconnected(mPendingCall, new DisconnectCause(DisconnectCause.CANCELED));
            markCallAsRemoved(mPendingCall);
            mPendingCall = null;
            mPendingCallConfirm.complete(null);
            mPendingCallConfirm = null;
        }
    }

    /**
     * Called from {@link #startOutgoingCall(Uri, PhoneAccountHandle, Bundle, UserHandle, Intent, String)} when
     * a managed call is added while there are ongoing self-managed calls.  Starts
     * {@link ConfirmCallDialogActivity} to prompt the user to see if they wish to place the
     * outgoing call or not.
     * @param call The call to confirm.
     */
    private void startCallConfirmation(Call call, CompletableFuture<Call> confirmationFuture) {
        if (mPendingCall != null) {
            Log.i(this, "startCallConfirmation: call %s is already pending; disconnecting %s",
                    mPendingCall.getId(), call.getId());
            markCallDisconnectedDueToSelfManagedCall(call);
            confirmationFuture.complete(null);
            return;
        }
        Log.addEvent(call, LogUtils.Events.USER_CONFIRMATION);
        mPendingCall = call;
        mPendingCallConfirm = confirmationFuture;

        // Figure out the name of the app in charge of the self-managed call(s).
        Call activeCall = (Call) mConnectionSvrFocusMgr.getCurrentFocusCall();
        if (activeCall != null) {
            CharSequence ongoingAppName = activeCall.getTargetPhoneAccountLabel();
            Log.i(this, "startCallConfirmation: callId=%s, ongoingApp=%s", call.getId(),
                    ongoingAppName);

            Intent confirmIntent = new Intent(mContext, ConfirmCallDialogActivity.class);
            confirmIntent.putExtra(ConfirmCallDialogActivity.EXTRA_OUTGOING_CALL_ID, call.getId());
            confirmIntent.putExtra(ConfirmCallDialogActivity.EXTRA_ONGOING_APP_NAME, ongoingAppName);
            confirmIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            mContext.startActivityAsUser(confirmIntent, UserHandle.CURRENT);
        }
    }

    /**
     * Disconnects all self-managed calls.
     */
    private void disconnectSelfManagedCalls(String reason) {
        // Disconnect all self-managed calls to make priority for emergency call.
        // Use Call.disconnect() to command the ConnectionService to disconnect the calls.
        // CallsManager.markCallAsDisconnected doesn't actually tell the ConnectionService to
        // disconnect.
        mCalls.stream()
                .filter(c -> c.isSelfManaged())
                .forEach(c -> c.disconnect(reason));

        // When disconnecting all self-managed calls, switch audio routing back to the baseline
        // route.  This ensures if, for example, the self-managed ConnectionService was routed to
        // speakerphone that we'll switch back to earpiece for the managed call which necessitated
        // disconnecting the self-managed calls.
        mCallAudioManager.switchBaseline();
    }

    /**
     * Dumps the state of the {@link CallsManager}.
     *
     * @param pw The {@code IndentingPrintWriter} to write the state to.
     */
    public void dump(IndentingPrintWriter pw) {
        mContext.enforceCallingOrSelfPermission(android.Manifest.permission.DUMP, TAG);
        if (mCalls != null) {
            pw.println("mCalls: ");
            pw.increaseIndent();
            for (Call call : mCalls) {
                pw.println(call);
            }
            pw.decreaseIndent();
        }

        if (mPendingCall != null) {
            pw.print("mPendingCall:");
            pw.println(mPendingCall.getId());
        }

        if (mPendingRedirectedOutgoingCallInfo.size() > 0) {
            pw.print("mPendingRedirectedOutgoingCallInfo:");
            pw.println(mPendingRedirectedOutgoingCallInfo.keySet().stream().collect(
                    Collectors.joining(", ")));
        }

        if (mPendingUnredirectedOutgoingCallInfo.size() > 0) {
            pw.print("mPendingUnredirectedOutgoingCallInfo:");
            pw.println(mPendingUnredirectedOutgoingCallInfo.keySet().stream().collect(
                    Collectors.joining(", ")));
        }

        if (mCallAudioManager != null) {
            pw.println("mCallAudioManager:");
            pw.increaseIndent();
            mCallAudioManager.dump(pw);
            pw.decreaseIndent();
        }

        if (mTtyManager != null) {
            pw.println("mTtyManager:");
            pw.increaseIndent();
            mTtyManager.dump(pw);
            pw.decreaseIndent();
        }

        if (mInCallController != null) {
            pw.println("mInCallController:");
            pw.increaseIndent();
            mInCallController.dump(pw);
            pw.decreaseIndent();
        }

        if (mDefaultDialerCache != null) {
            pw.println("mDefaultDialerCache:");
            pw.increaseIndent();
            mDefaultDialerCache.dumpCache(pw);
            pw.decreaseIndent();
        }

        if (mConnectionServiceRepository != null) {
            pw.println("mConnectionServiceRepository:");
            pw.increaseIndent();
            mConnectionServiceRepository.dump(pw);
            pw.decreaseIndent();
        }

        if (mRoleManagerAdapter != null && mRoleManagerAdapter instanceof RoleManagerAdapterImpl) {
            RoleManagerAdapterImpl impl = (RoleManagerAdapterImpl) mRoleManagerAdapter;
            pw.println("mRoleManager:");
            pw.increaseIndent();
            impl.dump(pw);
            pw.decreaseIndent();
        }
    }

    /**
    * For some disconnected causes, we show a dialog when it's a mmi code or potential mmi code.
    *
    * @param call The call.
    */
    private void maybeShowErrorDialogOnDisconnect(Call call) {
        if (call.getState() == CallState.DISCONNECTED && (isPotentialMMICode(call.getHandle())
                || isPotentialInCallMMICode(call.getHandle())) && !mCalls.contains(call)) {
            DisconnectCause disconnectCause = call.getDisconnectCause();
            if (!TextUtils.isEmpty(disconnectCause.getDescription()) && (disconnectCause.getCode()
                    == DisconnectCause.ERROR)) {
                Intent errorIntent = new Intent(mContext, ErrorDialogActivity.class);
                errorIntent.putExtra(ErrorDialogActivity.ERROR_MESSAGE_STRING_EXTRA,
                        disconnectCause.getDescription());
                errorIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                mContext.startActivityAsUser(errorIntent, UserHandle.CURRENT);
            }
        }
    }

    private void setIntentExtrasAndStartTime(Call call, Bundle extras) {
        if (extras != null) {
            // Create our own instance to modify (since extras may be Bundle.EMPTY)
            extras = new Bundle(extras);
        } else {
            extras = new Bundle();
        }

        // Specifies the time telecom began routing the call. This is used by the dialer for
        // analytics.
        extras.putLong(TelecomManager.EXTRA_CALL_TELECOM_ROUTING_START_TIME_MILLIS,
              SystemClock.elapsedRealtime());

        call.setIntentExtras(extras);
    }

    private void setCallSourceToAnalytics(Call call, Intent originalIntent) {
        if (originalIntent == null) {
            return;
        }

        int callSource = originalIntent.getIntExtra(TelecomManager.EXTRA_CALL_SOURCE,
                Analytics.CALL_SOURCE_UNSPECIFIED);

        // Call source is only used by metrics, so we simply set it to Analytics directly.
        call.getAnalytics().setCallSource(callSource);
    }

    private boolean isVoicemail(Uri callHandle, PhoneAccount phoneAccount) {
        if (callHandle == null) {
            return false;
        }
        if (PhoneAccount.SCHEME_VOICEMAIL.equals(callHandle.getScheme())) {
            return true;
        }
        return phoneAccount != null && mPhoneAccountRegistrar.isVoiceMailNumber(
                phoneAccount.getAccountHandle(),
                callHandle.getSchemeSpecificPart());
    }

    /**
     * Notifies the {@link android.telecom.ConnectionService} associated with a
     * {@link PhoneAccountHandle} that the attempt to create a new connection has failed.
     *
     * @param phoneAccountHandle The {@link PhoneAccountHandle}.
     * @param call The {@link Call} which could not be added.
     */
    private void notifyCreateConnectionFailed(PhoneAccountHandle phoneAccountHandle, Call call) {
        if (phoneAccountHandle == null) {
            return;
        }
        ConnectionServiceWrapper service = mConnectionServiceRepository.getService(
                phoneAccountHandle.getComponentName(), phoneAccountHandle.getUserHandle());
        if (service == null) {
            Log.i(this, "Found no connection service.");
            return;
        } else {
            call.setConnectionService(service);
            service.createConnectionFailed(call);
        }
    }

    /**
     * Notifies the {@link android.telecom.ConnectionService} associated with a
     * {@link PhoneAccountHandle} that the attempt to create a new connection has failed.
     *
     * @param phoneAccountHandle The {@link PhoneAccountHandle}.
     * @param call The {@link Call} which could not be added.
     */
    private void notifyCreateConferenceFailed(PhoneAccountHandle phoneAccountHandle, Call call) {
        if (phoneAccountHandle == null) {
            return;
        }
        ConnectionServiceWrapper service = mConnectionServiceRepository.getService(
                phoneAccountHandle.getComponentName(), phoneAccountHandle.getUserHandle());
        if (service == null) {
            Log.i(this, "Found no connection service.");
            return;
        } else {
            call.setConnectionService(service);
            service.createConferenceFailed(call);
        }
    }


    /**
     * Notifies the {@link android.telecom.ConnectionService} associated with a
     * {@link PhoneAccountHandle} that the attempt to handover a call has failed.
     *
     * @param call The handover call
     * @param reason The error reason code for handover failure
     */
    private void notifyHandoverFailed(Call call, int reason) {
        ConnectionServiceWrapper service = call.getConnectionService();
        service.handoverFailed(call, reason);
        call.setDisconnectCause(new DisconnectCause(DisconnectCause.CANCELED));
        call.disconnect("handover failed");
    }

    /**
     * Called in response to a {@link Call} receiving a {@link Call#sendCallEvent(String, Bundle)}
     * of type {@link android.telecom.Call#EVENT_REQUEST_HANDOVER} indicating the
     * {@link android.telecom.InCallService} has requested a handover to another
     * {@link android.telecom.ConnectionService}.
     *
     * We will explicitly disallow a handover when there is an emergency call present.
     *
     * @param handoverFromCall The {@link Call} to be handed over.
     * @param handoverToHandle The {@link PhoneAccountHandle} to hand over the call to.
     * @param videoState The desired video state of {@link Call} after handover.
     * @param initiatingExtras Extras associated with the handover, to be passed to the handover
     *               {@link android.telecom.ConnectionService}.
     */
    private void requestHandoverViaEvents(Call handoverFromCall,
                                          PhoneAccountHandle handoverToHandle,
                                          int videoState, Bundle initiatingExtras) {

        handoverFromCall.sendCallEvent(android.telecom.Call.EVENT_HANDOVER_FAILED, null);
        Log.addEvent(handoverFromCall, LogUtils.Events.HANDOVER_REQUEST, "legacy request denied");
    }

    /**
     * Called in response to a {@link Call} receiving a {@link Call#handoverTo(PhoneAccountHandle,
     * int, Bundle)} indicating the {@link android.telecom.InCallService} has requested a
     * handover to another {@link android.telecom.ConnectionService}.
     *
     * We will explicitly disallow a handover when there is an emergency call present.
     *
     * @param handoverFromCall The {@link Call} to be handed over.
     * @param handoverToHandle The {@link PhoneAccountHandle} to hand over the call to.
     * @param videoState The desired video state of {@link Call} after handover.
     * @param extras Extras associated with the handover, to be passed to the handover
     *               {@link android.telecom.ConnectionService}.
     */
    private void requestHandover(Call handoverFromCall, PhoneAccountHandle handoverToHandle,
                                 int videoState, Bundle extras) {

        // Send an error back if there are any ongoing emergency calls.
        if (isInEmergencyCall()) {
            handoverFromCall.onHandoverFailed(
                    android.telecom.Call.Callback.HANDOVER_FAILURE_ONGOING_EMERGENCY_CALL);
            return;
        }

        // If source and destination phone accounts don't support handover, send an error back.
        boolean isHandoverFromSupported = isHandoverFromPhoneAccountSupported(
                handoverFromCall.getTargetPhoneAccount());
        boolean isHandoverToSupported = isHandoverToPhoneAccountSupported(handoverToHandle);
        if (!isHandoverFromSupported || !isHandoverToSupported) {
            handoverFromCall.onHandoverFailed(
                    android.telecom.Call.Callback.HANDOVER_FAILURE_NOT_SUPPORTED);
            return;
        }

        Log.addEvent(handoverFromCall, LogUtils.Events.HANDOVER_REQUEST, handoverToHandle);

        // Create a new instance of Call
        PhoneAccount account =
                mPhoneAccountRegistrar.getPhoneAccount(handoverToHandle, getCurrentUserHandle());
        boolean isSelfManaged = account != null && account.isSelfManaged();

        Call call = new Call(getNextCallId(), mContext,
                this, mLock, mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                handoverFromCall.getHandle(), null,
                null, null,
                Call.CALL_DIRECTION_OUTGOING, false,
                false, mClockProxy, mToastFactory);
        call.initAnalytics();

        // Set self-managed and voipAudioMode if destination is self-managed CS
        call.setIsSelfManaged(isSelfManaged);
        if (isSelfManaged) {
            call.setIsVoipAudioMode(true);
        }
        call.setInitiatingUser(getCurrentUserHandle());

        // Ensure we don't try to place an outgoing call with video if video is not
        // supported.
        if (VideoProfile.isVideo(videoState) && account != null &&
                !account.hasCapabilities(PhoneAccount.CAPABILITY_VIDEO_CALLING)) {
            call.setVideoState(VideoProfile.STATE_AUDIO_ONLY);
        } else {
            call.setVideoState(videoState);
        }

        // Set target phone account to destAcct.
        call.setTargetPhoneAccount(handoverToHandle);

        if (account != null && account.getExtras() != null && account.getExtras()
                    .getBoolean(PhoneAccount.EXTRA_ALWAYS_USE_VOIP_AUDIO_MODE)) {
            Log.d(this, "requestHandover: defaulting to voip mode for call %s",
                        call.getId());
            call.setIsVoipAudioMode(true);
        }

        // Set call state to connecting
        call.setState(
                CallState.CONNECTING,
                handoverToHandle == null ? "no-handle" : handoverToHandle.toString());

        // Mark as handover so that the ConnectionService knows this is a handover request.
        if (extras == null) {
            extras = new Bundle();
        }
        extras.putBoolean(TelecomManager.EXTRA_IS_HANDOVER_CONNECTION, true);
        extras.putParcelable(TelecomManager.EXTRA_HANDOVER_FROM_PHONE_ACCOUNT,
                handoverFromCall.getTargetPhoneAccount());
        setIntentExtrasAndStartTime(call, extras);

        // Add call to call tracker
        if (!mCalls.contains(call)) {
            addCall(call);
        }

        Log.addEvent(handoverFromCall, LogUtils.Events.START_HANDOVER,
                "handOverFrom=%s, handOverTo=%s", handoverFromCall.getId(), call.getId());

        handoverFromCall.setHandoverDestinationCall(call);
        handoverFromCall.setHandoverState(HandoverState.HANDOVER_FROM_STARTED);
        call.setHandoverState(HandoverState.HANDOVER_TO_STARTED);
        call.setHandoverSourceCall(handoverFromCall);
        call.setNewOutgoingCallIntentBroadcastIsDone();

        // Auto-enable speakerphone if the originating intent specified to do so, if the call
        // is a video call, of if using speaker when docked
        final boolean useSpeakerWhenDocked = mContext.getResources().getBoolean(
                R.bool.use_speaker_when_docked);
        final boolean useSpeakerForDock = isSpeakerphoneEnabledForDock();
        final boolean useSpeakerForVideoCall = isSpeakerphoneAutoEnabledForVideoCalls(videoState);
        call.setStartWithSpeakerphoneOn(false || useSpeakerForVideoCall
                || (useSpeakerWhenDocked && useSpeakerForDock));
        call.setVideoState(videoState);

        final boolean isOutgoingCallPermitted = isOutgoingCallPermitted(call,
                call.getTargetPhoneAccount());

        // If the account has been set, proceed to place the outgoing call.
        if (call.isSelfManaged() && !isOutgoingCallPermitted) {
            notifyCreateConnectionFailed(call.getTargetPhoneAccount(), call);
        } else if (!call.isSelfManaged() && hasSelfManagedCalls() && !call.isEmergencyCall()) {
            markCallDisconnectedDueToSelfManagedCall(call);
        } else {
            if (call.isEmergencyCall()) {
                // Disconnect all self-managed calls to make priority for emergency call.
                disconnectSelfManagedCalls("emergency call");
            }

            call.startCreateConnection(mPhoneAccountRegistrar);
        }

    }

    /**
     * Determines if handover from the specified {@link PhoneAccountHandle} is supported.
     *
     * @param from The {@link PhoneAccountHandle} the handover originates from.
     * @return {@code true} if handover is currently allowed, {@code false} otherwise.
     */
    private boolean isHandoverFromPhoneAccountSupported(PhoneAccountHandle from) {
        return getBooleanPhoneAccountExtra(from, PhoneAccount.EXTRA_SUPPORTS_HANDOVER_FROM);
    }

    /**
     * Determines if handover to the specified {@link PhoneAccountHandle} is supported.
     *
     * @param to The {@link PhoneAccountHandle} the handover it to.
     * @return {@code true} if handover is currently allowed, {@code false} otherwise.
     */
    private boolean isHandoverToPhoneAccountSupported(PhoneAccountHandle to) {
        return getBooleanPhoneAccountExtra(to, PhoneAccount.EXTRA_SUPPORTS_HANDOVER_TO);
    }

    /**
     * Retrieves a boolean phone account extra.
     * @param handle the {@link PhoneAccountHandle} to retrieve the extra for.
     * @param key The extras key.
     * @return {@code true} if the extra {@link PhoneAccount} extra is true, {@code false}
     *      otherwise.
     */
    private boolean getBooleanPhoneAccountExtra(PhoneAccountHandle handle, String key) {
        PhoneAccount phoneAccount = getPhoneAccountRegistrar().getPhoneAccountUnchecked(handle);
        if (phoneAccount == null) {
            return false;
        }

        Bundle fromExtras = phoneAccount.getExtras();
        if (fromExtras == null) {
            return false;
        }
        return fromExtras.getBoolean(key);
    }

    /**
     * Determines if there is an existing handover in process.
     * @return {@code true} if a call in the process of handover exists, {@code false} otherwise.
     */
    private boolean isHandoverInProgress() {
        return mCalls.stream().filter(c -> c.getHandoverSourceCall() != null ||
                c.getHandoverDestinationCall() != null).count() > 0;
    }

    private void broadcastUnregisterIntent(PhoneAccountHandle accountHandle) {
        Intent intent =
                new Intent(TelecomManager.ACTION_PHONE_ACCOUNT_UNREGISTERED);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        intent.putExtra(
                TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE, accountHandle);
        Log.i(this, "Sending phone-account %s unregistered intent as user", accountHandle);
        mContext.sendBroadcastAsUser(intent, UserHandle.ALL,
                PERMISSION_PROCESS_PHONE_ACCOUNT_REGISTRATION);

        String dialerPackage = mDefaultDialerCache.getDefaultDialerApplication(
                getCurrentUserHandle().getIdentifier());
        if (!TextUtils.isEmpty(dialerPackage)) {
            Intent directedIntent = new Intent(TelecomManager.ACTION_PHONE_ACCOUNT_UNREGISTERED)
                    .setPackage(dialerPackage);
            directedIntent.putExtra(
                    TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE, accountHandle);
            Log.i(this, "Sending phone-account unregistered intent to default dialer");
            mContext.sendBroadcastAsUser(directedIntent, UserHandle.ALL, null);
        }
        return ;
    }

    private void broadcastRegisterIntent(PhoneAccountHandle accountHandle) {
        Intent intent = new Intent(
                TelecomManager.ACTION_PHONE_ACCOUNT_REGISTERED);
        intent.addFlags(Intent.FLAG_RECEIVER_INCLUDE_BACKGROUND);
        intent.putExtra(TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE,
                accountHandle);
        Log.i(this, "Sending phone-account %s registered intent as user", accountHandle);
        mContext.sendBroadcastAsUser(intent, UserHandle.ALL,
                PERMISSION_PROCESS_PHONE_ACCOUNT_REGISTRATION);

        String dialerPackage = mDefaultDialerCache.getDefaultDialerApplication(
                getCurrentUserHandle().getIdentifier());
        if (!TextUtils.isEmpty(dialerPackage)) {
            Intent directedIntent = new Intent(TelecomManager.ACTION_PHONE_ACCOUNT_REGISTERED)
                    .setPackage(dialerPackage);
            directedIntent.putExtra(
                    TelecomManager.EXTRA_PHONE_ACCOUNT_HANDLE, accountHandle);
            Log.i(this, "Sending phone-account registered intent to default dialer");
            mContext.sendBroadcastAsUser(directedIntent, UserHandle.ALL, null);
        }
        return ;
    }

    public void acceptHandover(Uri srcAddr, int videoState, PhoneAccountHandle destAcct) {
        final String handleScheme = srcAddr.getSchemeSpecificPart();
        Call fromCall = mCalls.stream()
                .filter((c) -> mPhoneNumberUtilsAdapter.isSamePhoneNumber(
                        (c.getHandle() == null ? null : c.getHandle().getSchemeSpecificPart()),
                        handleScheme))
                .findFirst()
                .orElse(null);

        Call call = new Call(
                getNextCallId(),
                mContext,
                this,
                mLock,
                mConnectionServiceRepository,
                mPhoneNumberUtilsAdapter,
                srcAddr,
                null /* gatewayInfo */,
                null /* connectionManagerPhoneAccount */,
                destAcct,
                Call.CALL_DIRECTION_INCOMING /* callDirection */,
                false /* forceAttachToExistingConnection */,
                false, /* isConference */
                mClockProxy,
                mToastFactory);

        if (fromCall == null || isHandoverInProgress() ||
                !isHandoverFromPhoneAccountSupported(fromCall.getTargetPhoneAccount()) ||
                !isHandoverToPhoneAccountSupported(destAcct) ||
                isInEmergencyCall()) {
            Log.w(this, "acceptHandover: Handover not supported");
            notifyHandoverFailed(call,
                    android.telecom.Call.Callback.HANDOVER_FAILURE_NOT_SUPPORTED);
            return;
        }

        PhoneAccount phoneAccount = mPhoneAccountRegistrar.getPhoneAccountUnchecked(destAcct);
        if (phoneAccount == null) {
            Log.w(this, "acceptHandover: Handover not supported. phoneAccount = null");
            notifyHandoverFailed(call,
                    android.telecom.Call.Callback.HANDOVER_FAILURE_NOT_SUPPORTED);
            return;
        }
        call.setIsSelfManaged(phoneAccount.isSelfManaged());
        if (call.isSelfManaged() || (phoneAccount.getExtras() != null &&
                phoneAccount.getExtras().getBoolean(
                        PhoneAccount.EXTRA_ALWAYS_USE_VOIP_AUDIO_MODE))) {
            call.setIsVoipAudioMode(true);
        }
        if (!phoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_VIDEO_CALLING)) {
            call.setVideoState(VideoProfile.STATE_AUDIO_ONLY);
        } else {
            call.setVideoState(videoState);
        }

        call.initAnalytics();
        call.addListener(this);

        fromCall.setHandoverDestinationCall(call);
        call.setHandoverSourceCall(fromCall);
        call.setHandoverState(HandoverState.HANDOVER_TO_STARTED);
        fromCall.setHandoverState(HandoverState.HANDOVER_FROM_STARTED);

        if (isSpeakerEnabledForVideoCalls() && VideoProfile.isVideo(videoState)) {
            // Ensure when the call goes active that it will go to speakerphone if the
            // handover to call is a video call.
            call.setStartWithSpeakerphoneOn(true);
        }

        Bundle extras = call.getIntentExtras();
        if (extras == null) {
            extras = new Bundle();
        }
        extras.putBoolean(TelecomManager.EXTRA_IS_HANDOVER_CONNECTION, true);
        extras.putParcelable(TelecomManager.EXTRA_HANDOVER_FROM_PHONE_ACCOUNT,
                fromCall.getTargetPhoneAccount());

        call.startCreateConnection(mPhoneAccountRegistrar);
    }

    public ConnectionServiceFocusManager getConnectionServiceFocusManager() {
        return mConnectionSvrFocusMgr;
    }

    private boolean canHold(Call call) {
        return call.can(Connection.CAPABILITY_HOLD) && call.getState() != CallState.DIALING;
    }

    private boolean supportsHold(Call call) {
        return call.can(Connection.CAPABILITY_SUPPORT_HOLD);
    }

    private final class ActionSetCallState implements PendingAction {

        private final Call mCall;
        private final int mState;
        private final String mTag;

        ActionSetCallState(Call call, int state, String tag) {
            mCall = call;
            mState = state;
            mTag = tag;
        }

        @Override
        public void performAction() {
            synchronized (mLock) {
                Log.d(this, "perform set call state for %s, state = %s", mCall, mState);
                setCallState(mCall, mState, mTag);
            }
        }
    }

    private final class ActionUnHoldCall implements PendingAction {
        private final Call mCall;
        private final String mPreviouslyHeldCallId;

        ActionUnHoldCall(Call call, String previouslyHeldCallId) {
            mCall = call;
            mPreviouslyHeldCallId = previouslyHeldCallId;
        }

        @Override
        public void performAction() {
            synchronized (mLock) {
                Log.d(this, "perform unhold call for %s", mCall);
                mCall.unhold("held " + mPreviouslyHeldCallId);
            }
        }
    }

    private final class ActionAnswerCall implements PendingAction {
        private final Call mCall;
        private final int mVideoState;

        ActionAnswerCall(Call call, int videoState) {
            mCall = call;
            mVideoState = videoState;
        }

        @Override
        public void performAction() {
            synchronized (mLock) {
                Log.d(this, "perform answer call for %s, videoState = %d", mCall, mVideoState);
                for (CallsManagerListener listener : mListeners) {
                    listener.onIncomingCallAnswered(mCall);
                }

                // We do not update the UI until we get confirmation of the answer() through
                // {@link #markCallAsActive}.
                if (mCall.getState() == CallState.RINGING) {
                    mCall.answer(mVideoState);
                    setCallState(mCall, CallState.ANSWERED, "answered");
                } else if (mCall.getState() == CallState.SIMULATED_RINGING) {
                    // If the call's in simulated ringing, we don't have to wait for the CS --
                    // we can just declare it active.
                    setCallState(mCall, CallState.ACTIVE, "answering simulated ringing");
                    Log.addEvent(mCall, LogUtils.Events.REQUEST_SIMULATED_ACCEPT);
                } else if (mCall.getState() == CallState.ANSWERED) {
                    // In certain circumstances, the connection service can lose track of a request
                    // to answer a call. Therefore, if the user presses answer again, still send it
                    // on down, but log a warning in the process and don't change the call state.
                    mCall.answer(mVideoState);
                    Log.w(this, "Duplicate answer request for call %s", mCall.getId());
                }
                if (isSpeakerphoneAutoEnabledForVideoCalls(mVideoState)) {
                    mCall.setStartWithSpeakerphoneOn(true);
                }
            }
        }
    }

    @VisibleForTesting(visibility = VisibleForTesting.Visibility.PACKAGE)
    public static final class RequestCallback implements
            ConnectionServiceFocusManager.RequestFocusCallback {
        private PendingAction mPendingAction;

        RequestCallback(PendingAction pendingAction) {
            mPendingAction = pendingAction;
        }

        @Override
        public void onRequestFocusDone(ConnectionServiceFocusManager.CallFocus call) {
            if (mPendingAction != null) {
                mPendingAction.performAction();
            }
        }
    }

    public void resetConnectionTime(Call call) {
        call.setConnectTimeMillis(System.currentTimeMillis());
        call.setConnectElapsedTimeMillis(SystemClock.elapsedRealtime());
        if (mCalls.contains(call)) {
            for (CallsManagerListener listener : mListeners) {
                listener.onConnectionTimeChanged(call);
            }
        }
    }

    public Context getContext() {
        return mContext;
    }

    /**
     * Determines if there is an ongoing emergency call. This can be either an outgoing emergency
     * call, or a number which has been identified by the number as an emergency call.
     * @return {@code true} if there is an ongoing emergency call, {@code false} otherwise.
     */
    public boolean isInEmergencyCall() {
        return mCalls.stream().filter(c -> (c.isEmergencyCall()
                || c.isNetworkIdentifiedEmergencyCall()) && !c.isDisconnected()).count() > 0;
    }

    /**
     * Trigger a recalculation of support for CAPABILITY_CAN_PULL_CALL for external calls due to
     * a possible emergency call being added/removed.
     */
    private void updateExternalCallCanPullSupport() {
        boolean isInEmergencyCall = isInEmergencyCall();
        // Remove the capability to pull an external call in the case that we are in an emergency
        // call.
        mCalls.stream().filter(Call::isExternalCall).forEach(
                c->c.setIsPullExternalCallSupported(!isInEmergencyCall));
    }

    /**
     * Trigger display of an error message to the user; we do this outside of dialer for calls which
     * fail to be created and added to Dialer.
     * @param messageId The string resource id.
     */
    private void showErrorMessage(int messageId) {
        final Intent errorIntent = new Intent(mContext, ErrorDialogActivity.class);
        errorIntent.putExtra(ErrorDialogActivity.ERROR_MESSAGE_ID_EXTRA, messageId);
        errorIntent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivityAsUser(errorIntent, UserHandle.CURRENT);
    }

    /**
     * Handles changes to a {@link PhoneAccount}.
     *
     * Checks for changes to video calling availability and updates whether calls for that phone
     * account are video capable.
     *
     * @param registrar The {@link PhoneAccountRegistrar} originating the change.
     * @param phoneAccount The {@link PhoneAccount} which changed.
     */
    private void handlePhoneAccountChanged(PhoneAccountRegistrar registrar,
            PhoneAccount phoneAccount) {
        Log.i(this, "handlePhoneAccountChanged: phoneAccount=%s", phoneAccount);
        boolean isVideoNowSupported = phoneAccount.hasCapabilities(
                PhoneAccount.CAPABILITY_VIDEO_CALLING);
        mCalls.stream()
                .filter(c -> phoneAccount.getAccountHandle().equals(c.getTargetPhoneAccount()))
                .forEach(c -> c.setVideoCallingSupportedByPhoneAccount(isVideoNowSupported));
    }

    /**
     * Determines if two {@link Call} instances originated from either the same target
     * {@link PhoneAccountHandle} or connection manager {@link PhoneAccountHandle}.
     * @param call1 The first call
     * @param call2 The second call
     * @return {@code true} if both calls are from the same target or connection manager
     * {@link PhoneAccountHandle}.
     */
    public static boolean areFromSameSource(@NonNull Call call1, @NonNull Call call2) {
        PhoneAccountHandle call1ConnectionMgr = call1.getConnectionManagerPhoneAccount();
        PhoneAccountHandle call2ConnectionMgr = call2.getConnectionManagerPhoneAccount();

        if (call1ConnectionMgr != null && call2ConnectionMgr != null
                && PhoneAccountHandle.areFromSamePackage(call1ConnectionMgr, call2ConnectionMgr)) {
            // Both calls share the same connection manager package, so they are from the same
            // source.
            return true;
        }

        PhoneAccountHandle call1TargetAcct = call1.getTargetPhoneAccount();
        PhoneAccountHandle call2TargetAcct = call2.getTargetPhoneAccount();
        // Otherwise if the target phone account for both is the same package, they're the same
        // source.
        return PhoneAccountHandle.areFromSamePackage(call1TargetAcct, call2TargetAcct);
    }

    public LinkedList<HandlerThread> getGraphHandlerThreads() {
        return mGraphHandlerThreads;
    }

    private void maybeSendPostCallScreenIntent(Call call) {
        if (call.isEmergencyCall() || (call.isNetworkIdentifiedEmergencyCall()) ||
                (call.getPostCallPackageName() == null)) {
            return;
        }

        Intent intent = new Intent(ACTION_POST_CALL);
        intent.setPackage(call.getPostCallPackageName());
        intent.putExtra(EXTRA_HANDLE, call.getHandle());
        intent.putExtra(EXTRA_DISCONNECT_CAUSE, call.getDisconnectCause().getCode());
        long duration = call.getAgeMillis();
        int durationCode = DURATION_VERY_SHORT;
        if ((duration >= VERY_SHORT_CALL_TIME_MS) && (duration < SHORT_CALL_TIME_MS)) {
            durationCode = DURATION_SHORT;
        } else if ((duration >= SHORT_CALL_TIME_MS) && (duration < MEDIUM_CALL_TIME_MS)) {
            durationCode = DURATION_MEDIUM;
        } else if (duration >= MEDIUM_CALL_TIME_MS) {
            durationCode = DURATION_LONG;
        }
        intent.putExtra(EXTRA_CALL_DURATION, durationCode);
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        mContext.startActivityAsUser(intent, mCurrentUserHandle);
    }
}
