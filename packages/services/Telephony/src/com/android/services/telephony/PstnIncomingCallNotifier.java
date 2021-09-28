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

import android.net.Uri;
import android.os.AsyncResult;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.os.SystemClock;
import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;
import android.text.TextUtils;

import com.android.internal.telephony.Call;
import com.android.internal.telephony.CallStateException;
import com.android.internal.telephony.Connection;
import com.android.internal.telephony.GsmCdmaPhone;
import com.android.internal.telephony.Phone;
import com.android.internal.telephony.PhoneConstants;
import com.android.internal.telephony.cdma.CdmaCallWaitingNotification;
import com.android.internal.telephony.imsphone.ImsExternalCallTracker;
import com.android.internal.telephony.imsphone.ImsExternalConnection;
import com.android.internal.telephony.imsphone.ImsPhoneConnection;
import com.android.phone.NumberVerificationManager;
import com.android.phone.PhoneUtils;
import com.android.telephony.Rlog;

import java.util.List;
import java.util.Objects;
import java.util.stream.Collectors;

/**
 * Listens to incoming-call events from the associated phone object and notifies Telecom upon each
 * occurence. One instance of these exists for each of the telephony-based call services.
 */
final class PstnIncomingCallNotifier {
    private static final String LOG_TAG = "PstnIncomingCallNotifier";

    /** New ringing connection event code. */
    private static final int EVENT_NEW_RINGING_CONNECTION = 100;
    private static final int EVENT_CDMA_CALL_WAITING = 101;
    private static final int EVENT_UNKNOWN_CONNECTION = 102;

    /**
     * The max amount of time to wait before hanging up a call that was for number verification.
     *
     * The delay is so that the remote end has time to hang up the call after receiving the
     * verification signal so that the call doesn't go to voicemail.
     */
    private static final int MAX_NUMBER_VERIFICATION_HANGUP_DELAY_MILLIS = 10000;

    /** The phone object to listen to. */
    private final Phone mPhone;

    /**
     * Used to listen to events from {@link #mPhone}.
     */
    private final Handler mHandler = new Handler() {
        @Override
        public void handleMessage(Message msg) {
            switch(msg.what) {
                case EVENT_NEW_RINGING_CONNECTION:
                    handleNewRingingConnection((AsyncResult) msg.obj);
                    break;
                case EVENT_CDMA_CALL_WAITING:
                    handleCdmaCallWaiting((AsyncResult) msg.obj);
                    break;
                case EVENT_UNKNOWN_CONNECTION:
                    handleNewUnknownConnection((AsyncResult) msg.obj);
                    break;
                default:
                    break;
            }
        }
    };

    /**
     * Persists the specified parameters and starts listening to phone events.
     *
     * @param phone The phone object for listening to incoming calls.
     */
    PstnIncomingCallNotifier(Phone phone) {
        if (phone == null) {
            throw new NullPointerException();
        }

        mPhone = phone;

        registerForNotifications();
    }

    void teardown() {
        unregisterForNotifications();
    }

    /**
     * Register for notifications from the base phone.
     */
    private void registerForNotifications() {
        if (mPhone != null) {
            Log.i(this, "Registering: %s", mPhone);
            mPhone.registerForNewRingingConnection(mHandler, EVENT_NEW_RINGING_CONNECTION, null);
            mPhone.registerForCallWaiting(mHandler, EVENT_CDMA_CALL_WAITING, null);
            mPhone.registerForUnknownConnection(mHandler, EVENT_UNKNOWN_CONNECTION, null);
        }
    }

    private void unregisterForNotifications() {
        if (mPhone != null) {
            Log.i(this, "Unregistering: %s", mPhone);
            mPhone.unregisterForNewRingingConnection(mHandler);
            mPhone.unregisterForCallWaiting(mHandler);
            mPhone.unregisterForUnknownConnection(mHandler);
        }
    }

    /**
     * Verifies the incoming call and triggers sending the incoming-call intent to Telecom.
     *
     * @param asyncResult The result object from the new ringing event.
     */
    private void handleNewRingingConnection(AsyncResult asyncResult) {
        Log.d(this, "handleNewRingingConnection");
        Connection connection = (Connection) asyncResult.result;
        if (connection != null) {
            Call call = connection.getCall();
            // Check if we have a pending number verification request.
            if (connection.getAddress() != null) {
                if (NumberVerificationManager.getInstance()
                        .checkIncomingCall(connection.getAddress())) {
                    // Disconnect the call if it matches, after a delay
                    mHandler.postDelayed(() -> {
                        try {
                            connection.hangup();
                        } catch (CallStateException e) {
                            Log.i(this, "Remote end hung up call verification call");
                        }
                        // TODO: use an app-supplied delay (needs new API), not to exceed the
                        // existing max.
                    }, MAX_NUMBER_VERIFICATION_HANGUP_DELAY_MILLIS);
                    return;
                }
            }

            // Final verification of the ringing state before sending the intent to Telecom.
            if (call != null && call.getState().isRinging()) {
                sendIncomingCallIntent(connection);
            }
        }
    }

    private void handleCdmaCallWaiting(AsyncResult asyncResult) {
        Log.d(this, "handleCdmaCallWaiting");
        CdmaCallWaitingNotification ccwi = (CdmaCallWaitingNotification) asyncResult.result;
        Call call = mPhone.getRingingCall();
        if (call.getState() == Call.State.WAITING) {
            Connection connection = call.getLatestConnection();
            if (connection != null) {
                String number = connection.getAddress();
                int presentation = connection.getNumberPresentation();

                if (presentation != PhoneConstants.PRESENTATION_ALLOWED
                        && presentation == ccwi.numberPresentation) {
                    // Presentation of number not allowed, but the presentation of the Connection
                    // and the call waiting presentation match.
                    Log.i(this, "handleCdmaCallWaiting: inform telecom of waiting call; "
                                    + "presentation = %d", presentation);
                    sendIncomingCallIntent(connection);
                } else if (!TextUtils.isEmpty(number) && Objects.equals(number, ccwi.number)) {
                    // Presentation of the number is allowed, so we ensure the number matches the
                    // one in the call waiting information.
                    Log.i(this, "handleCdmaCallWaiting: inform telecom of waiting call; "
                            + "number = %s", Rlog.pii(LOG_TAG, number));
                    sendIncomingCallIntent(connection);
                } else {
                    Log.w(this, "handleCdmaCallWaiting: presentation or number do not match, not"
                            + " informing telecom of call: %s", ccwi);
                }
            }
        }
    }

    private void handleNewUnknownConnection(AsyncResult asyncResult) {
        Log.i(this, "handleNewUnknownConnection");
        if (!(asyncResult.result instanceof Connection)) {
            Log.w(this, "handleNewUnknownConnection called with non-Connection object");
            return;
        }
        Connection connection = (Connection) asyncResult.result;
        if (connection != null) {
            // Because there is a handler between telephony and here, it causes this action to be
            // asynchronous which means that the call can switch to DISCONNECTED by the time it gets
            // to this code. Check here to ensure we are not adding a disconnected or IDLE call.
            Call.State state = connection.getState();
            if (state == Call.State.DISCONNECTED || state == Call.State.IDLE) {
                Log.i(this, "Skipping new unknown connection because it is idle. " + connection);
                return;
            }

            Call call = connection.getCall();
            if (call != null && call.getState().isAlive()) {
                addNewUnknownCall(connection);
            }
        }
    }

    private void addNewUnknownCall(Connection connection) {
        Log.i(this, "addNewUnknownCall, connection is: %s", connection);

        if (!maybeSwapAnyWithUnknownConnection(connection)) {
            Log.i(this, "determined new connection is: %s", connection);
            Bundle extras = new Bundle();
            if (connection.getNumberPresentation() == TelecomManager.PRESENTATION_ALLOWED &&
                    !TextUtils.isEmpty(connection.getAddress())) {
                Uri uri = Uri.fromParts(PhoneAccount.SCHEME_TEL, connection.getAddress(), null);
                extras.putParcelable(TelecomManager.EXTRA_UNKNOWN_CALL_HANDLE, uri);
            }
            // ImsExternalConnections are keyed by a unique mCallId; include this as an extra on
            // the call to addNewUknownCall in Telecom.  This way when the request comes back to the
            // TelephonyConnectionService, we will be able to determine which unknown connection is
            // being added.
            if (connection instanceof ImsExternalConnection) {
                ImsExternalConnection externalConnection = (ImsExternalConnection) connection;
                extras.putInt(ImsExternalCallTracker.EXTRA_IMS_EXTERNAL_CALL_ID,
                        externalConnection.getCallId());
            }

            // Specifies the time the call was added. This is used by the dialer for analytics.
            extras.putLong(TelecomManager.EXTRA_CALL_CREATED_TIME_MILLIS,
                    SystemClock.elapsedRealtime());

            PhoneAccountHandle handle = findCorrectPhoneAccountHandle();
            if (handle == null) {
                try {
                    connection.hangup();
                } catch (CallStateException e) {
                    // connection already disconnected. Do nothing
                }
            } else {
                TelecomManager tm = mPhone.getContext().getSystemService(TelecomManager.class);
                tm.addNewUnknownCall(handle, extras);
            }
        } else {
            Log.i(this, "swapped an old connection, new one is: %s", connection);
        }
    }

    /**
     * Sends the incoming call intent to telecom.
     */
    private void sendIncomingCallIntent(Connection connection) {
        Bundle extras = new Bundle();
        if (connection.getNumberPresentation() == TelecomManager.PRESENTATION_ALLOWED &&
                !TextUtils.isEmpty(connection.getAddress())) {
            Uri uri = Uri.fromParts(PhoneAccount.SCHEME_TEL, connection.getAddress(), null);
            extras.putParcelable(TelecomManager.EXTRA_INCOMING_CALL_ADDRESS, uri);
        }

        // Specifies the time the call was added. This is used by the dialer for analytics.
        extras.putLong(TelecomManager.EXTRA_CALL_CREATED_TIME_MILLIS,
                SystemClock.elapsedRealtime());

        if (connection.getPhoneType() == PhoneConstants.PHONE_TYPE_IMS) {
            if (((ImsPhoneConnection) connection).isRttEnabledForCall()) {
                extras.putBoolean(TelecomManager.EXTRA_START_CALL_WITH_RTT, true);
            }
            if (((ImsPhoneConnection) connection).isIncomingCallAutoRejected()) {
                extras.putString(TelecomManager.EXTRA_CALL_DISCONNECT_MESSAGE,
                        TelecomManager.CALL_AUTO_DISCONNECT_MESSAGE_STRING);
            }
        }

        PhoneAccountHandle handle = findCorrectPhoneAccountHandle();
        if (handle == null) {
            try {
                connection.hangup();
            } catch (CallStateException e) {
                // connection already disconnected. Do nothing
            }
            Log.wtf(LOG_TAG, "sendIncomingCallIntent: failed to add new call because no phone"
                    + " account could be found for the call");
        } else {
            TelecomManager tm = mPhone.getContext().getSystemService(TelecomManager.class);
            try {
                if (connection.isMultiparty()) {
                    tm.addNewIncomingConference(handle, extras);
                } else {
                    tm.addNewIncomingCall(handle, extras);
                }
            } catch (SecurityException se) {
                // If we get a security exception, the most likely cause is:
                // "This PhoneAccountHandle is not registered for this user"
                // If this happens, then it means that for whatever reason the phone account which
                // we are trying to use to add the new incoming call no longer exists in Telecom.
                // This can happen if the handle of the phone account changes.  The likely root
                // cause of this would be a change in active SIM profile for an MVNO style carrier
                // which aggregates multiple carriers together.

                // We will log a list of the available handles ourselves here; PhoneAccountHandle
                // oscures the ID in the toString.  Rlog.pii will do a secure hash on userdebug
                // builds so at least we could tell if the handle we tried is different from the one
                // which we attempted to use.
                List<PhoneAccountHandle> handles = tm.getCallCapablePhoneAccounts();
                String availableHandles = handles.stream()
                        .map(h -> "[" + h.getComponentName() + " "
                                + Rlog.pii(LOG_TAG, h.getId()) + "]")
                        .collect(Collectors.joining(","));
                String attemptedHandle = "[" + handle.getComponentName() + " "
                        + Rlog.pii(LOG_TAG, handle.getId()) + "]";
                Log.wtf(LOG_TAG, "sendIncomingCallIntent: failed to add new call " + connection
                        + " because the handle " + attemptedHandle
                        + " is not in the list of registered handles " + availableHandles
                        + " - call was rejected.");

                // Since the phone account handle we're trying to use is not valid, we have no other
                // recourse but to reject the incoming call.
                try {
                    connection.hangup();
                } catch (CallStateException e) {
                    // connection already disconnected. Do nothing
                }
            }
        }
    }

    /**
     * Returns the PhoneAccount associated with this {@code PstnIncomingCallNotifier}'s phone. On a
     * device with No SIM or in airplane mode, it can return an Emergency-only PhoneAccount. If no
     * PhoneAccount is registered with telecom, return null.
     * @return A valid PhoneAccountHandle that is registered to Telecom or null if there is none
     * registered.
     */
    private PhoneAccountHandle findCorrectPhoneAccountHandle() {
        TelecomAccountRegistry telecomAccountRegistry = TelecomAccountRegistry.getInstance(null);
        // Check to see if a the SIM PhoneAccountHandle Exists for the Call.
        PhoneAccountHandle handle = PhoneUtils.makePstnPhoneAccountHandle(mPhone);
        if (telecomAccountRegistry.hasAccountEntryForPhoneAccount(handle)) {
            return handle;
        }
        // The PhoneAccountHandle does not match any PhoneAccount registered in Telecom.
        // This is only known to happen if there is no SIM card in the device and the device
        // receives an MT call while in ECM. Use the Emergency PhoneAccount to receive the account
        // if it exists.
        PhoneAccountHandle emergencyHandle =
                PhoneUtils.makePstnPhoneAccountHandleWithPrefix(mPhone, "", true);
        if(telecomAccountRegistry.hasAccountEntryForPhoneAccount(emergencyHandle)) {
            Log.i(this, "Receiving MT call in ECM. Using Emergency PhoneAccount Instead.");
            return emergencyHandle;
        }
        Log.w(this, "PhoneAccount not found.");
        return null;
    }

    /**
     * Define cait.Connection := com.android.internal.telephony.Connection
     *
     * Given a previously unknown cait.Connection, check to see if it's likely a replacement for
     * another cait.Connnection we already know about. If it is, then we silently swap it out
     * underneath within the relevant {@link TelephonyConnection}, using
     * {@link TelephonyConnection#setOriginalConnection(Connection)}, and return {@code true}.
     * Otherwise, we return {@code false}.
     */
    private boolean maybeSwapAnyWithUnknownConnection(Connection unknown) {
        if (!unknown.isIncoming()) {
            TelecomAccountRegistry registry = TelecomAccountRegistry.getInstance(null);
            if (registry != null) {
                TelephonyConnectionService service = registry.getTelephonyConnectionService();
                if (service != null) {
                    for (android.telecom.Connection telephonyConnection : service
                            .getAllConnections()) {
                        if (telephonyConnection instanceof TelephonyConnection) {
                            if (maybeSwapWithUnknownConnection(
                                    (TelephonyConnection) telephonyConnection,
                                    unknown)) {
                                return true;
                            }
                        }
                    }
                }
            }
        }
        return false;
    }

    private boolean maybeSwapWithUnknownConnection(
            TelephonyConnection telephonyConnection,
            Connection unknown) {
        Connection original = telephonyConnection.getOriginalConnection();
        if (original != null && !original.isIncoming()
                && Objects.equals(original.getAddress(), unknown.getAddress())) {
            // If the new unknown connection is an external connection, don't swap one with an
            // actual connection.  This means a call got pulled away.  We want the actual connection
            // to disconnect.
            if (unknown instanceof ImsExternalConnection &&
                    !(telephonyConnection
                            .getOriginalConnection() instanceof ImsExternalConnection)) {
                Log.v(this, "maybeSwapWithUnknownConnection - not swapping regular connection " +
                        "with external connection.");
                return false;
            }

            telephonyConnection.setOriginalConnection(unknown);

            // Do not call hang up if the original connection is an ImsExternalConnection, it is
            // not supported.
            if (original instanceof ImsExternalConnection) {
                return true;
            }
            // If the connection we're replacing was a GSM or CDMA connection, call upon the call
            // tracker to perform cleanup of calls.  This ensures that we don't end up with a
            // call stuck in the call tracker preventing other calls from being placed.
            if (original.getCall() != null && original.getCall().getPhone() != null &&
                    original.getCall().getPhone() instanceof GsmCdmaPhone) {

                GsmCdmaPhone phone = (GsmCdmaPhone) original.getCall().getPhone();
                phone.getCallTracker().cleanupCalls();
                Log.i(this, "maybeSwapWithUnknownConnection - Invoking call tracker cleanup "
                        + "for connection: " + original);
            }
            return true;
        }
        return false;
    }
}
