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
package com.android.compatibility.common.tradefed.targetprep;

import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BaseTargetPreparer;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.TargetSetupError;

/**
 * Special preparer used to check the build fingerprint of a device against an expected one.
 *
 * <p>An "unaltered" fingerprint might be available. Which reflects that the official fingerprint
 * has been modified for business reason, but we still want to validate the original device is the
 * same.
 */
public final class BuildFingerPrintPreparer extends BaseTargetPreparer {

    /**
     * These 3 options cannot really be injected directly, but are needed to be copied during retry
     * and sharding.
     */
    @Option(name = "expected-fingerprint")
    private String mExpectedFingerprint = null;

    @Option(name = "expected-vendor-fingerprint")
    private String mExpectedVendorFingerprint = null;

    @Option(name = "unaltered-fingerprint")
    private String mUnalteredFingerprint = null;

    private String mFingerprintProperty = "ro.build.fingerprint";
    private String mVendorFingerprintProperty = "ro.vendor.build.fingerprint";

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        if (mExpectedFingerprint == null) {
            throw new TargetSetupError("build fingerprint shouldn't be null",
                    device.getDeviceDescriptor());
        }
        try {
            String compare = mExpectedFingerprint;
            if (mUnalteredFingerprint != null) {
                compare = mUnalteredFingerprint;
            }
            String currentBuildFingerprint = device.getProperty(mFingerprintProperty);
            if (!compare.equals(currentBuildFingerprint)) {
                throw new TargetSetupError(
                        String.format(
                                "Device build fingerprint must match '%s'. Found '%s' instead.",
                                compare, currentBuildFingerprint),
                        device.getDeviceDescriptor());
            }
            if (mExpectedVendorFingerprint != null) {
                String currentBuildVendorFingerprint =
                        device.getProperty(mVendorFingerprintProperty);
                // Replace by empty string if null to do a proper comparison.
                if (currentBuildVendorFingerprint == null) {
                    currentBuildVendorFingerprint = "";
                }
                if (!mExpectedVendorFingerprint.equals(currentBuildVendorFingerprint)) {
                    throw new TargetSetupError(
                            String.format(
                                    "Device vendor build fingerprint must match '%s'. Found '%s' "
                                            + "instead.",
                                    mExpectedVendorFingerprint, currentBuildVendorFingerprint),
                            device.getDeviceDescriptor());
                }
            }
        } catch (DeviceNotAvailableException e) {
            throw new RuntimeException(e);
        }
    }

    /**
     * Sets the expected fingerprint we are checking against.
     */
    public void setExpectedFingerprint(String expectedFingerprint) {
        mExpectedFingerprint = expectedFingerprint;
    }

    /**
     * Returns the expected fingerprint.
     */
    public String getExpectedFingerprint() {
        return mExpectedFingerprint;
    }

    /**
     * Allow to override the base fingerprint property. In some cases, we want to check the
     * "ro.vendor.build.fingerpint" for example.
     */
    public void setFingerprintProperty(String property) {
        mFingerprintProperty = property;
    }

    /** Sets the unchanged original fingerprint. */
    public void setUnalteredFingerprint(String unalteredFingerprint) {
        mUnalteredFingerprint = unalteredFingerprint;
    }

    /** Set the property value associated with ro.vendor.build.fingerprint */
    public void setExpectedVendorFingerprint(String expectedVendorFingerprint) {
        mExpectedVendorFingerprint = expectedVendorFingerprint;
    }
}
