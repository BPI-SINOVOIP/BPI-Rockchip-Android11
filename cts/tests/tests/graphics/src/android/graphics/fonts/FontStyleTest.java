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

package android.graphics.fonts;

import static org.junit.Assert.assertEquals;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class FontStyleTest {
    private static final String TAG = "FontStyleTest";

    @Test
    public void testBuilder_construct() {
        new FontStyle();
        new FontStyle(FontStyle.FONT_WEIGHT_THIN, FontStyle.FONT_SLANT_ITALIC);
        new FontStyle(350, FontStyle.FONT_SLANT_ITALIC);
    }

    @Test
    public void testBuilder_construct_withArgs() {
        FontStyle fs = new FontStyle(FontStyle.FONT_WEIGHT_THIN, FontStyle.FONT_SLANT_ITALIC);
        assertEquals(FontStyle.FONT_WEIGHT_THIN, fs.getWeight());
        assertEquals(FontStyle.FONT_SLANT_ITALIC, fs.getSlant());

        fs = new FontStyle(350, FontStyle.FONT_SLANT_UPRIGHT);
        assertEquals(350, fs.getWeight());
        assertEquals(FontStyle.FONT_SLANT_UPRIGHT, fs.getSlant());
    }

    @Test
    public void testBuilder_defaultValues() {
        assertEquals(FontStyle.FONT_WEIGHT_NORMAL, new FontStyle().getWeight());
        assertEquals(FontStyle.FONT_SLANT_UPRIGHT, new FontStyle().getSlant());
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_constructor_too_small_weight() {
        new FontStyle(FontStyle.FONT_WEIGHT_MIN - 1, FontStyle.FONT_SLANT_UPRIGHT);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_constructor_too_large_weight() {
        new FontStyle(FontStyle.FONT_WEIGHT_MAX + 1, FontStyle.FONT_SLANT_UPRIGHT);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_constructor_invalid_slant() {
        new FontStyle(FontStyle.FONT_WEIGHT_THIN, -1);
    }
}
