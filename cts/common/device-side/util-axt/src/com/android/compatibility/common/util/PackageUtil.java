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

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.util.Log;

import androidx.test.InstrumentationRegistry;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Device-side utility class for PackageManager-related operations
 */
public class PackageUtil {

    private static final String TAG = PackageUtil.class.getSimpleName();

    private static final int SYSTEM_APP_MASK =
            ApplicationInfo.FLAG_SYSTEM | ApplicationInfo.FLAG_UPDATED_SYSTEM_APP;
    private static final char[] HEX_ARRAY = "0123456789ABCDEF".toCharArray();

    /** Returns true if a package with the given name exists on the device */
    public static boolean exists(String packageName) {
        try {
            return (getPackageManager().getApplicationInfo(packageName,
                    PackageManager.GET_META_DATA) != null);
        } catch(PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /** Returns true if a package with the given name AND SHA digest exists on the device */
    public static boolean exists(String packageName, String sha) {
        try {
            if (getPackageManager().getApplicationInfo(
                    packageName, PackageManager.GET_META_DATA) == null) {
                return false;
            }
            return sha.equals(computePackageSignatureDigest(packageName));
        } catch (NoSuchAlgorithmException | PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /** Returns true if the app for the given package name is a system app for this device */
    public static boolean isSystemApp(String packageName) {
        try {
            ApplicationInfo ai = getPackageManager().getApplicationInfo(packageName,
                    PackageManager.GET_META_DATA);
            return ai != null && ((ai.flags & SYSTEM_APP_MASK) != 0);
        } catch(PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /**
     * Returns true if the app for the given package name is a privileged system app for this
     * device
     */
    public static boolean isPrivilegedSystemApp(String packageName) {
        try {
            ApplicationInfo ai = getPackageManager().getApplicationInfo(packageName,
                    PackageManager.GET_META_DATA);
            return ai != null && ((ai.flags & SYSTEM_APP_MASK) != 0) && ai.isPrivilegedApp();
        } catch(PackageManager.NameNotFoundException e) {
            return false;
        }
    }

    /** Returns the version string of the package name, or null if the package can't be found */
    public static String getVersionString(String packageName) {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(packageName,
                    PackageManager.GET_META_DATA);
            return info.versionName;
        } catch (PackageManager.NameNotFoundException | NullPointerException e) {
            Log.w(TAG, "Could not find version string for package " + packageName);
            return null;
        }
    }

    /**
     * Returns the version code for the package name, or null if the package can't be found.
     * If before API Level 28, return a long version of the (otherwise deprecated) versionCode.
     */
    public static Long getLongVersionCode(String packageName) {
        try {
            PackageInfo info = getPackageManager().getPackageInfo(packageName,
                    PackageManager.GET_META_DATA);
            // Make no assumptions about the device's API level, and use the (now deprecated)
            // versionCode for older devices.
            return (ApiLevelUtil.isAtLeast(28)) ?
                    info.getLongVersionCode() : (long) info.versionCode;
        } catch (PackageManager.NameNotFoundException | NullPointerException e) {
            Log.w(TAG, "Could not find version string for package " + packageName);
            return null;
        }
    }

    /**
     * Compute the signature SHA digest for a package.
     * @param package the name of the package for which the signature SHA digest is requested
     * @return the signature SHA digest
     */
    public static String computePackageSignatureDigest(String packageName)
            throws NoSuchAlgorithmException, PackageManager.NameNotFoundException {
        PackageInfo packageInfo = getPackageManager()
                .getPackageInfo(packageName, PackageManager.GET_SIGNATURES);
        MessageDigest messageDigest = MessageDigest.getInstance("SHA256");
        messageDigest.update(packageInfo.signatures[0].toByteArray());

        final byte[] digest = messageDigest.digest();
        final int digestLength = digest.length;
        final int charCount = 3 * digestLength - 1;

        final char[] chars = new char[charCount];
        for (int i = 0; i < digestLength; i++) {
            final int byteHex = digest[i] & 0xFF;
            chars[i * 3] = HEX_ARRAY[byteHex >>> 4];
            chars[i * 3 + 1] = HEX_ARRAY[byteHex & 0x0F];
            if (i < digestLength - 1) {
                chars[i * 3 + 2] = ':';
            }
        }
        return new String(chars);
    }

    private static PackageManager getPackageManager() {
        return InstrumentationRegistry.getInstrumentation().getTargetContext().getPackageManager();
    }

    private static boolean hasDeviceFeature(final String requiredFeature) {
        return InstrumentationRegistry.getContext()
                .getPackageManager()
                .hasSystemFeature(requiredFeature);
    }

    /**
     * Rotation support is indicated by explicitly having both landscape and portrait
     * features or not listing either at all.
     */
    public static boolean supportsRotation() {
        final boolean supportsLandscape = hasDeviceFeature(PackageManager.FEATURE_SCREEN_LANDSCAPE);
        final boolean supportsPortrait = hasDeviceFeature(PackageManager.FEATURE_SCREEN_PORTRAIT);
        return (supportsLandscape && supportsPortrait)
                || (!supportsLandscape && !supportsPortrait);
    }
}
