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

import static com.google.common.truth.Truth.assertThat;

import static java.lang.Math.abs;

public class GetPasswordExpirationTest extends BaseDeviceAdminTest {
    final long TIMEOUT_RESET_TEST = 86400000L /* 1 day */;

    public void testGetPasswordExpiration() throws Exception {
        final long[] testTimeouts = new long[] {
                86400000L /* 1 day */, 864000000L /* 10 days */, 8640000000L /* 100 days */};
        for (long testTimeout : testTimeouts) {
            mDevicePolicyManager.setPasswordExpirationTimeout(
                    ADMIN_RECEIVER_COMPONENT, testTimeout);
            checkPasswordExpiration("Password expiration time incorrect", testTimeout, 5000);
        }
        // Set password expiration timeout to 0 clears the expiration date.
        mDevicePolicyManager.setPasswordExpirationTimeout(ADMIN_RECEIVER_COMPONENT, 0L);
        assertThat(mDevicePolicyManager.getPasswordExpiration(ADMIN_RECEIVER_COMPONENT))
                .isEqualTo(0L);
    }

    public void testGetPasswordExpirationUpdatedAfterPasswordReset_beforeReset() throws Exception {
        mDevicePolicyManager.setPasswordExpirationTimeout(
                ADMIN_RECEIVER_COMPONENT, TIMEOUT_RESET_TEST);
        checkPasswordExpiration("Password expiration time incorrect", TIMEOUT_RESET_TEST, 5000);
    }

    public void testGetPasswordExpirationUpdatedAfterPasswordReset_afterReset() throws Exception {
        checkPasswordExpiration("Password expiration time not refreshed correctly"
                + " after reseting password", TIMEOUT_RESET_TEST, 10000);
    }

    private void checkPasswordExpiration(String error, long timeout, long tolerance) {
        assertTrue(error, abs(System.currentTimeMillis() + timeout
                - mDevicePolicyManager.getPasswordExpiration(
                ADMIN_RECEIVER_COMPONENT)) <= tolerance);
    }
}