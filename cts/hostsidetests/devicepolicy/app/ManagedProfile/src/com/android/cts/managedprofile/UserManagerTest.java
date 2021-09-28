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

package com.android.cts.managedprofile;

import static com.google.common.truth.Truth.assertThat;

import android.app.UiAutomation;
import android.os.Process;
import android.os.UserHandle;
import android.os.UserManager;
import android.test.AndroidTestCase;

import androidx.test.platform.app.InstrumentationRegistry;

import java.util.HashSet;
import java.util.List;

public class UserManagerTest extends AndroidTestCase {

    private UserManager mUserManager;
    private UiAutomation mUiAutomation;

    @Override
    protected void setUp() throws Exception {
        super.setUp();
        mUserManager = mContext.getSystemService(UserManager.class);
        mUiAutomation = InstrumentationRegistry.getInstrumentation().getUiAutomation();
    }

    @Override
    protected void tearDown() throws Exception {
        mUiAutomation.dropShellPermissionIdentity();
        super.tearDown();
    }

    public void testIsManagedProfileReturnsTrue() {
        assertTrue(mUserManager.isManagedProfile());
    }

    public void testIsManagedProfileReturnsFalse() {
        assertFalse(mUserManager.isManagedProfile());
    }

    public void testGetAllProfiles() {
        List<UserHandle> profiles = mUserManager.getAllProfiles();
        assertThat(profiles).hasSize(2);
        assertThat(profiles).contains(Process.myUserHandle());
    }

    public void testCreateProfile_managedProfile() {
        mUiAutomation.adoptShellPermissionIdentity("android.permission.CREATE_USERS");

        UserHandle newProfile = mUserManager.createProfile("testProfile1",
                UserManager.USER_TYPE_PROFILE_MANAGED, new HashSet<String>());
        assertThat(newProfile).isNotNull();

        List<UserHandle> profiles = mUserManager.getAllProfiles();
        assertThat(profiles).contains(newProfile);
    }

    /** This test should be run as the managed profile
     *  by com.android.cts.devicepolicy.ManagedProfileTest
     */
    public void testIsProfileReturnsTrue_runAsProfile() {
        mUiAutomation.adoptShellPermissionIdentity("android.permission.INTERACT_ACROSS_USERS");
        assertThat(mUserManager.isProfile()).isTrue();
    }

    /** This test should be run as the parent profile
     *  by com.android.cts.devicepolicy.ManagedProfileTest
     */
    public void testIsProfileReturnsFalse_runAsPrimary() {
        mUiAutomation.adoptShellPermissionIdentity("android.permission.INTERACT_ACROSS_USERS");
        assertThat(mUserManager.isProfile()).isFalse();
    }
}
