/*
 * Copyright (C) 2013 The Android Open Source Project
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

package com.android.tradefed.command.remote;

import com.android.ddmlib.IDevice;
import com.android.ddmlib.IDevice.DeviceState;
import com.android.tradefed.device.DeviceAllocationState;
import com.android.tradefed.device.TestDeviceState;

import java.io.Serializable;

/** A class containing information describing a device under test. */
public class DeviceDescriptor implements Serializable {
    private static final long serialVersionUID = 1L;

    private final String mSerial;
    private final String mDisplaySerial;
    private final boolean mIsStubDevice;
    private final DeviceState mDeviceState; // Ddmlib updated state.
    private final DeviceAllocationState mState;
    private final TestDeviceState mTestDeviceState; // Tradefed updated state.
    private final String mProduct;
    private final String mProductVariant;
    private final String mSdkVersion;
    private final String mBuildId;
    private final String mBatteryLevel;
    private final String mDeviceClass;
    private final String mMacAddress;
    private final String mSimState;
    private final String mSimOperator;
    private final IDevice mIDevice;
    private final boolean mIsTemporary;

    public DeviceDescriptor() {
        this(null, false, null, null, null, null, null, null);
    }

    public DeviceDescriptor(String serial, boolean isStubDevice, DeviceAllocationState state,
            String product, String productVariant, String sdkVersion, String buildId,
            String batteryLevel) {
        this(serial, isStubDevice, state, product, productVariant, sdkVersion, buildId,
                batteryLevel, "", "", "", "");
    }

    public DeviceDescriptor(String serial, boolean isStubDevice, DeviceAllocationState state,
            String product, String productVariant, String sdkVersion, String buildId,
            String batteryLevel, String deviceClass, String macAddress, String simState,
            String simOperator) {
        this(
                serial,
                null,
                isStubDevice,
                null,
                state,
                null,
                product,
                productVariant,
                sdkVersion,
                buildId,
                batteryLevel,
                deviceClass,
                macAddress,
                simState,
                simOperator,
                false,
                null);
    }

    public DeviceDescriptor(
            String serial,
            boolean isStubDevice,
            DeviceAllocationState state,
            String product,
            String productVariant,
            String sdkVersion,
            String buildId,
            String batteryLevel,
            String deviceClass,
            String macAddress,
            String simState,
            String simOperator,
            IDevice idevice) {
        this(
                serial,
                null,
                isStubDevice,
                null,
                state,
                null,
                product,
                productVariant,
                sdkVersion,
                buildId,
                batteryLevel,
                deviceClass,
                macAddress,
                simState,
                simOperator,
                false,
                idevice);
    }

    public DeviceDescriptor(
            String serial,
            boolean isStubDevice,
            DeviceState deviceState,
            DeviceAllocationState state,
            String product,
            String productVariant,
            String sdkVersion,
            String buildId,
            String batteryLevel,
            String deviceClass,
            String macAddress,
            String simState,
            String simOperator,
            IDevice idevice) {
        this(
                serial,
                null,
                isStubDevice,
                deviceState,
                state,
                null,
                product,
                productVariant,
                sdkVersion,
                buildId,
                batteryLevel,
                deviceClass,
                macAddress,
                simState,
                simOperator,
                false,
                idevice);
    }

    public DeviceDescriptor(
            String serial,
            String displaySerial,
            boolean isStubDevice,
            DeviceState deviceState,
            DeviceAllocationState state,
            TestDeviceState testDeviceState,
            String product,
            String productVariant,
            String sdkVersion,
            String buildId,
            String batteryLevel,
            String deviceClass,
            String macAddress,
            String simState,
            String simOperator,
            boolean isTemporary,
            IDevice idevice) {
        mSerial = serial;
        mDisplaySerial = displaySerial;
        mIsStubDevice = isStubDevice;
        mDeviceState = deviceState;
        mTestDeviceState = testDeviceState;
        mState = state;
        mProduct = product;
        mProductVariant = productVariant;
        mSdkVersion = sdkVersion;
        mBuildId = buildId;
        mBatteryLevel = batteryLevel;
        mDeviceClass = deviceClass;
        mMacAddress = macAddress;
        mSimState = simState;
        mSimOperator = simOperator;
        mIsTemporary = isTemporary;
        mIDevice = idevice;
    }

    /** Used for easy state updating in ClusterDeviceMonitor. */
    public DeviceDescriptor(DeviceDescriptor d, DeviceAllocationState state) {
        this(
                d.getSerial(),
                d.getDisplaySerial(),
                d.isStubDevice(),
                d.getDeviceState(),
                state,
                d.getTestDeviceState(),
                d.getProduct(),
                d.getProductVariant(),
                d.getSdkVersion(),
                d.getBuildId(),
                d.getBatteryLevel(),
                d.getDeviceClass(),
                d.getMacAddress(),
                d.getSimState(),
                d.getSimOperator(),
                d.isTemporary(),
                d.getIDevice());
    }

    /** Used for easy state updating of serial for placeholder devices. */
    public DeviceDescriptor(DeviceDescriptor d, String serial, String displaySerial) {
        this(
                serial,
                displaySerial,
                d.isStubDevice(),
                d.getDeviceState(),
                d.getState(),
                d.getTestDeviceState(),
                d.getProduct(),
                d.getProductVariant(),
                d.getSdkVersion(),
                d.getBuildId(),
                d.getBatteryLevel(),
                d.getDeviceClass(),
                d.getMacAddress(),
                d.getSimState(),
                d.getSimOperator(),
                d.isTemporary(),
                d.getIDevice());
    }

    public String getSerial() {
        return mSerial;
    }

    public String getDisplaySerial() {
        return mDisplaySerial;
    }

    public boolean isStubDevice() {
        return mIsStubDevice;
    }

    public DeviceState getDeviceState() {
        return mDeviceState;
    }

    public DeviceAllocationState getState() {
        return mState;
    }

    public TestDeviceState getTestDeviceState() {
        return mTestDeviceState;
    }

    public String getProduct() {
        return mProduct;
    }

    public String getProductVariant() {
        return mProductVariant;
    }

    public String getDeviceClass() {
        return mDeviceClass;
    }

    /*
     * This version maps to the ro.build.version.sdk property.
     */
    public String getSdkVersion() {
        return mSdkVersion;
    }

    public String getBuildId() {
        return mBuildId;
    }

    public String getBatteryLevel() {
        return mBatteryLevel;
    }

    public String getMacAddress() {
        return mMacAddress;
    }

    public String getSimState() {
        return mSimState;
    }

    public String getSimOperator() {
        return mSimOperator;
    }

    /** Returns whether or not the device will be deleted at the end of the invocation. */
    public boolean isTemporary() {
        return mIsTemporary;
    }

    private IDevice getIDevice() {
        return mIDevice;
    }

    public String getProperty(String name) {
        if (mIDevice == null) {
            throw new UnsupportedOperationException("this descriptor does not have IDevice");
        }
        return mIDevice.getProperty(name);
    }

    /**
     * Provides a description with serials, product and build id
     */
    @Override
    public String toString() {
        return String.format("[%s %s:%s %s]", mSerial, mProduct, mProductVariant, mBuildId);
    }
}
