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

package com.android.car.settings.datausage;

import static android.net.TrafficStats.UID_TETHERING;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.net.NetworkStats;

import androidx.lifecycle.Lifecycle;
import androidx.test.core.app.ApplicationProvider;

import com.android.car.settings.common.LogicalPreferenceGroup;
import com.android.car.settings.common.PreferenceControllerTestHelper;
import com.android.car.settings.common.ProgressBarPreference;
import com.android.car.settings.testutils.ShadowUidDetailProvider;
import com.android.car.settings.testutils.ShadowUserManager;
import com.android.settingslib.net.UidDetail;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Unit test for {@link AppDataUsagePreferenceController}. */
@RunWith(RobolectricTestRunner.class)
@Config(shadows = {ShadowUidDetailProvider.class, ShadowUserManager.class})
public class AppDataUsagePreferenceControllerTest {

    private Context mContext;
    private LogicalPreferenceGroup mLogicalPreferenceGroup;
    private AppDataUsagePreferenceController mController;
    private PreferenceControllerTestHelper<AppDataUsagePreferenceController>
            mPreferenceControllerHelper;

    @Mock
    private UidDetail mUidDetail;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = ApplicationProvider.getApplicationContext();
        mLogicalPreferenceGroup = new LogicalPreferenceGroup(mContext);
        mPreferenceControllerHelper = new PreferenceControllerTestHelper<>(mContext,
                AppDataUsagePreferenceController.class, mLogicalPreferenceGroup);
        mController = mPreferenceControllerHelper.getController();

        mPreferenceControllerHelper.markState(Lifecycle.State.CREATED);
    }

    @After
    public void tearDown() {
        ShadowUidDetailProvider.reset();
    }

    @Test
    public void defaultInitialize_hasNoPreference() {
        assertThat(mLogicalPreferenceGroup.getPreferenceCount()).isEqualTo(0);
    }

    @Test
    public void onDataLoaded_dataNotLoaded_hasNoPreference() {
        mController.onDataLoaded(null, new int[0]);

        assertThat(mLogicalPreferenceGroup.getPreferenceCount()).isEqualTo(0);
    }

    @Test
    public void onDataLoaded_statsSizeZero_hasNoPreference() {
        NetworkStats networkStats = new NetworkStats(0, 0);

        mController.onDataLoaded(networkStats, new int[0]);

        assertThat(mLogicalPreferenceGroup.getPreferenceCount()).isEqualTo(0);
    }

    @Test
    public void onDataLoaded_statsLoaded_hasTwoPreference() {
        NetworkStats networkStats = new NetworkStats(0, 0);
        NetworkStats.Entry entry1 = new NetworkStats.Entry();
        entry1.rxBytes = 100;
        NetworkStats.Entry entry2 = new NetworkStats.Entry();
        entry2.uid = UID_TETHERING;
        entry2.rxBytes = 200;

        mController.onDataLoaded(networkStats.addEntry(entry1).addEntry(entry2), new int[0]);

        assertThat(mLogicalPreferenceGroup.getPreferenceCount()).isEqualTo(2);
    }

    @Test
    public void onDataLoaded_statsLoaded_hasOnePreference() {
        ShadowUidDetailProvider.setUidDetail(mUidDetail);
        NetworkStats networkStats = new NetworkStats(0, 0);
        NetworkStats.Entry entry1 = new NetworkStats.Entry();
        entry1.rxBytes = 100;
        NetworkStats.Entry entry2 = new NetworkStats.Entry();
        entry2.uid = UID_TETHERING;
        entry2.rxBytes = 200;

        mController.onDataLoaded(networkStats.addEntry(entry1).addEntry(entry2), new int[0]);

        ProgressBarPreference preference1 =
                (ProgressBarPreference) mLogicalPreferenceGroup.getPreference(0);
        ProgressBarPreference preference2 =
                (ProgressBarPreference) mLogicalPreferenceGroup.getPreference(1);
        assertThat(mLogicalPreferenceGroup.getPreferenceCount()).isEqualTo(2);
        assertThat(preference1.getProgress()).isEqualTo(100);
        assertThat(preference2.getProgress()).isEqualTo(50);
    }
}
