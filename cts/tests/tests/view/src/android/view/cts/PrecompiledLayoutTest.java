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

package android.view.cts;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.test.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;

public class PrecompiledLayoutTest {
    private LayoutInflater mInflater;

    @Before
    public void setup() {
        Context context = InstrumentationRegistry.getTargetContext();
        mInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);
    }

    /**
     * Check whether two layouts are equivalent. We do this by making sure the
     * classes match, that they have the same ID, and that ViewGroups have the
     * same children.
     */
    private void assertEquivalentLayouts(View lhs, View rhs) {
        // Make sure both are null or both are not null.
        Assert.assertEquals(lhs == null, rhs == null);
        if (lhs == null) {
            // If the two objects are null, then they are the same.
            return;
        }

        // The two views should have the same class and ID
        Assert.assertSame(lhs.getClass(), rhs.getClass());
        Assert.assertEquals(lhs.getId(), rhs.getId());

        // If these are ViewGroups, make sure the children are also equivalent.
        if (lhs instanceof ViewGroup) {
            ViewGroup lhsGroup = (ViewGroup) lhs;
            ViewGroup rhsGroup = (ViewGroup) rhs;

            Assert.assertEquals(lhsGroup.getChildCount(), rhsGroup.getChildCount());

            for (int i = 0; i < lhsGroup.getChildCount(); i++) {
                assertEquivalentLayouts(lhsGroup.getChildAt(i), rhsGroup.getChildAt(i));
            }
        }
    }

    private void compareInflation(int resourceId) {
        // Inflate without layout precompilation.
        mInflater.setPrecompiledLayoutsEnabledForTesting(false);
        View interpreted = mInflater.inflate(resourceId, null);

        // Inflate with layout precompilation
        mInflater.setPrecompiledLayoutsEnabledForTesting(true);
        View precompiled = mInflater.inflate(resourceId, null);

        assertEquivalentLayouts(interpreted, precompiled);
    }

    // The following tests make sure that we get equivalent layouts when
    // inflated with and without precompilation.

    @Test
    public void equivalentInflaterLayout() {
        compareInflation(R.layout.inflater_layout);
    }

    @Test
    public void equivalentInflaterOverrideThemeLayout() {
        compareInflation(R.layout.inflater_override_theme_layout);
    }

    @Test
    public void equivalentInflaterLayoutTags() {
        compareInflation(R.layout.inflater_layout_tags);
    }
}
