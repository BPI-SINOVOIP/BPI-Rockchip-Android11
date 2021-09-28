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

import android.app.admin.DevicePolicyManager;
import android.os.UserManager;

public class DevicePolicyLoggingParentTest extends BaseDeviceAdminTest {

    private DevicePolicyManager mParentDevicePolicyManager;

    @Override
    protected void setUp() throws Exception {
        super.setUp();

        mParentDevicePolicyManager =
                mDevicePolicyManager.getParentProfileInstance(ADMIN_RECEIVER_COMPONENT);
    }

    public void testCameraDisabledLogged() {
        mParentDevicePolicyManager.setCameraDisabled(ADMIN_RECEIVER_COMPONENT, true);
        mParentDevicePolicyManager.setCameraDisabled(ADMIN_RECEIVER_COMPONENT, false);
    }

    public void testUserRestrictionLogged() {
        mParentDevicePolicyManager.addUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONFIG_LOCATION);
        mParentDevicePolicyManager.clearUserRestriction(ADMIN_RECEIVER_COMPONENT,
                UserManager.DISALLOW_CONFIG_LOCATION);
    }

}
