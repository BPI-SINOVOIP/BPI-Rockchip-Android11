/*
 * Copyright (C) 2008 The Android Open Source Project
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

package android.widget.cts;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test {@link TextView#isHorizontallyScrollable}.
 */
@SmallTest
@RunWith(AndroidJUnit4.class)
public class TextViewIsHorizontallyScrollableTest {
    private ViewGroup mViewGroup;

    @Before
    public void setup() {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final LayoutInflater inflater = LayoutInflater.from(context);
        mViewGroup = (ViewGroup) inflater.inflate(R.layout.textview_isHorizontallyScrollable_layout,
                null);
    }

    @Test
    public void testIsHorizontallyScrollingDefaultIsFalse() {
        final TextView textView = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_default);

        assertFalse(textView.isHorizontallyScrollable());
    }

    @Test
    public void testIsHorizontallyScrollingSameAsGiven() {
        final TextView textView = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_default);

        textView.setHorizontallyScrolling(true);
        assertTrue(textView.isHorizontallyScrollable());
    }

    @Test
    public void testIsHorizontallyScrollingTrueToFalse() {
        final TextView textView = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_default);
        textView.setHorizontallyScrolling(true);
        assertTrue(textView.isHorizontallyScrollable());

        textView.setHorizontallyScrolling(false);
        assertFalse(textView.isHorizontallyScrollable());
    }

    @Test
    public void testIsHorizontallyScrollingSetInXML() {
        final TextView textViewTrue = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_true);
        // It should return true here. But because of this bug b/120448952,
        // singleLine will overwrite scrollHorizontally.
        assertFalse(textViewTrue.isHorizontallyScrollable());

        final TextView textViewFalse = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_false);
        assertFalse(textViewFalse.isHorizontallyScrollable());
    }

    @Test
    public void testIsHorizontallyScrollingSetInXML_returnTrueWhenSingleLineIsTrue() {
        final TextView textViewDefault = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_default_singleLine_true);
        assertTrue(textViewDefault.isHorizontallyScrollable());

        final TextView textViewTrue = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_true_singleLine_true);
        assertTrue(textViewTrue.isHorizontallyScrollable());

        final TextView textViewFalse = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_false_singleLine_true);
        assertTrue(textViewFalse.isHorizontallyScrollable());
    }

    @Test
    public void testIsHorizontallyScrollingSetInXML_returnGivenValueWhenSingleLineIsFalse() {
        final TextView textViewDefault = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_default_singleLine_false);
        assertFalse(textViewDefault.isHorizontallyScrollable());

        final TextView textViewTrue = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_true_singleLine_false);
        // It should return true here. But because of this bug b/120448952,
        // singleLine will overwrite scrollHorizontally.
        assertFalse(textViewTrue.isHorizontallyScrollable());

        final TextView textViewFalse = mViewGroup.findViewById(
                R.id.textView_scrollHorizontally_false_singleLine_false);
        assertFalse(textViewFalse.isHorizontallyScrollable());
    }
}
