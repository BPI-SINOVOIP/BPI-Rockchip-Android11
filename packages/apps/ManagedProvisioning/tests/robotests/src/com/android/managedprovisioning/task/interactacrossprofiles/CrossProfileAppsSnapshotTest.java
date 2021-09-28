/*
 * Copyright 2020, The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package com.android.managedprovisioning.task.interactacrossprofiles;

import static com.google.common.truth.Truth.assertThat;
import static org.robolectric.Shadows.shadowOf;

import static org.junit.Assert.fail;

import android.app.admin.DevicePolicyManager;
import android.content.Context;
import android.os.UserManager;

import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.HashSet;
import java.util.Set;

/**
 * Unit-tests for {@link CrossProfileAppsSnapshot}.
 */
@RunWith(RobolectricTestRunner.class)
public class CrossProfileAppsSnapshotTest {
    private static final String TEST_PACKAGE_NAME_1 = "com.test.packagea";
    private static final String TEST_PACKAGE_NAME_2 = "com.test.packageb";
    private static final int TEST_USER_ID = 123;

    private final Context mContext = RuntimeEnvironment.application;
    private final UserManager mUserManager = mContext.getSystemService(UserManager.class);
    private final CrossProfileAppsSnapshot mCrossProfileAppsSnapshot =
            new CrossProfileAppsSnapshot(mContext);
    private final DevicePolicyManager mDevicePolicyManager =
            mContext.getSystemService(DevicePolicyManager.class);

    @Before
    public void setUp() {
        shadowOf(mUserManager).addUser(TEST_USER_ID, "Username", /* flags= */ 0);
    }

    @Test
    public void construct_nullContext_throwsNullPointerException() {
        try {
            new CrossProfileAppsSnapshot(/* context= */ null);
            fail();
        } catch (NullPointerException expected) { }
    }

    @Test
    public void hasSnapshot_noSnapshotTaken_returnsFalse() {
        assertThat(mCrossProfileAppsSnapshot.hasSnapshot(TEST_USER_ID)).isFalse();
    }

    @Test
    public void hasSnapshot_snapshotTaken_returnsTrue() {
        mCrossProfileAppsSnapshot.takeNewSnapshot(TEST_USER_ID);

        assertThat(mCrossProfileAppsSnapshot.hasSnapshot(TEST_USER_ID)).isTrue();
    }

    @Test
    public void getSnapshot_noSnapshotTaken_returnsEmptySet() {
        assertThat(mCrossProfileAppsSnapshot.getSnapshot(TEST_USER_ID)).isEmpty();
    }

    @Test
    public void getSnapshot_snapshotTaken_returnsPreviousCrossProfilePackages() {
        setDefaultCrossProfilePackages(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
        mCrossProfileAppsSnapshot.takeNewSnapshot(TEST_USER_ID);

        assertThat(mCrossProfileAppsSnapshot.getSnapshot(TEST_USER_ID))
                .containsExactly(TEST_PACKAGE_NAME_1, TEST_PACKAGE_NAME_2);
    }

    private void setDefaultCrossProfilePackages(String... defaultCrossProfilePackages) {
        Set<String> packages = new HashSet<>();
        for (String packageName : defaultCrossProfilePackages) {
            packages.add(packageName);
        }

        shadowOf(mDevicePolicyManager).setDefaultCrossProfilePackages(packages);
    }
}
