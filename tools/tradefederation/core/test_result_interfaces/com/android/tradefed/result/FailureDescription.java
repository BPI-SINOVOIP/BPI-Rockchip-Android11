/*
 * Copyright (C) 2020 The Android Open Source Project
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
package com.android.tradefed.result;

import com.android.tradefed.result.error.ErrorIdentifier;
import com.android.tradefed.result.proto.TestRecordProto;

import javax.annotation.Nullable;

/**
 * The class describing a failure information in Trade Federation. This class contains the debugging
 * information and context of the failure that helps understanding the issue.
 */
public class FailureDescription {
    // The error message generated from the failure
    private String mErrorMessage;
    // Optional: The category of the failure
    private @Nullable TestRecordProto.FailureStatus mFailureStatus =
            TestRecordProto.FailureStatus.UNSET;
    // Optional: Context of the action in progress during the failure
    private @Nullable ActionInProgress mActionInProgress = ActionInProgress.UNSET;
    // Optional: A free-formed text that help debugging the failure
    private @Nullable String mDebugHelpMessage = null;
    // Optional: The exception that triggered the failure
    private @Nullable Throwable mCause = null;
    // Whether or not the error is retriable by Tradefed auto-retry. By Default we retry it all.
    private boolean mRetriable = true;

    // Error identifiers
    // Optional: The error identifier and its code
    private @Nullable ErrorIdentifier mErrorId = null;
    // Optional: The class that raised the error
    private @Nullable String mOrigin = null;

    FailureDescription() {}

    /**
     * Set the {@link com.android.tradefed.result.proto.TestRecordProto.FailureStatus} associated
     * with the failure.
     */
    public FailureDescription setFailureStatus(TestRecordProto.FailureStatus status) {
        mFailureStatus = status;
        return this;
    }

    /** Returns the FailureStatus associated with the failure. Can be null. */
    public @Nullable TestRecordProto.FailureStatus getFailureStatus() {
        if (TestRecordProto.FailureStatus.UNSET.equals(mFailureStatus)) {
            return null;
        }
        return mFailureStatus;
    }

    /** Sets the action in progress during the failure. */
    public FailureDescription setActionInProgress(ActionInProgress action) {
        mActionInProgress = action;
        return this;
    }

    /** Returns the action in progress during the failure. Can be null. */
    public @Nullable ActionInProgress getActionInProgress() {
        return mActionInProgress;
    }

    /** Sets the debug help message for the failure. */
    public FailureDescription setDebugHelpMessage(String message) {
        mDebugHelpMessage = message;
        return this;
    }

    /** Returns the debug help message. Can be null. */
    public @Nullable String getDebugHelpMessage() {
        return mDebugHelpMessage;
    }

    /** Sets the exception that caused the failure if any. */
    public FailureDescription setCause(Throwable cause) {
        mCause = cause;
        return this;
    }

    /** Returns the exception that caused the failure. Can be null. */
    public @Nullable Throwable getCause() {
        return mCause;
    }

    /** Sets whether or not the failure is retriable. */
    public FailureDescription setRetriable(boolean retriable) {
        mRetriable = retriable;
        return this;
    }

    /** Returns whether or not the error is retriable or not. */
    public boolean isRetriable() {
        return mRetriable;
    }

    /** Sets the {@link ErrorIdentifier} representing the failure. */
    public FailureDescription setErrorIdentifier(ErrorIdentifier errorId) {
        mErrorId = errorId;
        return this;
    }

    /** Returns the {@link ErrorIdentifier} representing the failure. Can be null. */
    public ErrorIdentifier getErrorIdentifier() {
        return mErrorId;
    }

    /** Sets the origin of the error. */
    public FailureDescription setOrigin(String origin) {
        mOrigin = origin;
        return this;
    }

    /** Returns the origin of the error. Can be null. */
    public String getOrigin() {
        return mOrigin;
    }

    /** Sets the error message. */
    public void setErrorMessage(String errorMessage) {
        mErrorMessage = errorMessage;
    }

    /** Returns the error message associated with the failure. */
    public String getErrorMessage() {
        return mErrorMessage;
    }

    @Override
    public String toString() {
        // For backward compatibility of result interface, toString falls back to the simple message
        return mErrorMessage;
    }

    /**
     * Create a {@link FailureDescription} based on the error message generated from the failure.
     *
     * @param errorMessage The error message from the failure.
     * @return the created {@link FailureDescription}
     */
    public static FailureDescription create(String errorMessage) {
        return create(errorMessage, null);
    }

    /**
     * Create a {@link FailureDescription} based on the error message generated from the failure.
     *
     * @param errorMessage The error message from the failure.
     * @param status The status associated with the failure.
     * @return the created {@link FailureDescription}
     */
    public static FailureDescription create(
            String errorMessage, @Nullable TestRecordProto.FailureStatus status) {
        FailureDescription info = new FailureDescription();
        info.mErrorMessage = errorMessage;
        info.mFailureStatus = status;
        return info;
    }

    @Override
    public int hashCode() {
        final int prime = 31;
        int result = 1;
        result = prime * result + ((mActionInProgress == null) ? 0 : mActionInProgress.hashCode());
        result = prime * result + ((mDebugHelpMessage == null) ? 0 : mDebugHelpMessage.hashCode());
        result = prime * result + ((mErrorMessage == null) ? 0 : mErrorMessage.hashCode());
        result = prime * result + ((mFailureStatus == null) ? 0 : mFailureStatus.hashCode());
        return result;
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj == null) return false;
        if (getClass() != obj.getClass()) return false;
        FailureDescription other = (FailureDescription) obj;
        if (mActionInProgress != other.mActionInProgress) return false;
        if (mDebugHelpMessage == null) {
            if (other.mDebugHelpMessage != null) return false;
        } else if (!mDebugHelpMessage.equals(other.mDebugHelpMessage)) return false;
        if (mErrorMessage == null) {
            if (other.mErrorMessage != null) return false;
        } else if (!mErrorMessage.equals(other.mErrorMessage)) return false;
        if (mFailureStatus != other.mFailureStatus) return false;
        return true;
    }
}
