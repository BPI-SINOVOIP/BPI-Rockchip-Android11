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

package android.cts.backup;

import android.platform.test.annotations.AppModeFull;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.testtype.DeviceJUnit4ClassRunner;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Optional;

/**
 * Verifies that serial number for ancestral work profile is read/written correctly. The host-side
 * part is needed to create/remove a test user as the test is run for a new secondary user to avoid
 * changing serial numbers for existing users.
 */
@RunWith(DeviceJUnit4ClassRunner.class)
@AppModeFull
public class ProfileSerialNumberHostSideTest extends BaseMultiUserBackupHostSideTest {
    private static final String APK_NAME = "CtsProfileSerialNumberApp.apk";
    private static final String TEST_PACKAGE = "android.cts.backup.profileserialnumberapp";
    private static final String TEST_CLASS = TEST_PACKAGE + ".ProfileSerialNumberTest";

    private ITestDevice mDevice;
    private Optional<Integer> mProfileUserId = Optional.empty();

    @Before
    @Override
    public void setUp() throws Exception {
        mDevice = getDevice();
        super.setUp();

        int userId = mDevice.getCurrentUser();
        int profileUserId = createProfileUser(userId, "Profile-Serial-Number");
        mProfileUserId = Optional.of(profileUserId);
        startUserAndInitializeForBackup(profileUserId);
        installPackageAsUser(APK_NAME, profileUserId);
    }

    @After
    @Override
    public void tearDown() throws Exception {
        if (mProfileUserId.isPresent()) {
            int profileUserId = mProfileUserId.get();
            uninstallPackageAsUser(TEST_PACKAGE, profileUserId);
            mDevice.removeUser(profileUserId);
            mProfileUserId = Optional.empty();
        }

        super.tearDown();
    }

    /**
     * Test reading and writing of serial number for user's ancestral profile.
     *
     * <p>Test logic:
     *
     * <ol>
     *   <li>Set ancestral serial number for user with {@link
     *       BackupManager#setAncestralSerialNumber(long)}.
     *   <li>Get user by ancestral serial number with {@link
     *       BackupManager#getUserForAncestralSerialNumber(long)} and verify the same user is
     *       returned.
     * </ol>
     */
    @Test
    public void testSetAndGetAncestralSerialNumber() throws Exception {
        checkDeviceTest("testSetAndGetAncestralSerialNumber");
    }

    /**
     * Test that {@link BackupManager#getUserForAncestralSerialNumber(long)} returns {@code null}
     * when the given serial number is not assigned to any user.
     */
    @Test
    public void testGetUserForAncestralSerialNumber_returnsNull_whenNoUserHasGivenSerialNumber()
            throws Exception {
        checkDeviceTest(
                "testGetUserForAncestralSerialNumber_returnsNull_whenNoUserHasGivenSerialNumber");
    }

    private void checkDeviceTest(String methodName) throws DeviceNotAvailableException {
        checkDeviceTestAsUser(TEST_PACKAGE, TEST_CLASS, methodName, mProfileUserId.get());
    }
}
