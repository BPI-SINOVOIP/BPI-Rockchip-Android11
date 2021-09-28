/*
 * Copyright (C) 2016 The Android Open Source Project
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

package com.android.server.telecom.callfiltering;

import android.provider.CallLog;
import android.provider.CallLog.Calls;
import android.text.TextUtils;

import java.util.Objects;

public class CallFilteringResult {
    public static class Builder {
        private boolean mShouldAllowCall;
        private boolean mShouldReject;
        private boolean mShouldAddToCallLog;
        private boolean mShouldShowNotification;
        private boolean mShouldSilence = false;
        private boolean mShouldScreenViaAudio = false;
        private boolean mContactExists = false;
        private int mCallBlockReason = Calls.BLOCK_REASON_NOT_BLOCKED;
        private CharSequence mCallScreeningAppName = null;
        private String mCallScreeningComponentName = null;

        public Builder setShouldAllowCall(boolean shouldAllowCall) {
            mShouldAllowCall = shouldAllowCall;
            return this;
        }

        public Builder setShouldReject(boolean shouldReject) {
            mShouldReject = shouldReject;
            return this;
        }

        public Builder setShouldAddToCallLog(boolean shouldAddToCallLog) {
            mShouldAddToCallLog = shouldAddToCallLog;
            return this;
        }

        public Builder setShouldShowNotification(boolean shouldShowNotification) {
            mShouldShowNotification = shouldShowNotification;
            return this;
        }

        public Builder setShouldSilence(boolean shouldSilence) {
            mShouldSilence = shouldSilence;
            return this;
        }

        public Builder setCallBlockReason(int callBlockReason) {
            mCallBlockReason = callBlockReason;
            return this;
        }

        public Builder setShouldScreenViaAudio(boolean shouldScreenViaAudio) {
            mShouldScreenViaAudio = shouldScreenViaAudio;
            return this;
        }

        public Builder setCallScreeningAppName(CharSequence callScreeningAppName) {
            mCallScreeningAppName = callScreeningAppName;
            return this;
        }

        public Builder setCallScreeningComponentName(String callScreeningComponentName) {
            mCallScreeningComponentName = callScreeningComponentName;
            return this;
        }

        public Builder setContactExists(boolean contactExists) {
            mContactExists = contactExists;
            return this;
        }

        public static Builder from(CallFilteringResult result) {
            return new Builder()
                    .setShouldAllowCall(result.shouldAllowCall)
                    .setShouldReject(result.shouldReject)
                    .setShouldAddToCallLog(result.shouldAddToCallLog)
                    .setShouldShowNotification(result.shouldShowNotification)
                    .setShouldSilence(result.shouldSilence)
                    .setCallBlockReason(result.mCallBlockReason)
                    .setShouldScreenViaAudio(result.shouldScreenViaAudio)
                    .setCallScreeningAppName(result.mCallScreeningAppName)
                    .setCallScreeningComponentName(result.mCallScreeningComponentName)
                    .setContactExists(result.contactExists);
        }

        public CallFilteringResult build() {
            return new CallFilteringResult(mShouldAllowCall, mShouldReject, mShouldSilence,
                    mShouldAddToCallLog, mShouldShowNotification, mCallBlockReason,
                    mCallScreeningAppName, mCallScreeningComponentName, mShouldScreenViaAudio,
                    mContactExists);
        }
    }

    public boolean shouldAllowCall;
    public boolean shouldReject;
    public boolean shouldSilence;
    public boolean shouldAddToCallLog;
    public boolean shouldScreenViaAudio = false;
    public boolean shouldShowNotification;
    public int mCallBlockReason;
    public CharSequence mCallScreeningAppName;
    public String mCallScreeningComponentName;
    public boolean contactExists;

    private CallFilteringResult(boolean shouldAllowCall, boolean shouldReject, boolean
            shouldSilence, boolean shouldAddToCallLog, boolean shouldShowNotification, int
            callBlockReason, CharSequence callScreeningAppName, String callScreeningComponentName,
            boolean shouldScreenViaAudio, boolean contactExists) {
        this.shouldAllowCall = shouldAllowCall;
        this.shouldReject = shouldReject;
        this.shouldSilence = shouldSilence;
        this.shouldAddToCallLog = shouldAddToCallLog;
        this.shouldShowNotification = shouldShowNotification;
        this.shouldScreenViaAudio = shouldScreenViaAudio;
        this.mCallBlockReason = callBlockReason;
        this.mCallScreeningAppName = callScreeningAppName;
        this.mCallScreeningComponentName = callScreeningComponentName;
        this.contactExists = contactExists;
    }

    /**
     * Combine this CallFilteringResult with another, returning a CallFilteringResult with the more
     * restrictive properties of the two. Where there are multiple call filtering components which
     * block a call, the first filter from {@link BlockCheckerFilter},
     * {@link DirectToVoicemailFilter}, {@link CallScreeningServiceFilter} which blocked a call
     * shall be used to populate the call block reason, component name, etc.
     */
    public CallFilteringResult combine(CallFilteringResult other) {
        if (other == null) {
            return this;
        }

        if (isBlockedByProvider(mCallBlockReason)) {
            return getCombinedCallFilteringResult(other, mCallBlockReason,
                null /*callScreeningAppName*/, null /*callScreeningComponentName*/);
        } else if (isBlockedByProvider(other.mCallBlockReason)) {
            return getCombinedCallFilteringResult(other, other.mCallBlockReason,
                null /*callScreeningAppName*/, null /*callScreeningComponentName*/);
        }

        if (mCallBlockReason == Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL
            || other.mCallBlockReason == Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL) {
            return getCombinedCallFilteringResult(other, Calls.BLOCK_REASON_DIRECT_TO_VOICEMAIL,
                null /*callScreeningAppName*/, null /*callScreeningComponentName*/);
        }

        if (shouldReject && mCallBlockReason == CallLog.Calls.BLOCK_REASON_CALL_SCREENING_SERVICE) {
            return getCombinedCallFilteringResult(other, Calls.BLOCK_REASON_CALL_SCREENING_SERVICE,
                mCallScreeningAppName, mCallScreeningComponentName);
        } else if (other.shouldReject && other.mCallBlockReason == CallLog.Calls
            .BLOCK_REASON_CALL_SCREENING_SERVICE) {
            return getCombinedCallFilteringResult(other, Calls.BLOCK_REASON_CALL_SCREENING_SERVICE,
                other.mCallScreeningAppName, other.mCallScreeningComponentName);
        }

        if (shouldScreenViaAudio) {
            return getCombinedCallFilteringResult(other, Calls.BLOCK_REASON_NOT_BLOCKED,
                    mCallScreeningAppName, mCallScreeningComponentName);
        } else if (other.shouldScreenViaAudio) {
            return getCombinedCallFilteringResult(other, Calls.BLOCK_REASON_NOT_BLOCKED,
                    other.mCallScreeningAppName, other.mCallScreeningComponentName);
        }

        return new Builder()
                .setShouldAllowCall(shouldAllowCall && other.shouldAllowCall)
                .setShouldReject(shouldReject || other.shouldReject)
                .setShouldSilence(shouldSilence || other.shouldSilence)
                .setShouldAddToCallLog(shouldAddToCallLog && other.shouldAddToCallLog)
                .setShouldShowNotification(shouldShowNotification && other.shouldShowNotification)
                .setShouldScreenViaAudio(shouldScreenViaAudio || other.shouldScreenViaAudio)
                .setContactExists(contactExists || other.contactExists)
                .build();
    }

    private boolean isBlockedByProvider(int blockReason) {
        if (blockReason == Calls.BLOCK_REASON_BLOCKED_NUMBER
            || blockReason == Calls.BLOCK_REASON_UNKNOWN_NUMBER
            || blockReason == Calls.BLOCK_REASON_RESTRICTED_NUMBER
            || blockReason == Calls.BLOCK_REASON_PAY_PHONE
            || blockReason == Calls.BLOCK_REASON_NOT_IN_CONTACTS) {
            return true;
        }

        return false;
    }

    private CallFilteringResult getCombinedCallFilteringResult(CallFilteringResult other,
        int callBlockReason, CharSequence callScreeningAppName, String callScreeningComponentName) {
        return new Builder()
                .setShouldAllowCall(shouldAllowCall && other.shouldAllowCall)
                .setShouldReject(shouldReject || other.shouldReject)
                .setShouldSilence(shouldSilence || other.shouldSilence)
                .setShouldAddToCallLog(shouldAddToCallLog && other.shouldAddToCallLog)
                .setShouldShowNotification(shouldShowNotification && other.shouldShowNotification)
                .setShouldScreenViaAudio(shouldScreenViaAudio || other.shouldScreenViaAudio)
                .setCallBlockReason(callBlockReason)
                .setCallScreeningAppName(callScreeningAppName)
                .setCallScreeningComponentName(callScreeningComponentName)
                .setContactExists(contactExists || other.contactExists)
                .build();
    }


    @Override
    public boolean equals(Object o) {
        if (this == o) return true;
        if (o == null || getClass() != o.getClass()) return false;

        CallFilteringResult that = (CallFilteringResult) o;

        if (shouldAllowCall != that.shouldAllowCall) return false;
        if (shouldReject != that.shouldReject) return false;
        if (shouldSilence != that.shouldSilence) return false;
        if (shouldAddToCallLog != that.shouldAddToCallLog) return false;
        if (shouldShowNotification != that.shouldShowNotification) return false;
        if (mCallBlockReason != that.mCallBlockReason) return false;
        if (contactExists != that.contactExists) return false;

        if (!Objects.equals(mCallScreeningAppName, that.mCallScreeningAppName)) return false;
        if (!Objects.equals(mCallScreeningComponentName, that.mCallScreeningComponentName)) {
            return false;
        }
        return true;
    }

    @Override
    public int hashCode() {
        int result = (shouldAllowCall ? 1 : 0);
        result = 31 * result + (shouldReject ? 1 : 0);
        result = 31 * result + (shouldSilence ? 1 : 0);
        result = 31 * result + (shouldAddToCallLog ? 1 : 0);
        result = 31 * result + (shouldShowNotification ? 1 : 0);
        return result;
    }

    @Override
    public String toString() {
        StringBuilder sb = new StringBuilder();
        sb.append("[");
        if (shouldAllowCall) {
            sb.append("Allow");
        } else if (shouldReject) {
            sb.append("Reject");
        } else if (shouldSilence) {
            sb.append("Silence");
        } else {
            sb.append("Ignore");
        }

        if (shouldScreenViaAudio) {
            sb.append(", audio processing");
        }

        if (shouldAddToCallLog) {
            sb.append(", logged");
        }

        if (shouldShowNotification) {
            sb.append(", notified");
        }

        if (contactExists) {
            sb.append(", contact exists");
        }

        if (mCallBlockReason != 0) {
            sb.append(", mCallBlockReason = ");
            sb.append(mCallBlockReason);
        }

        if (!TextUtils.isEmpty(mCallScreeningAppName)) {
            sb.append(", mCallScreeningAppName = ");
            sb.append(mCallScreeningAppName);
        }

        if (!TextUtils.isEmpty(mCallScreeningComponentName)) {
            sb.append(", mCallScreeningComponentName = ");
            sb.append(mCallScreeningComponentName);
        }
        sb.append("]");

        return sb.toString();
    }
}
