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

package com.android.car.companiondevicesupport.api.external;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.os.Parcel;
import android.os.Parcelable;

import com.android.car.connecteddevice.model.ConnectedDevice;

import java.util.Objects;

/** Representation of a connected device.  */
public final class CompanionDevice implements Parcelable {

    private final String mDeviceId;

    private final String mDeviceName;

    private final boolean mIsActiveUser;

    private final boolean mHasSecureChannel;

    public CompanionDevice(ConnectedDevice device) {
        this(device.getDeviceId(), device.getDeviceName(), device.isAssociatedWithActiveUser(),
                device.hasSecureChannel());
    }

    private CompanionDevice(Parcel in) {
        this(in.readString(), in.readString(), in.readBoolean(), in.readBoolean());
    }

    public CompanionDevice(@NonNull String deviceId, @Nullable String deviceName,
            boolean isActiveUser, boolean hasSecureChannel) {
        mDeviceId = deviceId;
        mDeviceName = deviceName;
        mIsActiveUser = isActiveUser;
        mHasSecureChannel = hasSecureChannel;
    }

    /** Returns the device's id. */
    @NonNull
    public String getDeviceId() {
        return mDeviceId;
    }

    /** Returns the device's name. */
    @Nullable
    public String getDeviceName() {
        return mDeviceName;
    }

    /** Returns whether this device belongs to the currently active user. */
    public boolean isActiveUser() {
        return mIsActiveUser;
    }

    /**
     * Returns whether a secure has been established with this device. Secure communication is only
     * available once this returns `true`.
     */
    public boolean hasSecureChannel() {
        return mHasSecureChannel;
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel dest, int flags) {
        dest.writeString(mDeviceId);
        dest.writeString(mDeviceName);
        dest.writeBoolean(mIsActiveUser);
        dest.writeBoolean(mHasSecureChannel);
    }

    @Override
    public boolean equals(Object obj) {
        if (obj == this) {
            return true;
        }
        if (!(obj instanceof CompanionDevice)) {
            return false;
        }
        CompanionDevice companionDevice = (CompanionDevice) obj;
        return Objects.equals(mDeviceId, companionDevice.getDeviceId())
                && Objects.equals(mDeviceName, companionDevice.getDeviceName())
                && mIsActiveUser == companionDevice.isActiveUser()
                && mHasSecureChannel == companionDevice.hasSecureChannel();
    }

    @Override
    public int hashCode() {
        return Objects.hash(mDeviceId, mDeviceName, mIsActiveUser, mHasSecureChannel);
    }

    @Override
    public String toString() {
        return "CompanionDevice{deviceId=" + mDeviceId
                + ", deviceName=" + mDeviceName
                + ", isActiveUser=" + mIsActiveUser
                + ", hasSecureChannel=" + mHasSecureChannel
                + "}";
    }

    /** Returns {@link ConnectedDevice} equivalent of this device. */
    @NonNull
    public ConnectedDevice toConnectedDevice() {
        return new ConnectedDevice(mDeviceId, mDeviceName, mIsActiveUser, mHasSecureChannel);
    }

    public static final Parcelable.Creator<CompanionDevice> CREATOR =
            new Parcelable.Creator<CompanionDevice>() {

        @Override
        public CompanionDevice createFromParcel(Parcel source) {
            return new CompanionDevice(source);
        }

        @Override
        public CompanionDevice[] newArray(int size) {
            return new CompanionDevice[size];
        }
    };
}
