/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the
 * License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */

package com.android.systemui.qs;

import static com.android.systemui.statusbar.phone.AutoTileManager.INVERSION;
import static com.android.systemui.statusbar.phone.AutoTileManager.SAVER;
import static com.android.systemui.statusbar.phone.AutoTileManager.WORK;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.os.UserHandle;
import android.provider.Settings.Secure;
import android.testing.AndroidTestingRunner;
import android.testing.TestableLooper.RunWithLooper;

import androidx.test.filters.SmallTest;

import com.android.systemui.Prefs;
import com.android.systemui.Prefs.Key;
import com.android.systemui.SysuiTestCase;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

@RunWith(AndroidTestingRunner.class)
@RunWithLooper
@SmallTest
public class AutoAddTrackerTest extends SysuiTestCase {

    private static final int USER = 0;

    private AutoAddTracker mAutoTracker;

    @Before
    public void setUp() {
        Secure.putString(mContext.getContentResolver(), Secure.QS_AUTO_ADDED_TILES, "");
    }

    @Test
    public void testMigration() {
        Prefs.putBoolean(mContext, Key.QS_DATA_SAVER_ADDED, true);
        Prefs.putBoolean(mContext, Key.QS_WORK_ADDED, true);
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();

        assertTrue(mAutoTracker.isAdded(SAVER));
        assertTrue(mAutoTracker.isAdded(WORK));
        assertFalse(mAutoTracker.isAdded(INVERSION));

        // These keys have been removed; retrieving their values should always return the default.
        assertTrue(Prefs.getBoolean(mContext, Key.QS_DATA_SAVER_ADDED, true ));
        assertFalse(Prefs.getBoolean(mContext, Key.QS_DATA_SAVER_ADDED, false));
        assertTrue(Prefs.getBoolean(mContext, Key.QS_WORK_ADDED, true));
        assertFalse(Prefs.getBoolean(mContext, Key.QS_WORK_ADDED, false));

        mAutoTracker.destroy();
    }

    @Test
    public void testChangeFromBackup() {
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();

        assertFalse(mAutoTracker.isAdded(SAVER));

        Secure.putString(mContext.getContentResolver(), Secure.QS_AUTO_ADDED_TILES, SAVER);
        mAutoTracker.mObserver.onChange(false);

        assertTrue(mAutoTracker.isAdded(SAVER));

        mAutoTracker.destroy();
    }

    @Test
    public void testSetAdded() {
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();

        assertFalse(mAutoTracker.isAdded(SAVER));
        mAutoTracker.setTileAdded(SAVER);

        assertTrue(mAutoTracker.isAdded(SAVER));

        mAutoTracker.destroy();
    }

    @Test
    public void testPersist() {
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();

        assertFalse(mAutoTracker.isAdded(SAVER));
        mAutoTracker.setTileAdded(SAVER);

        mAutoTracker.destroy();
        mAutoTracker = new AutoAddTracker(mContext, USER);

        assertTrue(mAutoTracker.isAdded(SAVER));

        mAutoTracker.destroy();
    }

    @Test
    public void testIndependentUsers() {
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();
        mAutoTracker.setTileAdded(SAVER);

        mAutoTracker = new AutoAddTracker(mContext, USER + 1);
        assertFalse(mAutoTracker.isAdded(SAVER));
    }

    @Test
    public void testChangeUser() {
        mAutoTracker = new AutoAddTracker(mContext, USER);
        mAutoTracker.initialize();
        mAutoTracker.setTileAdded(SAVER);

        mAutoTracker = new AutoAddTracker(mContext, USER + 1);
        mAutoTracker.changeUser(UserHandle.of(USER));
        assertTrue(mAutoTracker.isAdded(SAVER));
    }
}