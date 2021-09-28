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

package com.android.tradefed.targetprep;

import static com.google.common.base.MoreObjects.firstNonNull;

import com.android.tradefed.config.OptionClass;
import com.android.tradefed.device.DeviceNotAvailableException;
import com.android.tradefed.device.ITestDevice;
import com.android.tradefed.invoker.TestInformation;
import com.android.tradefed.log.LogUtil.CLog;

/** A {@link ITargetPreparer} that removes secondary users on teardown. */
@OptionClass(alias = "user-cleaner")
public class UserCleaner extends BaseTargetPreparer {

    @Override
    public void setUp(TestInformation testInfo)
            throws TargetSetupError, BuildError, DeviceNotAvailableException {
        // no-op
    }

    @Override
    public void tearDown(TestInformation testInfo, Throwable e) throws DeviceNotAvailableException {
        ITestDevice device = testInfo.getDevice();
        // treat current user as primary if no primary user found
        int ownerId = firstNonNull(device.getPrimaryUserId(), device.getCurrentUser());
        device.switchUser(ownerId);

        for (Integer id : device.listUsers()) {
            if (id != ownerId && !device.removeUser(id)) {
                CLog.w("Failed to remove user %d", id);
            }
        }
    }
}
