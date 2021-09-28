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

package android.view.accessibility.cts;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertSame;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.platform.test.annotations.Presubmit;
import android.view.accessibility.AccessibilityNodeInfo;
import android.view.accessibility.AccessibilityNodeInfo.CollectionItemInfo;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Class for testing {@link CollectionItemInfo}.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
public class AccessibilityNodeInfo_CollectionItemInfoTest {

    @Rule
    public final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @SmallTest
    @Test
    public void testObtain() {
        CollectionItemInfo c;

        c = CollectionItemInfo.obtain(0, 1, 2, 3, true);
        assertNotNull(c);
        verifyCollectionItemInfo(c, 0, 1, 2, 3, true, false);

        c = CollectionItemInfo.obtain(4, 5, 6, 7, true, true);
        assertNotNull(c);
        verifyCollectionItemInfo(c, 4, 5, 6, 7, true, true);
    }

    @SmallTest
    @Test
    public void testConstructor() {
        CollectionItemInfo c;

        c = new CollectionItemInfo(0, 1, 2, 3, true);
        verifyCollectionItemInfo(c, 0, 1, 2, 3, true, false);

        c = new CollectionItemInfo(4, 5, 6, 7, true, true);
        verifyCollectionItemInfo(c, 4, 5, 6, 7, true, true);
    }

    /**
     * Verifies all properties of the <code>info</code> with input expected values.
     */
    public static void verifyCollectionItemInfo(AccessibilityNodeInfo.CollectionItemInfo info,
            int rowIndex, int rowSpan, int columnIndex, int columnSpan, boolean heading,
            boolean selected) {
        assertEquals(rowIndex, info.getRowIndex());
        assertEquals(rowSpan, info.getRowSpan());
        assertEquals(columnIndex, info.getColumnIndex());
        assertEquals(columnSpan, info.getColumnSpan());
        assertSame(heading, info.isHeading());
        assertSame(selected, info.isSelected());
    }
}
