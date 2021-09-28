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

package android.content.om.cts;

import static android.content.om.OverlayInfo.CREATOR;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.fail;

import android.content.om.OverlayInfo;
import android.os.Parcel;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Verifies the member variables inside {@link OverlayInfo}
 */
@RunWith(AndroidJUnit4.class)
public class OverlayInfoTest {

    private static final String PKG_NAME = "source package name";
    private static final String TARGET_PKG_NAME = "target package name";
    private static final String TARGET_OVERLAYABLE_NAME = "target package name";
    private static final String CATEGORY = "sample category";
    private static final String BASE_CODE_PATH = "base code path";

    @Test
    public void testEnsureValidState() {
        OverlayInfo info = new OverlayInfo(PKG_NAME, TARGET_PKG_NAME, TARGET_OVERLAYABLE_NAME,
                CATEGORY, BASE_CODE_PATH,
                0, 0, 0, true);
        assertNotNull(info);
    }

    @Test
    public void testEnsureState_disabled() {
        OverlayInfo info = new OverlayInfo(PKG_NAME, TARGET_PKG_NAME, TARGET_OVERLAYABLE_NAME,
                CATEGORY, BASE_CODE_PATH,
                0, 0, 0, true);
        assertFalse(info.isEnabled());
    }

    @Test
    public void testEnsureValidState_fail() {
        try {
            OverlayInfo info = new OverlayInfo(null, TARGET_PKG_NAME, TARGET_OVERLAYABLE_NAME,
                    CATEGORY, BASE_CODE_PATH,
                    0, 0, 0, true);
            fail("Should throw exception.");
        } catch (IllegalArgumentException e) {
            // no op, working as intended.
        }

        try {
            OverlayInfo info = new OverlayInfo(PKG_NAME, null, TARGET_OVERLAYABLE_NAME, CATEGORY,
                    BASE_CODE_PATH,
                    0, 0, 0, true);
            fail("Should throw exception.");
        } catch (IllegalArgumentException e) {
            // no op, working as intended.
        }

        try {
            OverlayInfo info = new OverlayInfo(PKG_NAME, TARGET_PKG_NAME, TARGET_OVERLAYABLE_NAME,
                    CATEGORY, null,
                    0, 0, 0, true);
            fail("Should throw exception.");
        } catch (IllegalArgumentException e) {
            // no op, working as intended.
        }
    }

    @Test
    public void testWriteToParcel() {
        OverlayInfo info = new OverlayInfo(PKG_NAME, TARGET_PKG_NAME, TARGET_OVERLAYABLE_NAME,
                CATEGORY, BASE_CODE_PATH,
                0, 0, 0, true);
        Parcel p = Parcel.obtain();
        info.writeToParcel(p, 0);
        p.setDataPosition(0);
        OverlayInfo copyInfo = CREATOR.createFromParcel(p);
        p.recycle();

        assertEquals(info.packageName, copyInfo.packageName);
        assertEquals(info.targetPackageName, copyInfo.targetPackageName);
        assertEquals(info.targetOverlayableName, copyInfo.targetOverlayableName);
        assertEquals(info.category, copyInfo.category);
        assertEquals(info.baseCodePath, copyInfo.baseCodePath);
        assertEquals(info.state, copyInfo.state);
        assertEquals(info.userId, copyInfo.userId);
        assertEquals(info.priority, copyInfo.priority);
        assertEquals(info.isMutable, copyInfo.isMutable);
    }
}