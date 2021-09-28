/*
 * Copyright (C) 2019 Google Inc.
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

package android.cts.backup.restoresessionapp;

import static androidx.test.InstrumentationRegistry.getTargetContext;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.SharedPreferences;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.runner.RunWith;

@RunWith(AndroidJUnit4.class)
public class BaseRestoreSessionAppTest {
    private static final String SHARED_PREFERENCES_FILE = "restore_session_app_prefs";

    private SharedPreferences mPreferences;

    @Before
    public void setUp() {
        mPreferences =
                getTargetContext()
                        .getSharedPreferences(SHARED_PREFERENCES_FILE, Context.MODE_PRIVATE);
    }

    protected void clearSharedPrefs() {
        mPreferences.edit().clear().commit();
    }

    protected void checkSharedPrefsDontExist(String prefKey) {
        assertThat(mPreferences.getInt(prefKey, 0)).isEqualTo(0);
    }

    protected void saveValuesToSharedPrefs(String prefKey, int prefValue) {
        mPreferences.edit().putInt(prefKey, prefValue).commit();
    }

    protected void checkSharedPrefsExist(String prefKey, int prefValue) {
        assertThat(mPreferences.getInt(prefKey, 0)).isEqualTo(prefValue);
    }
}
