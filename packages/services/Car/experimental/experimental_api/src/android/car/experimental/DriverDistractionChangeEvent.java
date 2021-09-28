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

package android.car.experimental;

import android.annotation.FloatRange;
import android.os.Parcel;
import android.os.Parcelable;

import com.android.internal.util.Preconditions;

import java.util.Objects;

/**
 * Event about a change in the driver's distraction.
 *
 * @hide
 */
public class DriverDistractionChangeEvent implements Parcelable {

    private final long mElapsedRealtimeTimestamp;

    @FloatRange(from = 0.0f, to = 1.0f)
    private final float mAwarenessPercentage;

    /**
     * Creator for {@link Parcelable}.
     */
    public static final Parcelable.Creator<DriverDistractionChangeEvent> CREATOR =
            new Parcelable.Creator<DriverDistractionChangeEvent>() {
                public DriverDistractionChangeEvent createFromParcel(Parcel in) {
                    return new DriverDistractionChangeEvent(in);
                }

                public DriverDistractionChangeEvent[] newArray(int size) {
                    return new DriverDistractionChangeEvent[size];
                }
            };


    private DriverDistractionChangeEvent(Builder builder) {
        mElapsedRealtimeTimestamp = builder.mElapsedRealtimeTimestamp;
        mAwarenessPercentage = builder.mAwarenessPercentage;
    }

    /**
     * Parcelable constructor.
     */
    private DriverDistractionChangeEvent(Parcel in) {
        mElapsedRealtimeTimestamp = in.readLong();
        mAwarenessPercentage = in.readFloat();
    }

    /**
     * Returns the timestamp for the change event, in milliseconds since boot, including time spent
     * in sleep.
     */
    public long getElapsedRealtimeTimestamp() {
        return mElapsedRealtimeTimestamp;
    }

    /**
     * Returns the current driver driver awareness value as a percentage of the required awareness
     * of the situation.
     * <ul>
     * <li>0% indicates that the driver has no situational awareness of the surrounding environment.
     * <li>100% indicates that the driver has the target amount of situational awareness
     * necessary for the current driving environment.
     * </ul>
     *
     * <p>If the required awareness of the current driving environment is 0, such as when the car
     * is in park, then the awareness percentage will be 100%.
     *
     * <p>Since this is a percentage, 0% will always correspond with a 0.0 driver awareness level.
     * However, the rest of the range will get squeezed or compressed in time depending on the
     * required awareness of the situation.
     *
     * <p>For example, suppose that driver awareness can go from 1.0 to 0.0 with 6 seconds of
     * eyes-off-road time and that the driver has just started looking off-road. Now consider
     * these two scenarios:
     * <ol>
     * <li>Scenario 1: The required awareness of the driving environment is 1.0:
     * After 0 seconds: 100% awareness
     * After 3 seconds: 50% awareness
     * After 4.5 seconds: 25% awareness
     * After 6 seconds: 0% awareness
     * <li>Scenario 2: The required awareness of the driving environment is 0.5:
     * After 0 seconds: 100% awareness
     * After 3 seconds: 100% awareness
     * After 4.5 seconds: 50% awareness
     * After 6 seconds: 0% awareness
     * </ol>
     */
    @FloatRange(from = 0.0f, to = 1.0f)
    public float getAwarenessPercentage() {
        return mAwarenessPercentage;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(mElapsedRealtimeTimestamp);
        dest.writeFloat(mAwarenessPercentage);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof DriverDistractionChangeEvent)) {
            return false;
        }
        DriverDistractionChangeEvent that = (DriverDistractionChangeEvent) o;
        return mElapsedRealtimeTimestamp == that.mElapsedRealtimeTimestamp
                && Float.compare(that.mAwarenessPercentage, mAwarenessPercentage) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mElapsedRealtimeTimestamp, mAwarenessPercentage);
    }

    @Override
    public String toString() {
        return String.format(
                "DriverDistractionChangeEvent{mElapsedRealtimeTimestamp=%s, "
                        + "mAwarenessPercentage=%s}",
                mElapsedRealtimeTimestamp, mAwarenessPercentage);
    }


    /**
     * Builder for {@link DriverDistractionChangeEvent}.
     */
    public static class Builder {
        private long mElapsedRealtimeTimestamp;

        @FloatRange(from = 0.0f, to = 1.0f)
        private float mAwarenessPercentage;

        /**
         * Set the timestamp for the change event, in milliseconds since boot, including time spent
         * in sleep.
         */
        public Builder setElapsedRealtimeTimestamp(long timestamp) {
            mElapsedRealtimeTimestamp = timestamp;
            return this;
        }

        /**
         * Set the current driver driver awareness value as a percentage of the required awareness
         * of the situation.
         */
        public Builder setAwarenessPercentage(
                @FloatRange(from = 0.0f, to = 1.0f) float awarenessPercentage) {
            Preconditions.checkArgument(
                    awarenessPercentage >= 0 && awarenessPercentage <= 1,
                    "awarenessPercentage must be between 0 and 1");
            mAwarenessPercentage = awarenessPercentage;
            return this;
        }

        /**
         * Build and return the {@link DriverDistractionChangeEvent} object.
         */
        public DriverDistractionChangeEvent build() {
            return new DriverDistractionChangeEvent(this);
        }

    }

}
