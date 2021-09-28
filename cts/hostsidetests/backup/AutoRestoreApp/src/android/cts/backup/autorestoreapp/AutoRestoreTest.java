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

package android.cts.backup.autorestoreapp;

import static androidx.test.InstrumentationRegistry.getTargetContext;

import static com.google.common.truth.Truth.assertThat;


import android.app.backup.BackupManager;
import android.content.Context;
import android.content.SharedPreferences;
import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Device side routines to be invoked by the host side AutoRestoreHostSideTest. These are not
 * designed to be called in any other way, as they rely on state set up by the host side test.
 */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class AutoRestoreTest {
    private static final String SHARED_PREFERENCES_FILE = "auto_restore_shared_prefs";
    private static final String PREF_KEY = "auto_restore_pref";
    private static final int PREF_VALUE = 123;

    private BackupManager mBackupManager;
    private SharedPreferences mPreferences;

    @Before
    public void setUp() {
        Context context = getTargetContext();
        mBackupManager = new BackupManager(context);
        mPreferences = context.getSharedPreferences(SHARED_PREFERENCES_FILE, Context.MODE_PRIVATE);
    }

    @Test
    public void saveValuesToSharedPrefs() {
        mPreferences.edit().putInt(PREF_KEY, PREF_VALUE).commit();
    }

    @Test
    public void testCheckSharedPrefsExist() {
        assertThat(mPreferences.getInt(PREF_KEY, 0)).isEqualTo(PREF_VALUE);
    }

    @Test
    public void testCheckSharedPrefsDontExist() {
        assertThat(mPreferences.getInt(PREF_KEY, 0)).isEqualTo(0);
    }

    @Test
    public void enableAutoRestore() {
        setAutoRestoreEnabled(true);
    }

    @Test
    public void disableAutoRestore() {
        setAutoRestoreEnabled(false);
    }

    private void setAutoRestoreEnabled(boolean enabled) {
        SystemUtil.runWithShellPermissionIdentity(
                () -> {
                    mBackupManager.setAutoRestore(enabled);
                });
    }
}
