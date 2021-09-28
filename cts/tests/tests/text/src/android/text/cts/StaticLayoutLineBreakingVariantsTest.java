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

package android.text.cts;

import static org.junit.Assert.assertEquals;

import android.content.Context;
import android.graphics.Typeface;
import android.os.LocaleList;
import android.text.StaticLayout;
import android.text.TextPaint;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class StaticLayoutLineBreakingVariantsTest {

    private static TextPaint setupPaint(LocaleList locales) {
        // The test font covers all KATAKANA LETTERS (U+30A1..U+30FC) in Japanese and they all have
        // 1em width.
        Context context = InstrumentationRegistry.getTargetContext();
        TextPaint paint = new TextPaint();
        paint.setTypeface(Typeface.createFromAsset(context.getAssets(),
                  "fonts/BreakVariantsTest.ttf"));
        paint.setTextSize(10.0f);  // Make 1em == 10px.
        paint.setTextLocales(locales);
        return paint;
    }

    private static StaticLayout buildLayout(String text, LocaleList locales, int width) {
        return StaticLayout.Builder.obtain(
                text, 0, text.length(), setupPaint(locales), width).build();
    }

    private static void assertLineBreak(String text, String locale, int width, String... expected) {
        final StaticLayout layout = buildLayout(text, LocaleList.forLanguageTags(locale), width);
        assertEquals(expected.length, layout.getLineCount());

        int currentExpectedOffset = 0;
        for (int i = 0; i < expected.length; ++i) {
            currentExpectedOffset += expected[i].length();
            final String msg = i + "th line should be " + expected[i] + ", but it was "
                    + text.substring(layout.getLineStart(i), layout.getLineEnd(i));
            assertEquals(msg, currentExpectedOffset, layout.getLineEnd(i));
        }
    }

    // The following three test cases verifies the loose/normal/strict line break segmentations
    // works on Android. For more details see
    // http://cldr.unicode.org/development/development-process/design-proposals/specifying-text-break-variants-in-locale-ids

    // The test string is "Battery Saver" in Japanese. Here are the list of breaking points.
    //        \u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC\u30D0\u30FC
    // loose :^     ^     ^     ^     ^     ^     ^     ^     ^     ^
    // strict:^           ^     ^           ^           ^           ^
    private static final String SAMPLE_TEXT =
            "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC\u30D0\u30FC";

    @Test
    public void testBreakVariant_loose() {
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 90, SAMPLE_TEXT);
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 80,
                "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC\u30D0",
                "\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 70,
                "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 60,
                "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB",
                "\u30FC\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 50,
                "\u30D0\u30C3\u30C6\u30EA\u30FC",
                "\u30BB\u30FC\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 40,
                "\u30D0\u30C3\u30C6\u30EA",
                "\u30FC\u30BB\u30FC\u30D0",
                "\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 30,
                "\u30D0\u30C3\u30C6",
                "\u30EA\u30FC\u30BB",
                "\u30FC\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 20,
                "\u30D0\u30C3",
                "\u30C6\u30EA",
                "\u30FC\u30BB",
                "\u30FC\u30D0",
                "\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-loose", 10,
                "\u30D0",
                "\u30C3",
                "\u30C6",
                "\u30EA",
                "\u30FC",
                "\u30BB",
                "\u30FC",
                "\u30D0",
                "\u30FC");
    }

    @Test
    public void testBreakVariant_strict() {
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 90, SAMPLE_TEXT);
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 80,
                "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 70,
                "\u30D0\u30C3\u30C6\u30EA\u30FC\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 60,
                "\u30D0\u30C3\u30C6\u30EA\u30FC",
                "\u30BB\u30FC\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 50,
                "\u30D0\u30C3\u30C6\u30EA\u30FC",
                "\u30BB\u30FC\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 40,
                "\u30D0\u30C3\u30C6",
                "\u30EA\u30FC\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 30,
                "\u30D0\u30C3\u30C6",
                "\u30EA\u30FC",
                "\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 20,
                "\u30D0\u30C3",
                "\u30C6",
                "\u30EA\u30FC",
                "\u30BB\u30FC",
                "\u30D0\u30FC");
        assertLineBreak(SAMPLE_TEXT, "ja-JP-u-lb-strict", 10,
                "\u30D0",
                "\u30C3",
                "\u30C6",
                "\u30EA",
                "\u30FC",
                "\u30BB",
                "\u30FC",
                "\u30D0",
                "\u30FC");
    }
}
