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
 * limitations under the License
 */

package android.telecom.cts;

import static com.android.compatibility.common.util.SystemUtil.runWithShellPermissionIdentity;

import android.telecom.PhoneAccount;
import android.telecom.PhoneAccountHandle;
import android.telecom.TelecomManager;

/**
 * Tests use of APIs related to changing the default outgoing phone account.
 */
public class DefaultPhoneAccountTest extends BaseTelecomTestWithMockServices {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mContext = getInstrumentation().getContext();
    }

    /**
     * Verifies that {@link TelecomManager#getUserSelectedOutgoingPhoneAccount()} is able to
     * retrieve the user-selected outgoing phone account.
     * Given that there is a user-selected default, also verifies that
     * {@link TelecomManager#getDefaultOutgoingPhoneAccount(String)} reports this value as well.
     * Note: This test depends on
     * {@code TelecomManager#setUserSelectedOutgoingPhoneAccount(PhoneAccountHandle)} being run
     * through the telecom shell command in order to change the user-selected default outgoing
     * account.
     * @throws Exception
     */
    public void testDefaultIsSet() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        // Make sure to set the default outgoing phone account to the new connection service
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE | FLAG_SET_DEFAULT);

        PhoneAccountHandle handle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, handle);

        PhoneAccountHandle defaultOutgoing = mTelecomManager.getDefaultOutgoingPhoneAccount(
                PhoneAccount.SCHEME_TEL);
        assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, defaultOutgoing);
    }

    /**
     * Verifies that {@link TelecomManager#getUserSelectedOutgoingPhoneAccount()} is able to
     * retrieve the user-selected outgoing phone account.
     * Given that there is a user-selected default, also verifies that
     * {@link TelecomManager#getDefaultOutgoingPhoneAccount(String)} reports this value as well.
     * @throws Exception
     */
    public void testSetUserSelectedOutgoingPhoneAccount() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }
        // Make sure to set the default outgoing phone account to the new connection service
        setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);

        PhoneAccountHandle previousOutgoingPhoneAccount =
                mTelecomManager.getUserSelectedOutgoingPhoneAccount();

        try {
            // Use TelecomManager API to set the outgoing phone account.
            runWithShellPermissionIdentity(() ->
                    mTelecomManager.setUserSelectedOutgoingPhoneAccount(
                            TestUtils.TEST_PHONE_ACCOUNT_HANDLE));

            PhoneAccountHandle handle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
            assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, handle);

            PhoneAccountHandle defaultOutgoing = mTelecomManager.getDefaultOutgoingPhoneAccount(
                    PhoneAccount.SCHEME_TEL);
            assertEquals(TestUtils.TEST_PHONE_ACCOUNT_HANDLE, defaultOutgoing);
        } finally {
            // Restore the default outgoing phone account.
            TestUtils.setDefaultOutgoingPhoneAccount(getInstrumentation(),
                    previousOutgoingPhoneAccount);
        }
    }

    /**
     * Verifies operation of the {@link TelecomManager#getDefaultOutgoingPhoneAccount(String)} API
     * where there is NO user selected default outgoing phone account.
     * In AOSP, this mimics the user having changed the
     * Phone --> Settings --> Call Settings --> Calling accounts --> Make Calls With
     * option to "Ask first".
     *
     * The test assumes that a device either has a single sim, or has multiple sims.
     * In either case, it registers another TEL outgoing calling account.
     *
     * We can expect two things:
     * 1. {@link TelecomManager#getUserSelectedOutgoingPhoneAccount()} returns null, since the
     *    "ask first" option was chosen.
     * 2. {@link TelecomManager#getUserSelectedOutgoingPhoneAccount()} returns null, since there is
     *    now 2 or more potential outgoing phone accounts with the TEL scheme.
     * @throws Exception
     */
    public void testGetDefaultOutgoingNoUserSelected() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        PhoneAccountHandle previousOutgoingPhoneAccount =
                mTelecomManager.getUserSelectedOutgoingPhoneAccount();

        // Clear the default outgoing phone account; this is the same as saying "ask every time" in
        // the user settings.
        TestUtils.setDefaultOutgoingPhoneAccount(getInstrumentation(),
                null /* clear default */);
        try {
            // Register another TEL URI phone account; since we expect devices to have at minimum
            // 1 sim, this ensures that we have a scenario where there are multiple potential
            // outgoing phone accounts with the TEL scheme.
            setupConnectionService(null, FLAG_REGISTER | FLAG_ENABLE);

            // There should be NO user selected default outgoing account (we cleared it).
            PhoneAccountHandle handle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
            assertEquals(null, handle);

            // There should be multiple potential TEL phone accounts now, so we expect null here.
            PhoneAccountHandle defaultOutgoing = mTelecomManager.getDefaultOutgoingPhoneAccount(
                    PhoneAccount.SCHEME_TEL);
            assertEquals(null, defaultOutgoing);
        } finally {
            // Restore the default outgoing phone account.
            TestUtils.setDefaultOutgoingPhoneAccount(getInstrumentation(),
                    previousOutgoingPhoneAccount);
        }
    }

    /**
     * Verifies correct operation of the
     * {@link TelecomManager#getDefaultOutgoingPhoneAccount(String)} API.
     * The purpose of this CTS test is to verify the following scenarios:
     * 1. Where there is NO user selected default outgoing phone account and there is a single
     *    potential phone account, that phone account should be returned.
     * 2. Where there is NO user selected default outgoing phone account and there are multiple
     *    potential phone accounts, null should be returned.
     * This test performs this operation using a test URI scheme to remove dependencies on the
     * number of potential sims in a device, however the test cases above should pass even if the
     * TEL uri scheme was being tested.
     * @throws Exception
     */
    public void testGetDefaultOutgoingPhoneAccountOneOrMany() throws Exception {
        if (!mShouldTestTelecom) {
            return;
        }

        PhoneAccountHandle previousOutgoingPhoneAccount =
                mTelecomManager.getUserSelectedOutgoingPhoneAccount();

        // Clear the default outgoing phone account; this is the same as saying "ask every time" in
        // the user settings.
        TestUtils.setDefaultOutgoingPhoneAccount(getInstrumentation(),
                null /* clear default */);

        try {
            // Lets register a new phone account using a test URI scheme 'foobuzz' (this avoids
            // conflicts with any sims on the device).
            registerAndEnablePhoneAccount(TestUtils.TEST_DEFAULT_PHONE_ACCOUNT_1);

            // There should be NO user selected default outgoing account (we cleared it above).
            PhoneAccountHandle handle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
            assertEquals(null, handle);

            // There should be a single potential phone account in the 'foobuzz' scheme, so it
            // should be reported as the default outgoing phone account.
            PhoneAccountHandle defaultOutgoing = mTelecomManager.getDefaultOutgoingPhoneAccount(
                    TestUtils.TEST_URI_SCHEME);
            assertEquals(TestUtils.TEST_DEFAULT_PHONE_ACCOUNT_HANDLE_1, defaultOutgoing);

            // Next, lets register another new phone account using the test URI scheme 'foobuzz'.
            registerAndEnablePhoneAccount(TestUtils.TEST_DEFAULT_PHONE_ACCOUNT_2);

            // There should still be NO user selected default outgoing account (we cleared it
            // above).
            handle = mTelecomManager.getUserSelectedOutgoingPhoneAccount();
            assertEquals(null, handle);

            // Now that there are two potential outgoing accounts in the same scheme and nothing is
            // chosen as the default, the default outgoing phone account should be "null".
            defaultOutgoing = mTelecomManager.getDefaultOutgoingPhoneAccount(
                    TestUtils.TEST_URI_SCHEME);
            assertEquals(null, defaultOutgoing);
        } finally {
            mTelecomManager.unregisterPhoneAccount(TestUtils.TEST_DEFAULT_PHONE_ACCOUNT_HANDLE_1);
            mTelecomManager.unregisterPhoneAccount(TestUtils.TEST_DEFAULT_PHONE_ACCOUNT_HANDLE_2);

            // Restore the default outgoing phone account.
            TestUtils.setDefaultOutgoingPhoneAccount(getInstrumentation(),
                    previousOutgoingPhoneAccount);
        }
    }

    private void registerAndEnablePhoneAccount(PhoneAccount phoneAccount) throws Exception {
        mTelecomManager.registerPhoneAccount(phoneAccount);
        TestUtils.enablePhoneAccount(getInstrumentation(), phoneAccount.getAccountHandle());
        // Wait till the adb commands have executed and account is enabled in Telecom database.
        assertPhoneAccountEnabled(phoneAccount.getAccountHandle());
    }
}
