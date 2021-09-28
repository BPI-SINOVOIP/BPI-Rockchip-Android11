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
package com.android.cts.deviceandprofileowner;

import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_ALPHABETIC;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_ALPHANUMERIC;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_COMPLEX;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_NUMERIC;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_SOMETHING;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED;

import android.app.KeyguardManager;
import android.app.admin.DevicePolicyManager;

import androidx.test.InstrumentationRegistry;

public class ResetPasswordWithTokenTest extends BaseDeviceAdminTest {

    private static final String SHORT_PASSWORD = "1234";
    private static final String COMPLEX_PASSWORD = "abc123.";

    private static final byte[] TOKEN0 = "abcdefghijklmnopqrstuvwxyz0123456789".getBytes();
    private static final byte[] TOKEN1 = "abcdefghijklmnopqrstuvwxyz012345678*".getBytes();

    private static final String ARG_ALLOW_FAILURE = "allowFailure";

    private boolean mShouldRun;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        Boolean allowFailure = Boolean.parseBoolean(InstrumentationRegistry.getArguments()
                .getString(ARG_ALLOW_FAILURE));
        mShouldRun = setUpResetPasswordToken(allowFailure);
    }

    @Override
    protected void tearDown() throws Exception {
        if (mShouldRun) {
            cleanUpResetPasswordToken();
        }
        resetComplexPasswordRestrictions();
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_UNSPECIFIED);
        super.tearDown();
    }

    public void testBadTokenShouldFail() {
        if (!mShouldRun) {
            return;
        }
        // resetting password with wrong token should fail
        assertFalse(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                SHORT_PASSWORD, TOKEN1, 0));
    }

    public void testChangePasswordWithToken() {
        if (!mShouldRun) {
            return;
        }
        // try changing password with token
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                SHORT_PASSWORD, TOKEN0, 0));

        // Set a strong password constraint and expect the sufficiency check to fail
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_NUMERIC);
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 6);
        assertPasswordSufficiency(false);

        // try changing to a stronger password and verify it satisfies requested constraint
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                COMPLEX_PASSWORD, TOKEN0, 0));
        assertPasswordSufficiency(true);
    }

    public void testResetPasswordFailIfQualityNotMet() {
        if (!mShouldRun) {
            return;
        }
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_NUMERIC);
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 6);

        assertFalse(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                SHORT_PASSWORD, TOKEN0, 0));

        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                COMPLEX_PASSWORD, TOKEN0, 0));
    }

    public void testPasswordMetricAfterResetPassword() {
        if (!mShouldRun) {
            return;
        }
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_COMPLEX);
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 1);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 1);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 0);
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                COMPLEX_PASSWORD, TOKEN0, 0));

        // Change required complexity and verify new password satisfies it
        // First set a slightly stronger requirement and expect password sufficiency is false
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 3);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 3);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 2);
        assertPasswordSufficiency(false);
        // Then sets the appropriate quality and verify it should pass
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 1);
        assertPasswordSufficiency(true);
    }

    public void testClearPasswordWithToken() {
        if (!mShouldRun) {
            return;
        }
        KeyguardManager km = mContext.getSystemService(KeyguardManager.class);
        // First set a password
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                SHORT_PASSWORD, TOKEN0, 0));
        assertTrue(km.isDeviceSecure());

        // clear password with token
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT, null,
                TOKEN0, 0));
        assertFalse(km.isDeviceSecure());
    }

    public void testPasswordQuality_something() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_SOMETHING);
        assertEquals(PASSWORD_QUALITY_SOMETHING,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        String caseDescription = "initial";
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription); // can't change.
        assertPasswordSucceeds("abcd1234", caseDescription);
    }

    public void testPasswordQuality_numeric() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_NUMERIC);
        assertEquals(PASSWORD_QUALITY_NUMERIC,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);            // failure

        String caseDescription = "initial";
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 10);
        caseDescription = "minimum password length = 10";
        assertEquals(10, mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("abcd", caseDescription);
        assertPasswordFails("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 4);
        caseDescription = "minimum password length = 4";
        assertEquals(4, mDevicePolicyManager.getPasswordMinimumLength(
                ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(true);

        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);
    }

    public void testPasswordQuality_alphabetic() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_ALPHABETIC);
        assertEquals(PASSWORD_QUALITY_ALPHABETIC,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        String caseDescription = "initial";
        assertPasswordFails("1234", caseDescription);      // can't change
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 10);
        caseDescription = "minimum password length = 10";
        assertEquals(10, mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("abcd", caseDescription);
        assertPasswordFails("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 4);
        caseDescription = "minimum password length = 4";
        assertEquals(4, mDevicePolicyManager.getPasswordMinimumLength(
                ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(true);

        assertPasswordFails("1234", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);
    }

    public void testPasswordQuality_alphanumeric() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                PASSWORD_QUALITY_ALPHANUMERIC);
        assertEquals(PASSWORD_QUALITY_ALPHANUMERIC,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        String caseDescription = "initial";
        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 10);
        caseDescription = "minimum password length = 10";
        assertEquals(10, mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(false);

        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("abcd", caseDescription);
        assertPasswordFails("abcd1234", caseDescription);

        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 4);
        caseDescription = "minimum password length = 4";
        assertEquals(4, mDevicePolicyManager.getPasswordMinimumLength(
                ADMIN_RECEIVER_COMPONENT));
        assertPasswordSufficiency(true);

        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("abcd", caseDescription);
        assertPasswordSucceeds("abcd1234", caseDescription);
    }

    public void testPasswordQuality_complexUpperCase() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum UpperCase=0";
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("aBc1", caseDescription);
        assertPasswordSucceeds("ABC1", caseDescription);
        assertPasswordSucceeds("ABCD", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum UpperCase=1";
        assertPasswordFails("abc1", caseDescription);
        assertPasswordSucceeds("aBc1", caseDescription);
        assertPasswordSucceeds("ABC1", caseDescription);
        assertPasswordSucceeds("ABCD", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum UpperCase=3";
        assertPasswordFails("abc1", caseDescription);
        assertPasswordFails("aBC1", caseDescription);
        assertPasswordSucceeds("ABC1", caseDescription);
        assertPasswordSucceeds("ABCD", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    public void testPasswordQuality_complexLowerCase() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum LowerCase=0";
        assertPasswordSucceeds("ABCD", caseDescription);
        assertPasswordSucceeds("aBC1", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum LowerCase=1";
        assertPasswordFails("ABCD", caseDescription);
        assertPasswordSucceeds("aBC1", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum LowerCase=3";
        assertPasswordFails("ABCD", caseDescription);
        assertPasswordFails("aBC1", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    public void testPasswordQuality_complexLetters() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum Letters=0";
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordSucceeds("a123", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Letters=1";
        assertPasswordFails("1234", caseDescription);
        assertPasswordSucceeds("a123", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Letters=3";
        assertPasswordFails("1234", caseDescription);
        assertPasswordFails("a123", caseDescription);
        assertPasswordSucceeds("abc1", caseDescription);
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    public void testPasswordQuality_complexNumeric() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum Numeric=0";
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("1abc", caseDescription);
        assertPasswordSucceeds("123a", caseDescription);
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Numeric=1";
        assertPasswordFails("abcd", caseDescription);
        assertPasswordSucceeds("1abc", caseDescription);
        assertPasswordSucceeds("123a", caseDescription);
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Numeric=3";
        assertPasswordFails("abcd", caseDescription);
        assertPasswordFails("1abc", caseDescription);
        assertPasswordSucceeds("123a", caseDescription);
        assertPasswordSucceeds("1234", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    public void testPasswordQuality_complexSymbols() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum Symbols=0";
        assertPasswordSucceeds("abcd", caseDescription);
        assertPasswordSucceeds("_bc1", caseDescription);
        assertPasswordSucceeds("@#!1", caseDescription);
        assertPasswordSucceeds("_@#!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Symbols=1";
        assertPasswordFails("abcd", caseDescription);
        assertPasswordSucceeds("_bc1", caseDescription);
        assertPasswordSucceeds("@#!1", caseDescription);
        assertPasswordSucceeds("_@#!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum Symbols=3";
        assertPasswordFails("abcd", caseDescription);
        assertPasswordFails("_bc1", caseDescription);
        assertPasswordSucceeds("@#!1", caseDescription);
        assertPasswordSucceeds("_@#!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    public void testPasswordQuality_complexNonLetter() {
        if (!mShouldRun) {
            return;
        }

        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        assertEquals(PASSWORD_QUALITY_COMPLEX,
                mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT));
        resetComplexPasswordRestrictions();

        String caseDescription = "minimum NonLetter=0";
        assertPasswordSucceeds("Abcd", caseDescription);
        assertPasswordSucceeds("_bcd", caseDescription);
        assertPasswordSucceeds("3bcd", caseDescription);
        assertPasswordSucceeds("_@3c", caseDescription);
        assertPasswordSucceeds("_25!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, 1);
        assertEquals(1, mDevicePolicyManager.getPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum NonLetter=1";
        assertPasswordFails("Abcd", caseDescription);
        assertPasswordSucceeds("_bcd", caseDescription);
        assertPasswordSucceeds("3bcd", caseDescription);
        assertPasswordSucceeds("_@3c", caseDescription);
        assertPasswordSucceeds("_25!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short

        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, 3);
        assertEquals(3, mDevicePolicyManager.getPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT));
        caseDescription = "minimum NonLetter=3";
        assertPasswordFails("Abcd", caseDescription);
        assertPasswordFails("_bcd", caseDescription);
        assertPasswordFails("3bcd", caseDescription);
        assertPasswordSucceeds("_@3c", caseDescription);
        assertPasswordSucceeds("_25!", caseDescription);
        assertPasswordFails("123", caseDescription); // too short
    }

    private boolean setUpResetPasswordToken(boolean acceptFailure) {
        // set up a token
        assertFalse(mDevicePolicyManager.isResetPasswordTokenActive(ADMIN_RECEIVER_COMPONENT));

        try {
            // On devices with password token disabled, calling this method will throw
            // a security exception. If that's anticipated, then return early without failing.
            assertTrue(mDevicePolicyManager.setResetPasswordToken(ADMIN_RECEIVER_COMPONENT,
                    TOKEN0));
        } catch (SecurityException e) {
            if (acceptFailure &&
                    e.getMessage().equals("Escrow token is disabled on the current user")) {
                return false;
            } else {
                throw e;
            }
        }
        assertTrue(mDevicePolicyManager.isResetPasswordTokenActive(ADMIN_RECEIVER_COMPONENT));
        return true;
    }

    private void cleanUpResetPasswordToken() {
        // First remove device lock
        mDevicePolicyManager.setPasswordQuality(ADMIN_RECEIVER_COMPONENT,
                DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED);
        assertTrue(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT, null,
                TOKEN0, 0));

        // Then remove token and check it succeeds
        assertTrue(mDevicePolicyManager.clearResetPasswordToken(ADMIN_RECEIVER_COMPONENT));
        assertFalse(mDevicePolicyManager.isResetPasswordTokenActive(ADMIN_RECEIVER_COMPONENT));
        assertFalse(mDevicePolicyManager.resetPasswordWithToken(ADMIN_RECEIVER_COMPONENT,
                SHORT_PASSWORD, TOKEN0, 0));
    }

    private void assertPasswordFails(String password, String restriction) {
        try {
            boolean passwordResetResult = mDevicePolicyManager.resetPasswordWithToken(
                    ADMIN_RECEIVER_COMPONENT, password, TOKEN0, /* flags= */0);
            assertFalse("Password '" + password + "' should have failed on " + restriction,
                    passwordResetResult);
        } catch (IllegalArgumentException e) {
            // yesss, we have failed!
        }
    }

    private void assertPasswordSucceeds(String password, String restriction) {
        boolean passwordResetResult = mDevicePolicyManager.resetPasswordWithToken(
                ADMIN_RECEIVER_COMPONENT, password, TOKEN0, /* flags= */0);
        assertTrue("Password '" + password + "' failed on " + restriction, passwordResetResult);
        assertPasswordSufficiency(true);
    }

    private void resetComplexPasswordRestrictions() {
        final int quality = mDevicePolicyManager.getPasswordQuality(ADMIN_RECEIVER_COMPONENT);
        if (quality < PASSWORD_QUALITY_NUMERIC) {
            return;
        }
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, 0);
        if (quality < PASSWORD_QUALITY_COMPLEX) {
            return;
        }
        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, 0);
        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, 0);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, 0);
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, 0);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, 0);
        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, 0);
    }
}