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

package com.android.car.companiondevicesupport.api.external;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.os.Parcel;
import android.os.Parcelable;

import java.util.Objects;

/** Representation of an associated device.  */
public final class AssociatedDevice implements Parcelable {

    private final String mDeviceId;

    private final String mDeviceAddress;

    private final String mDeviceName;

    private final boolean mIsConnectionEnabled;

    public AssociatedDevice(@NonNull String deviceId, @NonNull String deviceAddress,
            @Nullable String deviceName, boolean isConnectionEnabled) {
        mDeviceId = deviceId;
        mDeviceAddress = deviceAddress;
        mDeviceName = deviceName;
        mIsConnectionEnabled = isConnectionEnabled;
    }

    public AssociatedDevice(com.android.car.connecteddevice.model.AssociatedDevice device) {
        this(device.getDeviceId(), device.getDeviceAddress(), device.getDeviceName(),
                device.isConnectionEnabled());
    }

    private AssociatedDevice(Parcel in) {
        this(in.readString(), in.readString(), in.readString(), in.readBoolean());
    }

    @NonNull
    public String getDeviceId() {
        return mDeviceId;
    }

    @NonNull
    public String getDeviceAddress() {
        return mDeviceAddress;
    }

    @Nullable
    public String getDeviceName() {
        return mDeviceName;
    }

    public boolean isConnectionEnabled() {
        return mIsConnectionEnabled;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(mDeviceId);
        dest.writeString(mDeviceAddress);
        dest.writeString(mDeviceName);
        dest.writeBoolean(mIsConnectionEnabled);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }
        if (!(obj instanceof AssociatedDevice)) {
            return false;
        }
        AssociatedDevice device = (AssociatedDevice) obj;
        return Objects.equals(device.mDeviceId, mDeviceId)
                && Objects.equals(device.mDeviceAddress, mDeviceAddress)
                && Objects.equals(device.mDeviceName, mDeviceName)
                && device.mIsConnectionEnabled == mIsConnectionEnabled;
    }

    @Override
    public int hashCode() {
        return Objects.hash(mDeviceId, mDeviceAddress, mDeviceName, mIsConnectionEnabled);
    }

    public static final Parcelable.Creator<AssociatedDevice> CREATOR =
            new Parcelable.Creator<AssociatedDevice>() {
        @Override
        public AssociatedDevice createFromParcel(Parcel source) {
            return new AssociatedDevice(source);
        }

        @Override
        public AssociatedDevice[] newArray(int size) {
            return new AssociatedDevice[size];
        }
    };
}
