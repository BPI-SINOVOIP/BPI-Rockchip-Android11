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

import static com.android.car.settings.common.ExtraSettingsPreferenceController.META_DATA_DISTRACTION_OPTIMIZED;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_ICON_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_SUMMARY_URI;
import static com.android.settingslib.drawer.TileUtils.META_DATA_PREFERENCE_TITLE_URI;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.lifecycle.LifecycleOwner;
import androidx.preference.Preference;
import androidx.preference.PreferenceManager;
import androidx.preference.PreferenceScreen;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;
import com.android.car.settings.testutils.ResourceTestUtils;
import com.android.car.settings.testutils.TestContentProvider;
import com.android.car.ui.preference.CarUiPreference;
import com.android.car.ui.preference.UxRestrictablePreference;
import com.android.settingslib.core.lifecycle.Lifecycle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import java.util.HashMap;
import java.util.Map;

@RunWith(AndroidJUnit4.class)
public class ExtraSettingsPreferenceControllerTest {
    private static final Intent FAKE_INTENT = new Intent();
    private static final CarUxRestrictions NO_SETUP_UX_RESTRICTIONS =
            new CarUxRestrictions.Builder(/* reqOpt= */ true,
                    CarUxRestrictions.UX_RESTRICTIONS_NO_SETUP, /* timestamp= */ 0).build();

    private static final CarUxRestrictions BASELINE_UX_RESTRICTIONS =
            new CarUxRestrictions.Builder(/* reqOpt= */ true,
                    CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

    private static final CarUxRestrictions NO_UX_RESTRICTIONS =
            new CarUxRestrictions.Builder(/* reqOpt= */ false,
                    CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

    private static final String TEST_PROVIDER =
            "content://com.android.car.settings.testutils.TestContentProvider";

    private LifecycleOwner mLifecycleOwner;
    private Lifecycle mLifecycle;

    private Context mContext = ApplicationProvider.getApplicationContext();
    private PreferenceManager mPreferenceManager;
    private PreferenceScreen mScreen;
    private FakeExtraSettingsPreferenceController mPreferenceController;
    private CarUiPreference mPreference;
    private Map<Preference, Bundle> mPreferenceBundleMap;
    private Bundle mMetaData;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private ExtraSettingsLoader mExtraSettingsLoaderMock;

    @Before
    @UiThreadTest
    public void setUp() {
        mLifecycleOwner = () -> mLifecycle;
        mLifecycle = new Lifecycle(mLifecycleOwner);

        MockitoAnnotations.initMocks(this);

        mPreferenceController = new FakeExtraSettingsPreferenceController(mContext,
                /* preferenceKey= */ "key", mFragmentController,
                BASELINE_UX_RESTRICTIONS);

        mPreferenceManager = new PreferenceManager(mContext);
        mScreen = mPreferenceManager.createPreferenceScreen(mContext);
        mScreen.setIntent(FAKE_INTENT);
        mPreferenceController.setPreference(mScreen);
        mPreference = spy(new CarUiPreference(mContext));
        mPreference.setIntent(new Intent().setPackage("com.android.car.settings"));

        mPreferenceBundleMap = new HashMap<>();
        mMetaData = new Bundle();
    }

    @Test
    public void onUxRestrictionsChanged_restricted_preferenceRestricted() {
        mMetaData.putBoolean(META_DATA_DISTRACTION_OPTIMIZED, false);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(true);
    }

    @Test
    public void onUxRestrictionsChanged_unrestricted_preferenceUnrestricted() {
        mMetaData.putBoolean(META_DATA_DISTRACTION_OPTIMIZED, false);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(false);
    }

    @Test
    public void onUxRestrictionsChanged_restricted_viewOnly_preferenceUnrestricted() {
        mMetaData.putBoolean(META_DATA_DISTRACTION_OPTIMIZED, false);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.setAvailabilityStatus(PreferenceController.AVAILABLE_FOR_VIEWING);
        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(false);
    }

    @Test
    public void onUxRestrictionsChanged_distractionOptimized_preferenceUnrestricted() {
        mMetaData.putBoolean(META_DATA_DISTRACTION_OPTIMIZED, true);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        Mockito.reset(mPreference);
        mPreferenceController.onUxRestrictionsChanged(NO_SETUP_UX_RESTRICTIONS);

        verify((UxRestrictablePreference) mPreference).setUxRestricted(false);
    }

    @Test
    public void onCreate_hasDynamicTitleData_placeholderAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_TITLE_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mPreference).setTitle(
                ResourceTestUtils.getString(mContext, "empty_placeholder"));
    }

    @Test
    public void onCreate_hasDynamicSummaryData_placeholderAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_SUMMARY_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mPreference).setSummary(
                ResourceTestUtils.getString(mContext, "empty_placeholder"));
    }

    @Test
    public void onCreate_hasDynamicIconData_placeholderAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_ICON_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        verify(mPreference).setIcon(R.drawable.ic_placeholder);
    }

    @Test
    public void onCreate_hasDynamicTitleData_TitleSet() {
        mMetaData.putString(META_DATA_PREFERENCE_TITLE_URI,
                TEST_PROVIDER + "/getText/textKey");

        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(mPreference.getTitle()).isEqualTo(TestContentProvider.TEST_TEXT_CONTENT);
    }

    @Test
    public void onCreate_hasDynamicSummaryData_summarySet() {
        mMetaData.putString(META_DATA_PREFERENCE_SUMMARY_URI,
                TEST_PROVIDER + "/getText/textKey");
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(mPreference.getSummary()).isEqualTo(TestContentProvider.TEST_TEXT_CONTENT);
    }

    @Test
    public void onCreate_hasDynamicIconData_iconSet() {
        mMetaData.putString(META_DATA_PREFERENCE_ICON_URI,
                TEST_PROVIDER + "/getIcon/iconKey");

        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);

        InOrder inOrder = inOrder(mPreference);
        inOrder.verify(mPreference).setIcon(R.drawable.ic_placeholder);
        inOrder.verify(mPreference).setIcon(any(Drawable.class));
    }

    @Test
    public void onStart_hasDynamicTitleData_observerAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_TITLE_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);

        assertThat(mPreferenceController.mObservers.size()).isEqualTo(1);
    }

    @Test
    public void onStart_hasDynamicSummaryData_observerAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_SUMMARY_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);

        assertThat(mPreferenceController.mObservers.size()).isEqualTo(1);
    }

    @Test
    public void onStart_hasDynamicIconData_observerAdded() {
        mMetaData.putString(META_DATA_PREFERENCE_ICON_URI, TEST_PROVIDER);
        mPreferenceBundleMap.put(mPreference, mMetaData);
        when(mExtraSettingsLoaderMock.loadPreferences(FAKE_INTENT)).thenReturn(
                mPreferenceBundleMap);
        mPreferenceController.setExtraSettingsLoader(mExtraSettingsLoaderMock);

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);

        assertThat(mPreferenceController.mObservers.size()).isEqualTo(1);
    }

    private static class FakeExtraSettingsPreferenceController extends
            ExtraSettingsPreferenceController {

        private int mAvailabilityStatus;

        FakeExtraSettingsPreferenceController(Context context, String preferenceKey,
                FragmentController fragmentController, CarUxRestrictions uxRestrictions) {
            super(context, preferenceKey, fragmentController, uxRestrictions);
            mAvailabilityStatus = AVAILABLE;
        }

        @Override
        void executeBackgroundTask(Runnable r) {
            // run task immediately on main thread
            r.run();
        }

        @Override
        void executeUiTask(Runnable r) {
            // run task immediately on main thread
            r.run();
        }

        @Override
        protected int getAvailabilityStatus() {
            return mAvailabilityStatus;
        }

        public void setAvailabilityStatus(int availabilityStatus) {
            mAvailabilityStatus = availabilityStatus;
        }
    }
}
