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

package android.cts.backup.profilekeyvalueapp;

import static androidx.test.InstrumentationRegistry.getTargetContext;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.content.SharedPreferences;
import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Device side test invoked by profile host side tests. */
@RunWith(AndroidJUnit4.class)
@AppModeFull
public class ProfileKeyValueBackupRestoreTest {
    static final String TEST_PREFS_NAME = "user-test-prefs";
    private static final String PREFS_USER_KEY = "userId";
    private static final int PREFS_USER_DEFAULT = -1000;

    private int mUserId;
    private SharedPreferences mPreferences;

    @Before
    public void setUp() {
        Context context = getTargetContext();
        mUserId = context.getUserId();
        mPreferences = context.getSharedPreferences(TEST_PREFS_NAME, Context.MODE_PRIVATE);
    }

    @Test
    public void assertSharedPrefsIsEmpty() {
        int userIdPref = mPreferences.getInt(PREFS_USER_KEY, PREFS_USER_DEFAULT);
        assertThat(userIdPref).isEqualTo(PREFS_USER_DEFAULT);
    }

    @Test
    public void writeSharedPrefsAndAssertSuccess() {
        SharedPreferences.Editor editor = mPreferences.edit();
        editor.putInt(PREFS_USER_KEY, mUserId);
        assertThat(editor.commit()).isTrue();
    }

    @Test
    public void assertSharedPrefsRestored() {
        int userIdPref = mPreferences.getInt(PREFS_USER_KEY, PREFS_USER_DEFAULT);
        assertThat(userIdPref).isEqualTo(mUserId);
    }
}
