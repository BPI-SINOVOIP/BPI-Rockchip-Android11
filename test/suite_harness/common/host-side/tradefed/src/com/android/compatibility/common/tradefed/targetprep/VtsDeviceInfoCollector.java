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

package com.android.compatibility.common.tradefed.targetprep;

import com.android.tradefed.build.IBuildInfo;
import com.android.tradefed.config.Option;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.targetprep.BuildError;
import com.android.tradefed.targetprep.ITargetPreparer;
import com.android.tradefed.targetprep.TargetSetupError;
import com.android.tradefed.util.ArrayUtil;

import com.google.api.client.util.Strings;

import java.util.HashMap;
import java.util.Map;
import java.util.Map.Entry;

/**
 * Collects device info. This's a fork of DeviceInfoCollector and is forked in order to simplify the
 * change deployment process and reduce the deployment time, which are critical for VTS services.
 */
public class VtsDeviceInfoCollector implements ITargetPreparer {
    // TODO(trong): remove "cts:" prefix, will need a custom ResultReporter.
    private static final Map<String, String> BUILD_KEYS = new HashMap<>();
    private static final Map<String, String> BUILD_LEGACY_PROPERTIES = new HashMap<>();

    // The name of a system property which tells whether to stop properly configured
    // native servers where properly configured means a server's init.rc is
    // configured to stop when that property's value is 1.
    static final String SYSPROP_VTS_NATIVE_SERVER = "vts.native_server.on";

    @Deprecated
    @Option(name = "disable-framework", description = "Initialize device by stopping framework.")
    private boolean mDisableFramework = false;

    static {
        BUILD_KEYS.put("cts:build_id", "ro.build.id");
        BUILD_KEYS.put("cts:build_product", "ro.product.name");
        BUILD_KEYS.put("cts:build_device", "ro.product.device");
        BUILD_KEYS.put("cts:build_board", "ro.product.board");
        BUILD_KEYS.put("cts:build_manufacturer", "ro.product.vendor.manufacturer");
        BUILD_KEYS.put("cts:build_brand", "ro.product.brand");
        BUILD_KEYS.put("cts:build_model", "ro.product.vendor.model");
        BUILD_KEYS.put("cts:build_type", "ro.build.type");
        BUILD_KEYS.put("cts:build_tags", "ro.build.tags");
        /**
         * build_fingerprint is used for certification; build_fingerprint_unaltered is for retry.
         */
        BUILD_KEYS.put("cts:build_fingerprint_unaltered", "ro.build.fingerprint");
        BUILD_KEYS.put("cts:build_abi", "ro.product.cpu.abi");
        BUILD_KEYS.put("cts:build_abi2", "ro.product.cpu.abi2");
        BUILD_KEYS.put("cts:build_abis", "ro.product.cpu.abilist");
        BUILD_KEYS.put("cts:build_abis_32", "ro.product.cpu.abilist32");
        BUILD_KEYS.put("cts:build_abis_64", "ro.product.cpu.abilist64");
        BUILD_KEYS.put("cts:build_first_api_level", "ro.product.first_api_level");
        BUILD_KEYS.put("cts:build_serial", "ro.serialno");
        BUILD_KEYS.put("cts:build_version_release", "ro.build.version.release");
        BUILD_KEYS.put("cts:build_version_sdk", "ro.build.version.sdk");
        BUILD_KEYS.put("cts:build_version_base_os", "ro.build.version.base_os");
        BUILD_KEYS.put("cts:build_version_security_patch", "ro.build.version.security_patch");
        BUILD_KEYS.put("cts:build_reference_fingerprint", "ro.build.reference.fingerprint");
        BUILD_KEYS.put("cts:build_system_fingerprint", "ro.build.fingerprint");
        BUILD_KEYS.put("cts:build_vendor_fingerprint", "ro.vendor.build.fingerprint");
        BUILD_KEYS.put("cts:build_vendor_manufacturer", "ro.product.vendor.manufacturer");
        BUILD_KEYS.put("cts:build_vendor_model", "ro.product.vendor.model");

        BUILD_LEGACY_PROPERTIES.put(
                "ro.product.vendor.manufacturer", "ro.vendor.product.manufacturer");
        BUILD_LEGACY_PROPERTIES.put("ro.product.vendor.model", "ro.vendor.product.model");
    }

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        IBuildInfo buildInfo = testInfo.getBuildInfo();
        for (Entry<String, String> entry : BUILD_KEYS.entrySet()) {
            String propertyValue = device.getProperty(entry.getValue());
            if ((propertyValue == null || propertyValue.length() == 0)
                    && BUILD_LEGACY_PROPERTIES.containsKey(entry.getValue())) {
                propertyValue = device.getProperty(BUILD_LEGACY_PROPERTIES.get(entry.getValue()));
            }
            buildInfo.addBuildAttribute(entry.getKey(), ArrayUtil.join(",", propertyValue));
        }

        // cts:build_fingerprint in the report consists of:
        // {build_brand}/{build_product}/{build_device}:{build_version_release}/{build_id}/
        // {build_version_incremental}:{build_type}/{build_tags}
        //
        // cts:build_version_incremental should be the same as the {build_version_incremental} in
        // cts:build_fingerprint. If ODM fingerprint exists, the value is
        // {ro.odm.build.version.incremental}. Otherwise, it is
        // {ro.vendor.build.version.incremental}.
        //
        // The {build_device} in {ro.odm.build.fingerprint} can be {ro.product.device} or
        // {ro.product.odm.device}_{ro.boot.product.hardware.sku}. The latter one is rewritten as
        // {ro.product.odm.device}.
        String buildFingerprint = device.getProperty("ro.odm.build.fingerprint");
        String buildVersionIncremental = null;
        if (!Strings.isNullOrEmpty(buildFingerprint)) {
            String[] splitBuildFingerprint = buildFingerprint.split("/");
            if (splitBuildFingerprint.length <= 2) {
                throw new TargetSetupError(
                        "Cannot parse ODM fingerprint: " + buildFingerprint,
                        device.getDeviceDescriptor());
            }
            // Need root to read that property
            buildVersionIncremental = device.getProperty("ro.odm.build.version.incremental");

            String odmDevice = device.getProperty("ro.product.odm.device");
            String sku = device.getProperty("ro.boot.product.hardware.sku");
            if (!Strings.isNullOrEmpty(odmDevice) && !Strings.isNullOrEmpty(sku)) {
                String odmDeviceAndSku = odmDevice + "_" + sku;
                if (splitBuildFingerprint[2].startsWith(odmDeviceAndSku + ":")) {
                    splitBuildFingerprint[2] =
                            odmDevice
                                    + splitBuildFingerprint[2].substring(odmDeviceAndSku.length());
                    buildFingerprint = String.join("/", splitBuildFingerprint);
                }
            }
        } else {
            buildFingerprint = device.getProperty("ro.vendor.build.fingerprint");
            // Need root to read that property
            buildVersionIncremental = device.getProperty("ro.vendor.build.version.incremental");
        }

        buildInfo.addBuildAttribute("cts:build_fingerprint", nullToEmpty(buildFingerprint));
        buildInfo.addBuildAttribute(
                "cts:build_version_incremental", nullToEmpty(buildVersionIncremental));
    }

    private static String nullToEmpty(String value) {
        return value == null ? "" : value;
    }
}
