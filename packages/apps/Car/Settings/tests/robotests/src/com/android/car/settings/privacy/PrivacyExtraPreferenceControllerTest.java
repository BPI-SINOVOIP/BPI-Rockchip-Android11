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
package com.android.car.settings.privacy;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.lifecycle.Lifecycle;
import androidx.preference.Preference;
import androidx.preference.PreferenceGroup;

import com.android.car.settings.common.ExtraSettingsLoader;
import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;

import java.util.HashMap;
import java.util.Map;

@RunWith(RobolectricTestRunner.class)
public class PrivacyExtraPreferenceControllerTest {

    private static final Intent FAKE_INTENT = new Intent();

    private Context mContext;
    private PreferenceGroup mPreferenceGroup;
    private PrivacyExtraPreferenceController mController;
    private PreferenceControllerTestHelper<PrivacyExtraPreferenceController>
            mPreferenceControllerHelper;

    @Mock
    private ExtraSettingsLoader mExtraSettingsLoaderMock;

    @Mock
    private Drawable mDrawable;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mContext = RuntimeEnvironment.application;
        mPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mPreferenceGroup.setIntent(FAKE_INTENT);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                PrivacyExtraPreferenceController.class, mPreferenceGroup);
        mController = mPreferenceControllerHelper.getController();
    }

    @Test
    public void onLoadPreferences_noPreferencesHaveIcons() {
        Map<Preference, Bundle> preferenceBundleWithIconsMap = new HashMap<>();
        for (int i = 0; i < 3; i++) {
            Preference pref = new Preference(mContext);
            pref.setIcon(mDrawable);
            preferenceBundleWithIconsMap.put(pref, new Bundle());
        }

        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                preferenceBundleWithIconsMap);

        mController.setExtraSettingsLoader(mExtraSettingsLoaderMock);
        mPreferenceControllerHelper.markState(Lifecycle.State.CREATED);
        mController.refreshUi();

        for (int i = 0; i < mPreferenceGroup.getPreferenceCount(); i++) {
            assertThat(mPreferenceGroup.getPreference(i).getIcon()).isNull();
        }
    }
}
