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

import java.util.concurrent.CountDownLatch;
import java.util.concurrent.TimeUnit;

public class PasswordExpirationTest extends BaseDeviceAdminTest {

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mOnPasswordExpiryTimeoutCalled = new CountDownLatch(1);
    }

    @Override
    protected void tearDown() throws Exception {
        super.tearDown();
        mDevicePolicyManager.setPasswordExpirationTimeout(ADMIN_RECEIVER_COMPONENT, 0);
        mOnPasswordExpiryTimeoutCalled = null;
    }

    public void testPasswordExpires() throws Exception {
        final long testTimeoutMs = 5000;
        // Set a significantly longer wait time to check that the countDownLatch is called to prevent
        // tests flakiness due to setups delays.
        final long testWaitTimeMs = 300000;
        mDevicePolicyManager.setPasswordExpirationTimeout(ADMIN_RECEIVER_COMPONENT, testTimeoutMs);
        assertTrue("The password expiry timeout was not triggered.",
                mOnPasswordExpiryTimeoutCalled.await(testWaitTimeMs, TimeUnit.MILLISECONDS));
    }

    public void testNoNegativeTimeout() {
        final long testTimeoutMs = -1;
        try {
            mDevicePolicyManager.setPasswordExpirationTimeout(ADMIN_RECEIVER_COMPONENT, testTimeoutMs);
            fail("Setting a negative value for a timeout is expected to throw an exception.");
        } catch (IllegalArgumentException e) {}
    }

    // TODO: investigate this test's failure. b/110976462
    /* public void testPasswordNotYetExpiredIsInEffect() throws Exception {
        final long testTimeoutMs = 5000;
        mDevicePolicyManager.setPasswordExpirationTimeout(ADMIN_RECEIVER_COMPONENT, testTimeoutMs);
        assertFalse("The password expiry timeout was triggered too early.",
                mOnPasswordExpiryTimeoutCalled.await(testTimeoutMs, TimeUnit.MILLISECONDS));
    }
    */
}
