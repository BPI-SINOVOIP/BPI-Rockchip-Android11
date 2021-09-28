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
 * limitations under the License
 */

package android.cts.backup.profileserialnumberapp;

import static androidx.test.InstrumentationRegistry.getInstrumentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;

import android.app.backup.BackupManager;
import android.os.Binder;
import android.platform.test.annotations.AppModeFull;

import androidx.test.runner.AndroidJUnit4;

import com.android.compatibility.common.util.SystemUtil;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/** Device side test invoked by ProfileSerialNumberHostSideTest. */
@AppModeFull
@RunWith(AndroidJUnit4.class)
public class ProfileSerialNumberTest {
    private static final long USER_SERIAL_NUMBER = 124;
    private static final long INVALID_USER_SERIAL_NUMBER = -1000;

    private BackupManager mBackupManager;

    @Before
    public void setUp() throws Exception {
        mBackupManager = new BackupManager(getInstrumentation().getTargetContext());
    }

    @Test
    public void testSetAndGetAncestralSerialNumber() {
        SystemUtil.runWithShellPermissionIdentity(() -> {
            mBackupManager.setAncestralSerialNumber(USER_SERIAL_NUMBER);
        });

        assertEquals(Binder.getCallingUserHandle(),
                mBackupManager.getUserForAncestralSerialNumber(USER_SERIAL_NUMBER));
    }

    @Test
    public void testGetUserForAncestralSerialNumber_returnsNull_whenNoUserHasGivenSerialNumber() {
        assertNull(mBackupManager.getUserForAncestralSerialNumber(INVALID_USER_SERIAL_NUMBER));
    }
}
