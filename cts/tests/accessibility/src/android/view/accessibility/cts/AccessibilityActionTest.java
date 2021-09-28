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

package android.view.accessibility.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertSame;

import android.os.Parcel;
import android.platform.test.annotations.Presubmit;
import android.view.accessibility.AccessibilityNodeInfo.AccessibilityAction;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Class for testing {@link AccessibilityAction}.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
@SmallTest
public class AccessibilityActionTest {
    private static final int ACTION_ID = 0x11100100;
    private static final String LABEL = "label";

    /**
     * Tests parcelling of the class.
     */
    @Test
    public void testParcel() {
        AccessibilityAction systemAction =
                new AccessibilityAction(ACTION_ID, LABEL);

        final Parcel parcel = Parcel.obtain();
        systemAction.writeToParcel(parcel, systemAction.describeContents());
        parcel.setDataPosition(0);
        AccessibilityAction result =
                AccessibilityAction.CREATOR.createFromParcel(parcel);

        assertEquals(ACTION_ID, result.getId());
        assertEquals(LABEL, result.getLabel());
    }

    /**
     * Tests constructor of the class.
     */
    @Test
    public void testConstructor() {
        AccessibilityAction systemAction =
                new AccessibilityAction(ACTION_ID, LABEL);

        assertEquals(ACTION_ID, systemAction.getId());
        assertEquals(LABEL, systemAction.getLabel());
    }

}
