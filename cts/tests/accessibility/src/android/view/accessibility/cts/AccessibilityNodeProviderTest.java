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

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;

import android.accessibility.cts.common.AccessibilityDumpOnFailureRule;
import android.platform.test.annotations.Presubmit;
import android.view.accessibility.AccessibilityNodeProvider;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Class for testing {@link AccessibilityNodeProvider}.
 */
@Presubmit
@RunWith(AndroidJUnit4.class)
public class AccessibilityNodeProviderTest {

    @Rule
    public final AccessibilityDumpOnFailureRule mDumpOnFailureRule =
            new AccessibilityDumpOnFailureRule();

    @SmallTest
    @Test
    public void testDefaultBehavior() {
        AccessibilityNodeProvider p = new AccessibilityNodeProvider() {
            // Class is abstract, but has no abstract methods.
        };

        assertNull(p.createAccessibilityNodeInfo(0));
        assertNull(p.findAccessibilityNodeInfosByText(null, 0));
        assertNull(p.findFocus(0));
        assertFalse(p.performAction(0, 0, null));
    }
}
