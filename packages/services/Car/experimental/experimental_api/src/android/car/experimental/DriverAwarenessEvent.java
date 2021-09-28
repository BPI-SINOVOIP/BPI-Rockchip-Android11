/*
 * Copyright (C) 2019 The Android Open Source Project
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

import java.util.Objects;

/**
 * Event about a driver's awareness level at a certain point in time.
 *
 * <p>Driver Awareness is an abstract concept based on a driver's cognitive situational awareness
 * of the environment around them. This metric can be approximated based on signals about the
 * driver's behavior, such as where they are looking or how much their interact with the headunit
 * in the car.
 *
 * <p>Furthermore, what constitutes the boundaries of no awareness and full awareness must be based
 * on the UX Research through real-world studies and driving simulation. It is the responsibility
 * of {@link DriverAwarenessSupplier}s to understand how their sensor input fits with current
 * research in order to determine the appropriate awareness value.
 *
 * @hide
 */
public final class DriverAwarenessEvent implements Parcelable {

    private final long mTimeStamp;

    @FloatRange(from = 0.0f, to = 1.0f)
    private final float mAwarenessValue;

    /**
     * Creator for {@link Parcelable}.
     */
    public static final Parcelable.Creator<DriverAwarenessEvent> CREATOR =
            new Parcelable.Creator<DriverAwarenessEvent>() {
                public DriverAwarenessEvent createFromParcel(Parcel in) {
                    return new DriverAwarenessEvent(in);
                }

                public DriverAwarenessEvent[] newArray(int size) {
                    return new DriverAwarenessEvent[size];
                }
            };

    /**
     * Creates an instance of a {@link DriverAwarenessEvent}.
     *
     * @param timeStamp      the time that the awareness value was sampled, in milliseconds elapsed
     *                       since system boot
     * @param awarenessValue the driver's awareness level at this point in time, ranging from 0, no
     *                       awareness, to 1, full awareness
     */
    public DriverAwarenessEvent(long timeStamp,
            @FloatRange(from = 0.0f, to = 1.0f) float awarenessValue) {
        mTimeStamp = timeStamp;
        mAwarenessValue = awarenessValue;
    }

    /**
     * Parcelable constructor.
     */
    private DriverAwarenessEvent(Parcel in) {
        mTimeStamp = in.readLong();
        mAwarenessValue = in.readFloat();
    }

    /**
     * Returns the time at which this driver awareness value was inferred based on the car's
     * sensors. It is the elapsed time in milliseconds since system boot.
     */
    public long getTimeStamp() {
        return mTimeStamp;
    }

    /**
     * The current driver awareness value, where 0 is no awareness and 1 is full awareness.
     */
    @FloatRange(from = 0.0f, to = 1.0f)
    public float getAwarenessValue() {
        return mAwarenessValue;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(mTimeStamp);
        dest.writeFloat(mAwarenessValue);
    }

    @Override
    public boolean equals(Object o) {
        if (this == o) {
            return true;
        }
        if (!(o instanceof DriverAwarenessEvent)) {
            return false;
        }
        DriverAwarenessEvent that = (DriverAwarenessEvent) o;
        return mTimeStamp == that.mTimeStamp
                && Float.compare(that.mAwarenessValue, mAwarenessValue) == 0;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mTimeStamp, mAwarenessValue);
    }

    @Override
    public String toString() {
        return String.format("DriverAwarenessEvent{timeStamp=%s, awarenessValue=%s}",
                mTimeStamp, mAwarenessValue);
    }
}
