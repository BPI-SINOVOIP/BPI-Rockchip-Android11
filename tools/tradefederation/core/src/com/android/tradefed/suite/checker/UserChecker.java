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
 * limitations under the License.
 */
package com.android.tradefed.suite.checker;

import java.util.ArrayList;
import java.util.Collection;
import java.util.Map;

import com.android.tradefed.config.Option;
import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.device.UserInfo;
import com.android.tradefed.log.LogUtil.CLog;
import com.android.tradefed.suite.checker.StatusCheckerResult.CheckStatus;

/**
 * Checks if users have changed during the test.
 *
 * <p>Optionally can also setup the current user.
 */
@OptionClass(alias = "user-system-checker")
public class UserChecker implements ISystemStatusChecker {

    @Option(
        name = "user-type",
        description = "The type of user to switch to before each module run."
    )
    private UserInfo.UserType mUserToSwitchTo = UserInfo.UserType.CURRENT;

    @Option(
        name = "user-cleanup",
        description =
                "If true, attempt to cleanup any changes made to users:"
                        + "\n - switch to previous current-user"
                        + "\n - remove any created users"
                        + "\n\nThis does NOT:"
                        + "\n - attempt to re-create a user that was deleted"
                        + "\n - start/stop existing users if their running status changed"
    )
    private boolean mCleanup = false;

    private UserInfo mPreCurrentUserInfo = null;
    private Map<Integer, UserInfo> mPreUsersInfo = null;
    private int mSwitchedToUserId = -1;

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult preExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {

        mPreUsersInfo = device.getUserInfos();
        mPreCurrentUserInfo = mPreUsersInfo.get(device.getCurrentUser());

        if (mPreCurrentUserInfo.isUserType(mUserToSwitchTo, mPreCurrentUserInfo.userId())) {
            CLog.i(
                    "Current user %d is already user type %s, no action.",
                    mPreCurrentUserInfo.userId(), mUserToSwitchTo.toString());
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }

        mSwitchedToUserId = findMatchingUser(mPreUsersInfo.values());
        if (mSwitchedToUserId < 0) {
            mSwitchedToUserId =
                    device.createUser(
                            /* name= */ "Tf" + mUserToSwitchTo.toString().toLowerCase(),
                            /* guest= */ mUserToSwitchTo.isGuest(),
                            /* ephemeral= */ false);
            CLog.i(
                    "No user of type %s found, created user %d",
                    mUserToSwitchTo.toString(), mSwitchedToUserId);
        }

        CLog.i(
                "Current user is %d, switching to user %s of type %s",
                mPreCurrentUserInfo.userId(), mSwitchedToUserId, mUserToSwitchTo);
        if (!device.switchUser(mSwitchedToUserId)) {
            return statusFail(String.format("Failed to switch to user %d", mSwitchedToUserId));
        } else {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
    }

    /** {@inheritDoc} */
    @Override
    public StatusCheckerResult postExecutionCheck(ITestDevice device)
            throws DeviceNotAvailableException {
        Map<Integer, UserInfo> postUsersInfo = device.getUserInfos();
        UserInfo postCurrentUserInfo = postUsersInfo.get(device.getCurrentUser());

        ArrayList<String> errors = new ArrayList<>();

        if (mPreCurrentUserInfo.userId() != postCurrentUserInfo.userId()) {
            if (postCurrentUserInfo.userId() != mSwitchedToUserId) {
                errors.add(
                        String.format(
                                "User %d was the currentUser before, has changed to %d",
                                mPreCurrentUserInfo.userId(), postCurrentUserInfo.userId()));
            }
            if (mCleanup) {
                if (!device.switchUser(mPreCurrentUserInfo.userId())) {
                    errors.add(
                            String.format(
                                    "Failed to switch back to previous current user %d."
                                            + " Check if it was removed.",
                                    mPreCurrentUserInfo.userId()));
                }
            }
        }

        for (UserInfo preUserInfo : mPreUsersInfo.values()) {
            int preUserId = preUserInfo.userId();
            if (!postUsersInfo.containsKey(preUserId)) {
                errors.add(String.format("User %d no longer exists after test", preUserId));
                continue;
            }

            UserInfo postUserInfo = postUsersInfo.get(preUserId);
            if (preUserInfo.isRunning() != postUserInfo.isRunning()) {
                CLog.w(
                        "User %d running status changed from %b -> %b",
                        preUserId, preUserInfo.isRunning(), postUserInfo.isRunning());
            }
        }

        for (int postUserId : postUsersInfo.keySet()) {
            if (!mPreUsersInfo.containsKey(postUserId)) {
                if (mSwitchedToUserId != postUserId) {
                    errors.add(
                            String.format(
                                    "User %d was created during test and not deleted", postUserId));
                }
                if (mCleanup) {
                    if (!device.removeUser(postUserId)) {
                        errors.add(String.format("Failed to remove new user %d", postUserId));
                    }
                }
            }
        }

        if (errors.size() > 0) {
            return statusFail(String.join("\n", errors));
        } else {
            return new StatusCheckerResult(CheckStatus.SUCCESS);
        }
    }

    /** Return the userId of a matching user, or -1 if none match. */
    private int findMatchingUser(Collection<UserInfo> usersInfo) {
        for (UserInfo userInfo : usersInfo) {
            if (userInfo.isUserType(mUserToSwitchTo, mPreCurrentUserInfo.userId())) {
                return userInfo.userId();
            }
        }
        return -1;
    }

    private static StatusCheckerResult statusFail(String msg) {
        StatusCheckerResult result = new StatusCheckerResult(CheckStatus.FAILED);
        result.setErrorMessage(msg);
        return result;
    }
}
