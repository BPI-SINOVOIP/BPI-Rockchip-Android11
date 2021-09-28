/*
 * Copyright (C) 2016 The Android Open Source Project
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
import android.view.accessibility.AccessibilityNodeInfo.CollectionInfo;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Class for testing {@link CollectionInfo}.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
public class AccessibilityNodeInfo_CollectionInfoTest  {

    @Rule
    public final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @SmallTest
    @Test
    public void testObtain() {
        CollectionInfo c;

        c = CollectionInfo.obtain(0, 1, true);
        assertNotNull(c);
        verifyCollectionInfo(c, 0, 1, true, CollectionInfo.SELECTION_MODE_NONE);

        c = CollectionInfo.obtain(1, 2, true, CollectionInfo.SELECTION_MODE_MULTIPLE);
        assertNotNull(c);
        verifyCollectionInfo(c, 1, 2, true, CollectionInfo.SELECTION_MODE_MULTIPLE);
    }

    @SmallTest
    @Test
    public void testConstructor() {
        CollectionInfo c;

        c = new CollectionInfo(0, 1, true);
        verifyCollectionInfo(c, 0, 1, true, CollectionInfo.SELECTION_MODE_NONE);

        c = new CollectionInfo(1, 2, true, CollectionInfo.SELECTION_MODE_MULTIPLE);
        verifyCollectionInfo(c, 1, 2, true, CollectionInfo.SELECTION_MODE_MULTIPLE);
    }

    /**
     * Verifies all properties of the <code>info</code> with input expected values.
     */
    public static void verifyCollectionInfo(CollectionInfo info, int rowCount, int columnCount,
            boolean hierarchical, int selectionMode) {
        assertEquals(rowCount, info.getRowCount());
        assertEquals(columnCount, info.getColumnCount());
        assertSame(hierarchical, info.isHierarchical());
        assertEquals(selectionMode, info.getSelectionMode());
    }
}
