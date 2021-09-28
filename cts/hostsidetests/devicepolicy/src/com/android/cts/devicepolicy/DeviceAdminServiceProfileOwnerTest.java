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
 * limitations under the License.
 */
package com.android.cts.devicepolicy;

import android.platform.test.annotations.LargeTest;

import org.junit.Test;

public class DeviceAdminServiceProfileOwnerTest extends BaseDeviceAdminServiceTest {

    private int mUserId;

    @Override
    protected int getUserId() {
        return mUserId;
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        if (isTestEnabled()) {
            mUserId = createUser();
        }
    }

    @Override
    public void tearDown() throws Exception {
        super.tearDown();
    }

    @Override
    protected boolean isTestEnabled() {
        return mHasFeature && mSupportsMultiUser;
    }

    @Override
    protected void setAsOwnerOrFail(String component) throws Exception {
        setProfileOwnerOrFail(component, getUserId());
    }

    @Override
    @LargeTest
    @Test
    public void testAll() throws Throwable {
        super.testAll();
    }
}
