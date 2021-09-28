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
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.util.Pair;

import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.io.File;
import java.util.HashSet;
import java.util.Set;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class NativeSystemFontTest {

    public Set<NativeSystemFontHelper.FontDescriptor> convertToDescriptors(Set<Font> fonts) {
        HashSet<NativeSystemFontHelper.FontDescriptor> out = new HashSet<>();
        for (Font f : fonts) {
            NativeSystemFontHelper.FontDescriptor font =
                    new NativeSystemFontHelper.FontDescriptor();
            font.mFilePath = f.getFile().getAbsolutePath();
            font.mWeight = f.getStyle().getWeight();
            font.mSlant = f.getStyle().getSlant();
            font.mIndex = f.getTtcIndex();
            font.mAxes = f.getAxes();
            font.mLocale = f.getLocaleList();
            out.add(font);
        }
        return out;
    }

    @Test
    public void testSameResultAsJava() {
        Set<NativeSystemFontHelper.FontDescriptor> javaFonts =
                convertToDescriptors(SystemFonts.getAvailableFonts());
        Set<NativeSystemFontHelper.FontDescriptor> nativeFonts =
                NativeSystemFontHelper.getAvailableFonts();

        assertEquals(javaFonts.size(), nativeFonts.size());

        for (NativeSystemFontHelper.FontDescriptor f : nativeFonts) {
            assertTrue(javaFonts.contains(f));
        }

        for (NativeSystemFontHelper.FontDescriptor f : javaFonts) {
            assertTrue(nativeFonts.contains(f));
        }
    }

    @Test
    public void testMatchFamilyStyleCharacter() {
        Pair<File, Integer> fontForA = NativeSystemFontHelper.matchFamilyStyleCharacter(
                "sans", 400, false, "en-US", 0 /* default family variant */, "A");
        Pair<File, Integer> fontForB = NativeSystemFontHelper.matchFamilyStyleCharacter(
                "sans", 400, false, "en-US", 0 /* default family variant */, "B");
        assertEquals(fontForA, fontForB);
    }

    @Test
    public void testMatchFamilyStyleCharacter_fallback() {
        Pair<File, Integer> fontForA = NativeSystemFontHelper.matchFamilyStyleCharacter(
                "Unknown-Generic-Family", 400, false, "en-US", 0 /* default family variant */, "A");
        Pair<File, Integer> fontForB = NativeSystemFontHelper.matchFamilyStyleCharacter(
                "Another-Unknown-Generic-Family", 400, false, "en-US",
                0 /* default family variant */, "B");
        assertEquals(fontForA, fontForB);
    }

    @Test
    public void testMatchFamilyStyleCharacter_notCrash() {
        String[] genericFamilies = {
            "sans", "sans-serif", "monospace", "cursive", "fantasy",  // generic families
            "Helvetica", "Roboto", "Times",  // known family names but not supported by Android
            "Unknown Families",  // Random string
        };

        int[] weights = {
            0, 150, 400, 700, 1000, // valid weights
            -100, 1100  // out-of-range
        };

        boolean[] italics = { false, true };

        String[] languageTags = {
            // Valid language tags
            "", "en-US", "und", "ja-JP,zh-CN", "en-Latn", "en-Zsye-US", "en-GB", "en-GB,en-AU",
            // Invalid language tags
            "aaa", "100", "\u3042", "-"
        };

        int[] familyVariants = { 0, 1, 2 };  // Family variants, DEFAULT, COMPACT and ELEGANT.

        String[] inputTexts = {
            "A", "B", "abc", // Alphabet input
            "\u3042", "\u3042\u3046\u3048", "\u4F60\u597D",  // CJK characters
            "\u0627\u0644\u0639\u064E\u0631\u064E\u0628\u0650\u064A\u064E\u0651\u0629",  // Arabic
            // Emoji, emoji sequence and surrogate pairs
            "\uD83D\uDE00", "\uD83C\uDDFA\uD83C\uDDF8", "\uD83D\uDC68\u200D\uD83C\uDFA4",
            // Unpaired surrogate pairs
            "\uD83D", "\uDE00", "\uDE00\uD83D",

        };

        for (String familyName : genericFamilies) {
            for (int weight : weights) {
                for (boolean italic : italics) {
                    for (String languageTag : languageTags) {
                        for (int familyVariant : familyVariants) {
                            for (String inputText : inputTexts) {
                                Pair<File, Integer> result =
                                        NativeSystemFontHelper.matchFamilyStyleCharacter(
                                                familyName, weight, italic, languageTag,
                                                familyVariant, inputText);
                                // We cannot expcet much here since OEM can change font
                                // configurations.
                                // At least, a font must be assigned for the first character.
                                assertTrue(result.second >= 1);

                                final File fontFile = result.first;
                                assertTrue(fontFile.exists());
                                assertTrue(fontFile.isAbsolute());
                                assertTrue(fontFile.isFile());
                                assertTrue(fontFile.canRead());
                                assertFalse(fontFile.canExecute());
                                assertFalse(fontFile.canWrite());
                                assertTrue(fontFile.length() > 0);
                            }
                        }
                    }
                }
            }
        }
    }
}
