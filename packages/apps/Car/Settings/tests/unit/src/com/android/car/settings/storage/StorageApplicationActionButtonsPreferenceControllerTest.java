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

package com.android.car.settings.storage;

import static com.android.car.settings.common.ActionButtonsPreference.ActionButtons;
import static com.android.car.settings.storage.StorageApplicationActionButtonsPreferenceController.CONFIRM_CLEAR_STORAGE_DIALOG_TAG;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyString;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.testng.Assert.assertThrows;

import android.app.usage.StorageStats;
import android.car.drivingstate.CarUxRestrictions;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;

import androidx.fragment.app.DialogFragment;
import androidx.lifecycle.LifecycleOwner;
import androidx.loader.app.LoaderManager;
import androidx.test.annotation.UiThreadTest;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.settings.R;
import com.android.car.settings.common.ActionButtonInfo;
import com.android.car.settings.common.ActionButtonsPreference;
import com.android.car.settings.common.FragmentController;
import com.android.car.settings.common.PreferenceControllerTestUtil;
import com.android.car.settings.testutils.ResourceTestUtils;
import com.android.car.settings.testutils.TestLifecycleOwner;
import com.android.settingslib.RestrictedLockUtils;
import com.android.settingslib.applications.ApplicationsState;
import com.android.settingslib.applications.StorageStatsSource;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

@RunWith(AndroidJUnit4.class)
public class StorageApplicationActionButtonsPreferenceControllerTest {

    private static final String PACKAGE_NAME = "Test Package Name";
    private static final String TEST_MANAGE_STORAGE_ACTIVITY = "TestActivity";
    private static final String SOURCE = "source";
    private static final int UID = 12;
    private static final String LABEL = "label";
    private static final String SIZE_STR = "12.34 MB";

    private Context mContext = ApplicationProvider.getApplicationContext();
    private LifecycleOwner mLifecycleOwner;
    private ActionButtonsPreference mActionButtonsPreference;
    private StorageApplicationActionButtonsPreferenceController mPreferenceController;
    private CarUxRestrictions mCarUxRestrictions;

    @Mock
    private FragmentController mFragmentController;
    @Mock
    private ApplicationsState.AppEntry mMockAppEntry;
    @Mock
    private AppsStorageStatsManager mMockAppsStorageStatsManager;
    @Mock
    private LoaderManager mMockLoaderManager;
    @Mock
    private PackageManager mMockPm;
    @Mock
    private RestrictedLockUtils.EnforcedAdmin mEnforcedAdmin;

    @Before
    @UiThreadTest // required because controller uses a handler
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mLifecycleOwner = new TestLifecycleOwner();

        ApplicationInfo appInfo = new ApplicationInfo();
        appInfo.uid = UID;
        appInfo.sourceDir = SOURCE;
        appInfo.packageName = PACKAGE_NAME;
        appInfo.manageSpaceActivityName = TEST_MANAGE_STORAGE_ACTIVITY;

        ApplicationsState.AppEntry appEntry = new ApplicationsState.AppEntry(mContext, appInfo,
                1234L);
        appEntry.label = LABEL;
        appEntry.sizeStr = SIZE_STR;
        appEntry.icon = mContext.getDrawable(R.drawable.test_icon);
        appEntry.info.packageName = PACKAGE_NAME;

        mCarUxRestrictions = new CarUxRestrictions.Builder(/* reqOpt= */ true,
                CarUxRestrictions.UX_RESTRICTIONS_BASELINE, /* timestamp= */ 0).build();

        mActionButtonsPreference = new ActionButtonsPreference(mContext);
        mPreferenceController = new StorageApplicationActionButtonsPreferenceController(mContext,
                /* preferenceKey= */ "key", mFragmentController, mCarUxRestrictions);

        mPreferenceController.setPackageManager(mMockPm);
    }

    @Test
    public void testCheckInitialized_noAppEntry_throwException() {
        mPreferenceController.setPackageName(PACKAGE_NAME).setAppsStorageStatsManager(
                mMockAppsStorageStatsManager).setLoaderManager(mMockLoaderManager);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void testCheckInitialized_noPackageNameEntry_throwException() {
        mPreferenceController.setAppEntry(mMockAppEntry).setAppsStorageStatsManager(
                mMockAppsStorageStatsManager).setLoaderManager(mMockLoaderManager);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void testCheckInitialized_noAppsStorageStatsManagerEntry_throwException() {
        mPreferenceController.setAppEntry(mMockAppEntry).setPackageName(
                PACKAGE_NAME).setLoaderManager(mMockLoaderManager);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void testCheckInitialized_noLoaderManager_throwException() {
        mPreferenceController.setAppEntry(mMockAppEntry).setPackageName(
                PACKAGE_NAME).setAppsStorageStatsManager(mMockAppsStorageStatsManager);
        assertThrows(IllegalStateException.class,
                () -> PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                        mActionButtonsPreference));
    }

    @Test
    public void onCreate_defaultStatus_shouldShowCacheButtons() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getClearCacheButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "storage_clear_cache_btn_text"));
    }

    @Test
    public void onCreate_defaultStatus_shouldShowClearStorageButtons() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        assertThat(getClearStorageButton().getText()).isEqualTo(
                ResourceTestUtils.getString(mContext, "storage_clear_user_data_text"));
    }

    @Test
    public void handleClearCacheClick_disallowedBySystem_shouldNotDeleteApplicationCache() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(false);

        doNothing().when(mMockPm).deleteApplicationCacheFiles(anyString(), any());

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 0;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);
        getClearCacheButton().getOnClickListener().onClick(/* view= */ null);

        verify(mMockPm, never()).deleteApplicationCacheFiles(anyString(), any());
    }

    @Test
    public void handleClearCacheClick_allowedBySystem_shouldNotDeleteApplicationCache() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(true);

        doNothing().when(mMockPm).deleteApplicationCacheFiles(anyString(), any());

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 0;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);
        getClearCacheButton().getOnClickListener().onClick(/* view= */ null);

        verify(mMockPm).deleteApplicationCacheFiles(anyString(), any());
    }

    @Test
    public void handleClearDataClick_disallowedBySystem_shouldNotShowDialogToClear() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(false);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 10;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);

        assertThat(getClearStorageButton().isEnabled()).isFalse();
    }

    @Test
    public void handleClearDataClick_allowedBySystem_shouldShowDialogToClear() {
        mMockAppEntry.info = new ApplicationInfo();
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(true);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);
        getClearStorageButton().getOnClickListener().onClick(/* view= */ null);

        verify(mFragmentController).showDialog(any(DialogFragment.class),
                eq(CONFIRM_CLEAR_STORAGE_DIALOG_TAG));
    }

    @Test
    public void handleClearDataClick_hasValidManageSpaceActivity_shouldNotShowDialogToClear() {
        ApplicationInfo info = new ApplicationInfo();
        info.packageName = PACKAGE_NAME;
        info.manageSpaceActivityName = TEST_MANAGE_STORAGE_ACTIVITY;
        mMockAppEntry.info = info;
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(true);

        when(mMockPm.resolveActivity(any(Intent.class), eq(0))).thenReturn(new ResolveInfo());

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);
        getClearStorageButton().getOnClickListener().onClick(/* view= */ null);

        verify(mFragmentController, never()).showDialog(any(DialogFragment.class),
                eq(CONFIRM_CLEAR_STORAGE_DIALOG_TAG));
    }

    @Test
    public void handleClearDataClick_hasInvalidManageSpaceActivity_shouldShowDialogToClear() {
        ApplicationInfo info = new ApplicationInfo();
        info.packageName = PACKAGE_NAME;
        info.manageSpaceActivityName = TEST_MANAGE_STORAGE_ACTIVITY;
        mMockAppEntry.info = info;
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);
        mPreferenceController.onStart(mLifecycleOwner);
        mPreferenceController.setAppsControlDisallowedAdmin(mEnforcedAdmin);
        mPreferenceController.setAppsControlDisallowedBySystem(true);

        when(mMockPm.resolveActivity(any(Intent.class), eq(0))).thenReturn(null);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);
        getClearStorageButton().getOnClickListener().onClick(/* view= */ null);

        verify(mFragmentController).showDialog(any(DialogFragment.class),
                eq(CONFIRM_CLEAR_STORAGE_DIALOG_TAG));
    }

    @Test
    public void onDataLoaded_noResult_buttonsShouldBeDisabled() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        mPreferenceController.onDataLoaded(null, false, false);

        assertThat(getClearCacheButton().isEnabled()).isFalse();
        assertThat(getClearStorageButton().isEnabled()).isFalse();
    }

    @Test
    public void onDataLoaded_resultLoaded_cacheButtonsShouldBeEnabled() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);

        assertThat(getClearCacheButton().isEnabled()).isTrue();
    }

    @Test
    public void onDataLoaded_resultLoaded_dataButtonsShouldBeEnabled() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);

        assertThat(getClearStorageButton().isEnabled()).isTrue();
    }

    @Test
    public void updateUiWithSize_resultLoaded_cacheButtonDisabledAndDataButtonsEnabled() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 50;
        stats.cacheBytes = 0;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);

        assertThat(getClearCacheButton().isEnabled()).isFalse();
        assertThat(getClearStorageButton().isEnabled()).isTrue();
    }

    @Test
    public void onDataLoaded_resultLoaded_cacheButtonEnabledAndDataButtonDisabled() {
        setupAndAssignPreference();

        mPreferenceController.onCreate(mLifecycleOwner);

        StorageStats stats = new StorageStats();
        stats.codeBytes = 100;
        stats.dataBytes = 10;
        stats.cacheBytes = 10;
        StorageStatsSource.AppStorageStats storageStats =
                new StorageStatsSource.AppStorageStatsImpl(stats);

        mPreferenceController.onDataLoaded(storageStats, false, false);

        assertThat(getClearCacheButton().isEnabled()).isTrue();
        assertThat(getClearStorageButton().isEnabled()).isFalse();
    }

    private void setupAndAssignPreference() {
        mPreferenceController.setAppEntry(mMockAppEntry).setPackageName(
                PACKAGE_NAME).setAppsStorageStatsManager(mMockAppsStorageStatsManager)
                .setLoaderManager(mMockLoaderManager);
        PreferenceControllerTestUtil.assignPreference(mPreferenceController,
                mActionButtonsPreference);
    }

    private ActionButtonInfo getClearCacheButton() {
        return mActionButtonsPreference.getButton(ActionButtons.BUTTON2);
    }

    private ActionButtonInfo getClearStorageButton() {
        return mActionButtonsPreference.getButton(ActionButtons.BUTTON1);
    }
}
