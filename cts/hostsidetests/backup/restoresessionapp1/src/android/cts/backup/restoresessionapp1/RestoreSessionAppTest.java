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

package android.cts.backup.restoresessionapp1;

import androidx.test.runner.AndroidJUnit4;

import android.cts.backup.restoresessionapp.BaseRestoreSessionAppTest;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Device side routines to be invoked by the host side RestoreSessionHostSideTest. These are not
 * designed to be called in any other way, as they rely on state set up by the host side test.
 */
@RunWith(AndroidJUnit4.class)
public class RestoreSessionAppTest extends BaseRestoreSessionAppTest {
    private static final String SHARED_PREFERENCES_KEY = "test_key_1";
    private static final int SHARED_PREFERENCES_VALUE = 123;

    @Test
    public void testClearSharedPrefs() {
        clearSharedPrefs();
    }

    @Test
    public void testCheckSharedPrefsDontExist() {
        checkSharedPrefsDontExist(SHARED_PREFERENCES_KEY);
    }

    @Test
    public void testSaveValuesToSharedPrefs() {
        saveValuesToSharedPrefs(SHARED_PREFERENCES_KEY, SHARED_PREFERENCES_VALUE);
    }

    @Test
    public void testCheckSharedPrefsExist() {
        checkSharedPrefsExist(SHARED_PREFERENCES_KEY, SHARED_PREFERENCES_VALUE);
    }
}
