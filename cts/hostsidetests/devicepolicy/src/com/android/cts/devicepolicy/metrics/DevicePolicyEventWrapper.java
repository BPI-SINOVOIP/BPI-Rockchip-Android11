/*
 * Copyright (C) 2018 The Android Open Source Project
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
package com.android.cts.devicepolicy.metrics;

import android.stats.devicepolicy.StringList;
import android.stats.devicepolicy.EventId;
import com.android.os.AtomsProto.DevicePolicyEvent;
import java.util.Arrays;
import java.util.Locale;
import java.util.Objects;

/**
 * Wrapper over <code>DevicePolicyEvent</code> atom as defined in
 * <code>frameworks/base/cmds/statsd/src/atoms.proto</code>.
 * @see Builder
 */
public final class DevicePolicyEventWrapper {
    private final int mEventId;
    private final int mIntValue;
    private final boolean mBooleanValue;
    private final long mTimePeriodMs;
    private final String[] mStringArrayValue;
    private final String mAdminPackageName;

    private DevicePolicyEventWrapper(Builder builder) {
        mEventId = builder.mEventId;
        mIntValue = builder.mIntValue;
        mBooleanValue = builder.mBooleanValue;
        mTimePeriodMs = builder.mTimePeriodMs;
        mStringArrayValue = builder.mStringArrayValue;
        mAdminPackageName = builder.mAdminPackageName;
    }

    /**
     * Constructs a {@link DevicePolicyEventWrapper} from a <code>DevicePolicyEvent</code> atom.
     */
    static DevicePolicyEventWrapper fromDevicePolicyAtom(DevicePolicyEvent atom) {
        return new Builder(atom.getEventId().getNumber())
                .setAdminPackageName(atom.getAdminPackageName())
                .setInt(atom.getIntegerValue())
                .setBoolean(atom.getBooleanValue())
                .setTimePeriod(atom.getTimePeriodMillis())
                .setStrings(getStringArray(atom.getStringListValue()))
                .build();
    }

    /**
     * Converts a <code>StringList</code> proto object to <code>String[]</code>.
     */
    private static String[] getStringArray(StringList stringList) {
        final int count = stringList.getStringValueCount();
        if (count == 0) {
            return null;
        }
        final String[] result = new String[count];
        for (int i = 0; i < count; i++) {
            result[i] = stringList.getStringValue(i);
        }
        return result;
    }

    int getEventId() {
        return mEventId;
    }

    @Override
    public boolean equals(Object obj) {
        if (!(obj instanceof DevicePolicyEventWrapper)) {
            return false;
        }
        final DevicePolicyEventWrapper other = (DevicePolicyEventWrapper) obj;
        return mEventId == other.mEventId
                && mIntValue == other.mIntValue
                && mBooleanValue == other.mBooleanValue
                && mTimePeriodMs == other.mTimePeriodMs
                && Objects.equals(mAdminPackageName, other.mAdminPackageName)
                && Arrays.equals(mStringArrayValue, other.mStringArrayValue);
    }

    @Override
    public String toString() {
        return String.format(Locale.getDefault(), "{ eventId: %s(%d), intValue: %d, "
                        + "booleanValue: %s, timePeriodMs: %d, adminPackageName: %s, "
                        + "stringArrayValue: %s }",
                EventId.forNumber(mEventId), mEventId, mIntValue, mBooleanValue,
                mTimePeriodMs, mAdminPackageName, Arrays.toString(mStringArrayValue));
    }

    /**
     * Builder class for {@link DevicePolicyEventWrapper}.
     */
    public static class Builder {
        private final int mEventId;
        private int mIntValue;
        private boolean mBooleanValue;
        private long mTimePeriodMs;
        private String[] mStringArrayValue;
        // Default value for Strings when getting dump is "", not null.
        private String mAdminPackageName = "";

        public Builder(int eventId) {
            this.mEventId = eventId;
        }

        public Builder setInt(int value) {
            mIntValue = value;
            return this;
        }

        public Builder setBoolean(boolean value) {
            mBooleanValue = value;
            return this;
        }

        public Builder setTimePeriod(long timePeriodMs) {
            mTimePeriodMs = timePeriodMs;
            return this;
        }

        public Builder setStrings(String... values) {
            mStringArrayValue = values;
            return this;
        }

        public Builder setStrings(String value, String[] values) {
            if (values == null) {
                throw new IllegalArgumentException("values cannot be null.");
            }
            mStringArrayValue = new String[values.length + 1];
            mStringArrayValue[0] = value;
            System.arraycopy(values, 0, mStringArrayValue, 1, values.length);
            return this;
        }

        public Builder setStrings(String value1, String value2, String[] values) {
            if (values == null) {
                throw new IllegalArgumentException("values cannot be null.");
            }
            mStringArrayValue = new String[values.length + 2];
            mStringArrayValue[0] = value1;
            mStringArrayValue[1] = value2;
            System.arraycopy(values, 0, mStringArrayValue, 2, values.length);
            return this;
        }

        public Builder setAdminPackageName(String packageName) {
            mAdminPackageName = packageName;
            return this;
        }

        public DevicePolicyEventWrapper build() {
            return new DevicePolicyEventWrapper(this);
        }
    }
}
