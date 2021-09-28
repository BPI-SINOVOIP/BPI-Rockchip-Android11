/*
 * Copyright (C) 2020  The Android Open Source Project
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
 * limitations under the License
 */

package android.cts.backup.preservedsettingsapp;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static com.google.common.truth.Truth.assertThat;

import android.app.UiAutomation;
import android.content.ContentResolver;
import android.content.Context;
import android.content.SharedPreferences;
import android.platform.test.annotations.AppModeFull;
import android.provider.Settings;
import android.util.FeatureFlagUtils;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;

/**
 * Device side routines to be invoked by the host side PreservedSettingsRestoreHostSideTest. These
 * are not designed to be called in any other way, as they rely on state set up by the host side
 * test.
 */
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class PreservedSettingsRestoreTest {
    private static final String SHARED_PREFS_NAME = "preserve_settings_restore_test_prefs";
    private static final String OVERRIDEABLE_SETTING =
            Settings.Secure.ACCESSIBILITY_CAPTIONING_WINDOW_COLOR;
    private static final String NON_OVERRIDEABLE_SETTING =
            Settings.Secure.ACCESSIBILITY_CAPTIONING_FONT_SCALE;
    private static final String OVERRIDEABLE_SETTING_OLD_VALUE = "121";
    private static final String NON_OVERRIDEABLE_SETTING_OLD_VALUE = "0.6";
    private static final String OVERRIDEABLE_SETTING_NEW_VALUE = "123";
    private static final String NON_OVERRIDEABLE_SETTING_NEW_VALUE = "0.7";
    private static final String DEFAULT_SETTING_VALUE = "";

    private Context mContext;
    private ContentResolver mContentResolver;
    private File mSharedPreferencesFile;
    private SharedPreferences mSharedPreferences;
    private UiAutomation mUiAutomation;

    @Before
    public void setUp() throws Exception {
        mContext = getInstrumentation().getTargetContext();
        mContentResolver = mContext.getContentResolver();
        mSharedPreferencesFile = new File(mContext.getDataDir(), SHARED_PREFS_NAME);
        mSharedPreferencesFile.createNewFile();
        mSharedPreferences = mContext.getSharedPreferences(mSharedPreferencesFile,
                Context.MODE_PRIVATE);
        mUiAutomation = getInstrumentation().getUiAutomation();

        mUiAutomation.adoptShellPermissionIdentity();
    }

    @After
    public void tearDown() {
        mUiAutomation.dropShellPermissionIdentity();
    }

    @Test
    public void testOnlyOverrideableSettingsAreOverridden() {
        // The overrideable setting should have its original value that was backed up.
        assertThat(Settings.Secure.getString(mContentResolver, OVERRIDEABLE_SETTING)).isEqualTo(
                OVERRIDEABLE_SETTING_OLD_VALUE);
        // The non-overrideable setting should keep it current value and be unaffected by restore.
        assertThat(Settings.Secure.getString(mContentResolver, NON_OVERRIDEABLE_SETTING)).isEqualTo(
                NON_OVERRIDEABLE_SETTING_NEW_VALUE);
    }

    @Test
    public void modifySettings() {
        Settings.Secure.putString(mContentResolver, OVERRIDEABLE_SETTING,
                OVERRIDEABLE_SETTING_NEW_VALUE, /* overrideableByRestore */ true);
        Settings.Secure.putString(mContentResolver, NON_OVERRIDEABLE_SETTING,
                NON_OVERRIDEABLE_SETTING_NEW_VALUE);
    }

    @Test
    public void setupDevice() {
        setOriginalSettingValue(OVERRIDEABLE_SETTING, Settings.Secure.getString(mContentResolver,
                OVERRIDEABLE_SETTING));
        setOriginalSettingValue(NON_OVERRIDEABLE_SETTING,
                Settings.Secure.getString(mContentResolver, NON_OVERRIDEABLE_SETTING));

        // Enable the preserve settings feature.
        setOriginalSettingValue(FeatureFlagUtils.SETTINGS_DO_NOT_RESTORE_PRESERVED,
                String.valueOf(FeatureFlagUtils.isEnabled(mContext,
                        FeatureFlagUtils.SETTINGS_DO_NOT_RESTORE_PRESERVED)));
        Settings.Global.putString(mContentResolver,
                FeatureFlagUtils.SETTINGS_DO_NOT_RESTORE_PRESERVED, Boolean.TRUE.toString());

        Settings.Secure.putString(mContentResolver, OVERRIDEABLE_SETTING,
                OVERRIDEABLE_SETTING_OLD_VALUE, /* overrideableByRestore */ true);
        Settings.Secure.putString(mContentResolver, NON_OVERRIDEABLE_SETTING,
                NON_OVERRIDEABLE_SETTING_OLD_VALUE);
    }

    @Test
    public void cleanupDevice() throws Exception {
        // "Touch" the affected settings so that we're the last package that modified them.
        modifySettings();
        // Reset all secure settings modified by this package.
        Settings.Secure.resetToDefaults(mContentResolver, null);

        Settings.Secure.putString(mContentResolver, OVERRIDEABLE_SETTING,
                getOriginalSettingValue(OVERRIDEABLE_SETTING),
                /* overrideableByRestore */ true);
        Settings.Secure.putString(mContentResolver, NON_OVERRIDEABLE_SETTING,
                getOriginalSettingValue(NON_OVERRIDEABLE_SETTING),
                /* overrideableByRestore */ true);

        Settings.Global.putString(mContentResolver,
                FeatureFlagUtils.SETTINGS_DO_NOT_RESTORE_PRESERVED,
                getOriginalSettingValue(FeatureFlagUtils.SETTINGS_DO_NOT_RESTORE_PRESERVED));

        mSharedPreferencesFile.delete();
    }

    private String getOriginalSettingValue(String name) {
        return mSharedPreferences.getString(name, DEFAULT_SETTING_VALUE);
    }

    private void setOriginalSettingValue(String name, String value) {
        mSharedPreferences.edit().putString(name, value).commit();
    }
}
