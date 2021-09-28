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
 * limitations under the License
 */

package android.host.multiuser;

import android.platform.test.annotations.Presubmit;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Test verifies that users can be created/switched to without error dialogs shown to the user
 * Run: atest CreateUsersNoAppCrashesTest
 */
@RunWith(DeviceJUnit4ClassRunner.class)
public class CreateUsersNoAppCrashesTest extends BaseMultiUserTest {

    @Presubmit
    @Test
    public void testCanCreateGuestUser() throws Exception {
        if (!mSupportsMultiUser) {
            return;
        }
        int userId = getDevice().createUser(
                "TestUser_" + System.currentTimeMillis() /* name */,
                true /* guest */,
                false /* ephemeral */);
        assertSwitchToNewUser(userId);
        assertSwitchToUser(userId, mInitialUserId);
    }

    @Presubmit
    @Test
    public void testCanCreateSecondaryUser() throws Exception {
        if (!mSupportsMultiUser) {
            return;
        }
        int userId = getDevice().createUser(
                "TestUser_" + System.currentTimeMillis() /* name */,
                false /* guest */,
                false /* ephemeral */);
        assertSwitchToNewUser(userId);
        assertSwitchToUser(userId, mInitialUserId);
    }
}
