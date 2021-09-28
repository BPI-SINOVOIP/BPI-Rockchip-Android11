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

package com.android.car.settings.common;

import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE_URI;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.os.Bundle;

import androidx.preference.Preference;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.Collections;
import java.util.Map;

@RunWith(AndroidJUnit4.class)
public class ExtraSettingsLoaderTest {
    private static final String META_DATA_PREFERENCE_CATEGORY = "com.android.settings.category";
    private static final String FAKE_CATEGORY = "fake_category";
    private static final String FAKE_TITLE = "fake_title";
    private static final String FAKE_SUMMARY = "fake_summary";
    private static final String TEST_CONTENT_PROVIDER =
            "content://com.android.car.settings.testutils.TestContentProvider";

    private Context mContext = ApplicationProvider.getApplicationContext();
    private ExtraSettingsLoader mExtraSettingsLoader;

    @Mock
    private PackageManager mPm;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mExtraSettingsLoader = new ExtraSettingsLoader(mContext);
        mExtraSettingsLoader.setPackageManager(mPm);
    }

    @Test
    public void testLoadPreference_uriResources_shouldNotLoadStaticResources() {
        Intent intent = new Intent();
        intent.putExtra(META_DATA_PREFERENCE_CATEGORY, FAKE_CATEGORY);
        Bundle bundle = new Bundle();
        bundle.putString(META_DATA_PREFERENCE_TITLE, FAKE_TITLE);
        bundle.putString(META_DATA_PREFERENCE_SUMMARY, FAKE_SUMMARY);
        bundle.putString(META_DATA_PREFERENCE_CATEGORY, FAKE_CATEGORY);
        bundle.putString(META_DATA_PREFERENCE_TITLE_URI, TEST_CONTENT_PROVIDER);
        bundle.putString(META_DATA_PREFERENCE_SUMMARY_URI, TEST_CONTENT_PROVIDER);
        bundle.putString(META_DATA_PREFERENCE_ICON_URI, TEST_CONTENT_PROVIDER);

        ActivityInfo activityInfo = new ActivityInfo();
        activityInfo.metaData = bundle;
        activityInfo.packageName = "package_name";
        activityInfo.name = "class_name";

        ResolveInfo resolveInfoSystem = new ResolveInfo();
        resolveInfoSystem.system = true;
        resolveInfoSystem.activityInfo = activityInfo;

        when(mPm.queryIntentActivitiesAsUser(eq(intent), eq(PackageManager.GET_META_DATA),
                anyInt())).thenReturn(Collections.singletonList(resolveInfoSystem));
        Map<Preference, Bundle> preferenceToBundleMap = mExtraSettingsLoader.loadPreferences(
                intent);

        assertThat(preferenceToBundleMap).hasSize(1);

        for (Preference p : preferenceToBundleMap.keySet()) {
            assertThat(p.getTitle()).isNull();
            assertThat(p.getSummary()).isNull();
            assertThat(p.getIcon()).isNull();
        }
    }
}
