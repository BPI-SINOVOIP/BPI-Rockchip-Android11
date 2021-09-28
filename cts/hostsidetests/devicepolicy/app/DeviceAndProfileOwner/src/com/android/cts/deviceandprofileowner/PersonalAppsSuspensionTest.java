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

import static com.google.common.truth.Truth.assertThat;

import android.app.admin.DevicePolicyManager;
import android.content.ComponentName;
import android.content.Context;

import androidx.test.InstrumentationRegistry;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test for personal app suspension APIs available to POs on organization owned devices.
 */
@RunWith(AndroidJUnit4.class)
public class PersonalAppsSuspensionTest {
    private static final ComponentName ADMIN = BaseDeviceAdminTest.ADMIN_RECEIVER_COMPONENT;
    private final Context mContext = InstrumentationRegistry.getContext();
    private final DevicePolicyManager mDpm = mContext.getSystemService(DevicePolicyManager.class);

    @Test
    public void testSuspendPersonalApps() {
        mDpm.setPersonalAppsSuspended(ADMIN, true);
        assertThat(mDpm.getPersonalAppsSuspendedReasons(ADMIN))
                .isEqualTo(DevicePolicyManager.PERSONAL_APPS_SUSPENDED_EXPLICITLY);
    }

    @Test
    public void testUnsuspendPersonalApps() {
        mDpm.setPersonalAppsSuspended(ADMIN, false);
        assertThat(mDpm.getPersonalAppsSuspendedReasons(ADMIN))
                .isEqualTo(DevicePolicyManager.PERSONAL_APPS_NOT_SUSPENDED);
    }

    @Test
    public void testSetManagedProfileMaximumTimeOff1Sec() {
        mDpm.setManagedProfileMaximumTimeOff(ADMIN, 1000);
    }

    @Test
    public void testSetManagedProfileMaximumTimeOff() {
        final long timeout = 123456789;
        mDpm.setManagedProfileMaximumTimeOff(ADMIN, timeout);
        assertThat(mDpm.getManagedProfileMaximumTimeOff(ADMIN)).isEqualTo(timeout);
        mDpm.setManagedProfileMaximumTimeOff(ADMIN, 0);
        assertThat(mDpm.getManagedProfileMaximumTimeOff(ADMIN)).isEqualTo(0);
    }

    @Test
    public void testPersonalAppsSuspendedByTimeout() {
        assertThat(mDpm.getPersonalAppsSuspendedReasons(ADMIN))
                .isEqualTo(DevicePolicyManager.PERSONAL_APPS_SUSPENDED_PROFILE_TIMEOUT);
    }
}
