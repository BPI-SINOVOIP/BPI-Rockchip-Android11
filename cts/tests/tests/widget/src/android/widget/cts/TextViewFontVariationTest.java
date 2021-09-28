/*
 * Copyright (C) 2018 The Android Open Source Project
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

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

/**
 * Test for font variation attribute in TextView and TextAppearance
 */

@SmallTest
@RunWith(AndroidJUnit4.class)
public final class TextViewFontVariationTest {
    private String getTextViewFontVariationSettings(int id) {
        final Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        final LayoutInflater inflater = LayoutInflater.from(context);
        final ViewGroup container =
                (ViewGroup) inflater.inflate(R.layout.textview_fontvariation_test_layout, null);
        return ((TextView) container.findViewById(id)).getFontVariationSettings();
    }

    @Test
    public void testFontVariation() {
        assertEquals("'wdth' 25",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth25));
        assertEquals("'wdth' 50",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth50));
        assertEquals("'wght' 100",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wght100));
        assertEquals("'wght' 200",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wght200));
        assertEquals("'wdth' 25, 'wght' 100",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth25_wght100));
        assertEquals("'wdth' 25, 'wght' 200",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth25_wght200));
        assertEquals("'wdth' 50, 'wght' 100",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth50_wght100));
        assertEquals("'wdth' 50, 'wght' 200",
                getTextViewFontVariationSettings(R.id.textView_fontVariation_wdth50_wght200));
    }

    @Test
    public void testTextAppearance() {
        assertEquals("'wdth' 25",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth25));
        assertEquals("'wdth' 50",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth50));
        assertEquals("'wght' 100",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wght100));
        assertEquals("'wght' 200",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wght200));
        assertEquals("'wdth' 25, 'wght' 100",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth25_wght100));
        assertEquals("'wdth' 25, 'wght' 200",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth25_wght200));
        assertEquals("'wdth' 50, 'wght' 100",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth50_wght100));
        assertEquals("'wdth' 50, 'wght' 200",
                getTextViewFontVariationSettings(R.id.textAppearance_fontVariation_wdth50_wght200));
    }
}
