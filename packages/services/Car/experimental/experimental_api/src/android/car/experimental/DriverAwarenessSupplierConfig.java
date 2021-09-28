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

import android.os.Parcel;
import android.os.Parcelable;

/**
 * Configuration for an instance of {@link android.car.experimental.IDriverAwarenessSupplier}.
 */
public class DriverAwarenessSupplierConfig implements Parcelable {

    private final long mMaxStalenessMillis;

    /**
     * Creator for {@link Parcelable}.
     */
    public static final Parcelable.Creator<DriverAwarenessSupplierConfig> CREATOR =
            new Parcelable.Creator<DriverAwarenessSupplierConfig>() {
                public DriverAwarenessSupplierConfig createFromParcel(Parcel in) {
                    return new DriverAwarenessSupplierConfig(in);
                }

                public DriverAwarenessSupplierConfig[] newArray(int size) {
                    return new DriverAwarenessSupplierConfig[size];
                }
            };

    /**
     * Creates an instance of a {@link DriverAwarenessSupplierConfig}.
     */
    public DriverAwarenessSupplierConfig(long maxStalenessMillis) {
        mMaxStalenessMillis = maxStalenessMillis;
    }

    /**
     * Parcelable constructor.
     */
    private DriverAwarenessSupplierConfig(Parcel in) {
        mMaxStalenessMillis = in.readLong();
    }

    /**
     * Returns the duration in milliseconds after which the input from this supplier should be
     * considered stale.
     *
     * <p>Data should be sent more frequently than the staleness rate defined here.
     */
    public long getMaxStalenessMillis() {
        return mMaxStalenessMillis;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeLong(mMaxStalenessMillis);
    }


    @Override
    public String toString() {
        return String.format("DriverAwarenessEvent{mMaxStalenessMillis=%s}", mMaxStalenessMillis);
    }
}
