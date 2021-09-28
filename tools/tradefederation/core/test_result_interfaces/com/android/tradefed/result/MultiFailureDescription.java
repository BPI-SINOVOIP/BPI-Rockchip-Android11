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
import com.android.tradefed.result.proto.TestRecordProto.FailureStatus;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import javax.annotation.Nullable;

/**
 * Collect multiple {@link FailureDescription} in one holder. This can be used to carry all the
 * failures description when several attempts on the same test case or run are made, each resulting
 * in a failure.
 */
public final class MultiFailureDescription extends FailureDescription {

    private List<FailureDescription> mFailures = new ArrayList<>();

    public MultiFailureDescription(List<FailureDescription> failures) {
        super();
        addMultiFailures(failures);
    }

    public MultiFailureDescription(FailureDescription... failures) {
        this(Arrays.asList(failures));
    }

    /**
     * Add another failure to an existing {@link MultiFailureDescription}.
     *
     * @param failure The additional failure
     * @return The current {@link MultiFailureDescription}.
     */
    public MultiFailureDescription addFailure(FailureDescription failure) {
        mFailures.add(failure);
        return this;
    }

    /**
     * Returns the list of {@link FailureDescription} tracked by the {@link
     * MultiFailureDescription}.
     */
    public List<FailureDescription> getFailures() {
        return mFailures;
    }

    @Override
    public @Nullable FailureStatus getFailureStatus() {
        if (mFailures.isEmpty()) {
            return null;
        }
        // Default to the first reported failure
        return mFailures.get(0).getFailureStatus();
    }

    @Override
    public String getErrorMessage() {
        if (mFailures.isEmpty()) {
            return null;
        }
        // Default to the first reported failure
        return toString();
    }

    @Override
    public ErrorIdentifier getErrorIdentifier() {
        if (mFailures.isEmpty()) {
            return null;
        }
        // Default to the first reported failure
        return mFailures.get(0).getErrorIdentifier();
    }

    @Override
    public String getOrigin() {
        if (mFailures.isEmpty()) {
            return null;
        }
        // Default to the first reported failure
        return mFailures.get(0).getOrigin();
    }

    @Override
    public boolean isRetriable() {
        for (FailureDescription desc : mFailures) {
            if (desc.isRetriable()) {
                return true;
            }
        }
        // If none of the sub-failures are retriable, don't retry.
        return false;
    }

    @Override
    public String toString() {
        // Fallback to Single failure type
        if (mFailures.size() == 1) {
            return mFailures.get(0).toString();
        }
        StringBuilder sb =
                new StringBuilder(String.format("There were %d failures:", mFailures.size()));
        for (FailureDescription f : mFailures) {
            sb.append(String.format("\n  %s", f.toString()));
        }
        return sb.toString();
    }

    @Override
    public boolean equals(Object obj) {
        if (this == obj) return true;
        if (obj == null) return false;
        if (getClass() != obj.getClass()) return false;
        MultiFailureDescription other = (MultiFailureDescription) obj;
        if (other.mFailures.size() != this.mFailures.size()) {
            return false;
        }
        for (int i = 0; i < this.mFailures.size(); i++) {
            if (!this.mFailures.get(i).equals(other.mFailures.get(i))) {
                return false;
            }
        }
        return true;
    }

    /** Un-nest all the sub-MultiFailureDescription for ease of tracking. */
    private void addMultiFailures(List<FailureDescription> failures) {
        for (FailureDescription failure : failures) {
            if (failure instanceof MultiFailureDescription) {
                addMultiFailures(((MultiFailureDescription) failure).getFailures());
            } else {
                mFailures.add(failure);
            }
        }
    }
}
