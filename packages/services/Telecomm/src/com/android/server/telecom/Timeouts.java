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

import android.content.ContentResolver;
import android.provider.Settings;
import android.telecom.CallRedirectionService;
import java.util.concurrent.TimeUnit;

/**
 * A helper class which serves only to make it easier to lookup timeout values. This class should
 * never be instantiated, and only accessed through the {@link #get(String, long)} method.
 *
 * These methods are safe to call from any thread, including the UI thread.
 */
public final class Timeouts {
    public static class Adapter {
        public Adapter() {
        }

        public long getCallScreeningTimeoutMillis(ContentResolver cr) {
            return Timeouts.getCallScreeningTimeoutMillis(cr);
        }

        public long getCallRemoveUnbindInCallServicesDelay(ContentResolver cr) {
            return Timeouts.getCallRemoveUnbindInCallServicesDelay(cr);
        }

        public long getRetryBluetoothConnectAudioBackoffMillis(ContentResolver cr) {
            return Timeouts.getRetryBluetoothConnectAudioBackoffMillis(cr);
        }

        public long getBluetoothPendingTimeoutMillis(ContentResolver cr) {
            return Timeouts.getBluetoothPendingTimeoutMillis(cr);
        }

        public long getEmergencyCallbackWindowMillis(ContentResolver cr) {
            return Timeouts.getEmergencyCallbackWindowMillis(cr);
        }

        public long getUserDefinedCallRedirectionTimeoutMillis(ContentResolver cr) {
            return Timeouts.getUserDefinedCallRedirectionTimeoutMillis(cr);
        }

        public long getCarrierCallRedirectionTimeoutMillis(ContentResolver cr) {
            return Timeouts.getCarrierCallRedirectionTimeoutMillis(cr);
        }

        public long getPhoneAccountSuggestionServiceTimeout(ContentResolver cr) {
            return Timeouts.getPhoneAccountSuggestionServiceTimeout(cr);
        }

        public long getCallRecordingToneRepeatIntervalMillis(ContentResolver cr) {
            return Timeouts.getCallRecordingToneRepeatIntervalMillis(cr);
        }
    }

    /** A prefix to use for all keys so to not clobber the global namespace. */
    private static final String PREFIX = "telecom.";

    private Timeouts() {
    }

    /**
     * Returns the timeout value from Settings or the default value if it hasn't been changed. This
     * method is safe to call from any thread, including the UI thread.
     *
     * @param contentResolver The content resolved.
     * @param key             Settings key to retrieve.
     * @param defaultValue    Default value, in milliseconds.
     * @return The timeout value from Settings or the default value if it hasn't been changed.
     */
    private static long get(ContentResolver contentResolver, String key, long defaultValue) {
        return Settings.Secure.getLong(contentResolver, PREFIX + key, defaultValue);
    }

    /**
     * Returns the amount of time to wait before disconnecting a call that was canceled via
     * NEW_OUTGOING_CALL broadcast. This timeout allows apps which repost the call using a gateway
     * to reuse the existing call, preventing the call from causing a start->end->start jank in the
     * in-call UI.
     */
    public static long getNewOutgoingCallCancelMillis(ContentResolver contentResolver) {
        return get(contentResolver, "new_outgoing_call_cancel_ms", 500L);
    }

    /**
     * Returns the maximum amount of time to wait before disconnecting a call that was canceled via
     * NEW_OUTGOING_CALL broadcast. This prevents malicious or poorly configured apps from
     * forever tying up the Telecom stack.
     */
    public static long getMaxNewOutgoingCallCancelMillis(ContentResolver contentResolver) {
        return get(contentResolver, "max_new_outgoing_call_cancel_ms", 10000L);
    }

    /**
     * Returns the amount of time to play each DTMF tone after post dial continue.
     * This timeout allows the current tone to play for a certain amount of time before either being
     * interrupted by the next tone or terminated.
     */
    public static long getDelayBetweenDtmfTonesMillis(ContentResolver contentResolver) {
        return get(contentResolver, "delay_between_dtmf_tones_ms", 300L);
    }

    /**
     * Returns the amount of time to wait for an emergency call to be placed before routing to
     * a different call service. A value of 0 or less means no timeout should be used.
     */
    public static long getEmergencyCallTimeoutMillis(ContentResolver contentResolver) {
        return get(contentResolver, "emergency_call_timeout_millis", 25000L /* 25 seconds */);
    }

    /**
     * Returns the amount of time to wait for an emergency call to be placed before routing to
     * a different call service. This timeout is used only when the radio is powered off (for
     * example in airplane mode). A value of 0 or less means no timeout should be used.
     */
    public static long getEmergencyCallTimeoutRadioOffMillis(ContentResolver contentResolver) {
        return get(contentResolver, "emergency_call_timeout_radio_off_millis",
                60000L /* 1 minute */);
    }

    /**
     * Returns the amount of delay before unbinding the in-call services after all the calls
     * are removed.
     */
    public static long getCallRemoveUnbindInCallServicesDelay(ContentResolver contentResolver) {
        return get(contentResolver, "call_remove_unbind_in_call_services_delay",
                2000L /* 2 seconds */);
    }

    /**
     * Returns the amount of time for which bluetooth is considered connected after requesting
     * connection. This compensates for the amount of time it takes for the audio route to
     * actually change to bluetooth.
     */
    public static long getBluetoothPendingTimeoutMillis(ContentResolver contentResolver) {
        return get(contentResolver, "bluetooth_pending_timeout_millis", 5000L);
    }

    /**
     * Returns the amount of time to wait before retrying the connectAudio call. This is
     * necessary to account for the HeadsetStateMachine sometimes not being ready when we want to
     * connect to bluetooth audio immediately after a device connects.
     */
    public static long getRetryBluetoothConnectAudioBackoffMillis(ContentResolver contentResolver) {
        return get(contentResolver, "retry_bluetooth_connect_audio_backoff_millis", 500L);
    }

    /**
     * Returns the amount of time to wait for the phone account suggestion service to reply.
     */
    public static long getPhoneAccountSuggestionServiceTimeout(ContentResolver contentResolver) {
        return get(contentResolver, "phone_account_suggestion_service_timeout",
                5000L /* 5 seconds */);
    }

    /**
     * Returns the amount of time to wait for the call screening service to allow or disallow a
     * call.
     */
    public static long getCallScreeningTimeoutMillis(ContentResolver contentResolver) {
        return get(contentResolver, "call_screening_timeout", 5000L /* 5 seconds */);
    }

    /**
     * Returns the amount of time after an emergency call that incoming calls should be treated
     * as potential emergency callbacks.
     */
    public static long getEmergencyCallbackWindowMillis(ContentResolver contentResolver) {
        return get(contentResolver, "emergency_callback_window_millis",
                TimeUnit.MILLISECONDS.convert(5, TimeUnit.MINUTES));
    }

    /**
     * Returns the amount of time for an user-defined {@link CallRedirectionService}.
     *
     * @param contentResolver The content resolved.
     */
    public static long getUserDefinedCallRedirectionTimeoutMillis(ContentResolver contentResolver) {
        return get(contentResolver, "user_defined_call_redirection_timeout",
                5000L /* 5 seconds */);
    }

    /**
     * Returns the amount of time for a carrier {@link CallRedirectionService}.
     *
     * @param contentResolver The content resolved.
     */
    public static long getCarrierCallRedirectionTimeoutMillis(ContentResolver contentResolver) {
        return get(contentResolver, "carrier_call_redirection_timeout", 5000L /* 5 seconds */);
    }

    /**
     * Returns the number of milliseconds between two plays of the call recording tone.
     */
    public static long getCallRecordingToneRepeatIntervalMillis(ContentResolver contentResolver) {
        return get(contentResolver, "call_recording_tone_repeat_interval", 15000L /* 15 seconds */);
    }

    /**
     * Returns the number of milliseconds for which the system should exempt the default dialer from
     * power save restrictions due to the dialer needing to handle a missed call notification
     * (update call log, check VVM, etc...).
     */
    public static long getDialerMissedCallPowerSaveExemptionTimeMillis(
            ContentResolver contentResolver) {
        return get(contentResolver, "dialer_missed_call_power_save_exemption_time_millis",
                30000L /*30 seconds*/);
    }
}
