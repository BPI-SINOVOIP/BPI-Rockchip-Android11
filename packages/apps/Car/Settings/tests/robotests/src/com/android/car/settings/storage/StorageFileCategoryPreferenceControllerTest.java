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

package com.android.car.settings.storage;

import static android.os.storage.VolumeInfo.MOUNT_FLAG_PRIMARY;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.ApplicationInfo;
import android.content.pm.ResolveInfo;
import android.os.storage.VolumeInfo;

import androidx.lifecycle.Lifecycle;

import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.common.ProgressBarPreference;
import com.android.car.settings.testutils.ShadowStorageManagerVolumeProvider;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowApplicationPackageManager;

/** Unit test for {@link StorageFileCategoryPreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowStorageManagerVolumeProvider.class})
public class StorageFileCategoryPreferenceControllerTest {


    private Context mContext;
    private ProgressBarPreference mProgressBarPreference;
    private PreferenceControllerTestHelper<StorageFileCategoryPreferenceController>
            mPreferenceControllerHelper;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mProgressBarPreference = new ProgressBarPreference(mContext);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                StorageFileCategoryPreferenceController.class, mProgressBarPreference);
    }

    @After
    public void tearDown() {
        ShadowStorageManagerVolumeProvider.reset();
        ShadowApplicationPackageManager.reset();
    }

    @Test
    public void onCreate_nonResolvableIntent_notSelectable() {
        VolumeInfo volumeInfo = new VolumeInfo("id", VolumeInfo.TYPE_EMULATED, null, "id");
        volumeInfo.mountFlags = MOUNT_FLAG_PRIMARY;
        ShadowStorageManagerVolumeProvider.setVolumeInfo(volumeInfo);

        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mProgressBarPreference.isSelectable()).isFalse();
    }

    @Test
    public void onCreate_resolvableIntent_selectable() {
        VolumeInfo volumeInfo = new VolumeInfo("id", VolumeInfo.TYPE_EMULATED, null, "id");
        volumeInfo.mountFlags = MOUNT_FLAG_PRIMARY;
        Intent browseIntent = volumeInfo.buildBrowseIntent();
        ShadowStorageManagerVolumeProvider.setVolumeInfo(volumeInfo);

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = "com.test.package.name";
        activityInfo.name = "ClassName";
        activityInfo.applicationInfo = new ApplicationInfo();
        activityInfo.applicationInfo.packageName = "com.test.package.name";

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.system = true;
        resolveInfo.activityInfo = activityInfo;
        getShadowPackageManager().addResolveInfoForIntent(browseIntent, resolveInfo);

        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);

        assertThat(mProgressBarPreference.isSelectable()).isTrue();
    }

    @Test
    public void handlePreferenceClicked_currentUserAndNoActivityToHandleIntent_doesNotThrow() {
        VolumeInfo volumeInfo = new VolumeInfo("id", VolumeInfo.TYPE_EMULATED, null, "id");
        volumeInfo.mountFlags = MOUNT_FLAG_PRIMARY;
        ShadowStorageManagerVolumeProvider.setVolumeInfo(volumeInfo);

        mProgressBarPreference.performClick();

        assertThat(ShadowApplication.getInstance().getNextStartedActivity()).isNull();
    }

    @Test
    public void handlePreferenceClicked_currentUserAndActivityToHandleIntent_startsNewActivity() {
        VolumeInfo volumeInfo = new VolumeInfo("id", VolumeInfo.TYPE_EMULATED, null, "id");
        volumeInfo.mountFlags = MOUNT_FLAG_PRIMARY;
        Intent browseIntent = volumeInfo.buildBrowseIntent();
        ShadowStorageManagerVolumeProvider.setVolumeInfo(volumeInfo);

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.packageName = "com.test.package.name";
        activityInfo.name = "ClassName";
        activityInfo.applicationInfo = new ApplicationInfo();
        activityInfo.applicationInfo.packageName = "com.test.package.name";

        ResolveInfo resolveInfo = new ResolveInfo();
        resolveInfo.system = true;
        resolveInfo.activityInfo = activityInfo;
        getShadowPackageManager().addResolveInfoForIntent(browseIntent, resolveInfo);

        mPreferenceControllerHelper.sendLifecycleEvent(Lifecycle.Event.ON_CREATE);
        mProgressBarPreference.performClick();

        Intent intent = ShadowApplication.getInstance().getNextStartedActivity();
        assertThat(intent.getAction()).isEqualTo(browseIntent.getAction());
        assertThat(intent.getData()).isEqualTo(browseIntent.getData());
    }

    private ShadowApplicationPackageManager getShadowPackageManager() {
        return (ShadowApplicationPackageManager) Shadows.shadowOf(mContext.getPackageManager());
    }
}
