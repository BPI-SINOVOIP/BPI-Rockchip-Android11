/*
 * Copyright 2014, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.server.telecom;

import static android.telephony.CarrierConfigManager.KEY_SUPPORT_IMS_CONFERENCE_EVENT_PACKAGE_BOOL;

import android.annotation.Nullable;
import android.content.Context;
import android.content.Intent;
import android.location.Country;
import android.location.CountryDetector;
import android.net.Uri;
import android.os.AsyncTask;
import android.os.Looper;
import android.os.UserHandle;
import android.os.PersistableBundle;
import android.provider.CallLog.Calls;
import android.telecom.Connection;
import android.telecom.DisconnectCause;
import android.telecom.Log;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.VideoProfile;
import android.telephony.CarrierConfigManager;
import android.telephony.PhoneNumberUtils;
import android.telephony.SubscriptionManager;

// TODO: Needed for move to system service: import com.android.internal.R;
import com.android.internal.annotations.VisibleForTesting;
import android.telecom.CallerInfo;
import com.android.server.telecom.callfiltering.CallFilteringResult;

import java.util.Arrays;
import java.util.Locale;
import java.util.Objects;
import java.util.stream.Stream;

/**
 * Helper class that provides functionality to write information about calls and their associated
 * caller details to the call log. All logging activity will be performed asynchronously in a
 * background thread to avoid blocking on the main thread.
 */
@VisibleForTesting
public final class CallLogManager extends CallsManagerListenerBase {

    public interface LogCallCompletedListener {
        void onLogCompleted(@Nullable Uri uri);
    }

    /**
     * Parameter object to hold the arguments to add a call in the call log DB.
     */
    private static class AddCallArgs {
        /**
         * @param callerInfo Caller details.
         * @param number The phone number to be logged.
         * @param presentation Number presentation of the phone number to be logged.
         * @param callType The type of call (e.g INCOMING_TYPE). @see
         *     {@link android.provider.CallLog} for the list of values.
         * @param features The features of the call (e.g. FEATURES_VIDEO). @see
         *     {@link android.provider.CallLog} for the list of values.
         * @param creationDate Time when the call was created (milliseconds since epoch).
         * @param durationInMillis Duration of the call (milliseconds).
         * @param dataUsage Data usage in bytes, or null if not applicable.
         * @param isRead Indicates if the entry has been read or not.
         * @param logCallCompletedListener optional callback called after the call is logged.
         */
        public AddCallArgs(Context context, CallerInfo callerInfo, String number,
                String postDialDigits, String viaNumber, int presentation, int callType,
                int features, PhoneAccountHandle accountHandle, long creationDate,
                long durationInMillis, Long dataUsage, UserHandle initiatingUser, boolean isRead,
                @Nullable LogCallCompletedListener logCallCompletedListener, int callBlockReason,
                CharSequence callScreeningAppName, String callScreeningComponentName) {
            this.context = context;
            this.callerInfo = callerInfo;
            this.number = number;
            this.postDialDigits = postDialDigits;
            this.viaNumber = viaNumber;
            this.presentation = presentation;
            this.callType = callType;
            this.features = features;
            this.accountHandle = accountHandle;
            this.timestamp = creationDate;
            this.durationInSec = (int)(durationInMillis / 1000);
            this.dataUsage = dataUsage;
            this.initiatingUser = initiatingUser;
            this.isRead = isRead;
            this.logCallCompletedListener = logCallCompletedListener;
            this.callBockReason = callBlockReason;
            this.callScreeningAppName = callScreeningAppName;
            this.callScreeningComponentName = callScreeningComponentName;
        }
        // Since the members are accessed directly, we don't use the
        // mXxxx notation.
        public final Context context;
        public final CallerInfo callerInfo;
        public final String number;
        public final String postDialDigits;
        public final String viaNumber;
        public final int presentation;
        public final int callType;
        public final int features;
        public final PhoneAccountHandle accountHandle;
        public final long timestamp;
        public final int durationInSec;
        public final Long dataUsage;
        public final UserHandle initiatingUser;
        public final boolean isRead;

        @Nullable
        public final LogCallCompletedListener logCallCompletedListener;

        public final int callBockReason;
        public final CharSequence callScreeningAppName;
        public final String callScreeningComponentName;
    }

    private static final String TAG = CallLogManager.class.getSimpleName();

    // Copied from android.telephony.DisconnectCause.toString
    // TODO: come up with a better way to indicate in a android.telecom.DisconnectCause that
    // a conference was merged successfully
    private static final String REASON_IMS_MERGED_SUCCESSFULLY = "IMS_MERGED_SUCCESSFULLY";

    private final Context mContext;
    private final CarrierConfigManager mCarrierConfigManager;
    private final PhoneAccountRegistrar mPhoneAccountRegistrar;
    private final MissedCallNotifier mMissedCallNotifier;
    private static final String ACTION_CALLS_TABLE_ADD_ENTRY =
                "com.android.server.telecom.intent.action.CALLS_ADD_ENTRY";
    private static final String PERMISSION_PROCESS_CALLLOG_INFO =
                "android.permission.PROCESS_CALLLOG_INFO";
    private static final String CALL_TYPE = "callType";
    private static final String CALL_DURATION = "duration";

    private Object mLock;
    private String mCurrentCountryIso;

    public CallLogManager(Context context, PhoneAccountRegistrar phoneAccountRegistrar,
            MissedCallNotifier missedCallNotifier) {
        mContext = context;
        mCarrierConfigManager = (CarrierConfigManager) mContext
                .getSystemService(Context.CARRIER_CONFIG_SERVICE);
        mPhoneAccountRegistrar = phoneAccountRegistrar;
        mMissedCallNotifier = missedCallNotifier;
        mLock = new Object();
    }

    @Override
    public void onCallStateChanged(Call call, int oldState, int newState) {
        int disconnectCause = call.getDisconnectCause().getCode();
        boolean isNewlyDisconnected =
                newState == CallState.DISCONNECTED || newState == CallState.ABORTED;
        boolean isCallCanceled = isNewlyDisconnected && disconnectCause == DisconnectCause.CANCELED;

        if (!isNewlyDisconnected) {
            return;
        }

        if (shouldLogDisconnectedCall(call, oldState, isCallCanceled)) {
            int type;
            if (!call.isIncoming()) {
                type = Calls.OUTGOING_TYPE;
            } else if (disconnectCause == DisconnectCause.MISSED) {
                type = Calls.MISSED_TYPE;
            } else if (disconnectCause == DisconnectCause.ANSWERED_ELSEWHERE) {
                type = Calls.ANSWERED_EXTERNALLY_TYPE;
            } else if (disconnectCause == DisconnectCause.REJECTED) {
                type = Calls.REJECTED_TYPE;
            } else {
                type = Calls.INCOMING_TYPE;
            }
            // Always show the notification for managed calls. For self-managed calls, it is up to
            // the app to show the notification, so suppress the notification when logging the call.
            boolean showNotification = !call.isSelfManaged();
            logCall(call, type, showNotification, null /*result*/);
        }
    }

    /**
     * Log newly disconnected calls only if all of below conditions are met:
     * Call was NOT in the "choose account" phase when disconnected
     * Call is NOT a conference call which had children (unless it was remotely hosted).
     * Call is NOT a child call from a conference which was remotely hosted.
     * Call is NOT simulating a single party conference.
     * Call was NOT explicitly canceled, except for disconnecting from a conference.
     * Call is NOT an external call
     * Call is NOT disconnected because of merging into a conference.
     * Call is NOT a self-managed call OR call is a self-managed call which has indicated it
     * should be logged in its PhoneAccount
     */
    @VisibleForTesting
    public boolean shouldLogDisconnectedCall(Call call, int oldState, boolean isCallCanceled) {
        boolean shouldCallSelfManagedLogged = call.isLoggedSelfManaged()
                && (call.getHandoverState() == HandoverState.HANDOVER_NONE
                || call.getHandoverState() == HandoverState.HANDOVER_COMPLETE);

        // "Choose account" phase when disconnected
        if (oldState == CallState.SELECT_PHONE_ACCOUNT) {
            return false;
        }
        // A conference call which had children should not be logged, unless it was remotely hosted.
        if (call.isConference() && call.hadChildren() &&
                !call.hasProperty(Connection.PROPERTY_REMOTELY_HOSTED)) {
            return false;
        }

        // A conference call which had no children should not be logged; this case will occur on IMS
        // when no conference event package data is received.  We will have logged the participants
        // as they merge into the conference, so we should not log the conference itself.
        if (call.isConference() && !call.hadChildren() &&
                !call.hasProperty(Connection.PROPERTY_REMOTELY_HOSTED)) {
            return false;
        }

        // A child call of a conference which was remotely hosted; these didn't originate on this
        // device and should not be logged.
        if (call.getParentCall() != null && call.hasProperty(Connection.PROPERTY_REMOTELY_HOSTED)) {
            return false;
        }

        DisconnectCause cause = call.getDisconnectCause();
        if (isCallCanceled) {
            // No log when disconnecting to simulate a single party conference.
            if (cause != null
                    && DisconnectCause.REASON_EMULATING_SINGLE_CALL.equals(cause.getReason())) {
                return false;
            }
            // Explicitly canceled
            // Conference children connections only have CAPABILITY_DISCONNECT_FROM_CONFERENCE.
            // Log them when they are disconnected from conference.
            return (call.getConnectionCapabilities()
                    & Connection.CAPABILITY_DISCONNECT_FROM_CONFERENCE)
                    == Connection.CAPABILITY_DISCONNECT_FROM_CONFERENCE;
        }
        // An external call
        if (call.isExternalCall()) {
            return false;
        }

        // Call merged into conferences and marked with IMS_MERGED_SUCCESSFULLY.
        // Return false if the conference supports the participants packets for the carrier.
        // Otherwise, fall through. Merged calls would be associated with disconnected
        // connections because of special carrier requirements. Those calls don't look like
        // merged, e.g. could be one active and the other on hold.
        if (cause != null && REASON_IMS_MERGED_SUCCESSFULLY.equals(cause.getReason())) {
            int subscriptionId = mPhoneAccountRegistrar
                    .getSubscriptionIdForPhoneAccount(call.getTargetPhoneAccount());
            // By default, the conference should return a list of participants.
            if (subscriptionId == SubscriptionManager.INVALID_SUBSCRIPTION_ID) {
                return false;
            }

            PersistableBundle b = mCarrierConfigManager.getConfigForSubId(subscriptionId);
            if (b == null) {
                return false;
            }

            if (b.getBoolean(KEY_SUPPORT_IMS_CONFERENCE_EVENT_PACKAGE_BOOL, true)) {
                return false;
            }
        }

        // Call is NOT a self-managed call OR call is a self-managed call which has indicated it
        // should be logged in its PhoneAccount
        return !call.isSelfManaged() || shouldCallSelfManagedLogged;
    }

    void logCall(Call call, int type, boolean showNotificationForMissedCall, CallFilteringResult
            result) {
        if ((type == Calls.MISSED_TYPE || type == Calls.BLOCKED_TYPE) &&
                showNotificationForMissedCall) {
            logCall(call, type, new LogCallCompletedListener() {
                @Override
                public void onLogCompleted(@Nullable Uri uri) {
                    mMissedCallNotifier.showMissedCallNotification(
                            new MissedCallNotifier.CallInfo(call));
                }
            }, result);
        } else {
            logCall(call, type, null, result);
        }
    }

    /**
     * Logs a call to the call log based on the {@link Call} object passed in.
     *
     * @param call The call object being logged
     * @param callLogType The type of call log entry to log this call as. See:
     *     {@link android.provider.CallLog.Calls#INCOMING_TYPE}
     *     {@link android.provider.CallLog.Calls#OUTGOING_TYPE}
     *     {@link android.provider.CallLog.Calls#MISSED_TYPE}
     *     {@link android.provider.CallLog.Calls#BLOCKED_TYPE}
     * @param logCallCompletedListener optional callback called after the call is logged.
     * @param result is generated when call type is
     *     {@link android.provider.CallLog.Calls#BLOCKED_TYPE}.
     */
    void logCall(Call call, int callLogType,
        @Nullable LogCallCompletedListener logCallCompletedListener, CallFilteringResult result) {
        long creationTime;

        if (call.getConnectTimeMillis() != 0
                && call.getConnectTimeMillis() < call.getCreationTimeMillis()) {
            // If connected time is available, use connected time. The connected time might be
            // earlier than created time since it might come from carrier sent special SMS to
            // notifier user earlier missed call.
            creationTime = call.getConnectTimeMillis();
        } else {
            creationTime = call.getCreationTimeMillis();
        }

        final long age = call.getAgeMillis();

        final String logNumber = getLogNumber(call);

        Log.d(TAG, "logNumber set to: %s", Log.pii(logNumber));

        final PhoneAccountHandle emergencyAccountHandle =
                TelephonyUtil.getDefaultEmergencyPhoneAccount().getAccountHandle();

        String formattedViaNumber = PhoneNumberUtils.formatNumber(call.getViaNumber(),
                getCountryIso());
        formattedViaNumber = (formattedViaNumber != null) ?
                formattedViaNumber : call.getViaNumber();

        PhoneAccountHandle accountHandle = call.getTargetPhoneAccount();
        if (emergencyAccountHandle.equals(accountHandle)) {
            accountHandle = null;
        }

        Long callDataUsage = call.getCallDataUsage() == Call.DATA_USAGE_NOT_SET ? null :
                call.getCallDataUsage();

        int callFeatures = getCallFeatures(call.getVideoStateHistory(),
                call.getDisconnectCause().getCode() == DisconnectCause.CALL_PULLED,
                call.wasHighDefAudio(), call.wasWifi(),
                (call.getConnectionProperties() & Connection.PROPERTY_ASSISTED_DIALING) ==
                        Connection.PROPERTY_ASSISTED_DIALING,
                call.wasEverRttCall(),
                call.wasVolte());

        if (callLogType == Calls.BLOCKED_TYPE) {
            logCall(call.getCallerInfo(), logNumber, call.getPostDialDigits(), formattedViaNumber,
                    call.getHandlePresentation(), callLogType, callFeatures, accountHandle,
                    creationTime, age, callDataUsage, call.isEmergencyCall(),
                    call.getInitiatingUser(), call.isSelfManaged(), logCallCompletedListener,
                    result.mCallBlockReason, result.mCallScreeningAppName,
                    result.mCallScreeningComponentName);
        } else {
            logCall(call.getCallerInfo(), logNumber, call.getPostDialDigits(), formattedViaNumber,
                    call.getHandlePresentation(), callLogType, callFeatures, accountHandle,
                    creationTime, age, callDataUsage, call.isEmergencyCall(),
                    call.getInitiatingUser(), call.isSelfManaged(), logCallCompletedListener,
                    Calls.BLOCK_REASON_NOT_BLOCKED, null /*callScreeningAppName*/,
                    null /*callScreeningComponentName*/);
        }
    }

    /**
     * Inserts a call into the call log, based on the parameters passed in.
     *
     * @param callerInfo Caller details.
     * @param number The number the call was made to or from.
     * @param postDialDigits The post-dial digits that were dialed after the number,
     *                       if it was an outgoing call. Otherwise ''.
     * @param presentation
     * @param callType The type of call.
     * @param features The features of the call.
     * @param start The start time of the call, in milliseconds.
     * @param duration The duration of the call, in milliseconds.
     * @param dataUsage The data usage for the call, null if not applicable.
     * @param isEmergency {@code true} if this is an emergency call, {@code false} otherwise.
     * @param logCallCompletedListener optional callback called after the call is logged.
     * @param initiatingUser The user the call was initiated under.
     * @param isSelfManaged {@code true} if this is a self-managed call, {@code false} otherwise.
     * @param callBlockReason The reason why the call is blocked.
     * @param callScreeningAppName The call screening application name which block the call.
     * @param callScreeningComponentName The call screening component name which block the call.
     */
    private void logCall(
            CallerInfo callerInfo,
            String number,
            String postDialDigits,
            String viaNumber,
            int presentation,
            int callType,
            int features,
            PhoneAccountHandle accountHandle,
            long start,
            long duration,
            Long dataUsage,
            boolean isEmergency,
            UserHandle initiatingUser,
            boolean isSelfManaged,
            @Nullable LogCallCompletedListener logCallCompletedListener,
            int callBlockReason,
            CharSequence callScreeningAppName,
            String callScreeningComponentName) {

        // On some devices, to avoid accidental redialing of emergency numbers, we *never* log
        // emergency calls to the Call Log.  (This behavior is set on a per-product basis, based
        // on carrier requirements.)
        boolean okToLogEmergencyNumber = false;
        CarrierConfigManager configManager = (CarrierConfigManager) mContext.getSystemService(
                Context.CARRIER_CONFIG_SERVICE);
        PersistableBundle configBundle = configManager.getConfigForSubId(
                mPhoneAccountRegistrar.getSubscriptionIdForPhoneAccount(accountHandle));
        if (configBundle != null) {
            okToLogEmergencyNumber = configBundle.getBoolean(
                    CarrierConfigManager.KEY_ALLOW_EMERGENCY_NUMBERS_IN_CALL_LOG_BOOL);
        }

        // Don't log emergency numbers if the device doesn't allow it.
        final boolean isOkToLogThisCall = (!isEmergency || okToLogEmergencyNumber)
                && !isUnloggableNumber(number, configBundle);

        sendAddCallBroadcast(callType, duration);

        if (isOkToLogThisCall) {
            Log.d(TAG, "Logging Call log entry: " + callerInfo + ", "
                    + Log.pii(number) + "," + presentation + ", " + callType
                    + ", " + start + ", " + duration);
            boolean isRead = false;
            if (isSelfManaged) {
                // Mark self-managed calls are read since they're being handled by their own app.
                // Their inclusion in the call log is informational only.
                isRead = true;
            }
            AddCallArgs args = new AddCallArgs(mContext, callerInfo, number, postDialDigits,
                    viaNumber, presentation, callType, features, accountHandle, start, duration,
                    dataUsage, initiatingUser, isRead, logCallCompletedListener, callBlockReason,
                    callScreeningAppName, callScreeningComponentName);
            logCallAsync(args);
        } else {
          Log.d(TAG, "Not adding emergency call to call log.");
        }
    }

    private boolean isUnloggableNumber(String callNumber, PersistableBundle carrierConfig) {
        String normalizedNumber = PhoneNumberUtils.normalizeNumber(callNumber);
        String[] unloggableNumbersFromCarrierConfig = carrierConfig == null ? null
                : carrierConfig.getStringArray(
                        CarrierConfigManager.KEY_UNLOGGABLE_NUMBERS_STRING_ARRAY);
        String[] unloggableNumbersFromMccConfig = mContext.getResources()
                .getStringArray(com.android.internal.R.array.unloggable_phone_numbers);
        return Stream.concat(
                unloggableNumbersFromCarrierConfig == null ?
                        Stream.empty() : Arrays.stream(unloggableNumbersFromCarrierConfig),
                unloggableNumbersFromMccConfig == null ?
                        Stream.empty() : Arrays.stream(unloggableNumbersFromMccConfig)
        ).anyMatch(unloggableNumber -> Objects.equals(unloggableNumber, normalizedNumber));
    }

    /**
     * Based on the video state of the call, determines the call features applicable for the call.
     *
     * @param videoState The video state.
     * @param isPulledCall {@code true} if this call was pulled to another device.
     * @param isStoreHd {@code true} if this call was used HD.
     * @param isWifi {@code true} if this call was used wifi.
     * @param isUsingAssistedDialing {@code true} if this call used assisted dialing.
     * @return The call features.
     */
    private static int getCallFeatures(int videoState, boolean isPulledCall, boolean isStoreHd,
            boolean isWifi, boolean isUsingAssistedDialing, boolean isRtt, boolean isVolte) {
        int features = 0;
        if (VideoProfile.isVideo(videoState)) {
            features |= Calls.FEATURES_VIDEO;
        }
        if (isPulledCall) {
            features |= Calls.FEATURES_PULLED_EXTERNALLY;
        }
        if (isStoreHd) {
            features |= Calls.FEATURES_HD_CALL;
        }
        if (isWifi) {
            features |= Calls.FEATURES_WIFI;
        }
        if (isUsingAssistedDialing) {
            features |= Calls.FEATURES_ASSISTED_DIALING_USED;
        }
        if (isRtt) {
            features |= Calls.FEATURES_RTT;
        }
        if (isVolte) {
            features |= Calls.FEATURES_VOLTE;
        }
        return features;
    }

    /**
     * Retrieve the phone number from the call, and then process it before returning the
     * actual number that is to be logged.
     *
     * @param call The phone connection.
     * @return the phone number to be logged.
     */
    private String getLogNumber(Call call) {
        Uri handle = call.getOriginalHandle();

        if (handle == null) {
            return null;
        }

        String handleString = handle.getSchemeSpecificPart();
        if (!PhoneNumberUtils.isUriNumber(handleString)) {
            handleString = PhoneNumberUtils.stripSeparators(handleString);
        }
        return handleString;
    }

    /**
     * Adds the call defined by the parameters in the provided AddCallArgs to the CallLogProvider
     * using an AsyncTask to avoid blocking the main thread.
     *
     * @param args Prepopulated call details.
     * @return A handle to the AsyncTask that will add the call to the call log asynchronously.
     */
    public AsyncTask<AddCallArgs, Void, Uri[]> logCallAsync(AddCallArgs args) {
        return new LogCallAsyncTask().execute(args);
    }

    /**
     * Helper AsyncTask to access the call logs database asynchronously since database operations
     * can take a long time depending on the system's load. Since it extends AsyncTask, it uses
     * its own thread pool.
     */
    private class LogCallAsyncTask extends AsyncTask<AddCallArgs, Void, Uri[]> {

        private LogCallCompletedListener[] mListeners;

        @Override
        protected Uri[] doInBackground(AddCallArgs... callList) {
            int count = callList.length;
            Uri[] result = new Uri[count];
            mListeners = new LogCallCompletedListener[count];
            for (int i = 0; i < count; i++) {
                AddCallArgs c = callList[i];
                mListeners[i] = c.logCallCompletedListener;
                try {
                    // May block.
                    result[i] = addCall(c);
                } catch (Exception e) {
                    // This is very rare but may happen in legitimate cases.
                    // E.g. If the phone is encrypted and thus write request fails, it may cause
                    // some kind of Exception (right now it is IllegalArgumentException, but this
                    // might change).
                    //
                    // We don't want to crash the whole process just because of that, so just log
                    // it instead.
                    Log.e(TAG, e, "Exception raised during adding CallLog entry.");
                    result[i] = null;
                }
            }
            return result;
        }

        private Uri addCall(AddCallArgs c) {
            PhoneAccount phoneAccount = mPhoneAccountRegistrar
                    .getPhoneAccountUnchecked(c.accountHandle);
            if (phoneAccount != null &&
                    phoneAccount.hasCapabilities(PhoneAccount.CAPABILITY_MULTI_USER)) {
                if (c.initiatingUser != null &&
                        UserUtil.isManagedProfile(mContext, c.initiatingUser)) {
                    return addCall(c, c.initiatingUser);
                } else {
                    return addCall(c, null);
                }
            } else {
                return addCall(c, c.accountHandle == null ? null : c.accountHandle.getUserHandle());
            }
        }

        /**
         * Insert the call to a specific user or all users except managed profile.
         * @param c context
         * @param userToBeInserted user handle of user that the call going be inserted to. null
         *                         if insert to all users except managed profile.
         */
        private Uri addCall(AddCallArgs c, UserHandle userToBeInserted) {
            return Calls.addCall(c.callerInfo, c.context, c.number, c.postDialDigits, c.viaNumber,
                    c.presentation, c.callType, c.features, c.accountHandle, c.timestamp,
                    c.durationInSec, c.dataUsage, userToBeInserted == null,
                    userToBeInserted, c.isRead, c.callBockReason, c.callScreeningAppName,
                    c.callScreeningComponentName);
        }


        @Override
        protected void onPostExecute(Uri[] result) {
            for (int i = 0; i < result.length; i++) {
                Uri uri = result[i];
                /*
                 Performs a simple sanity check to make sure the call was written in the database.
                 Typically there is only one result per call so it is easy to identify which one
                 failed.
                 */
                if (uri == null) {
                    Log.w(TAG, "Failed to write call to the log.");
                }
                if (mListeners[i] != null) {
                    mListeners[i].onLogCompleted(uri);
                }
            }
        }
    }

    private void sendAddCallBroadcast(int callType, long duration) {
        Intent callAddIntent = new Intent(ACTION_CALLS_TABLE_ADD_ENTRY);
        callAddIntent.putExtra(CALL_TYPE, callType);
        callAddIntent.putExtra(CALL_DURATION, duration);
        mContext.sendBroadcast(callAddIntent, PERMISSION_PROCESS_CALLLOG_INFO);
    }

    private String getCountryIsoFromCountry(Country country) {
        if(country == null) {
            // Fallback to Locale if there are issues with CountryDetector
            Log.w(TAG, "Value for country was null. Falling back to Locale.");
            return Locale.getDefault().getCountry();
        }

        return country.getCountryIso();
    }

    /**
     * Get the current country code
     *
     * @return the ISO 3166-1 two letters country code of current country.
     */
    public String getCountryIso() {
        synchronized (mLock) {
            if (mCurrentCountryIso == null) {
                Log.i(TAG, "Country cache is null. Detecting Country and Setting Cache...");
                final CountryDetector countryDetector =
                        (CountryDetector) mContext.getSystemService(Context.COUNTRY_DETECTOR);
                Country country = null;
                if (countryDetector != null) {
                    country = countryDetector.detectCountry();

                    countryDetector.addCountryListener((newCountry) -> {
                        Log.startSession("CLM.oCD");
                        try {
                            synchronized (mLock) {
                                Log.i(TAG, "Country ISO changed. Retrieving new ISO...");
                                mCurrentCountryIso = getCountryIsoFromCountry(newCountry);
                            }
                        } finally {
                            Log.endSession();
                        }
                    }, Looper.getMainLooper());
                }
                mCurrentCountryIso = getCountryIsoFromCountry(country);
            }
            return mCurrentCountryIso;
        }
    }
}
