/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static org.testng.Assert.assertThrows;

import android.os.Process;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class SecondaryLockscreenTest extends BaseDeviceAdminTest {

    @Before
    @Override
    public void setUp() throws Exception {
        super.setUp();
        assertFalse(mDevicePolicyManager.isSecondaryLockscreenEnabled(Process.myUserHandle()));
    }

    @After
    @Override
    public void tearDown() throws Exception {
        assertFalse(mDevicePolicyManager.isSecondaryLockscreenEnabled(Process.myUserHandle()));
        super.tearDown();
    }

    public void testSetSecondaryLockscreen_notSupervisionApp_throwsSecurityException() {
        // This API is only available to the configured supervision app, which is not possible to
        // override as part of a CTS test, so just test that a security exception is thrown as
        // expected even for the DO/PO.
        assertThrows(SecurityException.class,
                () -> mDevicePolicyManager.setSecondaryLockscreenEnabled(ADMIN_RECEIVER_COMPONENT,
                        true));
    }
}