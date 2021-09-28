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
package android.content.pm.cts.shortcuthost;

import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(DeviceJUnit4ClassRunner.class)
public class ShortcutManagerMultiuserTest extends BaseShortcutManagerHostTest {
    private static final String TARGET_APK = "CtsShortcutMultiuserTest.apk";
    private static final String TARGET_PKG = "android.content.pm.cts.shortcut.multiuser";

    @Override
    public void tearDown() throws Exception {
        getDevice().uninstallPackage(TARGET_PKG);

        super.tearDown();
    }

    @Test
    public void testManagedUser() throws Exception {
        if (!mIsMultiuserSupported || !mIsManagedUserSupported) {
            return;
        }
        // First, create users
        final int profileId = createProfile(getPrimaryUserId());

        installAppAsUser(TARGET_APK, getPrimaryUserId());
        installAppAsUser(TARGET_APK, profileId);

        runDeviceTestsAsUser(TARGET_PKG, ".ShortcutManagerManagedUserTest",
                "test01_managedProfileNotStarted", getPrimaryUserId());

        getDevice().startUser(profileId, /* wait */ true);

        runDeviceTestsAsUser(TARGET_PKG, ".ShortcutManagerManagedUserTest",
                "test02_createShortuctsOnPrimaryUser", getPrimaryUserId());
        runDeviceTestsAsUser(TARGET_PKG, ".ShortcutManagerManagedUserTest",
                "test03_createShortuctsOnManagedProfile", profileId);

        runDeviceTestsAsUser(TARGET_PKG, ".ShortcutManagerManagedUserTest",
                "test04_getAndLaunch_primary", getPrimaryUserId());
    }

    @Test
    public void testSecondaryUser() throws Exception {
        if (!mIsMultiuserSupported) {
            return;
        }
        final int secondUserID = getOrCreateSecondaryUser();

        getDevice().startUser(secondUserID, true);
        getDevice().switchUser(secondUserID);
        installAppAsUser(TARGET_APK, secondUserID);

        runDeviceTestsAsUser(TARGET_PKG, ".ShortcutManagerSecondaryUserTest", secondUserID);

        getDevice().stopUser(secondUserID);
    }
}
