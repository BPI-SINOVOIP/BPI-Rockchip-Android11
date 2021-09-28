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

import com.android.compatibility.common.util.CddTest;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import java.util.concurrent.TimeUnit;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class SecondaryUsersTest extends BaseMultiUserTest {

    // Extra time to give the system to switch into secondary user after boot complete.
    private static final long SECONDARY_USER_BOOT_COMPLETE_TIMEOUT_MS = 30000;

    private static final long POLL_INTERVAL_MS = 100;

    @CddTest(requirement="9.5/A-1-2")
    @Test
    public void testSwitchToSecondaryUserBeforeBootComplete() throws Exception {
        if (!isAutomotiveDevice() || !mSupportsMultiUser) {
            return;
        }

        getDevice().nonBlockingReboot();
        getDevice().waitForBootComplete(TimeUnit.MINUTES.toMillis(2));

        boolean isUserSecondary = false;
        long ti = System.currentTimeMillis();

        // TODO(b/138944230): Verify if current user is secondary when the UI is ready for user
        // interaction. A possibility is to check if the CarLauncher is started in the
        // Activity Stack, but this becomes tricky in OEM implementation, where CarLauncher is
        // replaced with another launcher. Launcher can usually identify by
        // android.intent.category.HOME (type=home) and priority = -1000. But there is no clear way
        // to determine this via adb.
        while (System.currentTimeMillis() - ti < SECONDARY_USER_BOOT_COMPLETE_TIMEOUT_MS) {
            isUserSecondary = getDevice().isUserSecondary(getDevice().getCurrentUser());
            if (isUserSecondary) {
                break;
            }
            Thread.sleep(POLL_INTERVAL_MS);
        }
        Assert.assertTrue("Must switch to secondary user before boot complete", isUserSecondary);
    }
}
