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
 * limitations under the License.
 */

package com.android.tv.tuner.hdhomerun;

import android.os.Parcel;
import android.os.Parcelable;
import android.text.TextUtils;

/**
 * An HDHomeRun device detected on the network. This abstraction only contains network data
 * necessary to establish a connection with the device and does not represent a communication
 * channel with the device itself. Currently, we only support devices with HTTP streaming
 * functionality.
 */
public class HdHomeRunDevice implements Parcelable {
    private int mIpAddress;
    private int mDeviceType;
    private int mDeviceId;
    private int mTunerIndex;
    private String mDeviceModel;

    /**
     * Creates {@code HdHomeRunDevice} object from a parcel.
     *
     * @param parcel The parcel to create {@code HdHomeRunDevice} object from.
     */
    public HdHomeRunDevice(Parcel parcel) {
        mIpAddress = parcel.readInt();
        mDeviceType = parcel.readInt();
        mDeviceId = parcel.readInt();
        mTunerIndex = parcel.readInt();
        mDeviceModel = parcel.readString();
    }

    /**
     * Creates {@code HdHomeRunDevice} object from IP address, device type, device ID and tuner
     * index.
     *
     * @param ipAddress The IP address to create {@code HdHomeRunDevice} object from.
     * @param deviceType The device type to create {@code HdHomeRunDevice} object from.
     * @param deviceId The device ID to create {@code HdHomeRunDevice} object from.
     * @param tunerIndex The tuner index to {@code HdHomeRunDevice} object from.
     */
    public HdHomeRunDevice(
            int ipAddress, int deviceType, int deviceId, int tunerIndex, String deviceModel) {
        mIpAddress = ipAddress;
        mDeviceType = deviceType;
        mDeviceId = deviceId;
        mTunerIndex = tunerIndex;
        mDeviceModel = deviceModel;
    }

    /**
     * Returns the IP address.
     *
     * @return the IP address of this homerun device.
     */
    public int getIpAddress() {
        return mIpAddress;
    }

    /**
     * Returns the device type.
     *
     * @return the type of device for this homerun device.
     */
    public int getDeviceType() {
        return mDeviceType;
    }

    /**
     * Returns the device ID.
     *
     * @return the device ID of this homerun device.
     */
    public int getDeviceId() {
        return mDeviceId;
    }

    /**
     * Returns the tuner index.
     *
     * @return the tuner index of this homerun device.
     */
    public int getTunerIndex() {
        return mTunerIndex;
    }

    /**
     * Returns the device model.
     *
     * @return the device model of this homerun device.
     */
    public String getDeviceModel() {
        return mDeviceModel;
    }

    @Override
    public String toString() {
        String ipAddress =
                ""
                        + ((mIpAddress >>> 24) & 0xff)
                        + "."
                        + ((mIpAddress >>> 16) & 0xff)
                        + "."
                        + ((mIpAddress >>> 8) & 0xff)
                        + "."
                        + (mIpAddress & 0xff);
        return String.format("[%x-%d:%s]", mDeviceId, mTunerIndex, ipAddress);
    }

    @Override
    public int describeContents() {
        return 0;
    }

    @Override
    public void writeToParcel(Parcel out, int flags) {
        out.writeInt(mIpAddress);
        out.writeInt(mDeviceType);
        out.writeInt(mDeviceId);
        out.writeInt(mTunerIndex);
        out.writeString(mDeviceModel);
    }

    @Override
    public int hashCode() {
        int hash = 17;
        hash = hash * 31 + getIpAddress();
        hash = hash * 31 + getDeviceType();
        hash = hash * 31 + getDeviceId();
        hash = hash * 31 + getTunerIndex();
        return hash;
    }

    @Override
    public boolean equals(Object o) {
        if (!(o instanceof HdHomeRunDevice)) {
            return false;
        }
        HdHomeRunDevice rhs = (HdHomeRunDevice) o;
        return rhs != null
                && getIpAddress() == rhs.getIpAddress()
                && getDeviceType() == rhs.getDeviceType()
                && getDeviceId() == rhs.getDeviceId()
                && getTunerIndex() == rhs.getTunerIndex()
                && TextUtils.equals(getDeviceModel(), rhs.getDeviceModel());
    }

    public static final Parcelable.Creator<HdHomeRunDevice> CREATOR =
            new Parcelable.Creator<HdHomeRunDevice>() {

                @Override
                public HdHomeRunDevice createFromParcel(Parcel in) {
                    return new HdHomeRunDevice(in);
                }

                @Override
                public HdHomeRunDevice[] newArray(int size) {
                    return new HdHomeRunDevice[size];
                }
            };
}
