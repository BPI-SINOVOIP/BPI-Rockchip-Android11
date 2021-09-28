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

package com.android.cts.deviceandprofileowner;

import static android.Manifest.permission.READ_CONTACTS;
import static android.app.admin.DevicePolicyManager.KEYGUARD_DISABLE_FEATURES_NONE;
import static android.app.admin.DevicePolicyManager.KEYGUARD_DISABLE_FINGERPRINT;
import static android.app.admin.DevicePolicyManager.KEYGUARD_DISABLE_SECURE_CAMERA;
import static android.app.admin.DevicePolicyManager.KEYGUARD_DISABLE_TRUST_AGENTS;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED;
import static android.app.admin.DevicePolicyManager.PERMISSION_GRANT_STATE_DEFAULT;
import static android.app.admin.DevicePolicyManager.PERMISSION_GRANT_STATE_DENIED;
import static android.app.admin.DevicePolicyManager.PERMISSION_GRANT_STATE_GRANTED;
import static android.app.admin.DevicePolicyManager.PERMISSION_POLICY_AUTO_DENY;
import static android.app.admin.DevicePolicyManager.PERMISSION_POLICY_AUTO_GRANT;
import static android.app.admin.DevicePolicyManager.PERMISSION_POLICY_PROMPT;
import static android.provider.Settings.Secure.DEFAULT_INPUT_METHOD;
import static android.provider.Settings.Secure.SKIP_FIRST_USE_HINTS;

import android.app.admin.DevicePolicyManager;
import android.content.ContentResolver;
import android.content.Intent;
import android.os.UserManager;
import android.provider.Settings;

import androidx.test.InstrumentationRegistry;

/**
 * Invocations of {@link android.app.admin.DevicePolicyManager} methods which are either not CTS
 * tested or the CTS tests call too many other methods. Used to verify that the relevant metrics
 * are logged. Note that the metrics verification is done on the host-side.
 */
public class DevicePolicyLoggingTest extends BaseDeviceAdminTest {

    public static final String PACKAGE_NAME = "com.android.cts.permissionapp";
    public static final String PARAM_APP_TO_ENABLE = "app_to_enable";

    public void testPasswordMethodsLogged() {
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.PASSWORD_QUALITY_COMPLEX);
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 13);
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 14);
        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, 15);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 16);
        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, 17);
        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, 18);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 19);
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_UNSPECIFIED);
    }

    public void testLockNowLogged() {
        mDevicePolicyManager.lockNow(0);
    }

    public void testSetKeyguardDisabledFeaturesLogged() {
        mDevicePolicyManager.setKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT, KEYGUARD_DISABLE_FEATURES_NONE);
        mDevicePolicyManager.setKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT, KEYGUARD_DISABLE_FINGERPRINT);
        mDevicePolicyManager.setKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT, KEYGUARD_DISABLE_TRUST_AGENTS);
        mDevicePolicyManager.setKeyguardDisabledFeatures(ADMIN_RECEIVER_COMPONENT,
                KEYGUARD_DISABLE_FEATURES_NONE);
    }

    public void testSetKeyguardDisabledSecureCameraLogged() {
        mDevicePolicyManager.setKeyguardDisabledFeatures(
                ADMIN_RECEIVER_COMPONENT, KEYGUARD_DISABLE_SECURE_CAMERA);
    }

    public void testSetUserRestrictionLogged() {
        mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONFIG_LOCATION);
        mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONFIG_LOCATION);

        mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_ADJUST_VOLUME);
        mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_ADJUST_VOLUME);

        mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_AUTOFILL);
        mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_AUTOFILL);

        mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONTENT_CAPTURE);
        mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONTENT_CAPTURE);

        mDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONTENT_SUGGESTIONS);
        mDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONTENT_SUGGESTIONS);
    }

    public void testSetSecureSettingLogged()
            throws InterruptedException {
        final ContentResolver contentResolver = mContext.getContentResolver();
        final int skipFirstUseHintsInitial =
                Settings.Secure.getInt(contentResolver, SKIP_FIRST_USE_HINTS, 0);
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                SKIP_FIRST_USE_HINTS, "1");
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                SKIP_FIRST_USE_HINTS, "0");
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                SKIP_FIRST_USE_HINTS, String.valueOf(skipFirstUseHintsInitial));

        final String defaultInputMethodInitial =
                Settings.Secure.getString(contentResolver, DEFAULT_INPUT_METHOD);
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                DEFAULT_INPUT_METHOD, "com.example.input");
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                DEFAULT_INPUT_METHOD, null);
        mDevicePolicyManager.setSecureSetting(ADMIN_RECEIVER_COMPONENT,
                DEFAULT_INPUT_METHOD, defaultInputMethodInitial);
    }

    public void testSetPermissionPolicyLogged() {
        mDevicePolicyManager.setPermissionPolicy(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_POLICY_AUTO_DENY);
        mDevicePolicyManager.setPermissionPolicy(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_POLICY_AUTO_GRANT);
        mDevicePolicyManager.setPermissionPolicy(ADMIN_RECEIVER_COMPONENT,
                PERMISSION_POLICY_PROMPT);
    }

    public void testSetPermissionGrantStateLogged() throws InterruptedException {
        mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT,
                PACKAGE_NAME, READ_CONTACTS, PERMISSION_GRANT_STATE_GRANTED);
        mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT, PACKAGE_NAME,
                READ_CONTACTS, PERMISSION_GRANT_STATE_DENIED);
        mDevicePolicyManager.setPermissionGrantState(ADMIN_RECEIVER_COMPONENT, PACKAGE_NAME,
                READ_CONTACTS, PERMISSION_GRANT_STATE_DEFAULT);
    }

    public void testSetAutoTimeRequired() {
        final boolean initialValue = mDevicePolicyManager.getAutoTimeRequired();
        mDevicePolicyManager.setAutoTimeRequired(ADMIN_RECEIVER_COMPONENT, true);
        mDevicePolicyManager.setAutoTimeRequired(ADMIN_RECEIVER_COMPONENT, false);
        mDevicePolicyManager.setAutoTimeRequired(ADMIN_RECEIVER_COMPONENT, initialValue);
    }

    public void testSetAutoTimeEnabled() {
        final boolean initialValue = mDevicePolicyManager.getAutoTimeEnabled(
                ADMIN_RECEIVER_COMPONENT);
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, true);
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, false);
        mDevicePolicyManager.setAutoTimeEnabled(ADMIN_RECEIVER_COMPONENT, initialValue);
    }

    public void testEnableSystemAppLogged() {
        final String systemPackageToEnable =
                InstrumentationRegistry.getArguments().getString(PARAM_APP_TO_ENABLE);
        mDevicePolicyManager.enableSystemApp(ADMIN_RECEIVER_COMPONENT, systemPackageToEnable);
    }

    public void testEnableSystemAppWithIntentLogged() {
        final String systemPackageToEnable =
                InstrumentationRegistry.getArguments().getString(PARAM_APP_TO_ENABLE);
        final Intent intent =
                mContext.getPackageManager().getLaunchIntentForPackage(systemPackageToEnable);
        mDevicePolicyManager.enableSystemApp(ADMIN_RECEIVER_COMPONENT, intent);
    }

    public void testSetUninstallBlockedLogged() {
        mDevicePolicyManager.setUninstallBlocked(ADMIN_RECEIVER_COMPONENT, PACKAGE_NAME, true);
        mDevicePolicyManager.setUninstallBlocked(ADMIN_RECEIVER_COMPONENT, PACKAGE_NAME, false);
    }

    public void testDisallowAdjustVolumeMutedLogged() {
        final boolean initialValue =
                mDevicePolicyManager.isMasterVolumeMuted(ADMIN_RECEIVER_COMPONENT);
        mDevicePolicyManager.setMasterVolumeMuted(ADMIN_RECEIVER_COMPONENT, false);
        mDevicePolicyManager.setMasterVolumeMuted(ADMIN_RECEIVER_COMPONENT, true);
        mDevicePolicyManager.setMasterVolumeMuted(ADMIN_RECEIVER_COMPONENT, initialValue);
    }

    public void testSetPersonalAppsSuspendedLogged() {
        mDevicePolicyManager.setPersonalAppsSuspended(ADMIN_RECEIVER_COMPONENT, true);
        mDevicePolicyManager.setPersonalAppsSuspended(ADMIN_RECEIVER_COMPONENT, false);
    }
}
