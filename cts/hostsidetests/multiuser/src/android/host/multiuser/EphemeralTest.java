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
 * limitations under the License
 */
package android.host.multiuser;

import android.platform.test.annotations.Presubmit;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test verifies that ephemeral users are removed after switched away and after reboot.
 * Run: atest android.host.multiuser.EphemeralTest
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class EphemeralTest extends BaseMultiUserTest {

    /** Test to verify ephemeral user is removed after switch out to another user. */
    @Presubmit
    @Test
    public void testSwitchAndRemoveEphemeralUser() throws Exception {
        if (!mSupportsMultiUser) {
            return;
        }
        int ephemeralUserId = -1;
        try {
            ephemeralUserId = getDevice().createUser(
                    "TestUser_" + System.currentTimeMillis() /* name */,
                    false /* guest */,
                    true /* ephemeral */);
        } catch (Exception e) {
            CLog.w("Failed to create user. Skipping test.");
            return;
        }
        assertSwitchToNewUser(ephemeralUserId);
        assertSwitchToUser(ephemeralUserId, mInitialUserId);
        waitForUserRemove(ephemeralUserId);
        assertUserNotPresent(ephemeralUserId);
    }

    /** Test to verify ephemeral user is removed after reboot. */
    @Presubmit
    @Test
    public void testRebootAndRemoveEphemeralUser() throws Exception {
        if (!mSupportsMultiUser) {
            return;
        }
        int ephemeralUserId = -1;
        try {
            ephemeralUserId = getDevice().createUser(
                    "TestUser_" + System.currentTimeMillis() /* name */,
                    false /* guest */,
                    true /* ephemeral */);
        } catch (Exception e) {
            CLog.w("Failed to create user. Skipping test.");
            return;
        }
        assertSwitchToNewUser(ephemeralUserId);
        getDevice().reboot();
        assertUserNotPresent(ephemeralUserId);
    }
}
