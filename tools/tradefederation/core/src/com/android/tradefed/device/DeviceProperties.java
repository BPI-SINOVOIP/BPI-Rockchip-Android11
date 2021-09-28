/*
 * Copyright (C) 2017 The Android Open Source Project
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
package com.android.tradefed.device;

/**
 * Common constant definitions for device side property names
 */
public class DeviceProperties {

    /** property name for device board */
    public static final String BOARD = "ro.product.board";
    /** property name for device hardware */
    public static final String HARDWARE = "ro.hardware";
    /** proprty name to indicate device variant (e.g. flo vs dev) */
    public static final String VARIANT = "ro.product.vendor.device";
    /** Legacy O-MR1 property name to indicate device variant (e.g. flo vs dev) */
    public static final String VARIANT_LEGACY_O_MR1 = "ro.vendor.product.device";
    /** Legacy property name to indicate device variant (e.g. flo vs dev) */
    public static final String VARIANT_LEGACY_LESS_EQUAL_O = "ro.product.device";
    /** proprty name to indicate SDK version */
    public static final String SDK_VERSION = "ro.build.version.sdk";
    /** property name for device brand */
    public static final String BRAND = "ro.product.brand";
    /** property name for device product name */
    public static final String PRODUCT = "ro.product.name";
    /** property name for device release version, e.g. version 9 for Android Pie */
    public static final String RELEASE_VERSION = "ro.build.version.release";
    /** property name for device boot reason history */
    public static final String BOOT_REASON_HISTORY = "persist.sys.boot.reason.history";
    /** property name for the type of build */
    public static final String BUILD_TYPE = "ro.build.type";
    /** property name for the alias of the build name */
    public static final String BUILD_ALIAS = "ro.build.id";
    /** property name for the flavor of the device build */
    public static final String BUILD_FLAVOR = "ro.build.flavor";
    /** property name for whether or not the device is headless */
    public static final String BUILD_HEADLESS = "ro.build.headless";
    /** property name for the build id of the device */
    public static final String BUILD_ID = "ro.build.version.incremental";
    /** property name for the build codename of the device. Example: Q */
    public static final String BUILD_CODENAME = "ro.build.version.codename";
    /** property name for the build tags of the device */
    public static final String BUILD_TAGS = "ro.build.tags";
    /** property name for the SDK version that initially shipped on the device. */
    public static final String FIRST_API_LEVEL = "ro.product.first_api_level";
}
