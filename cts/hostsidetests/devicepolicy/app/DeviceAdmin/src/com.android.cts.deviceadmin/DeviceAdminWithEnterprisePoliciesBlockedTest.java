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
package com.android.cts.deviceadmin;

import android.app.admin.DevicePolicyManager;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;

/**
 * Device admin device side tests with enterprise policies disallowed.
 */
public class DeviceAdminWithEnterprisePoliciesBlockedTest extends DeviceAdminTest {
    public void testCameraDisabled() {
        boolean originalValue = dpm.getCameraDisabled(mAdminComponent);
        assertSecurityException(() -> dpm.setCameraDisabled(mAdminComponent, true));
        assertEquals(originalValue, dpm.getCameraDisabled(mAdminComponent));
    }

    public void testKeyguardDisabledFeatures() {
        int originalValue = dpm.getKeyguardDisabledFeatures(mAdminComponent);
        assertSecurityException(() -> dpm.setKeyguardDisabledFeatures(mAdminComponent,
            DevicePolicyManager.KEYGUARD_DISABLE_FEATURES_ALL));
        assertEquals(originalValue, dpm.getKeyguardDisabledFeatures(mAdminComponent));
    }

    public void testIsActivePasswordSufficient() {
        assertSecurityException(() -> dpm.isActivePasswordSufficient());
    }

    @Override
    public void testPasswordHistoryLength() {
        if (!mHasSecureLockScreen) {
            return;
        }
        int originalValue = dpm.getPasswordHistoryLength(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordHistoryLength(mAdminComponent, 3));
        assertEquals(originalValue, dpm.getPasswordHistoryLength(mAdminComponent));
    }

    public void testPasswordMinimumLength() {
        int originalValue = dpm.getPasswordMinimumLength(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumLength(mAdminComponent, 4));
        assertEquals(originalValue, dpm.getPasswordMinimumLength(mAdminComponent));
    }

    public void testPasswordMinimumLetters() {
        int originalValue = dpm.getPasswordMinimumLetters(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumLetters(mAdminComponent, 3));
        assertEquals(originalValue, dpm.getPasswordMinimumLetters(mAdminComponent));
    }

    public void testPasswordMinimumLowerCase() {
        int originalValue = dpm.getPasswordMinimumLowerCase(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumLowerCase(mAdminComponent, 3));
        assertEquals(originalValue, dpm.getPasswordMinimumLowerCase(mAdminComponent));
    }

    public void testPasswordMinimumNonLetter() {
        int originalValue = dpm.getPasswordMinimumNonLetter(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumNonLetter(mAdminComponent, 3));
        assertEquals(originalValue, dpm.getPasswordMinimumNonLetter(mAdminComponent));
    }

    public void testPasswordMinimumNumeric() {
        int originalValue = dpm.getPasswordMinimumNumeric(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumNumeric(mAdminComponent, 2));
        assertEquals(originalValue, dpm.getPasswordMinimumNumeric(mAdminComponent));
    }

    public void testPasswordMinimumSymbols() {
        int originalValue = dpm.getPasswordMinimumSymbols(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumSymbols(mAdminComponent, 2));
        assertEquals(originalValue, dpm.getPasswordMinimumSymbols(mAdminComponent));
    }

    public void testPasswordMinimumUpperCase() {
        int originalValue = dpm.getPasswordMinimumUpperCase(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordMinimumUpperCase(mAdminComponent, 3));
        assertEquals(originalValue, dpm.getPasswordMinimumUpperCase(mAdminComponent));
    }

    public void testPasswordQuality() {
        int originalValue = dpm.getPasswordQuality(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordQuality(mAdminComponent,
            DevicePolicyManager.PASSWORD_QUALITY_ALPHABETIC));
        assertEquals(originalValue, dpm.getPasswordQuality(mAdminComponent));
    }

    @Override
    public void testPasswordExpirationTimeout() {
        if (!mHasSecureLockScreen) {
            return;
        }
        long originalValue = dpm.getPasswordExpirationTimeout(mAdminComponent);
        assertSecurityException(() -> dpm.setPasswordExpirationTimeout(mAdminComponent, 1234L));
        assertEquals(originalValue, dpm.getPasswordExpirationTimeout(mAdminComponent));
    }

    private void assertSecurityException(Runnable r) {
        boolean securityExceptionThrown = false;
        try {
            r.run();
        } catch (SecurityException e) {
            securityExceptionThrown = true;
        }

        assertTrue("Expected SecurityException was not thrown", securityExceptionThrown);
    }
}
