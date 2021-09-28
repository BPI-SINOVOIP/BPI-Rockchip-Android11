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
 * limitations under the License
 */
package android.host.accounts;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.testtype.IDeviceTest;

import org.junit.After;
import org.junit.Before;

import java.util.ArrayList;

/**
 * Base class for multi user tests.
 */
public class BaseMultiUserTest implements IDeviceTest {

    /** Guest flag value from android/content/pm/UserInfo.java */
    private static final int FLAG_GUEST = 0x00000004;

    /**
     * Feature flag for automotive devices
     * https://source.android.com/compatibility/android-cdd#2_5_automotive_requirements
     */
    private static final String FEATURE_AUTOMOTIVE = "feature:android.hardware.type.automotive";
    private static final String FEATURE_MANAGED_USERS = "android.software.managed_users";

    /** Whether multi-user is supported. */
    protected boolean mSupportsMultiUser;
    protected boolean mSupportsManagedUsers;
    protected int mInitialUserId;
    protected int mPrimaryUserId;

    /** Users we shouldn't delete in the tests. */
    private ArrayList<Integer> mFixedUsers;

    private ITestDevice mDevice;

    @Before
    public void setUp() throws Exception {
        mSupportsMultiUser = getDevice().getMaxNumberOfUsersSupported() > 1;
        mSupportsManagedUsers = getDevice().hasFeature(FEATURE_MANAGED_USERS);

        mInitialUserId = getDevice().getCurrentUser();
        mPrimaryUserId = getDevice().getPrimaryUserId();

        // Test should not modify / remove any of the existing users.
        mFixedUsers = getDevice().listUsers();
    }

    @After
    public void tearDown() throws Exception {
        if (getDevice().getCurrentUser() != mInitialUserId) {
            CLog.w("User changed during test. Switching back to " + mInitialUserId);
            getDevice().switchUser(mInitialUserId);
        }
        // Remove the users created during this test.
        removeTestUsers();
    }

    @Override
    public void setDevice(ITestDevice device) {
        mDevice = device;
    }

    @Override
    public ITestDevice getDevice() {
        return mDevice;
    }

    /**
     * @param userId the userId of the parent for this profile
     * @return the userId of the created user
     */
    protected int createRestrictedProfile(int userId)
            throws DeviceNotAvailableException, IllegalStateException {
        return createUser("--profileOf " + userId + " --restricted");
    }

    /**
     * @param userId the userId of the parent for this profile
     * @return the userId of the created user
     */
    protected int createProfile(int userId)
            throws DeviceNotAvailableException, IllegalStateException {
        return createUser("--profileOf " + userId + " --managed");
    }

    /**
     * @return the userid of the created user
     */
    protected int createUser()
            throws DeviceNotAvailableException, IllegalStateException {
        return createUser("");
    }

    private int createUser(String extraParam)
            throws DeviceNotAvailableException, IllegalStateException {
        final String command =
                "pm create-user " + extraParam + " TestUser_" + System.currentTimeMillis();
        CLog.d("Starting command: " + command);
        final String output = getDevice().executeShellCommand(command);
        CLog.d("Output for command " + command + ": " + output);

        if (output.startsWith("Success")) {
            try {
                return Integer.parseInt(output.substring(output.lastIndexOf(" ")).trim());
            } catch (NumberFormatException e) {
                CLog.e("Failed to parse result: %s", output);
            }
        } else {
            CLog.e("Failed to create user: %s", output);
        }
        throw new IllegalStateException();
    }

    protected int createGuestUser() throws Exception {
        return mDevice.createUser(
                "TestUser_" + System.currentTimeMillis() /* name */,
                true /* guest */,
                false /* ephemeral */);
    }

    protected int getGuestUser() throws Exception {
        for (int userId : mDevice.listUsers()) {
            if ((mDevice.getUserFlags(userId) & FLAG_GUEST) != 0) {
                return userId;
            }
        }
        return -1;
    }

    protected boolean isAutomotiveDevice() throws Exception {
        return getDevice().hasFeature(FEATURE_AUTOMOTIVE);
    }

    private void removeTestUsers() throws Exception {
        for (int userId : getDevice().listUsers()) {
            if (!mFixedUsers.contains(userId)) {
                getDevice().removeUser(userId);
            }
        }
    }
}
