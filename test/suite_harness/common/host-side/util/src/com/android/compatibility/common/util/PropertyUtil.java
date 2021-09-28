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
package com.android.compatibility.common.util;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;

import java.util.HashMap;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Host-side utility class for reading properties and gathering information for testing
 * Android device compatibility.
 */
public class PropertyUtil {

    /**
     * Name of read-only property detailing the first API level for which the product was
     * shipped. Property should be undefined for factory ROM products.
     */
    public static final String FIRST_API_LEVEL = "ro.product.first_api_level";
    private static final String BUILD_TAGS_PROPERTY = "ro.build.tags";
    private static final String BUILD_TYPE_PROPERTY = "ro.build.type";
    private static final String MANUFACTURER_PROPERTY = "ro.product.manufacturer";
    private static final String TAG_DEV_KEYS = "dev-keys";
    private static final String VNDK_VERSION = "ro.vndk.version";

    /** Value to be returned by getPropertyInt() if property is not found */
    public static final int INT_VALUE_IF_UNSET = -1;

    public static final String GOOGLE_SETTINGS_QUERY =
            "content query --uri content://com.google.settings/partner";

    /** Returns whether the device build is a user build */
    public static boolean isUserBuild(ITestDevice device) throws DeviceNotAvailableException {
        return propertyEquals(device, BUILD_TYPE_PROPERTY, "user");
    }

    /** Returns whether this build is built with dev-keys */
    public static boolean isDevKeysBuild(ITestDevice device) throws DeviceNotAvailableException {
        String buildTags = device.getProperty(BUILD_TAGS_PROPERTY);
        for (String tag : buildTags.split(",")) {
            if (TAG_DEV_KEYS.equals(tag.trim())) {
                return true;
            }
        }
        return false;
    }

    /**
     * Return the first API level for this product. If the read-only property is unset,
     * this means the first API level is the current API level, and the current API level
     * is returned.
     */
    public static int getFirstApiLevel(ITestDevice device) throws DeviceNotAvailableException {
        String propString = device.getProperty(FIRST_API_LEVEL);
        return (propString == null) ? device.getApiLevel() : Integer.parseInt(propString);
    }

    /**
     * Return whether the SDK version of the vendor partiton is newer than the given API level.
     * If the property is set to non-integer value, this means the vendor partition is using
     * current API level and true is returned.
     */
    public static boolean isVendorApiLevelNewerThan(ITestDevice device, int apiLevel)
            throws DeviceNotAvailableException {
        int vendorApiLevel = getPropertyInt(device, VNDK_VERSION);
        if (vendorApiLevel == INT_VALUE_IF_UNSET) {
            return true;
        }
        return vendorApiLevel > apiLevel;
    }

    /**
     * Return the manufacturer of this product. If unset, return null.
     */
    public static String getManufacturer(ITestDevice device) throws DeviceNotAvailableException {
        return device.getProperty(MANUFACTURER_PROPERTY);
    }

    /** Returns a mapping from client ID names to client ID values */
    public static Map<String, String> getClientIds(ITestDevice device)
            throws DeviceNotAvailableException {
        Map<String,String> clientIds = new HashMap<>();
        String queryOutput = device.executeShellCommand(GOOGLE_SETTINGS_QUERY);
        for (String line : queryOutput.split("[\\r?\\n]+")) {
            // Expected line format: "Row: 1 _id=123, name=<property_name>, value=<property_value>"
            Pattern pattern = Pattern.compile("name=([a-z_]*), value=(.*)$");
            Matcher matcher = pattern.matcher(line);
            if (matcher.find()) {
                String name = matcher.group(1);
                String value = matcher.group(2);
                if (name.contains("client_id")) {
                    clientIds.put(name, value); // only add name-value pair for client ids
                }
            }
        }
        return clientIds;
    }

    /** Returns whether the property exists on this device */
    public static boolean propertyExists(ITestDevice device, String property)
            throws DeviceNotAvailableException {
        return device.getProperty(property) != null;
    }

    /** Returns whether the property value is equal to a given string */
    public static boolean propertyEquals(ITestDevice device, String property, String value)
            throws DeviceNotAvailableException {
        if (value == null) {
            return !propertyExists(device, property); // null value implies property does not exist
        }
        return value.equals(device.getProperty(property));
    }

    /**
     * Returns whether the property value matches a given regular expression. The method uses
     * String.matches(), requiring a complete match (i.e. expression matches entire value string)
     */
    public static boolean propertyMatches(ITestDevice device, String property, String regex)
            throws DeviceNotAvailableException {
        if (regex == null || regex.isEmpty()) {
            // null or empty pattern implies property does not exist
            return !propertyExists(device, property);
        }
        String value = device.getProperty(property);
        return (value == null) ? false : value.matches(regex);
    }

    /**
     * Retrieves the desired integer property, returning INT_VALUE_IF_UNSET if not found.
     */
    public static int getPropertyInt(ITestDevice device, String property)
            throws DeviceNotAvailableException {
        String value = device.getProperty(property);
        if (value == null) {
            return INT_VALUE_IF_UNSET;
        }
        try {
            return Integer.parseInt(value);
        } catch (NumberFormatException e) {
            return INT_VALUE_IF_UNSET;
        }
    }
}
