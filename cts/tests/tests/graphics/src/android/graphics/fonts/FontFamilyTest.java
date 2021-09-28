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
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNotSame;
import static org.junit.Assert.assertSame;
import static org.junit.Assert.assertTrue;

import android.content.res.AssetManager;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.IOException;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class FontFamilyTest {
    private static final String TAG = "FontFamilyTest";
    private static final String FONT_DIR = "fonts/family_selection/ttf/";

    @Test
    public void testBuilder_SingleFont() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font font = new Font.Builder(am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        FontFamily family = new FontFamily.Builder(font).build();
        assertNotNull(family);
        assertEquals(1, family.getSize());
        assertSame(font, family.getFont(0));
    }

    @Test
    public void testBuilder_MultipleFont() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font boldFont = new Font.Builder(
                am, FONT_DIR + "ascii_m3em_weight700_upright.ttf").build();
        FontFamily family = new FontFamily.Builder(regularFont).addFont(boldFont).build();
        assertNotNull(family);
        assertEquals(2, family.getSize());
        assertNotSame(family.getFont(0), family.getFont(1));
        assertTrue(family.getFont(0) == regularFont || family.getFont(0) == boldFont);
        assertTrue(family.getFont(1) == regularFont || family.getFont(1) == boldFont);
    }

    @Test
    public void testBuilder_MultipleFont_overrideWeight() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font boldFont = new Font.Builder(am, FONT_DIR + "ascii_g3em_weight400_upright.ttf")
                .setWeight(700).build();
        FontFamily family = new FontFamily.Builder(regularFont).addFont(boldFont).build();
        assertNotNull(family);
        assertEquals(2, family.getSize());
        assertNotSame(family.getFont(0), family.getFont(1));
        assertTrue(family.getFont(0) == regularFont || family.getFont(0) == boldFont);
        assertTrue(family.getFont(1) == regularFont || family.getFont(1) == boldFont);
    }

    @Test
    public void testBuilder_MultipleFont_overrideItalic() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font italicFont = new Font.Builder(am, FONT_DIR + "ascii_g3em_weight400_upright.ttf")
                .setSlant(FontStyle.FONT_SLANT_ITALIC).build();
        FontFamily family = new FontFamily.Builder(regularFont).addFont(italicFont).build();
        assertNotNull(family);
        assertEquals(2, family.getSize());
        assertNotSame(family.getFont(0), family.getFont(1));
        assertTrue(family.getFont(0) == regularFont || family.getFont(0) == italicFont);
        assertTrue(family.getFont(1) == regularFont || family.getFont(1) == italicFont);
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_MultipleFont_SameStyle() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font regularFont2 = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        new FontFamily.Builder(regularFont).addFont(regularFont2).build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_MultipleFont_SameStyle_overrideWeight() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font regularFont2 = new Font.Builder(am, FONT_DIR + "ascii_m3em_weight700_upright.ttf")
                .setWeight(400).build();
        new FontFamily.Builder(regularFont).addFont(regularFont2).build();
    }

    @Test(expected = IllegalArgumentException.class)
    public void testBuilder_MultipleFont_SameStyle_overrideItalic() throws IOException {
        AssetManager am = InstrumentationRegistry.getTargetContext().getAssets();
        Font regularFont = new Font.Builder(
                am, FONT_DIR + "ascii_g3em_weight400_upright.ttf").build();
        Font regularFont2 = new Font.Builder(am, FONT_DIR + "ascii_h3em_weight400_italic.ttf")
                .setSlant(FontStyle.FONT_SLANT_UPRIGHT).build();
        new FontFamily.Builder(regularFont).addFont(regularFont2).build();
    }
}
