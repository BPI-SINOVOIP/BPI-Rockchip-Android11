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

package com.android.services.telephony;

import android.annotation.NonNull;
import android.content.ActivityNotFoundException;
import android.content.BroadcastReceiver;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.Looper;
import android.telecom.Conference;
import android.telecom.Connection;
import android.telecom.ConnectionRequest;
import android.telecom.ConnectionService;
import android.telecom.DisconnectCause;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.telecom.VideoProfile;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.RadioAccessFamily;
import android.telephony.ServiceState;
import android.telephony.SubscriptionManager;
import android.telephony.TelephonyManager;
import android.telephony.emergency.EmergencyNumber;
import android.text.TextUtils;
import android.util.Pair;

import com.android.internal.annotations.VisibleForTesting;
import com.android.internal.telephony.Call;
import com.android.internal.telephony.CallStateException;
import com.android.internal.telephony.GsmCdmaPhone;
import com.android.internal.telephony.IccCard;
import com.android.internal.telephony.IccCardConstants;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.PhoneFactory;
import com.android.internal.telephony.PhoneSwitcher;
import com.android.internal.telephony.RIL;
import com.android.internal.telephony.SubscriptionController;
import com.android.internal.telephony.imsphone.ImsExternalCallTracker;
import com.android.internal.telephony.imsphone.ImsPhone;
import com.android.internal.telephony.imsphone.ImsPhoneConnection;
import com.android.phone.MMIDialogActivity;
import com.android.phone.PhoneUtils;
import com.android.phone.R;

import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.Collections;
import java.util.HashMap;
import java.util.LinkedList;
import java.util.List;
import java.util.Map;
import java.util.Queue;
import java.util.concurrent.CompletableFuture;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

import javax.annotation.Nullable;

/**
 * Service for making GSM and CDMA connections.
 */
public class TelephonyConnectionService extends ConnectionService {

    // Timeout before we continue with the emergency call without waiting for DDS switch response
    // from the modem.
    private static final int DEFAULT_DATA_SWITCH_TIMEOUT_MS = 1000;

    // If configured, reject attempts to dial numbers matching this pattern.
    private static final Pattern CDMA_ACTIVATION_CODE_REGEX_PATTERN =
            Pattern.compile("\\*228[0-9]{0,2}");

    private final TelephonyConnectionServiceProxy mTelephonyConnectionServiceProxy =
            new TelephonyConnectionServiceProxy() {
        @Override
        public Collection<Connection> getAllConnections() {
            return TelephonyConnectionService.this.getAllConnections();
        }
        @Override
        public void addConference(TelephonyConference mTelephonyConference) {
            TelephonyConnectionService.this.addTelephonyConference(mTelephonyConference);
        }
        @Override
        public void addConference(ImsConference mImsConference) {
            TelephonyConnectionService.this.addTelephonyConference(mImsConference);
        }
        @Override
        public void addExistingConnection(PhoneAccountHandle phoneAccountHandle,
                                          Connection connection) {
            TelephonyConnectionService.this
                    .addExistingConnection(phoneAccountHandle, connection);
        }
        @Override
        public void addExistingConnection(PhoneAccountHandle phoneAccountHandle,
                Connection connection, Conference conference) {
            TelephonyConnectionService.this
                    .addExistingConnection(phoneAccountHandle, connection, conference);
        }
        @Override
        public void addConnectionToConferenceController(TelephonyConnection connection) {
            TelephonyConnectionService.this.addConnectionToConferenceController(connection);
        }
    };

    private final BroadcastReceiver mTtyBroadcastReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            Log.v(this, "onReceive, action: %s", action);
            if (action.equals(TelecomManager.ACTION_TTY_PREFERRED_MODE_CHANGED)) {
                int newPreferredTtyMode = intent.getIntExtra(
                        TelecomManager.EXTRA_TTY_PREFERRED_MODE, TelecomManager.TTY_MODE_OFF);

                boolean isTtyNowEnabled = newPreferredTtyMode != TelecomManager.TTY_MODE_OFF;
                if (isTtyNowEnabled != mIsTtyEnabled) {
                    handleTtyModeChange(isTtyNowEnabled);
                }
            }
        }
    };

    private final TelephonyConferenceController mTelephonyConferenceController =
            new TelephonyConferenceController(mTelephonyConnectionServiceProxy);
    private final CdmaConferenceController mCdmaConferenceController =
            new CdmaConferenceController(this);
    private ImsConferenceController mImsConferenceController;

    private ComponentName mExpectedComponentName = null;
    private RadioOnHelper mRadioOnHelper;
    private EmergencyTonePlayer mEmergencyTonePlayer;
    private HoldTracker mHoldTracker;
    private boolean mIsTtyEnabled;

    // Contains one TelephonyConnection that has placed a call and a memory of which Phones it has
    // already tried to connect with. There should be only one TelephonyConnection trying to place a
    // call at one time. We also only access this cache from a TelephonyConnection that wishes to
    // redial, so we use a WeakReference that will become stale once the TelephonyConnection is
    // destroyed.
    @VisibleForTesting
    public Pair<WeakReference<TelephonyConnection>, Queue<Phone>> mEmergencyRetryCache;
    private Handler mDdsSwitchHandler;
    private HandlerThread mHandlerThread;
    private DeviceState mDeviceState = new DeviceState();

    /**
     * Keeps track of the status of a SIM slot.
     */
    private static class SlotStatus {
        public int slotId;
        // RAT capabilities
        public int capabilities;
        // By default, we will assume that the slots are not locked.
        public boolean isLocked = false;
        // Is the emergency number associated with the slot
        public boolean hasDialedEmergencyNumber = false;
        //SimState
        public int simState;

        public SlotStatus(int slotId, int capabilities) {
            this.slotId = slotId;
            this.capabilities = capabilities;
        }
    }

    /**
     * SubscriptionManager dependencies for testing.
     */
    @VisibleForTesting
    public interface SubscriptionManagerProxy {
        int getDefaultVoicePhoneId();
        int getSimStateForSlotIdx(int slotId);
        int getPhoneId(int subId);
    }

    private SubscriptionManagerProxy mSubscriptionManagerProxy = new SubscriptionManagerProxy() {
        @Override
        public int getDefaultVoicePhoneId() {
            return SubscriptionManager.getDefaultVoicePhoneId();
        }

        @Override
        public int getSimStateForSlotIdx(int slotId) {
            return SubscriptionManager.getSimStateForSlotIndex(slotId);
        }

        @Override
        public int getPhoneId(int subId) {
            return SubscriptionManager.getPhoneId(subId);
        }
    };

    /**
     * TelephonyManager dependencies for testing.
     */
    @VisibleForTesting
    public interface TelephonyManagerProxy {
        int getPhoneCount();
        boolean hasIccCard(int slotId);
        boolean isCurrentEmergencyNumber(String number);
        Map<Integer, List<EmergencyNumber>> getCurrentEmergencyNumberList();
    }

    private TelephonyManagerProxy mTelephonyManagerProxy;

    private class TelephonyManagerProxyImpl implements TelephonyManagerProxy {
        private final TelephonyManager mTelephonyManager;


        TelephonyManagerProxyImpl(Context context) {
            mTelephonyManager = new TelephonyManager(context);
        }

        @Override
        public int getPhoneCount() {
            return mTelephonyManager.getPhoneCount();
        }

        @Override
        public boolean hasIccCard(int slotId) {
            return mTelephonyManager.hasIccCard(slotId);
        }

        @Override
        public boolean isCurrentEmergencyNumber(String number) {
            try {
                return mTelephonyManager.isEmergencyNumber(number);
            } catch (IllegalStateException ise) {
                return false;
            }
        }

        @Override
        public Map<Integer, List<EmergencyNumber>> getCurrentEmergencyNumberList() {
            try {
                return mTelephonyManager.getEmergencyNumberList();
            } catch (IllegalStateException ise) {
                return new HashMap<>();
            }
        }
    }

    /**
     * PhoneFactory Dependencies for testing.
     */
    @VisibleForTesting
    public interface PhoneFactoryProxy {
        Phone getPhone(int index);
        Phone getDefaultPhone();
        Phone[] getPhones();
    }

    private PhoneFactoryProxy mPhoneFactoryProxy = new PhoneFactoryProxy() {
        @Override
        public Phone getPhone(int index) {
            return PhoneFactory.getPhone(index);
        }

        @Override
        public Phone getDefaultPhone() {
            return PhoneFactory.getDefaultPhone();
        }

        @Override
        public Phone[] getPhones() {
            return PhoneFactory.getPhones();
        }
    };

    /**
     * PhoneUtils dependencies for testing.
     */
    @VisibleForTesting
    public interface PhoneUtilsProxy {
        int getSubIdForPhoneAccountHandle(PhoneAccountHandle accountHandle);
        PhoneAccountHandle makePstnPhoneAccountHandle(Phone phone);
        PhoneAccountHandle makePstnPhoneAccountHandleWithPrefix(Phone phone, String prefix,
                boolean isEmergency);
    }

    private PhoneUtilsProxy mPhoneUtilsProxy = new PhoneUtilsProxy() {
        @Override
        public int getSubIdForPhoneAccountHandle(PhoneAccountHandle accountHandle) {
            return PhoneUtils.getSubIdForPhoneAccountHandle(accountHandle);
        }

        @Override
        public PhoneAccountHandle makePstnPhoneAccountHandle(Phone phone) {
            return PhoneUtils.makePstnPhoneAccountHandle(phone);
        }

        @Override
        public PhoneAccountHandle makePstnPhoneAccountHandleWithPrefix(Phone phone, String prefix,
                boolean isEmergency) {
            return PhoneUtils.makePstnPhoneAccountHandleWithPrefix(phone, prefix, isEmergency);
        }
    };

    /**
     * PhoneNumberUtils dependencies for testing.
     */
    @VisibleForTesting
    public interface PhoneNumberUtilsProxy {
        String convertToEmergencyNumber(Context context, String number);
    }

    private PhoneNumberUtilsProxy mPhoneNumberUtilsProxy = new PhoneNumberUtilsProxy() {
        @Override
        public String convertToEmergencyNumber(Context context, String number) {
            return PhoneNumberUtils.convertToEmergencyNumber(context, number);
        }
    };

    /**
     * PhoneSwitcher dependencies for testing.
     */
    @VisibleForTesting
    public interface PhoneSwitcherProxy {
        PhoneSwitcher getPhoneSwitcher();
    }

    private PhoneSwitcherProxy mPhoneSwitcherProxy = new PhoneSwitcherProxy() {
        @Override
        public PhoneSwitcher getPhoneSwitcher() {
            return PhoneSwitcher.getInstance();
        }
    };

    /**
     * Factory for Handler creation in order to remove flakiness during t esting.
     */
    @VisibleForTesting
    public interface HandlerFactory {
        HandlerThread createHandlerThread(String name);
        Handler createHandler(Looper looper);
    }

    private HandlerFactory mHandlerFactory = new HandlerFactory() {
        @Override
        public HandlerThread createHandlerThread(String name) {
            return new HandlerThread(name);
        }

        @Override
        public Handler createHandler(Looper looper) {
            return new Handler(looper);
        }
    };

    /**
     * DisconnectCause depends on PhoneGlobals in order to get a system context. Mock out
     * dependency for testing.
     */
    @VisibleForTesting
    public interface DisconnectCauseFactory {
        DisconnectCause toTelecomDisconnectCause(int telephonyDisconnectCause, String reason);
        DisconnectCause toTelecomDisconnectCause(int telephonyDisconnectCause,
                String reason, int phoneId);
    }

    private DisconnectCauseFactory mDisconnectCauseFactory = new DisconnectCauseFactory() {
        @Override
        public DisconnectCause toTelecomDisconnectCause(int telephonyDisconnectCause,
                String reason) {
            return DisconnectCauseUtil.toTelecomDisconnectCause(telephonyDisconnectCause, reason);
        }

        @Override
        public DisconnectCause toTelecomDisconnectCause(int telephonyDisconnectCause, String reason,
                int phoneId) {
            return DisconnectCauseUtil.toTelecomDisconnectCause(telephonyDisconnectCause, reason,
                    phoneId);
        }
    };

    /**
     * Overrides SubscriptionManager dependencies for testing.
     */
    @VisibleForTesting
    public void setSubscriptionManagerProxy(SubscriptionManagerProxy proxy) {
        mSubscriptionManagerProxy = proxy;
    }

    /**
     * Overrides TelephonyManager dependencies for testing.
     */
    @VisibleForTesting
    public void setTelephonyManagerProxy(TelephonyManagerProxy proxy) {
        mTelephonyManagerProxy = proxy;
    }

    /**
     * Overrides PhoneFactory dependencies for testing.
     */
    @VisibleForTesting
    public void setPhoneFactoryProxy(PhoneFactoryProxy proxy) {
        mPhoneFactoryProxy = proxy;
    }

    /**
     * Overrides configuration and settings dependencies for testing.
     */
    @VisibleForTesting
    public void setDeviceState(DeviceState state) {
        mDeviceState = state;
    }

    /**
     * Overrides radioOnHelper for testing.
     */
    @VisibleForTesting
    public void setRadioOnHelper(RadioOnHelper radioOnHelper) {
        mRadioOnHelper = radioOnHelper;
    }

    /**
     * Overrides PhoneSwitcher dependencies for testing.
     */
    @VisibleForTesting
    public void setPhoneSwitcherProxy(PhoneSwitcherProxy proxy) {
        mPhoneSwitcherProxy = proxy;
    }

    /**
     * Overrides PhoneNumberUtils dependencies for testing.
     */
    @VisibleForTesting
    public void setPhoneNumberUtilsProxy(PhoneNumberUtilsProxy proxy) {
        mPhoneNumberUtilsProxy = proxy;
    }

    /**
     * Overrides PhoneUtils dependencies for testing.
     */
    @VisibleForTesting
    public void setPhoneUtilsProxy(PhoneUtilsProxy proxy) {
        mPhoneUtilsProxy = proxy;
    }

    /**
     * Override Handler creation factory for testing.
     */
    @VisibleForTesting
    public void setHandlerFactory(HandlerFactory handlerFactory) {
        mHandlerFactory = handlerFactory;
    }

    /**
     * Override DisconnectCause creation for testing.
     */
    @VisibleForTesting
    public void setDisconnectCauseFactory(DisconnectCauseFactory factory) {
        mDisconnectCauseFactory = factory;
    }

    /**
     * A listener to actionable events specific to the TelephonyConnection.
     */
    private final TelephonyConnection.TelephonyConnectionListener mTelephonyConnectionListener =
            new TelephonyConnection.TelephonyConnectionListener() {
        @Override
        public void onOriginalConnectionConfigured(TelephonyConnection c) {
            addConnectionToConferenceController(c);
        }

        @Override
        public void onOriginalConnectionRetry(TelephonyConnection c, boolean isPermanentFailure) {
            retryOutgoingOriginalConnection(c, isPermanentFailure);
        }
    };

    private final TelephonyConferenceBase.TelephonyConferenceListener mTelephonyConferenceListener =
            new TelephonyConferenceBase.TelephonyConferenceListener() {
        @Override
        public void onConferenceMembershipChanged(Connection connection) {
            mHoldTracker.updateHoldCapability(connection.getPhoneAccountHandle());
        }
    };

    @Override
    public void onCreate() {
        super.onCreate();
        mImsConferenceController = new ImsConferenceController(
                TelecomAccountRegistry.getInstance(this),
                mTelephonyConnectionServiceProxy,
                // FeatureFlagProxy; used to determine if standalone call emulation is enabled.
                // TODO: Move to carrier config
                () -> true);
        setTelephonyManagerProxy(new TelephonyManagerProxyImpl(getApplicationContext()));
        mExpectedComponentName = new ComponentName(this, this.getClass());
        mEmergencyTonePlayer = new EmergencyTonePlayer(this);
        TelecomAccountRegistry.getInstance(this).setTelephonyConnectionService(this);
        mHoldTracker = new HoldTracker();
        mIsTtyEnabled = mDeviceState.isTtyModeEnabled(this);

        IntentFilter intentFilter = new IntentFilter(
                TelecomManager.ACTION_TTY_PREFERRED_MODE_CHANGED);
        registerReceiver(mTtyBroadcastReceiver, intentFilter);
        mHandlerThread = mHandlerFactory.createHandlerThread("DdsSwitchHandlerThread");
        mHandlerThread.start();
        Looper looper = mHandlerThread.getLooper();
        mDdsSwitchHandler = mHandlerFactory.createHandler(looper);
    }

    @Override
    public boolean onUnbind(Intent intent) {
        unregisterReceiver(mTtyBroadcastReceiver);
        mHandlerThread.quitSafely();
        return super.onUnbind(intent);
    }

    private Conference placeOutgoingConference(ConnectionRequest request,
            Connection resultConnection, Phone phone) {
        if (resultConnection instanceof TelephonyConnection) {
            return placeOutgoingConference((TelephonyConnection) resultConnection, phone, request);
        }
        return null;
    }

    private Conference placeOutgoingConference(TelephonyConnection conferenceHostConnection,
            Phone phone, ConnectionRequest request) {
        updatePhoneAccount(conferenceHostConnection, phone);
        com.android.internal.telephony.Connection originalConnection = null;
        try {
            originalConnection = phone.startConference(
                    getParticipantsToDial(request.getParticipants()),
                    new ImsPhone.ImsDialArgs.Builder()
                    .setVideoState(request.getVideoState())
                    .setRttTextStream(conferenceHostConnection.getRttTextStream())
                    .build());
        } catch (CallStateException e) {
            Log.e(this, e, "placeOutgoingConference, phone.startConference exception: " + e);
            handleCallStateException(e, conferenceHostConnection, phone);
            return null;
        }

        if (originalConnection == null) {
            Log.d(this, "placeOutgoingConference, phone.startConference returned null");
            conferenceHostConnection.setDisconnected(DisconnectCauseUtil.toTelecomDisconnectCause(
                    android.telephony.DisconnectCause.OUTGOING_FAILURE,
                    "conferenceHostConnection is null",
                    phone.getPhoneId()));
            conferenceHostConnection.clearOriginalConnection();
            conferenceHostConnection.destroy();
        } else {
            conferenceHostConnection.setOriginalConnection(originalConnection);
        }

        return prepareConference(conferenceHostConnection, request.getAccountHandle());
    }

    Conference prepareConference(Connection conn, PhoneAccountHandle phoneAccountHandle) {
        if (!(conn instanceof TelephonyConnection)) {
            Log.w(this, "prepareConference returning NULL conference");
            return null;
        }

        TelephonyConnection connection = (TelephonyConnection)conn;

        ImsConference conference = new ImsConference(TelecomAccountRegistry.getInstance(this),
                mTelephonyConnectionServiceProxy, connection,
                phoneAccountHandle, () -> true,
                ImsConferenceController.getCarrierConfig(connection.getPhone()));
        mImsConferenceController.addConference(conference);
        conference.setVideoState(connection,
                connection.getVideoState());
        conference.setVideoProvider(connection,
                connection.getVideoProvider());
        conference.setStatusHints(connection.getStatusHints());
        conference.setAddress(connection.getAddress(),
                connection.getAddressPresentation());
        conference.setCallerDisplayName(connection.getCallerDisplayName(),
                connection.getCallerDisplayNamePresentation());
        conference.setParticipants(connection.getParticipants());
        return conference;
    }

    @Override
    public @Nullable Conference onCreateIncomingConference(
            @Nullable PhoneAccountHandle connectionManagerPhoneAccount,
            @NonNull final ConnectionRequest request) {
        Log.i(this, "onCreateIncomingConference, request: " + request);
        Connection connection = onCreateIncomingConnection(connectionManagerPhoneAccount, request);
        Log.d(this, "onCreateIncomingConference, connection: %s", connection);
        if (connection == null) {
            Log.i(this, "onCreateIncomingConference, implementation returned null connection.");
            return Conference.createFailedConference(
                    new DisconnectCause(DisconnectCause.ERROR, "IMPL_RETURNED_NULL_CONNECTION"),
                    request.getAccountHandle());
        }

        final Phone phone = getPhoneForAccount(request.getAccountHandle(),
                false /* isEmergencyCall*/, null /* not an emergency call */);
        if (phone == null) {
            Log.d(this, "onCreateIncomingConference, phone is null");
            return Conference.createFailedConference(
                    DisconnectCauseUtil.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.OUT_OF_SERVICE,
                            "Phone is null"),
                    request.getAccountHandle());
        }

        return prepareConference(connection, request.getAccountHandle());
    }

    @Override
    public @Nullable Conference onCreateOutgoingConference(
            @Nullable PhoneAccountHandle connectionManagerPhoneAccount,
            @NonNull final ConnectionRequest request) {
        Log.i(this, "onCreateOutgoingConference, request: " + request);
        Connection connection = onCreateOutgoingConnection(connectionManagerPhoneAccount, request);
        Log.d(this, "onCreateOutgoingConference, connection: %s", connection);
        if (connection == null) {
            Log.i(this, "onCreateOutgoingConference, implementation returned null connection.");
            return Conference.createFailedConference(
                    new DisconnectCause(DisconnectCause.ERROR, "IMPL_RETURNED_NULL_CONNECTION"),
                    request.getAccountHandle());
        }

        final Phone phone = getPhoneForAccount(request.getAccountHandle(),
                false /* isEmergencyCall*/, null /* not an emergency call */);
        if (phone == null) {
            Log.d(this, "onCreateOutgoingConference, phone is null");
            return Conference.createFailedConference(
                    DisconnectCauseUtil.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.OUT_OF_SERVICE,
                            "Phone is null"),
                    request.getAccountHandle());
        }

        return placeOutgoingConference(request, connection, phone);
    }

    private String[] getParticipantsToDial(List<Uri> participants) {
        String[] participantsToDial = new String[participants.size()];
        int i = 0;
        for (Uri participant : participants) {
           participantsToDial[i] = participant.getSchemeSpecificPart();
           i++;
        }
        return participantsToDial;
    }

    @Override
    public Connection onCreateOutgoingConnection(
            PhoneAccountHandle connectionManagerPhoneAccount,
            final ConnectionRequest request) {
        Log.i(this, "onCreateOutgoingConnection, request: " + request);

        Uri handle = request.getAddress();
        boolean isAdhocConference = request.isAdhocConferenceCall();

        if (!isAdhocConference && handle == null) {
            Log.d(this, "onCreateOutgoingConnection, handle is null");
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.NO_PHONE_NUMBER_SUPPLIED,
                            "No phone number supplied"));
        }

        String scheme = handle.getScheme();
        String number;
        if (PhoneAccount.SCHEME_VOICEMAIL.equals(scheme)) {
            // TODO: We don't check for SecurityException here (requires
            // CALL_PRIVILEGED permission).
            final Phone phone = getPhoneForAccount(request.getAccountHandle(),
                    false /* isEmergencyCall */, null /* not an emergency call */);
            if (phone == null) {
                Log.d(this, "onCreateOutgoingConnection, phone is null");
                return Connection.createFailedConnection(
                        mDisconnectCauseFactory.toTelecomDisconnectCause(
                                android.telephony.DisconnectCause.OUT_OF_SERVICE,
                                "Phone is null"));
            }
            number = phone.getVoiceMailNumber();
            if (TextUtils.isEmpty(number)) {
                Log.d(this, "onCreateOutgoingConnection, no voicemail number set.");
                return Connection.createFailedConnection(
                        mDisconnectCauseFactory.toTelecomDisconnectCause(
                                android.telephony.DisconnectCause.VOICEMAIL_NUMBER_MISSING,
                                "Voicemail scheme provided but no voicemail number set.",
                                phone.getPhoneId()));
            }

            // Convert voicemail: to tel:
            handle = Uri.fromParts(PhoneAccount.SCHEME_TEL, number, null);
        } else {
            if (!PhoneAccount.SCHEME_TEL.equals(scheme)) {
                Log.d(this, "onCreateOutgoingConnection, Handle %s is not type tel", scheme);
                return Connection.createFailedConnection(
                        mDisconnectCauseFactory.toTelecomDisconnectCause(
                                android.telephony.DisconnectCause.INVALID_NUMBER,
                                "Handle scheme is not type tel"));
            }

            number = handle.getSchemeSpecificPart();
            if (TextUtils.isEmpty(number)) {
                Log.d(this, "onCreateOutgoingConnection, unable to parse number");
                return Connection.createFailedConnection(
                        mDisconnectCauseFactory.toTelecomDisconnectCause(
                                android.telephony.DisconnectCause.INVALID_NUMBER,
                                "Unable to parse number"));
            }

            final Phone phone = getPhoneForAccount(request.getAccountHandle(),
                    false /* isEmergencyCall*/, null /* not an emergency call */);
            if (phone != null && CDMA_ACTIVATION_CODE_REGEX_PATTERN.matcher(number).matches()) {
                // Obtain the configuration for the outgoing phone's SIM. If the outgoing number
                // matches the *228 regex pattern, fail the call. This number is used for OTASP, and
                // when dialed could lock LTE SIMs to 3G if not prohibited..
                boolean disableActivation = false;
                CarrierConfigManager cfgManager = (CarrierConfigManager)
                        phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
                if (cfgManager != null) {
                    disableActivation = cfgManager.getConfigForSubId(phone.getSubId())
                            .getBoolean(CarrierConfigManager.KEY_DISABLE_CDMA_ACTIVATION_CODE_BOOL);
                }

                if (disableActivation) {
                    return Connection.createFailedConnection(
                            mDisconnectCauseFactory.toTelecomDisconnectCause(
                                    android.telephony.DisconnectCause
                                            .CDMA_ALREADY_ACTIVATED,
                                    "Tried to dial *228",
                                    phone.getPhoneId()));
                }
            }
        }

        final boolean isEmergencyNumber = mTelephonyManagerProxy.isCurrentEmergencyNumber(number);
        // Find out if this is a test emergency number
        final boolean isTestEmergencyNumber = isEmergencyNumberTestNumber(number);

        // Convert into emergency number if necessary
        // This is required in some regions (e.g. Taiwan).
        if (isEmergencyNumber) {
            final Phone phone = getPhoneForAccount(request.getAccountHandle(), false,
                    handle.getSchemeSpecificPart());
            // We only do the conversion if the phone is not in service. The un-converted
            // emergency numbers will go to the correct destination when the phone is in-service,
            // so they will only need the special emergency call setup when the phone is out of
            // service.
            if (phone == null || phone.getServiceState().getState()
                    != ServiceState.STATE_IN_SERVICE) {
                String convertedNumber = mPhoneNumberUtilsProxy.convertToEmergencyNumber(this,
                        number);
                if (!TextUtils.equals(convertedNumber, number)) {
                    Log.i(this, "onCreateOutgoingConnection, converted to emergency number");
                    number = convertedNumber;
                    handle = Uri.fromParts(PhoneAccount.SCHEME_TEL, number, null);
                }
            }
        }
        final String numberToDial = number;

        final boolean isAirplaneModeOn = mDeviceState.isAirplaneModeOn(this);

        boolean needToTurnOnRadio = (isEmergencyNumber && (!isRadioOn() || isAirplaneModeOn))
                || isRadioPowerDownOnBluetooth();

        // Get the right phone object from the account data passed in.
        final Phone phone = getPhoneForAccount(request.getAccountHandle(), isEmergencyNumber,
                /* Note: when not an emergency, handle can be null for unknown callers */
                handle == null ? null : handle.getSchemeSpecificPart());

        if (needToTurnOnRadio) {
            final Uri resultHandle = handle;
            final int originalPhoneType = phone.getPhoneType();
            final Connection resultConnection = getTelephonyConnection(request, numberToDial,
                    isEmergencyNumber, resultHandle, phone);
            if (mRadioOnHelper == null) {
                mRadioOnHelper = new RadioOnHelper(this);
            }
            mRadioOnHelper.triggerRadioOnAndListen(new RadioOnStateListener.Callback() {
                @Override
                public void onComplete(RadioOnStateListener listener, boolean isRadioReady) {
                    handleOnComplete(isRadioReady, isEmergencyNumber, resultConnection, request,
                            numberToDial, resultHandle, originalPhoneType, phone);
                }

                @Override
                public boolean isOkToCall(Phone phone, int serviceState) {
                    // HAL 1.4 introduced a new variant of dial for emergency calls, which includes
                    // an isTesting parameter. For HAL 1.4+, do not wait for IN_SERVICE, this will
                    // be handled at the RIL/vendor level by emergencyDial(...).
                    boolean waitForInServiceToDialEmergency = isTestEmergencyNumber
                            && phone.getHalVersion().less(RIL.RADIO_HAL_VERSION_1_4);
                    if (isEmergencyNumber && !waitForInServiceToDialEmergency) {
                        // We currently only look to make sure that the radio is on before dialing.
                        // We should be able to make emergency calls at any time after the radio has
                        // been powered on and isn't in the UNAVAILABLE state, even if it is
                        // reporting the OUT_OF_SERVICE state.
                        return (phone.getState() == PhoneConstants.State.OFFHOOK)
                            || phone.getServiceStateTracker().isRadioOn();
                    } else {
                        // Wait until we are in service and ready to make calls. This can happen
                        // when we power down the radio on bluetooth to save power on watches or if
                        // it is a test emergency number and we have to wait for the device to move
                        // IN_SERVICE before the call can take place over normal routing.
                        return (phone.getState() == PhoneConstants.State.OFFHOOK)
                                // Do not wait for voice in service on opportunistic SIMs.
                                || SubscriptionController.getInstance().isOpportunistic(
                                        phone.getSubId())
                                || serviceState == ServiceState.STATE_IN_SERVICE;
                    }
                }
            }, isEmergencyNumber && !isTestEmergencyNumber, phone);
            // Return the still unconnected GsmConnection and wait for the Radios to boot before
            // connecting it to the underlying Phone.
            return resultConnection;
        } else {
            if (!canAddCall() && !isEmergencyNumber) {
                Log.d(this, "onCreateOutgoingConnection, cannot add call .");
                return Connection.createFailedConnection(
                        new DisconnectCause(DisconnectCause.ERROR,
                                getApplicationContext().getText(
                                        R.string.incall_error_cannot_add_call),
                                getApplicationContext().getText(
                                        R.string.incall_error_cannot_add_call),
                                "Add call restricted due to ongoing video call"));
            }

            if (!isEmergencyNumber) {
                final Connection resultConnection = getTelephonyConnection(request, numberToDial,
                        false, handle, phone);
                if (isAdhocConference) {
                    if (resultConnection instanceof TelephonyConnection) {
                        TelephonyConnection conn = (TelephonyConnection)resultConnection;
                        conn.setParticipants(request.getParticipants());
                    }
                    return resultConnection;
                } else {
                    return placeOutgoingConnection(request, resultConnection, phone);
                }
            } else {
                final Connection resultConnection = getTelephonyConnection(request, numberToDial,
                        true, handle, phone);
                mDdsSwitchHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        boolean result = delayDialForDdsSwitch(phone);
                        Log.i(this,
                                "onCreateOutgoingConn - delayDialForDdsSwitch result = " + result);
                        placeOutgoingConnection(request, resultConnection, phone);
                    }
                });
                return resultConnection;
            }
        }
    }

    private Connection placeOutgoingConnection(ConnectionRequest request,
            Connection resultConnection, Phone phone) {
        // If there was a failure, the resulting connection will not be a TelephonyConnection,
        // so don't place the call!
        if (resultConnection instanceof TelephonyConnection) {
            if (request.getExtras() != null && request.getExtras().getBoolean(
                    TelecomManager.EXTRA_USE_ASSISTED_DIALING, false)) {
                ((TelephonyConnection) resultConnection).setIsUsingAssistedDialing(true);
            }
            placeOutgoingConnection((TelephonyConnection) resultConnection, phone, request);
        }
        return resultConnection;
    }

    private boolean isEmergencyNumberTestNumber(String number) {
        number = PhoneNumberUtils.stripSeparators(number);
        Map<Integer, List<EmergencyNumber>> list =
                mTelephonyManagerProxy.getCurrentEmergencyNumberList();
        // Do not worry about which subscription the test emergency call is on yet, only detect that
        // it is an emergency.
        for (Integer sub : list.keySet()) {
            for (EmergencyNumber eNumber : list.get(sub)) {
                if (number.equals(eNumber.getNumber())
                        && eNumber.isFromSources(EmergencyNumber.EMERGENCY_NUMBER_SOURCE_TEST)) {
                    Log.i(this, "isEmergencyNumberTestNumber: " + number + " has been detected as "
                            + "a test emergency number.,");
                    return true;
                }
            }
        }
        return false;
    }

    /**
     * Whether the cellular radio is power off because the device is on Bluetooth.
     */
    private boolean isRadioPowerDownOnBluetooth() {
        final boolean allowed = mDeviceState.isRadioPowerDownAllowedOnBluetooth(this);
        final int cellOn = mDeviceState.getCellOnStatus(this);
        return (allowed && cellOn == PhoneConstants.CELL_ON_FLAG && !isRadioOn());
    }

    /**
     * Handle the onComplete callback of RadioOnStateListener.
     */
    private void handleOnComplete(boolean isRadioReady, boolean isEmergencyNumber,
            Connection originalConnection, ConnectionRequest request, String numberToDial,
            Uri handle, int originalPhoneType, Phone phone) {
        // Make sure the Call has not already been canceled by the user.
        if (originalConnection.getState() == Connection.STATE_DISCONNECTED) {
            Log.i(this, "Call disconnected before the outgoing call was placed. Skipping call "
                    + "placement.");
            if (isEmergencyNumber) {
                // If call is already canceled by the user, notify modem to exit emergency call
                // mode by sending radio on with forEmergencyCall=false.
                for (Phone curPhone : mPhoneFactoryProxy.getPhones()) {
                    curPhone.setRadioPower(true, false, false, true);
                }
            }
            return;
        }
        if (isRadioReady) {
            if (!isEmergencyNumber) {
                adjustAndPlaceOutgoingConnection(phone, originalConnection, request, numberToDial,
                        handle, originalPhoneType, false);
            } else {
                mDdsSwitchHandler.post(new Runnable() {
                    @Override
                    public void run() {
                        boolean result = delayDialForDdsSwitch(phone);
                        Log.i(this, "handleOnComplete - delayDialForDdsSwitch result = " + result);
                        adjustAndPlaceOutgoingConnection(phone, originalConnection, request,
                                numberToDial, handle, originalPhoneType, true);
                    }
                });
            }

        } else {
            Log.w(this, "onCreateOutgoingConnection, failed to turn on radio");
            closeOrDestroyConnection(originalConnection,
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.POWER_OFF,
                            "Failed to turn on radio."));
        }
    }

    private void adjustAndPlaceOutgoingConnection(Phone phone, Connection connectionToEvaluate,
            ConnectionRequest request, String numberToDial, Uri handle, int originalPhoneType,
            boolean isEmergencyNumber) {
        // If the PhoneType of the Phone being used is different than the Default Phone, then we
        // need to create a new Connection using that PhoneType and replace it in Telecom.
        if (phone.getPhoneType() != originalPhoneType) {
            Connection repConnection = getTelephonyConnection(request, numberToDial,
                    isEmergencyNumber, handle, phone);
            // If there was a failure, the resulting connection will not be a TelephonyConnection,
            // so don't place the call, just return!
            if (repConnection instanceof TelephonyConnection) {
                placeOutgoingConnection((TelephonyConnection) repConnection, phone, request);
            }
            // Notify Telecom of the new Connection type.
            // TODO: Switch out the underlying connection instead of creating a new
            // one and causing UI Jank.
            boolean noActiveSimCard = SubscriptionController.getInstance()
                    .getActiveSubInfoCount(phone.getContext().getOpPackageName(),
                            phone.getContext().getAttributionTag()) == 0;
            // If there's no active sim card and the device is in emergency mode, use E account.
            addExistingConnection(mPhoneUtilsProxy.makePstnPhoneAccountHandleWithPrefix(
                    phone, "", isEmergencyNumber && noActiveSimCard), repConnection);
            // Remove the old connection from Telecom after.
            closeOrDestroyConnection(connectionToEvaluate,
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.OUTGOING_CANCELED,
                            "Reconnecting outgoing Emergency Call.",
                            phone.getPhoneId()));
        } else {
            placeOutgoingConnection((TelephonyConnection) connectionToEvaluate, phone, request);
        }
    }

    /**
     * @return {@code true} if any other call is disabling the ability to add calls, {@code false}
     *      otherwise.
     */
    private boolean canAddCall() {
        Collection<Connection> connections = getAllConnections();
        for (Connection connection : connections) {
            if (connection.getExtras() != null &&
                    connection.getExtras().getBoolean(Connection.EXTRA_DISABLE_ADD_CALL, false)) {
                return false;
            }
        }
        return true;
    }

    private Connection getTelephonyConnection(final ConnectionRequest request, final String number,
            boolean isEmergencyNumber, final Uri handle, Phone phone) {

        if (phone == null) {
            final Context context = getApplicationContext();
            if (mDeviceState.shouldCheckSimStateBeforeOutgoingCall(this)) {
                // Check SIM card state before the outgoing call.
                // Start the SIM unlock activity if PIN_REQUIRED.
                final Phone defaultPhone = mPhoneFactoryProxy.getDefaultPhone();
                final IccCard icc = defaultPhone.getIccCard();
                IccCardConstants.State simState = IccCardConstants.State.UNKNOWN;
                if (icc != null) {
                    simState = icc.getState();
                }
                if (simState == IccCardConstants.State.PIN_REQUIRED) {
                    final String simUnlockUiPackage = context.getResources().getString(
                            R.string.config_simUnlockUiPackage);
                    final String simUnlockUiClass = context.getResources().getString(
                            R.string.config_simUnlockUiClass);
                    if (simUnlockUiPackage != null && simUnlockUiClass != null) {
                        Intent simUnlockIntent = new Intent().setComponent(new ComponentName(
                                simUnlockUiPackage, simUnlockUiClass));
                        simUnlockIntent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
                        try {
                            context.startActivity(simUnlockIntent);
                        } catch (ActivityNotFoundException exception) {
                            Log.e(this, exception, "Unable to find SIM unlock UI activity.");
                        }
                    }
                    return Connection.createFailedConnection(
                            mDisconnectCauseFactory.toTelecomDisconnectCause(
                                    android.telephony.DisconnectCause.OUT_OF_SERVICE,
                                    "SIM_STATE_PIN_REQUIRED"));
                }
            }

            Log.d(this, "onCreateOutgoingConnection, phone is null");
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.OUT_OF_SERVICE, "Phone is null"));
        }

        // Check both voice & data RAT to enable normal CS call,
        // when voice RAT is OOS but Data RAT is present.
        int state = phone.getServiceState().getState();
        if (state == ServiceState.STATE_OUT_OF_SERVICE) {
            int dataNetType = phone.getServiceState().getDataNetworkType();
            if (dataNetType == TelephonyManager.NETWORK_TYPE_LTE ||
                    dataNetType == TelephonyManager.NETWORK_TYPE_LTE_CA) {
                state = phone.getServiceState().getDataRegistrationState();
            }
        }

        // If we're dialing a non-emergency number and the phone is in ECM mode, reject the call if
        // carrier configuration specifies that we cannot make non-emergency calls in ECM mode.
        if (!isEmergencyNumber && phone.isInEcm()) {
            boolean allowNonEmergencyCalls = true;
            CarrierConfigManager cfgManager = (CarrierConfigManager)
                    phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
            if (cfgManager != null) {
                allowNonEmergencyCalls = cfgManager.getConfigForSubId(phone.getSubId())
                        .getBoolean(CarrierConfigManager.KEY_ALLOW_NON_EMERGENCY_CALLS_IN_ECM_BOOL);
            }

            if (!allowNonEmergencyCalls) {
                return Connection.createFailedConnection(
                        mDisconnectCauseFactory.toTelecomDisconnectCause(
                                android.telephony.DisconnectCause.CDMA_NOT_EMERGENCY,
                                "Cannot make non-emergency call in ECM mode.",
                                phone.getPhoneId()));
            }
        }

        if (!isEmergencyNumber) {
            switch (state) {
                case ServiceState.STATE_IN_SERVICE:
                case ServiceState.STATE_EMERGENCY_ONLY:
                    break;
                case ServiceState.STATE_OUT_OF_SERVICE:
                    if (phone.isUtEnabled() && number.endsWith("#")) {
                        Log.d(this, "onCreateOutgoingConnection dial for UT");
                        break;
                    } else {
                        return Connection.createFailedConnection(
                                mDisconnectCauseFactory.toTelecomDisconnectCause(
                                        android.telephony.DisconnectCause.OUT_OF_SERVICE,
                                        "ServiceState.STATE_OUT_OF_SERVICE",
                                        phone.getPhoneId()));
                    }
                case ServiceState.STATE_POWER_OFF:
                    // Don't disconnect if radio is power off because the device is on Bluetooth.
                    if (isRadioPowerDownOnBluetooth()) {
                        break;
                    }
                    return Connection.createFailedConnection(
                            mDisconnectCauseFactory.toTelecomDisconnectCause(
                                    android.telephony.DisconnectCause.POWER_OFF,
                                    "ServiceState.STATE_POWER_OFF",
                                    phone.getPhoneId()));
                default:
                    Log.d(this, "onCreateOutgoingConnection, unknown service state: %d", state);
                    return Connection.createFailedConnection(
                            mDisconnectCauseFactory.toTelecomDisconnectCause(
                                    android.telephony.DisconnectCause.OUTGOING_FAILURE,
                                    "Unknown service state " + state,
                                    phone.getPhoneId()));
            }
        }

        final boolean isTtyModeEnabled = mDeviceState.isTtyModeEnabled(this);
        if (VideoProfile.isVideo(request.getVideoState()) && isTtyModeEnabled
                && !isEmergencyNumber) {
            return Connection.createFailedConnection(mDisconnectCauseFactory.toTelecomDisconnectCause(
                    android.telephony.DisconnectCause.VIDEO_CALL_NOT_ALLOWED_WHILE_TTY_ENABLED,
                    null, phone.getPhoneId()));
        }

        // Check for additional limits on CDMA phones.
        final Connection failedConnection = checkAdditionalOutgoingCallLimits(phone);
        if (failedConnection != null) {
            return failedConnection;
        }

        // Check roaming status to see if we should block custom call forwarding codes
        if (blockCallForwardingNumberWhileRoaming(phone, number)) {
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.DIALED_CALL_FORWARDING_WHILE_ROAMING,
                            "Call forwarding while roaming",
                            phone.getPhoneId()));
        }


        final TelephonyConnection connection =
                createConnectionFor(phone, null, true /* isOutgoing */, request.getAccountHandle(),
                        request.getTelecomCallId(), request.isAdhocConferenceCall());
        if (connection == null) {
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.OUTGOING_FAILURE,
                            "Invalid phone type",
                            phone.getPhoneId()));
        }
        connection.setAddress(handle, PhoneConstants.PRESENTATION_ALLOWED);
        connection.setTelephonyConnectionInitializing();
        connection.setTelephonyVideoState(request.getVideoState());
        connection.setRttTextStream(request.getRttTextStream());
        connection.setTtyEnabled(isTtyModeEnabled);
        connection.setIsAdhocConferenceCall(request.isAdhocConferenceCall());
        connection.setParticipants(request.getParticipants());
        return connection;
    }

    @Override
    public Connection onCreateIncomingConnection(
            PhoneAccountHandle connectionManagerPhoneAccount,
            ConnectionRequest request) {
        Log.i(this, "onCreateIncomingConnection, request: " + request);
        // If there is an incoming emergency CDMA Call (while the phone is in ECBM w/ No SIM),
        // make sure the PhoneAccount lookup retrieves the default Emergency Phone.
        PhoneAccountHandle accountHandle = request.getAccountHandle();
        boolean isEmergency = false;
        if (accountHandle != null && PhoneUtils.EMERGENCY_ACCOUNT_HANDLE_ID.equals(
                accountHandle.getId())) {
            Log.i(this, "Emergency PhoneAccountHandle is being used for incoming call... " +
                    "Treat as an Emergency Call.");
            isEmergency = true;
        }
        Phone phone = getPhoneForAccount(accountHandle, isEmergency,
                /* Note: when not an emergency, handle can be null for unknown callers */
                request.getAddress() == null ? null : request.getAddress().getSchemeSpecificPart());
        if (phone == null) {
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.ERROR_UNSPECIFIED,
                            "Phone is null"));
        }

        Bundle extras = request.getExtras();
        String disconnectMessage = null;
        if (extras.containsKey(TelecomManager.EXTRA_CALL_DISCONNECT_MESSAGE)) {
            disconnectMessage = extras.getString(TelecomManager.EXTRA_CALL_DISCONNECT_MESSAGE);
            Log.i(this, "onCreateIncomingConnection Disconnect message " + disconnectMessage);
        }

        Call call = phone.getRingingCall();
        if (!call.getState().isRinging()
                || (disconnectMessage != null
                && disconnectMessage.equals(TelecomManager.CALL_AUTO_DISCONNECT_MESSAGE_STRING))) {
            Log.i(this, "onCreateIncomingConnection, no ringing call");
            Connection connection = Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.INCOMING_MISSED,
                            "Found no ringing call",
                            phone.getPhoneId()));

            long time = extras.getLong(TelecomManager.EXTRA_CALL_CREATED_EPOCH_TIME_MILLIS);
            if (time != 0) {
                Log.i(this, "onCreateIncomingConnection. Set connect time info.");
                connection.setConnectTimeMillis(time);
            }

            Uri address = extras.getParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS);
            if (address != null) {
                Log.i(this, "onCreateIncomingConnection. Set caller id info.");
                connection.setAddress(address, TelecomManager.PRESENTATION_ALLOWED);
            }

            return connection;
        }

        com.android.internal.telephony.Connection originalConnection =
                call.getState() == Call.State.WAITING ?
                    call.getLatestConnection() : call.getEarliestConnection();
        if (isOriginalConnectionKnown(originalConnection)) {
            Log.i(this, "onCreateIncomingConnection, original connection already registered");
            return Connection.createCanceledConnection();
        }

        TelephonyConnection connection =
                createConnectionFor(phone, originalConnection, false /* isOutgoing */,
                        request.getAccountHandle(), request.getTelecomCallId(),
                        request.isAdhocConferenceCall());
        handleIncomingRtt(request, originalConnection);
        if (connection == null) {
            return Connection.createCanceledConnection();
        } else {
            connection.setTtyEnabled(mDeviceState.isTtyModeEnabled(getApplicationContext()));
            return connection;
        }
    }

    private void handleIncomingRtt(ConnectionRequest request,
            com.android.internal.telephony.Connection originalConnection) {
        if (originalConnection == null
                || originalConnection.getPhoneType() != PhoneConstants.PHONE_TYPE_IMS) {
            if (request.isRequestingRtt()) {
                Log.w(this, "Requesting RTT on non-IMS call, ignoring");
            }
            return;
        }

        ImsPhoneConnection imsOriginalConnection = (ImsPhoneConnection) originalConnection;
        if (!request.isRequestingRtt()) {
            if (imsOriginalConnection.isRttEnabledForCall()) {
                Log.w(this, "Incoming call requested RTT but we did not get a RttTextStream");
            }
            return;
        }

        Log.i(this, "Setting RTT stream on ImsPhoneConnection in case we need it later");
        imsOriginalConnection.setCurrentRttTextStream(request.getRttTextStream());

        if (!imsOriginalConnection.isRttEnabledForCall()) {
            if (request.isRequestingRtt()) {
                Log.w(this, "Incoming call processed as RTT but did not come in as one. Ignoring");
            }
            return;
        }

        Log.i(this, "Setting the call to be answered with RTT on.");
        imsOriginalConnection.getImsCall().setAnswerWithRtt();
    }

    /**
     * Called by the {@link ConnectionService} when a newly created {@link Connection} has been
     * added to the {@link ConnectionService} and sent to Telecom.  Here it is safe to send
     * connection events.
     *
     * @param connection the {@link Connection}.
     */
    @Override
    public void onCreateConnectionComplete(Connection connection) {
        if (connection instanceof TelephonyConnection) {
            TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
            maybeSendInternationalCallEvent(telephonyConnection);
        }
    }

    @Override
    public void onCreateIncomingConnectionFailed(PhoneAccountHandle connectionManagerPhoneAccount,
            ConnectionRequest request) {
        Log.i(this, "onCreateIncomingConnectionFailed, request: " + request);
        // If there is an incoming emergency CDMA Call (while the phone is in ECBM w/ No SIM),
        // make sure the PhoneAccount lookup retrieves the default Emergency Phone.
        PhoneAccountHandle accountHandle = request.getAccountHandle();
        boolean isEmergency = false;
        if (accountHandle != null && PhoneUtils.EMERGENCY_ACCOUNT_HANDLE_ID.equals(
                accountHandle.getId())) {
            Log.w(this, "onCreateIncomingConnectionFailed:Emergency call failed... ");
            isEmergency = true;
        }
        Phone phone = getPhoneForAccount(accountHandle, isEmergency,
                /* Note: when not an emergency, handle can be null for unknown callers */
                request.getAddress() == null ? null : request.getAddress().getSchemeSpecificPart());
        if (phone == null) {
            Log.w(this, "onCreateIncomingConnectionFailed: can not find corresponding phone.");
            return;
        }

        Call call = phone.getRingingCall();
        if (!call.getState().isRinging()) {
            Log.w(this, "onCreateIncomingConnectionFailed, no ringing call found for failed call");
            return;
        }

        com.android.internal.telephony.Connection originalConnection =
                call.getState() == Call.State.WAITING
                        ? call.getLatestConnection() : call.getEarliestConnection();
        TelephonyConnection knownConnection =
                getConnectionForOriginalConnection(originalConnection);
        if (knownConnection != null) {
            Log.w(this, "onCreateIncomingConnectionFailed, original connection already registered."
                    + " Hanging it up.");
            knownConnection.onAbort();
            return;
        }

        TelephonyConnection connection =
                createConnectionFor(phone, originalConnection, false /* isOutgoing */,
                        request.getAccountHandle(), request.getTelecomCallId());
        if (connection == null) {
            Log.w(this, "onCreateIncomingConnectionFailed, TelephonyConnection created as null, "
                    + "ignoring.");
            return;
        }

        // We have to do all of this work because in some cases, hanging up the call maps to
        // different underlying signaling (CDMA), which is already encapsulated in
        // TelephonyConnection.
        connection.onReject();
        connection.close();
    }

    /**
     * Called by the {@link ConnectionService} when a newly created {@link Conference} has been
     * added to the {@link ConnectionService} and sent to Telecom.  Here it is safe to send
     * connection events.
     *
     * @param conference the {@link Conference}.
     */
    @Override
    public void onCreateConferenceComplete(Conference conference) {
        if (conference instanceof ImsConference) {
            ImsConference imsConference = (ImsConference)conference;
            TelephonyConnection telephonyConnection =
                    (TelephonyConnection)(imsConference.getConferenceHost());
            maybeSendInternationalCallEvent(telephonyConnection);
        }
    }

    public void onCreateIncomingConferenceFailed(PhoneAccountHandle connectionManagerPhoneAccount,
            ConnectionRequest request) {
        Log.i(this, "onCreateIncomingConferenceFailed, request: " + request);
        onCreateIncomingConnectionFailed(connectionManagerPhoneAccount, request);
    }

    @Override
    public void triggerConferenceRecalculate() {
        if (mTelephonyConferenceController.shouldRecalculate()) {
            mTelephonyConferenceController.recalculate();
        }
    }

    @Override
    public Connection onCreateUnknownConnection(PhoneAccountHandle connectionManagerPhoneAccount,
            ConnectionRequest request) {
        Log.i(this, "onCreateUnknownConnection, request: " + request);
        // Use the registered emergency Phone if the PhoneAccountHandle is set to Telephony's
        // Emergency PhoneAccount
        PhoneAccountHandle accountHandle = request.getAccountHandle();
        boolean isEmergency = false;
        if (accountHandle != null && PhoneUtils.EMERGENCY_ACCOUNT_HANDLE_ID.equals(
                accountHandle.getId())) {
            Log.i(this, "Emergency PhoneAccountHandle is being used for unknown call... " +
                    "Treat as an Emergency Call.");
            isEmergency = true;
        }
        Phone phone = getPhoneForAccount(accountHandle, isEmergency,
                /* Note: when not an emergency, handle can be null for unknown callers */
                request.getAddress() == null ? null : request.getAddress().getSchemeSpecificPart());
        if (phone == null) {
            return Connection.createFailedConnection(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(
                            android.telephony.DisconnectCause.ERROR_UNSPECIFIED,
                            "Phone is null"));
        }
        Bundle extras = request.getExtras();

        final List<com.android.internal.telephony.Connection> allConnections = new ArrayList<>();

        // Handle the case where an unknown connection has an IMS external call ID specified; we can
        // skip the rest of the guesswork and just grad that unknown call now.
        if (phone.getImsPhone() != null && extras != null &&
                extras.containsKey(ImsExternalCallTracker.EXTRA_IMS_EXTERNAL_CALL_ID)) {

            ImsPhone imsPhone = (ImsPhone) phone.getImsPhone();
            ImsExternalCallTracker externalCallTracker = imsPhone.getExternalCallTracker();
            int externalCallId = extras.getInt(ImsExternalCallTracker.EXTRA_IMS_EXTERNAL_CALL_ID,
                    -1);

            if (externalCallTracker != null) {
                com.android.internal.telephony.Connection connection =
                        externalCallTracker.getConnectionById(externalCallId);

                if (connection != null) {
                    allConnections.add(connection);
                }
            }
        }

        if (allConnections.isEmpty()) {
            final Call ringingCall = phone.getRingingCall();
            if (ringingCall.hasConnections()) {
                allConnections.addAll(ringingCall.getConnections());
            }
            final Call foregroundCall = phone.getForegroundCall();
            if ((foregroundCall.getState() != Call.State.DISCONNECTED)
                    && (foregroundCall.hasConnections())) {
                allConnections.addAll(foregroundCall.getConnections());
            }
            if (phone.getImsPhone() != null) {
                final Call imsFgCall = phone.getImsPhone().getForegroundCall();
                if ((imsFgCall.getState() != Call.State.DISCONNECTED) && imsFgCall
                        .hasConnections()) {
                    allConnections.addAll(imsFgCall.getConnections());
                }
            }
            final Call backgroundCall = phone.getBackgroundCall();
            if (backgroundCall.hasConnections()) {
                allConnections.addAll(phone.getBackgroundCall().getConnections());
            }
        }

        com.android.internal.telephony.Connection unknownConnection = null;
        for (com.android.internal.telephony.Connection telephonyConnection : allConnections) {
            if (!isOriginalConnectionKnown(telephonyConnection)) {
                unknownConnection = telephonyConnection;
                Log.d(this, "onCreateUnknownConnection: conn = " + unknownConnection);
                break;
            }
        }

        if (unknownConnection == null) {
            Log.i(this, "onCreateUnknownConnection, did not find previously unknown connection.");
            return Connection.createCanceledConnection();
        }

        // We should rely on the originalConnection to get the video state.  The request coming
        // from Telecom does not know the video state of the unknown call.
        int videoState = unknownConnection != null ? unknownConnection.getVideoState() :
                VideoProfile.STATE_AUDIO_ONLY;

        TelephonyConnection connection =
                createConnectionFor(phone, unknownConnection,
                        !unknownConnection.isIncoming() /* isOutgoing */,
                        request.getAccountHandle(), request.getTelecomCallId()
                );

        if (connection == null) {
            return Connection.createCanceledConnection();
        } else {
            connection.updateState();
            return connection;
        }
    }

    /**
     * Conferences two connections.
     *
     * Note: The {@link android.telecom.RemoteConnection#setConferenceableConnections(List)} API has
     * a limitation in that it can only specify conferenceables which are instances of
     * {@link android.telecom.RemoteConnection}.  In the case of an {@link ImsConference}, the
     * regular {@link Connection#setConferenceables(List)} API properly handles being able to merge
     * a {@link Conference} and a {@link Connection}.  As a result when, merging a
     * {@link android.telecom.RemoteConnection} into a {@link android.telecom.RemoteConference}
     * require merging a {@link ConferenceParticipantConnection} which is a child of the
     * {@link Conference} with a {@link TelephonyConnection}.  The
     * {@link ConferenceParticipantConnection} class does not have the capability to initiate a
     * conference merge, so we need to call
     * {@link TelephonyConnection#performConference(Connection)} on either {@code connection1} or
     * {@code connection2}, one of which is an instance of {@link TelephonyConnection}.
     *
     * @param connection1 A connection to merge into a conference call.
     * @param connection2 A connection to merge into a conference call.
     */
    @Override
    public void onConference(Connection connection1, Connection connection2) {
        if (connection1 instanceof TelephonyConnection) {
            ((TelephonyConnection) connection1).performConference(connection2);
        } else if (connection2 instanceof TelephonyConnection) {
            ((TelephonyConnection) connection2).performConference(connection1);
        } else {
            Log.w(this, "onConference - cannot merge connections " +
                    "Connection1: %s, Connection2: %2", connection1, connection2);
        }
    }

    @Override
    public void onConnectionAdded(Connection connection) {
        if (connection instanceof Holdable && !isExternalConnection(connection)) {
            mHoldTracker.addHoldable(
                    connection.getPhoneAccountHandle(), (Holdable) connection);
        }
    }

    @Override
    public void onConnectionRemoved(Connection connection) {
        if (connection instanceof Holdable && !isExternalConnection(connection)) {
            mHoldTracker.removeHoldable(connection.getPhoneAccountHandle(), (Holdable) connection);
        }
    }

    @Override
    public void onConferenceAdded(Conference conference) {
        if (conference instanceof Holdable) {
            mHoldTracker.addHoldable(conference.getPhoneAccountHandle(), (Holdable) conference);
        }
    }

    @Override
    public void onConferenceRemoved(Conference conference) {
        if (conference instanceof Holdable) {
            mHoldTracker.removeHoldable(conference.getPhoneAccountHandle(), (Holdable) conference);
        }
    }

    private boolean isExternalConnection(Connection connection) {
        return (connection.getConnectionProperties() & Connection.PROPERTY_IS_EXTERNAL_CALL)
                == Connection.PROPERTY_IS_EXTERNAL_CALL;
    }

    private boolean blockCallForwardingNumberWhileRoaming(Phone phone, String number) {
        if (phone == null || TextUtils.isEmpty(number) || !phone.getServiceState().getRoaming()) {
            return false;
        }
        boolean allowPrefixIms = true;
        String[] blockPrefixes = null;
        CarrierConfigManager cfgManager = (CarrierConfigManager)
                phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (cfgManager != null) {
            allowPrefixIms = cfgManager.getConfigForSubId(phone.getSubId()).getBoolean(
                    CarrierConfigManager.KEY_SUPPORT_IMS_CALL_FORWARDING_WHILE_ROAMING_BOOL,
                    true);
            if (allowPrefixIms && useImsForAudioOnlyCall(phone)) {
                return false;
            }
            blockPrefixes = cfgManager.getConfigForSubId(phone.getSubId()).getStringArray(
                    CarrierConfigManager.KEY_CALL_FORWARDING_BLOCKS_WHILE_ROAMING_STRING_ARRAY);
        }

        if (blockPrefixes != null) {
            for (String prefix : blockPrefixes) {
                if (number.startsWith(prefix)) {
                    return true;
                }
            }
        }
        return false;
    }

    private boolean useImsForAudioOnlyCall(Phone phone) {
        Phone imsPhone = phone.getImsPhone();

        return imsPhone != null
                && (imsPhone.isVolteEnabled() || imsPhone.isWifiCallingEnabled())
                && (imsPhone.getServiceState().getState() == ServiceState.STATE_IN_SERVICE);
    }

    private boolean isRadioOn() {
        boolean result = false;
        for (Phone phone : mPhoneFactoryProxy.getPhones()) {
            result |= phone.isRadioOn();
        }
        return result;
    }

    private Pair<WeakReference<TelephonyConnection>, Queue<Phone>> makeCachedConnectionPhonePair(
            TelephonyConnection c) {
        Queue<Phone> phones = new LinkedList<>(Arrays.asList(mPhoneFactoryProxy.getPhones()));
        return new Pair<>(new WeakReference<>(c), phones);
    }

    // Update the mEmergencyRetryCache by removing the Phone used to call the last failed emergency
    // number and then moving it to the back of the queue if it is not a permanent failure cause
    // from the modem.
    private void updateCachedConnectionPhonePair(TelephonyConnection c,
            boolean isPermanentFailure) {
        // No cache exists, create a new one.
        if (mEmergencyRetryCache == null) {
            Log.i(this, "updateCachedConnectionPhonePair, cache is null. Generating new cache");
            mEmergencyRetryCache = makeCachedConnectionPhonePair(c);
        // Cache is stale, create a new one with the new TelephonyConnection.
        } else if (mEmergencyRetryCache.first.get() != c) {
            Log.i(this, "updateCachedConnectionPhonePair, cache is stale. Regenerating.");
            mEmergencyRetryCache = makeCachedConnectionPhonePair(c);
        }

        Queue<Phone> cachedPhones = mEmergencyRetryCache.second;
        // Need to refer default phone considering ImsPhone because
        // cachedPhones is a list that contains default phones.
        Phone phoneUsed = c.getPhone().getDefaultPhone();
        if (phoneUsed == null) {
            return;
        }
        // Remove phone used from the list, but for temporary fail cause, it will be added
        // back to list further in this method. However in case of permanent failure, the
        // phone shouldn't be reused, hence it will not be added back again.
        cachedPhones.remove(phoneUsed);
        Log.i(this, "updateCachedConnectionPhonePair, isPermanentFailure:" + isPermanentFailure);
        if (!isPermanentFailure) {
            // In case of temporary failure, add the phone back, this will result adding it
            // to tail of list mEmergencyRetryCache.second, giving other phone more
            // priority and that is what we want.
            cachedPhones.offer(phoneUsed);
        }
    }

    /**
     * Updates a cache containing all of the slots that are available for redial at any point.
     *
     * - If a Connection returns with the disconnect cause EMERGENCY_TEMP_FAILURE, keep that phone
     * in the cache, but move it to the lowest priority in the list. Then, place the emergency call
     * on the next phone in the list.
     * - If a Connection returns with the disconnect cause EMERGENCY_PERM_FAILURE, remove that phone
     * from the cache and pull another phone from the cache to place the emergency call.
     *
     * This will continue until there are no more slots to dial on.
     */
    @VisibleForTesting
    public void retryOutgoingOriginalConnection(TelephonyConnection c, boolean isPermanentFailure) {
        int phoneId = (c.getPhone() == null) ? -1 : c.getPhone().getPhoneId();
        updateCachedConnectionPhonePair(c, isPermanentFailure);
        // Pull next phone to use from the cache or null if it is empty
        Phone newPhoneToUse = (mEmergencyRetryCache.second != null)
                ? mEmergencyRetryCache.second.peek() : null;
        if (newPhoneToUse != null) {
            int videoState = c.getVideoState();
            Bundle connExtras = c.getExtras();
            Log.i(this, "retryOutgoingOriginalConnection, redialing on Phone Id: " + newPhoneToUse);
            c.clearOriginalConnection();
            if (phoneId != newPhoneToUse.getPhoneId()) updatePhoneAccount(c, newPhoneToUse);
            placeOutgoingConnection(c, newPhoneToUse, videoState, connExtras);
        } else {
            // We have run out of Phones to use. Disconnect the call and destroy the connection.
            Log.i(this, "retryOutgoingOriginalConnection, no more Phones to use. Disconnecting.");
            closeOrDestroyConnection(c, new DisconnectCause(DisconnectCause.ERROR));
        }
    }

    private void updatePhoneAccount(TelephonyConnection connection, Phone phone) {
        PhoneAccountHandle pHandle = mPhoneUtilsProxy.makePstnPhoneAccountHandle(phone);
        // For ECall handling on MSIM, until the request reaches here (i.e PhoneApp), we don't know
        // on which phone account ECall can be placed. After deciding, we should notify Telecom of
        // the change so that the proper PhoneAccount can be displayed.
        Log.i(this, "updatePhoneAccount setPhoneAccountHandle, account = " + pHandle);
        connection.setPhoneAccountHandle(pHandle);
    }

    private void placeOutgoingConnection(
            TelephonyConnection connection, Phone phone, ConnectionRequest request) {
        placeOutgoingConnection(connection, phone, request.getVideoState(), request.getExtras());
    }

    private void placeOutgoingConnection(
            TelephonyConnection connection, Phone phone, int videoState, Bundle extras) {

        String number = (connection.getAddress() != null)
                ? connection.getAddress().getSchemeSpecificPart()
                : "";

        com.android.internal.telephony.Connection originalConnection = null;
        try {
            if (phone != null) {
                EmergencyNumber emergencyNumber =
                        phone.getEmergencyNumberTracker().getEmergencyNumber(number);
                if (emergencyNumber != null) {
                    phone.notifyOutgoingEmergencyCall(emergencyNumber);
                    if (!getAllConnections().isEmpty()) {
                        if (!shouldHoldForEmergencyCall(phone)) {
                            // If we do not support holding ongoing calls for an outgoing
                            // emergency call, disconnect the ongoing calls.
                            for (Connection c : getAllConnections()) {
                                if (!c.equals(connection)
                                        && c.getState() != Connection.STATE_DISCONNECTED
                                        && c instanceof TelephonyConnection) {
                                    ((TelephonyConnection) c).hangup(
                                            android.telephony.DisconnectCause
                                                    .OUTGOING_EMERGENCY_CALL_PLACED);
                                }
                            }
                            for (Conference c : getAllConferences()) {
                                if (c.getState() != Connection.STATE_DISCONNECTED
                                        && c instanceof Conference) {
                                    ((Conference) c).onDisconnect();
                                }
                            }
                        } else if (!isVideoCallHoldAllowed(phone)) {
                            // If we do not support holding ongoing video call for an outgoing
                            // emergency call, disconnect the ongoing video call.
                            for (Connection c : getAllConnections()) {
                                if (!c.equals(connection)
                                        && c.getState() == Connection.STATE_ACTIVE
                                        && VideoProfile.isVideo(c.getVideoState())
                                        && c instanceof TelephonyConnection) {
                                    ((TelephonyConnection) c).hangup(
                                            android.telephony.DisconnectCause
                                                    .OUTGOING_EMERGENCY_CALL_PLACED);
                                    break;
                                }
                            }
                        }
                    }
                }
                originalConnection = phone.dial(number, new ImsPhone.ImsDialArgs.Builder()
                        .setVideoState(videoState)
                        .setIntentExtras(extras)
                        .setRttTextStream(connection.getRttTextStream())
                        .build());
            }
        } catch (CallStateException e) {
            Log.e(this, e, "placeOutgoingConnection, phone.dial exception: " + e);
            handleCallStateException(e, connection, phone);
            return;
        }

        if (originalConnection == null) {
            int telephonyDisconnectCause = android.telephony.DisconnectCause.OUTGOING_FAILURE;
            // On GSM phones, null connection means that we dialed an MMI code
            if (phone.getPhoneType() == PhoneConstants.PHONE_TYPE_GSM) {
                Log.d(this, "dialed MMI code");
                int subId = phone.getSubId();
                Log.d(this, "subId: "+subId);
                telephonyDisconnectCause = android.telephony.DisconnectCause.DIALED_MMI;
                final Intent intent = new Intent(this, MMIDialogActivity.class);
                intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK |
                        Intent.FLAG_ACTIVITY_EXCLUDE_FROM_RECENTS);
                if (SubscriptionManager.isValidSubscriptionId(subId)) {
                    SubscriptionManager.putSubscriptionIdExtra(intent, subId);
                }
                startActivity(intent);
            }
            Log.d(this, "placeOutgoingConnection, phone.dial returned null");
            connection.setTelephonyConnectionDisconnected(
                    mDisconnectCauseFactory.toTelecomDisconnectCause(telephonyDisconnectCause,
                            "Connection is null", phone.getPhoneId()));
            connection.close();
        } else {
            connection.setOriginalConnection(originalConnection);
        }
    }

    private boolean isVideoCallHoldAllowed(Phone phone) {
         CarrierConfigManager cfgManager = (CarrierConfigManager)
                phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (cfgManager == null) {
            // For some reason CarrierConfigManager is unavailable, return default
            Log.w(this, "isVideoCallHoldAllowed: couldn't get CarrierConfigManager");
            return true;
        }
        return cfgManager.getConfigForSubId(phone.getSubId()).getBoolean(
                CarrierConfigManager.KEY_ALLOW_HOLD_VIDEO_CALL_BOOL, true);
    }

    private boolean shouldHoldForEmergencyCall(Phone phone) {
        CarrierConfigManager cfgManager = (CarrierConfigManager)
                phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (cfgManager == null) {
            // For some reason CarrierConfigManager is unavailable, return default
            Log.w(this, "shouldHoldForEmergencyCall: couldn't get CarrierConfigManager");
            return true;
        }
        return cfgManager.getConfigForSubId(phone.getSubId()).getBoolean(
                CarrierConfigManager.KEY_ALLOW_HOLD_CALL_DURING_EMERGENCY_BOOL, true);
    }

    private void handleCallStateException(CallStateException e, TelephonyConnection connection,
            Phone phone) {
        int cause = android.telephony.DisconnectCause.OUTGOING_FAILURE;
        switch (e.getError()) {
            case CallStateException.ERROR_OUT_OF_SERVICE:
                cause = android.telephony.DisconnectCause.OUT_OF_SERVICE;
                break;
            case CallStateException.ERROR_POWER_OFF:
                 cause = android.telephony.DisconnectCause.POWER_OFF;
                 break;
            case CallStateException.ERROR_ALREADY_DIALING:
                 cause = android.telephony.DisconnectCause.ALREADY_DIALING;
                 break;
            case CallStateException.ERROR_CALL_RINGING:
                 cause = android.telephony.DisconnectCause.CANT_CALL_WHILE_RINGING;
                 break;
            case CallStateException.ERROR_CALLING_DISABLED:
                 cause = android.telephony.DisconnectCause.CALLING_DISABLED;
                 break;
            case CallStateException.ERROR_TOO_MANY_CALLS:
                 cause = android.telephony.DisconnectCause.TOO_MANY_ONGOING_CALLS;
                 break;
            case CallStateException.ERROR_OTASP_PROVISIONING_IN_PROCESS:
                 cause = android.telephony.DisconnectCause.OTASP_PROVISIONING_IN_PROCESS;
                 break;
        }
        connection.setTelephonyConnectionDisconnected(
                DisconnectCauseUtil.toTelecomDisconnectCause(cause, e.getMessage(),
                        phone.getPhoneId()));
        connection.close();
    }

    private TelephonyConnection createConnectionFor(
            Phone phone,
            com.android.internal.telephony.Connection originalConnection,
            boolean isOutgoing,
            PhoneAccountHandle phoneAccountHandle,
            String telecomCallId) {
            return createConnectionFor(phone, originalConnection, isOutgoing, phoneAccountHandle,
                    telecomCallId, false);
    }

    private TelephonyConnection createConnectionFor(
            Phone phone,
            com.android.internal.telephony.Connection originalConnection,
            boolean isOutgoing,
            PhoneAccountHandle phoneAccountHandle,
            String telecomCallId,
            boolean isAdhocConference) {
        TelephonyConnection returnConnection = null;
        int phoneType = phone.getPhoneType();
        int callDirection = isOutgoing ? android.telecom.Call.Details.DIRECTION_OUTGOING
                : android.telecom.Call.Details.DIRECTION_INCOMING;
        if (phoneType == TelephonyManager.PHONE_TYPE_GSM) {
            returnConnection = new GsmConnection(originalConnection, telecomCallId, callDirection);
        } else if (phoneType == TelephonyManager.PHONE_TYPE_CDMA) {
            boolean allowsMute = allowsMute(phone);
            returnConnection = new CdmaConnection(originalConnection, mEmergencyTonePlayer,
                    allowsMute, callDirection, telecomCallId);
        }
        if (returnConnection != null) {
            if (!isAdhocConference) {
                // Listen to Telephony specific callbacks from the connection
                returnConnection.addTelephonyConnectionListener(mTelephonyConnectionListener);
            }
            returnConnection.setVideoPauseSupported(
                    TelecomAccountRegistry.getInstance(this).isVideoPauseSupported(
                            phoneAccountHandle));
            returnConnection.setManageImsConferenceCallSupported(
                    TelecomAccountRegistry.getInstance(this).isManageImsConferenceCallSupported(
                            phoneAccountHandle));
            returnConnection.setShowPreciseFailedCause(
                    TelecomAccountRegistry.getInstance(this).isShowPreciseFailedCause(
                            phoneAccountHandle));
            returnConnection.setTelephonyConnectionService(this);
        }
        return returnConnection;
    }

    private boolean isOriginalConnectionKnown(
            com.android.internal.telephony.Connection originalConnection) {
        return (getConnectionForOriginalConnection(originalConnection) != null);
    }

    private TelephonyConnection getConnectionForOriginalConnection(
            com.android.internal.telephony.Connection originalConnection) {
        for (Connection connection : getAllConnections()) {
            if (connection instanceof TelephonyConnection) {
                TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
                if (telephonyConnection.getOriginalConnection() == originalConnection) {
                    return telephonyConnection;
                }
            }
        }
        return null;
    }

    /**
     * Determines which {@link Phone} will be used to place the call.
     * @param accountHandle The {@link PhoneAccountHandle} which was sent from Telecom to place the
     *      call on.
     * @param isEmergency {@code true} if this is an emergency call, {@code false} otherwise.
     * @param emergencyNumberAddress When {@code isEmergency} is {@code true}, will be the phone
     *      of the emergency call.  Otherwise, this can be {@code null}  .
     * @return
     */
    private Phone getPhoneForAccount(PhoneAccountHandle accountHandle, boolean isEmergency,
                                     @Nullable String emergencyNumberAddress) {
        Phone chosenPhone = null;
        int subId = mPhoneUtilsProxy.getSubIdForPhoneAccountHandle(accountHandle);
        if (subId != SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
            int phoneId = mSubscriptionManagerProxy.getPhoneId(subId);
            chosenPhone = mPhoneFactoryProxy.getPhone(phoneId);
        }
        // If this is an emergency call and the phone we originally planned to make this call
        // with is not in service or was invalid, try to find one that is in service, using the
        // default as a last chance backup.
        if (isEmergency && (chosenPhone == null || !isAvailableForEmergencyCalls(chosenPhone))) {
            Log.d(this, "getPhoneForAccount: phone for phone acct handle %s is out of service "
                    + "or invalid for emergency call.", accountHandle);
            chosenPhone = getPhoneForEmergencyCall(emergencyNumberAddress);
            Log.d(this, "getPhoneForAccount: using subId: " +
                    (chosenPhone == null ? "null" : chosenPhone.getSubId()));
        }
        return chosenPhone;
    }

    /**
     * If needed, block until the the default data is is switched for outgoing emergency call, or
     * timeout expires.
     */
    private boolean delayDialForDdsSwitch(Phone phone) {
        if (phone == null) {
            return true;
        }
        try {
            return possiblyOverrideDefaultDataForEmergencyCall(phone).get(
                    DEFAULT_DATA_SWITCH_TIMEOUT_MS, TimeUnit.MILLISECONDS);
        } catch (Exception e) {
            Log.w(this, "delayDialForDdsSwitch - exception= "
                    + e.getMessage());
            return false;
        }
    }

    /**
     * If needed, block until Default Data subscription is switched for outgoing emergency call.
     *
     * In some cases, we need to try to switch the Default Data subscription before placing the
     * emergency call on DSDS devices. This includes the following situation:
     * - The modem does not support processing GNSS SUPL requests on the non-default data
     * subscription. For some carriers that do not provide a control plane fallback mechanism, the
     * SUPL request will be dropped and we will not be able to get the user's location for the
     * emergency call. In this case, we need to swap default data temporarily.
     * @param phone Evaluates whether or not the default data should be moved to the phone
     *              specified. Should not be null.
     */
    private CompletableFuture<Boolean> possiblyOverrideDefaultDataForEmergencyCall(
            @NonNull Phone phone) {
        int phoneCount = mTelephonyManagerProxy.getPhoneCount();
        // Do not override DDS if this is a single SIM device.
        if (phoneCount <= PhoneConstants.MAX_PHONE_COUNT_SINGLE_SIM) {
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }

        // Do not switch Default data if this device supports emergency SUPL on non-DDS.
        final boolean gnssSuplRequiresDefaultData =
                mDeviceState.isSuplDdsSwitchRequiredForEmergencyCall(this);
        if (!gnssSuplRequiresDefaultData) {
            Log.d(this, "possiblyOverrideDefaultDataForEmergencyCall: not switching DDS, does not "
                    + "require DDS switch.");
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }

        CarrierConfigManager cfgManager = (CarrierConfigManager)
                phone.getContext().getSystemService(Context.CARRIER_CONFIG_SERVICE);
        if (cfgManager == null) {
            // For some reason CarrierConfigManager is unavailable. Do not block emergency call.
            Log.w(this, "possiblyOverrideDefaultDataForEmergencyCall: couldn't get"
                    + "CarrierConfigManager");
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }

        // Only override default data if we are IN_SERVICE already.
        if (!isAvailableForEmergencyCalls(phone)) {
            Log.d(this, "possiblyOverrideDefaultDataForEmergencyCall: not switching DDS");
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }

        // Only override default data if we are not roaming, we do not want to switch onto a network
        // that only supports data plane only (if we do not know).
        boolean isRoaming = phone.getServiceState().getVoiceRoaming();
        // In some roaming conditions, we know the roaming network doesn't support control plane
        // fallback even though the home operator does. For these operators we will need to do a DDS
        // switch anyway to make sure the SUPL request doesn't fail.
        boolean roamingNetworkSupportsControlPlaneFallback = true;
        String[] dataPlaneRoamPlmns = cfgManager.getConfigForSubId(phone.getSubId()).getStringArray(
                CarrierConfigManager.Gps.KEY_ES_SUPL_DATA_PLANE_ONLY_ROAMING_PLMN_STRING_ARRAY);
        if (dataPlaneRoamPlmns != null && Arrays.asList(dataPlaneRoamPlmns).contains(
                phone.getServiceState().getOperatorNumeric())) {
            roamingNetworkSupportsControlPlaneFallback = false;
        }
        if (isRoaming && roamingNetworkSupportsControlPlaneFallback) {
            Log.d(this, "possiblyOverrideDefaultDataForEmergencyCall: roaming network is assumed "
                    + "to support CP fallback, not switching DDS.");
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }
        // Do not try to swap default data if we support CS fallback or it is assumed that the
        // roaming network supports control plane fallback, we do not want to introduce
        // a lag in emergency call setup time if possible.
        final boolean supportsCpFallback = cfgManager.getConfigForSubId(phone.getSubId())
                .getInt(CarrierConfigManager.Gps.KEY_ES_SUPL_CONTROL_PLANE_SUPPORT_INT,
                        CarrierConfigManager.Gps.SUPL_EMERGENCY_MODE_TYPE_CP_ONLY)
                != CarrierConfigManager.Gps.SUPL_EMERGENCY_MODE_TYPE_DP_ONLY;
        if (supportsCpFallback && roamingNetworkSupportsControlPlaneFallback) {
            Log.d(this, "possiblyOverrideDefaultDataForEmergencyCall: not switching DDS, carrier "
                    + "supports CP fallback.");
            return CompletableFuture.completedFuture(Boolean.TRUE);
        }

        // Get extension time, may be 0 for some carriers that support ECBM as well. Use
        // CarrierConfig default if format fails.
        int extensionTime = 0;
        try {
            extensionTime = Integer.parseInt(cfgManager.getConfigForSubId(phone.getSubId())
                    .getString(CarrierConfigManager.Gps.KEY_ES_EXTENSION_SEC_STRING, "0"));
        } catch (NumberFormatException e) {
            // Just use default.
        }
        CompletableFuture<Boolean> modemResultFuture = new CompletableFuture<>();
        try {
            Log.d(this, "possiblyOverrideDefaultDataForEmergencyCall: overriding DDS for "
                    + extensionTime + "seconds");
            mPhoneSwitcherProxy.getPhoneSwitcher().overrideDefaultDataForEmergency(
                    phone.getPhoneId(), extensionTime, modemResultFuture);
            // Catch all exceptions, we want to continue with emergency call if possible.
        } catch (Exception e) {
            Log.w(this, "possiblyOverrideDefaultDataForEmergencyCall: exception = "
                    + e.getMessage());
            modemResultFuture = CompletableFuture.completedFuture(Boolean.FALSE);
        }
        return modemResultFuture;
    }

    /**
     * Get the Phone to use for an emergency call of the given emergency number address:
     *  a) If there are multiple Phones with the Subscriptions that support the emergency number
     *     address, and one of them is the default voice Phone, consider the default voice phone
     *     if 1.4 HAL is supported, or if it is available for emergency call.
     *  b) If there are multiple Phones with the Subscriptions that support the emergency number
     *     address, and none of them is the default voice Phone, use one of these Phones if 1.4 HAL
     *     is supported, or if it is available for emergency call.
     *  c) If there is no Phone that supports the emergency call for the address, use the defined
     *     Priority list to select the Phone via {@link #getFirstPhoneForEmergencyCall}.
     */
    public Phone getPhoneForEmergencyCall(String emergencyNumberAddress) {
        // Find the list of available Phones for the given emergency number address
        List<Phone> potentialEmergencyPhones = new ArrayList<>();
        int defaultVoicePhoneId = mSubscriptionManagerProxy.getDefaultVoicePhoneId();
        for (Phone phone : mPhoneFactoryProxy.getPhones()) {
            if (phone.getEmergencyNumberTracker() != null) {
                if (phone.getEmergencyNumberTracker().isEmergencyNumber(
                        emergencyNumberAddress, true)) {
                    if (phone.getHalVersion().greaterOrEqual(RIL.RADIO_HAL_VERSION_1_4)
                            || isAvailableForEmergencyCalls(phone)) {
                        // a)
                        if (phone.getPhoneId() == defaultVoicePhoneId) {
                            Log.i(this, "getPhoneForEmergencyCall, Phone Id that supports"
                                    + " emergency number: " + phone.getPhoneId());
                            return phone;
                        }
                        potentialEmergencyPhones.add(phone);
                    }
                }
            }
        }
        // b)
        if (potentialEmergencyPhones.size() > 0) {
            Log.i(this, "getPhoneForEmergencyCall, Phone Id that supports emergency number:"
                    + potentialEmergencyPhones.get(0).getPhoneId());
            return getFirstPhoneForEmergencyCall(potentialEmergencyPhones);
        }
        // c)
        return getFirstPhoneForEmergencyCall();
    }

    @VisibleForTesting
    public Phone getFirstPhoneForEmergencyCall() {
        return getFirstPhoneForEmergencyCall(null);
    }

    /**
     * Retrieves the most sensible Phone to use for an emergency call using the following Priority
     *  list (for multi-SIM devices):
     *  1) The User's SIM preference for Voice calling
     *  2) The First Phone that is currently IN_SERVICE or is available for emergency calling
     *  3) Prioritize phones that have the dialed emergency number as part of their emergency
     *     number list
     *  4) If there is a PUK locked SIM, compare the SIMs that are not PUK locked. If all the SIMs
     *     are locked, skip to condition 5).
     *  5) The Phone with more Capabilities.
     *  6) The First Phone that has a SIM card in it (Starting from Slot 0...N)
     *  7) The Default Phone (Currently set as Slot 0)
     */
    @VisibleForTesting
    public Phone getFirstPhoneForEmergencyCall(List<Phone> phonesWithEmergencyNumber) {
        // 1)
        int phoneId = mSubscriptionManagerProxy.getDefaultVoicePhoneId();
        if (phoneId != SubscriptionManager.INVALID_PHONE_INDEX) {
            Phone defaultPhone = mPhoneFactoryProxy.getPhone(phoneId);
            if (defaultPhone != null && isAvailableForEmergencyCalls(defaultPhone)) {
                if (phonesWithEmergencyNumber == null
                        || phonesWithEmergencyNumber.contains(defaultPhone)) {
                    return defaultPhone;
                }
            }
        }

        Phone firstPhoneWithSim = null;
        int phoneCount = mTelephonyManagerProxy.getPhoneCount();
        List<SlotStatus> phoneSlotStatus = new ArrayList<>(phoneCount);
        for (int i = 0; i < phoneCount; i++) {
            Phone phone = mPhoneFactoryProxy.getPhone(i);
            if (phone == null) {
                continue;
            }
            // 2)
            if (isAvailableForEmergencyCalls(phone)) {
                if (phonesWithEmergencyNumber == null
                        || phonesWithEmergencyNumber.contains(phone)) {
                    // the slot has the radio on & state is in service.
                    Log.i(this,
                            "getFirstPhoneForEmergencyCall, radio on & in service, Phone Id:" + i);
                    return phone;
                }
            }
            // 5)
            // Store the RAF Capabilities for sorting later.
            int radioAccessFamily = phone.getRadioAccessFamily();
            SlotStatus status = new SlotStatus(i, radioAccessFamily);
            phoneSlotStatus.add(status);
            Log.i(this, "getFirstPhoneForEmergencyCall, RAF:" +
                    Integer.toHexString(radioAccessFamily) + " saved for Phone Id:" + i);
            // 4)
            // Report Slot's PIN/PUK lock status for sorting later.
            int simState = mSubscriptionManagerProxy.getSimStateForSlotIdx(i);
            // Record SimState.
            status.simState = simState;
            if (simState == TelephonyManager.SIM_STATE_PIN_REQUIRED ||
                    simState == TelephonyManager.SIM_STATE_PUK_REQUIRED) {
                status.isLocked = true;
            }
            // 3) Store if the Phone has the corresponding emergency number
            if (phonesWithEmergencyNumber != null) {
                for (Phone phoneWithEmergencyNumber : phonesWithEmergencyNumber) {
                    if (phoneWithEmergencyNumber != null
                            && phoneWithEmergencyNumber.getPhoneId() == i) {
                        status.hasDialedEmergencyNumber = true;
                    }
                }
            }
            // 6)
            if (firstPhoneWithSim == null && mTelephonyManagerProxy.hasIccCard(i)) {
                // The slot has a SIM card inserted, but is not in service, so keep track of this
                // Phone. Do not return because we want to make sure that none of the other Phones
                // are in service (because that is always faster).
                firstPhoneWithSim = phone;
                Log.i(this, "getFirstPhoneForEmergencyCall, SIM card inserted, Phone Id:" +
                        firstPhoneWithSim.getPhoneId());
            }
        }
        // 7)
        if (firstPhoneWithSim == null && phoneSlotStatus.isEmpty()) {
            if (phonesWithEmergencyNumber == null || phonesWithEmergencyNumber.isEmpty()) {
                // No Phones available, get the default
                Log.i(this, "getFirstPhoneForEmergencyCall, return default phone");
                return  mPhoneFactoryProxy.getDefaultPhone();
            }
            return phonesWithEmergencyNumber.get(0);
        } else {
            // 5)
            final int defaultPhoneId = mPhoneFactoryProxy.getDefaultPhone().getPhoneId();
            final Phone firstOccupiedSlot = firstPhoneWithSim;
            if (!phoneSlotStatus.isEmpty()) {
                // Only sort if there are enough elements to do so.
                if (phoneSlotStatus.size() > 1) {
                    Collections.sort(phoneSlotStatus, (o1, o2) -> {
                        if (!o1.hasDialedEmergencyNumber && o2.hasDialedEmergencyNumber) {
                            return -1;
                        }
                        if (o1.hasDialedEmergencyNumber && !o2.hasDialedEmergencyNumber) {
                            return 1;
                        }
                        // Sort by non-absent SIM.
                        if (o1.simState == TelephonyManager.SIM_STATE_ABSENT
                                && o2.simState != TelephonyManager.SIM_STATE_ABSENT) {
                            return -1;
                        }
                        if (o2.simState == TelephonyManager.SIM_STATE_ABSENT
                                && o1.simState != TelephonyManager.SIM_STATE_ABSENT) {
                            return 1;
                        }
                        // First start by seeing if either of the phone slots are locked. If they
                        // are, then sort by non-locked SIM first. If they are both locked, sort
                        // by capability instead.
                        if (o1.isLocked && !o2.isLocked) {
                            return -1;
                        }
                        if (o2.isLocked && !o1.isLocked) {
                            return 1;
                        }
                        // sort by number of RadioAccessFamily Capabilities.
                        int compare = RadioAccessFamily.compare(o1.capabilities, o2.capabilities);
                        if (compare == 0) {
                            if (firstOccupiedSlot != null) {
                                // If the RAF capability is the same, choose based on whether or
                                // not any of the slots are occupied with a SIM card (if both
                                // are, always choose the first).
                                if (o1.slotId == firstOccupiedSlot.getPhoneId()) {
                                    return 1;
                                } else if (o2.slotId == firstOccupiedSlot.getPhoneId()) {
                                    return -1;
                                }
                            } else {
                                // No slots have SIMs detected in them, so weight the default
                                // Phone Id greater than the others.
                                if (o1.slotId == defaultPhoneId) {
                                    return 1;
                                } else if (o2.slotId == defaultPhoneId) {
                                    return -1;
                                }
                            }
                        }
                        return compare;
                    });
                }
                int mostCapablePhoneId = phoneSlotStatus.get(phoneSlotStatus.size() - 1).slotId;
                Log.i(this, "getFirstPhoneForEmergencyCall, Using Phone Id: " + mostCapablePhoneId +
                        "with highest capability");
                return mPhoneFactoryProxy.getPhone(mostCapablePhoneId);
            } else {
                // 6)
                return firstPhoneWithSim;
            }
        }
    }

    /**
     * Returns true if the state of the Phone is IN_SERVICE or available for emergency calling only.
     */
    private boolean isAvailableForEmergencyCalls(Phone phone) {
        return ServiceState.STATE_IN_SERVICE == phone.getServiceState().getState() ||
                phone.getServiceState().isEmergencyOnly();
    }

    /**
     * Determines if the connection should allow mute.
     *
     * @param phone The current phone.
     * @return {@code True} if the connection should allow mute.
     */
    private boolean allowsMute(Phone phone) {
        // For CDMA phones, check if we are in Emergency Callback Mode (ECM).  Mute is disallowed
        // in ECM mode.
        if (phone.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
            if (phone.isInEcm()) {
                return false;
            }
        }

        return true;
    }

    TelephonyConnection.TelephonyConnectionListener getTelephonyConnectionListener() {
        return mTelephonyConnectionListener;
    }

    /**
     * When a {@link TelephonyConnection} has its underlying original connection configured,
     * we need to add it to the correct conference controller.
     *
     * @param connection The connection to be added to the controller
     */
    public void addConnectionToConferenceController(TelephonyConnection connection) {
        // TODO: Need to revisit what happens when the original connection for the
        // TelephonyConnection changes.  If going from CDMA --> GSM (for example), the
        // instance of TelephonyConnection will still be a CdmaConnection, not a GsmConnection.
        // The CDMA conference controller makes the assumption that it will only have CDMA
        // connections in it, while the other conference controllers aren't as restrictive.  Really,
        // when we go between CDMA and GSM we should replace the TelephonyConnection.
        if (connection.isImsConnection()) {
            Log.d(this, "Adding IMS connection to conference controller: " + connection);
            mImsConferenceController.add(connection);
            mTelephonyConferenceController.remove(connection);
            if (connection instanceof CdmaConnection) {
                mCdmaConferenceController.remove((CdmaConnection) connection);
            }
        } else {
            int phoneType = connection.getCall().getPhone().getPhoneType();
            if (phoneType == TelephonyManager.PHONE_TYPE_GSM) {
                Log.d(this, "Adding GSM connection to conference controller: " + connection);
                mTelephonyConferenceController.add(connection);
                if (connection instanceof CdmaConnection) {
                    mCdmaConferenceController.remove((CdmaConnection) connection);
                }
            } else if (phoneType == TelephonyManager.PHONE_TYPE_CDMA &&
                    connection instanceof CdmaConnection) {
                Log.d(this, "Adding CDMA connection to conference controller: " + connection);
                mCdmaConferenceController.add((CdmaConnection) connection);
                mTelephonyConferenceController.remove(connection);
            }
            Log.d(this, "Removing connection from IMS conference controller: " + connection);
            mImsConferenceController.remove(connection);
        }
    }

    /**
     * Create a new CDMA connection. CDMA connections have additional limitations when creating
     * additional calls which are handled in this method.  Specifically, CDMA has a "FLASH" command
     * that can be used for three purposes: merging a call, swapping unmerged calls, and adding
     * a new outgoing call. The function of the flash command depends on the context of the current
     * set of calls. This method will prevent an outgoing call from being made if it is not within
     * the right circumstances to support adding a call.
     */
    private Connection checkAdditionalOutgoingCallLimits(Phone phone) {
        if (phone.getPhoneType() == TelephonyManager.PHONE_TYPE_CDMA) {
            // Check to see if any CDMA conference calls exist, and if they do, check them for
            // limitations.
            for (Conference conference : getAllConferences()) {
                if (conference instanceof CdmaConference) {
                    CdmaConference cdmaConf = (CdmaConference) conference;

                    // If the CDMA conference has not been merged, add-call will not work, so fail
                    // this request to add a call.
                    if ((cdmaConf.getConnectionCapabilities()
                            & Connection.CAPABILITY_MERGE_CONFERENCE) != 0) {
                        return Connection.createFailedConnection(new DisconnectCause(
                                    DisconnectCause.RESTRICTED,
                                    null,
                                    getResources().getString(R.string.callFailed_cdma_call_limit),
                                    "merge-capable call exists, prevent flash command."));
                    }
                }
            }
        }

        return null; // null means nothing went wrong, and call should continue.
    }

    /**
     * For outgoing dialed calls, potentially send a ConnectionEvent if the user is on WFC and is
     * dialing an international number.
     * @param telephonyConnection The connection.
     */
    private void maybeSendInternationalCallEvent(TelephonyConnection telephonyConnection) {
        if (telephonyConnection == null || telephonyConnection.getPhone() == null ||
                telephonyConnection.getPhone().getDefaultPhone() == null) {
            return;
        }
        Phone phone = telephonyConnection.getPhone().getDefaultPhone();
        if (phone instanceof GsmCdmaPhone) {
            GsmCdmaPhone gsmCdmaPhone = (GsmCdmaPhone) phone;
            if (telephonyConnection.isOutgoingCall() &&
                    gsmCdmaPhone.isNotificationOfWfcCallRequired(
                            telephonyConnection.getOriginalConnection().getOrigDialString())) {
                // Send connection event to InCall UI to inform the user of the fact they
                // are potentially placing an international call on WFC.
                Log.i(this, "placeOutgoingConnection - sending international call on WFC " +
                        "confirmation event");
                telephonyConnection.sendTelephonyConnectionEvent(
                        TelephonyManager.EVENT_NOTIFY_INTERNATIONAL_CALL_ON_WFC, null);
            }
        }
    }

    private void handleTtyModeChange(boolean isTtyEnabled) {
        Log.i(this, "handleTtyModeChange; isTtyEnabled=%b", isTtyEnabled);
        mIsTtyEnabled = isTtyEnabled;
        for (Connection connection : getAllConnections()) {
            if (connection instanceof TelephonyConnection) {
                TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
                telephonyConnection.setTtyEnabled(isTtyEnabled);
            }
        }
    }

    private void closeOrDestroyConnection(Connection connection, DisconnectCause cause) {
        if (connection instanceof TelephonyConnection) {
            TelephonyConnection telephonyConnection = (TelephonyConnection) connection;
            telephonyConnection.setTelephonyConnectionDisconnected(cause);
            // Close destroys the connection and notifies TelephonyConnection listeners.
            telephonyConnection.close();
        } else {
            connection.setDisconnected(cause);
            connection.destroy();
        }
    }

    /**
     * Adds a {@link Conference} to the telephony ConnectionService and registers a listener for
     * changes to the conference.  Should be used instead of {@link #addConference(Conference)}.
     * @param conference The conference.
     */
    public void addTelephonyConference(@NonNull TelephonyConferenceBase conference) {
        addConference(conference);
        conference.addTelephonyConferenceListener(mTelephonyConferenceListener);
    }
}
