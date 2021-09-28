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

package android.appsecurity.cts;

import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.testtype.junit4.BaseHostJUnit4Test;

import org.junit.Assert;
import org.junit.Before;

import java.util.ArrayList;
import java.util.HashMap;

/**
 * Base class.
 */
public abstract class BaseAppSecurityTest extends BaseHostJUnit4Test {

    /** Whether multi-user is supported. */
    protected boolean mSupportsMultiUser;
    protected boolean mIsSplitSystemUser;
    protected int mPrimaryUserId;
    /** Users we shouldn't delete in the tests */
    private ArrayList<Integer> mFixedUsers;

    @Before
    public void setUpBaseAppSecurityTest() throws Exception {
        Assert.assertNotNull(getBuild()); // ensure build has been set before test is run.

        mSupportsMultiUser = getDevice().getMaxNumberOfUsersSupported() > 1;
        mPrimaryUserId = getDevice().getPrimaryUserId();
        mFixedUsers = new ArrayList<>();
        mFixedUsers.add(mPrimaryUserId);
        if (mPrimaryUserId != Utils.USER_SYSTEM) {
            mFixedUsers.add(Utils.USER_SYSTEM);
        }
    }

    protected void installTestAppForUser(String apk, int userId) throws Exception {
        installTestAppForUser(apk, false, userId);
    }

    protected void installTestAppForUser(String apk, boolean instant, int userId) throws Exception {
        if (userId < 0) {
            userId = mPrimaryUserId;
        }
        new InstallMultiple(instant)
                .addFile(apk)
                .allowTest()
                .forUser(userId)
                .run();
    }

    // TODO: We should be able to set test arguments from the BaseHostJUnit4Test methods
    protected void runDeviceTests(String packageName, String testClassName,
            String testMethodName, boolean instant) throws DeviceNotAvailableException {
        final HashMap<String, String> testArgs;
        if (instant) {
            testArgs = new HashMap<>();
            testArgs.put("is_instant", Boolean.TRUE.toString());
        } else {
            testArgs = null;
        }
        Utils.runDeviceTestsAsCurrentUser(getDevice(), packageName, testClassName, testMethodName,
                testArgs);
    }

    protected boolean isAppVisibleForUser(String packageName, int userId,
            boolean matchUninstalled) throws Exception {
        String command = "cmd package list packages --user " + userId;
        if (matchUninstalled) command += " -u";
        String output = getDevice().executeShellCommand(command);
        return output.contains(packageName);
    }

    protected class InstallMultiple extends BaseInstallMultiple<InstallMultiple> {
        public InstallMultiple() {
            this(false);
        }
        public InstallMultiple(boolean instant) {
            this(instant, true);
        }
        public InstallMultiple(boolean instant, boolean grantPermissions) {
            super(getDevice(), getBuild(), getAbi(), grantPermissions);
            addArg(instant ? "--instant" : "");
            addArg("--force-queryable");
        }
    }
}
