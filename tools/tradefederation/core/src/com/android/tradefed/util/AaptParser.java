/*
 * Copyright (C) 2012 The Android Open Source Project
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
package com.android.tradefed.util;

import com.android.tradefed.log.LogUtil.CLog;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Class that extracts info from apk by parsing output of 'aapt dump badging'.
 * <p/>
 * aapt must be on PATH
 */
public class AaptParser {
    private static final Pattern PKG_PATTERN = Pattern.compile(
            "^package:\\s+name='(.*?)'\\s+versionCode='(\\d*)'\\s+versionName='(.*?)'.*$",
            Pattern.MULTILINE);
    private static final Pattern LABEL_PATTERN = Pattern.compile(
            "^application-label:'(.+?)'.*$",
            Pattern.MULTILINE);
    private static final Pattern SDK_PATTERN = Pattern.compile(
            "^sdkVersion:'(\\d+)'", Pattern.MULTILINE);
    private static final Pattern TARGET_SDK_PATTERN =
            Pattern.compile("^targetSdkVersion:'(\\d+)'", Pattern.MULTILINE);
    /** Patterns for native code are not always present, so the list may stay empty. */
    private static final Pattern NATIVE_CODE_PATTERN =
            Pattern.compile("native-code: '(.*?)'( '.*?')*");

    private static final Pattern REQUEST_LEGACY_STORAGE_PATTERN =
            Pattern.compile("requestLegacyExternalStorage.*=\\(.*\\)(.*)", Pattern.MULTILINE);

    private static final Pattern ALT_NATIVE_CODE_PATTERN =
            Pattern.compile("alt-native-code: '(.*)'");
    private static final Pattern USES_PERMISSION_MANAGE_EXTERNAL_STORAGE_PATTERN =
            Pattern.compile(
                    "uses-permission: name='android\\.permission\\.MANAGE_EXTERNAL_STORAGE'");
    private static final int AAPT_TIMEOUT_MS = 60000;
    private static final int INVALID_SDK = -1;

    private String mPackageName;
    private String mVersionCode;
    private String mVersionName;
    private List<String> mNativeCode = new ArrayList<>();
    private String mLabel;
    private int mSdkVersion = INVALID_SDK;
    private int mTargetSdkVersion = 10000;
    private boolean mRequestLegacyStorage = false;
    private boolean mUsesPermissionManageExternalStorage;

    // @VisibleForTesting
    AaptParser() {
    }

    boolean parse(String aaptOut) {
        //CLog.e(aaptOut);
        Matcher m = PKG_PATTERN.matcher(aaptOut);
        if (m.find()) {
            mPackageName = m.group(1);
            mLabel = mPackageName;
            mVersionCode = m.group(2);
            mVersionName = m.group(3);
            m = LABEL_PATTERN.matcher(aaptOut);
            if (m.find()) {
                mLabel = m.group(1);
            }
            m = SDK_PATTERN.matcher(aaptOut);
            if (m.find()) {
                mSdkVersion = Integer.parseInt(m.group(1));
            }
            m = TARGET_SDK_PATTERN.matcher(aaptOut);
            if (m.find()) {
                mTargetSdkVersion = Integer.parseInt(m.group(1));
            }
            m = NATIVE_CODE_PATTERN.matcher(aaptOut);
            if (m.find()) {
                for (int i = 1; i <= m.groupCount(); i++) {
                    if (m.group(i) != null) {
                        mNativeCode.add(m.group(i).replace("'", "").trim());
                    }
                }
            }
            m = ALT_NATIVE_CODE_PATTERN.matcher(aaptOut);
            if (m.find()) {
                for (int i = 1; i <= m.groupCount(); i++) {
                    if (m.group(i) != null) {
                        mNativeCode.add(m.group(i).replace("'", ""));
                    }
                }
            }
            m = USES_PERMISSION_MANAGE_EXTERNAL_STORAGE_PATTERN.matcher(aaptOut);
            mUsesPermissionManageExternalStorage = m.find();
            return true;
        }
        CLog.e("Failed to parse package and version info from 'aapt dump badging'. stdout: '%s'",
                aaptOut);
        return false;
    }

    boolean parseXmlTree(String aaptOut) {
        Matcher m = REQUEST_LEGACY_STORAGE_PATTERN.matcher(aaptOut);
        if (m.find()) {
            // 0xffffffff is true and 0x0 is false
            mRequestLegacyStorage = m.group(1).equals("0xffffffff");
        }
        // REQUEST_LEGACY_STORAGE_PATTERN may or may not be present
        return true;
    }

    /**
     * Parse info from the apk.
     *
     * @param apkFile the apk file
     * @return the {@link AaptParser} or <code>null</code> if failed to extract the information
     */
    public static AaptParser parse(File apkFile) {
        CommandResult result =
                RunUtil.getDefault()
                        .runTimedCmdRetry(
                                AAPT_TIMEOUT_MS,
                                0L,
                                2,
                                "aapt",
                                "dump",
                                "badging",
                                apkFile.getAbsolutePath());

        String stderr = result.getStderr();
        if (stderr != null && !stderr.isEmpty()) {
            CLog.e("aapt dump badging stderr: %s", stderr);
        }
        AaptParser p = new AaptParser();
        if (!CommandStatus.SUCCESS.equals(result.getStatus()) || !p.parse(result.getStdout())) {
            CLog.e(
                    "Failed to run aapt on %s. stdout: %s",
                    apkFile.getAbsoluteFile(), result.getStdout());
            return null;
        }
        result =
                RunUtil.getDefault()
                        .runTimedCmdRetry(
                                AAPT_TIMEOUT_MS,
                                0L,
                                2,
                                "aapt",
                                "dump",
                                "xmltree",
                                apkFile.getAbsolutePath(),
                                "AndroidManifest.xml");

        stderr = result.getStderr();
        if (stderr != null && !stderr.isEmpty()) {
            CLog.e("aapt dump xmltree AndroidManifest.xml stderr: %s", stderr);
        }

        if (!CommandStatus.SUCCESS.equals(result.getStatus())
                || !p.parseXmlTree(result.getStdout())) {
            CLog.e(
                    "Failed to run aapt on %s. stdout: %s",
                    apkFile.getAbsoluteFile(), result.getStdout());
            return null;
        }
        return p;
    }

    public String getPackageName() {
        return mPackageName;
    }

    public String getVersionCode() {
        return mVersionCode;
    }

    public String getVersionName() {
        return mVersionName;
    }

    public List<String> getNativeCode() {
        return mNativeCode;
    }

    public String getLabel() {
        return mLabel;
    }

    public int getSdkVersion() {
        return mSdkVersion;
    }

    public int getTargetSdkVersion() {
        return mTargetSdkVersion;
    }

    /**
     * Check if the app is requesting legacy storage.
     *
     * @return boolean return true if requestLegacyExternalStorage is true in AndroidManifest.xml
     */
    public boolean isRequestingLegacyStorage() {
        return mRequestLegacyStorage;
    }

    public boolean isUsingPermissionManageExternalStorage() {
        return mUsesPermissionManageExternalStorage;
    }
}
