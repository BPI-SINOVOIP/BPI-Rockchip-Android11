/*
 * Copyright (C) 2019 The Android Open Source Project
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

import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_COMPLEX;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_NUMERIC;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_SOMETHING;
import static android.app.admin.DevicePolicyManager.PASSWORD_QUALITY_UNSPECIFIED;

import static org.testng.Assert.assertThrows;

/**
 * Class that tests password constraints API preconditions.
 */
public class PasswordRequirementsTest extends BaseDeviceAdminTest {
    private static final int TEST_VALUE = 5;
    private static final int DEFAULT_NUMERIC = 1;
    private static final int DEFAULT_LETTERS = 1;
    private static final int DEFAULT_UPPERCASE = 0;
    private static final int DEFAULT_LOWERCASE = 0;
    private static final int DEFAULT_NON_LETTER = 0;
    private static final int DEFAULT_SYMBOLS = 1;
    private static final int DEFAULT_LENGTH = 0;

    public void testPasswordConstraintsDoesntThrowAndPreservesValuesPreR() {
        // Pre-R password restrictions can be set in any order.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_SOMETHING);
        // These shouldn't throw.
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);

        // Make sure these values are preserved and not reset when quality is set low.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_UNSPECIFIED);
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT));
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT));
    }

    public void testSettingConstraintsWithLowQualityThrowsOnRPlus() {
        // On R and above quality should be set first.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_SOMETHING);

        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
    }

    public void testSettingConstraintsWithNumericQualityOnlyLengthAllowedOnRPlus() {
        // On R and above quality should be set first.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_NUMERIC);

        // This should be allowed now.
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);

        // These are still not allowed.
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
        assertThrows(IllegalStateException.class, () -> mDevicePolicyManager
                .setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, TEST_VALUE));
    }

    public void testSettingConstraintsWithComplexQualityAndResetWithLowerQuality() {
        // On R and above when quality is lowered, irrelevant requirements are getting reset.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_COMPLEX);
        // These shouldn't throw anymore
        mDevicePolicyManager.setPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);
        mDevicePolicyManager.setPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT, TEST_VALUE);

        // Downgrade to NUMERIC, only length makes sense after that.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_NUMERIC);

        // Length shouldn't be reset
        assertEquals(TEST_VALUE,
                mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));

        // But other requirements should.
        assertEquals(DEFAULT_NUMERIC,
                mDevicePolicyManager.getPasswordMinimumNumeric(ADMIN_RECEIVER_COMPONENT));
        assertEquals(DEFAULT_LETTERS,
                mDevicePolicyManager.getPasswordMinimumLetters(ADMIN_RECEIVER_COMPONENT));
        assertEquals(DEFAULT_UPPERCASE,
                mDevicePolicyManager.getPasswordMinimumUpperCase(ADMIN_RECEIVER_COMPONENT));
        assertEquals(DEFAULT_LOWERCASE,
                mDevicePolicyManager.getPasswordMinimumLowerCase(ADMIN_RECEIVER_COMPONENT));
        assertEquals(DEFAULT_NON_LETTER,
                mDevicePolicyManager.getPasswordMinimumNonLetter(ADMIN_RECEIVER_COMPONENT));
        assertEquals(DEFAULT_SYMBOLS,
                mDevicePolicyManager.getPasswordMinimumSymbols(ADMIN_RECEIVER_COMPONENT));

        // Downgrade to SOMETHING.
        mDevicePolicyManager.setPasswordQuality(
                ADMIN_RECEIVER_COMPONENT, PASSWORD_QUALITY_SOMETHING);

        // Now length should also be reset.
        assertEquals(DEFAULT_LENGTH,
                mDevicePolicyManager.getPasswordMinimumLength(ADMIN_RECEIVER_COMPONENT));

    }
}
