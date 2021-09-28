/*
 * Copyright (C) 2014 The Android Open Source Project
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

import static org.junit.Assert.assertTrue;
import static org.junit.Assume.assumeTrue;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Paint;
import android.graphics.fonts.Font;
import android.graphics.fonts.SystemFonts;
import android.icu.util.ULocale;
import android.os.LocaleList;
import android.util.TypedValue;
import android.widget.TextView;

import androidx.test.InstrumentationRegistry;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.Set;

@SmallTest
@RunWith(AndroidJUnit4.class)
public class MyanmarTest {

    private static final String SCRIPT_QAAG = "Qaag";
    private static final String SCRIPT_MYMR = "Mymr";

    /**
     * The following will be rendered correctly as a single cluster for both Unicode and Zawgyi.
     */
    private static final String UNICODE_CORRECT_ORDER = "\u1019\u102d\u102f";

    /**
     * The following will not be rendered correctly as a single cluster in Unicode. However it will
     * render correctly with Zawgyi.
     */
    private static final String UNICODE_WRONG_ORDER = "\u1019\u102f\u102d";

    private static boolean sHasBurmeseLocale;
    private static List<Locale> sZawgyiLocales = new ArrayList<>();
    private static List<Locale> sMymrLocales = new ArrayList<>();
    private Context mContext;

    @BeforeClass
    public static void setupLocales() {
        final String[] supportedLocaleNames = getSupportedLocales();
        for (String localeName : supportedLocaleNames) {
            final Locale locale = Locale.forLanguageTag(localeName);
            final String script = ULocale.addLikelySubtags(ULocale.forLocale(locale)).getScript();

            if (SCRIPT_QAAG.equals(script)) {
                sZawgyiLocales.add(locale);
            } else if (SCRIPT_MYMR.equals(script)) {
                sMymrLocales.add(locale);
            }

            sHasBurmeseLocale = (sMymrLocales.size() + sZawgyiLocales.size()) > 0;
        }
    }

    @Before
    public void setupTest() {
        mContext = InstrumentationRegistry.getTargetContext();
    }

    @Test
    public void testIfZawgyiExistsThenUnicodeExists() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sZawgyiLocales.isEmpty());

        assertTrue("If the system supports Zawgyi, it should also support Unicode Myanmar",
                sMymrLocales.size() > 0);
    }

    @Test
    public void testShouldHaveGlyphs() {
        assumeTrue(sHasBurmeseLocale);

        final Paint paint = new Paint();
        int index = 0;
        while (index < UNICODE_CORRECT_ORDER.length()) {
            int codepoint = UNICODE_CORRECT_ORDER.codePointAt(index);

            assertTrue("Should have glyph for " + Integer.toHexString(codepoint),
                    paint.hasGlyph(new String(Character.toChars(codepoint))));

            index += Character.charCount(codepoint);
        }
    }

    @Test
    public void testMyanmarUnicodeRenders() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sMymrLocales.isEmpty());

        assertTrue("Should render Unicode text correctly in Myanmar Unicode locale",
                isUnicodeRendersCorrectly(mContext, new LocaleList(sMymrLocales.get(0))));
    }

    @Test
    public void testUnicodeRenders_withValidLocaleList() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sMymrLocales.isEmpty());

        final LocaleList[] testLocales = new LocaleList[]{
                LocaleList.forLanguageTags("en-Latn-US"),
                LocaleList.forLanguageTags("en-Latn"),
                LocaleList.forLanguageTags("my-Mymr"),
                LocaleList.forLanguageTags("my-Mymr,my-Qaag"),
                LocaleList.forLanguageTags("my-Mymr-MM,my-Qaag-MM"),
                LocaleList.forLanguageTags("en-Latn,my-Mymr"),
                LocaleList.forLanguageTags("en-Latn-US,my-Mymr-MM"),
                LocaleList.forLanguageTags("en-Mymr,my-Qaag"),
                LocaleList.forLanguageTags("en-Mymr-MM,my-Qaag-MM"),
        };

        for (LocaleList localeList : testLocales) {
            assertTrue("Should render Unicode text correctly in locale " + localeList.toString(),
                    isUnicodeRendersCorrectly(mContext, localeList));
        }

    }

    @Test
    public void testZawgyiRenders() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sZawgyiLocales.isEmpty());

        assertTrue("Should render Zawgyi text correctly with Zawgyi system locale",
                isZawgyiRendersCorrectly(mContext, new LocaleList(sZawgyiLocales.get(0))));
    }

    @Test
    public void testZawgyiRenders_withValidLocaleList() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sZawgyiLocales.isEmpty());

        final LocaleList[] testLocales = new LocaleList[]{
                LocaleList.forLanguageTags("my-Qaag"),
                LocaleList.forLanguageTags("my-Qaag,my-Mymr"),
                LocaleList.forLanguageTags("my-Qaag-MM,my-Mymr-MM"),
                LocaleList.forLanguageTags("en-Latn,my-Qaag"),
                LocaleList.forLanguageTags("en-Latn-US,my-Qaag-MM"),
                LocaleList.forLanguageTags("en-Qaag,my-Mymr"),
                LocaleList.forLanguageTags("en-Qaag-MM,my-Mymr-MM"),
        };

        for (LocaleList localeList : testLocales) {
            assertTrue("Should render Zawgyi text correctly in locale " + localeList.toString(),
                    isZawgyiRendersCorrectly(mContext, localeList));
        }
    }

    @Test
    public void testIfZawgyiLocaleIsSupported_fontWithQaagShouldExists() {
        assumeTrue(sHasBurmeseLocale);
        assumeTrue(!sZawgyiLocales.isEmpty());

        boolean qaagFontExists = false;
        final Set<Font> availableFonts = SystemFonts.getAvailableFonts();
        for (Font font : availableFonts) {
            final LocaleList localeList = font.getLocaleList();
            for (int index = 0; index < localeList.size(); index++) {
                final Locale fontLocale = localeList.get(index);
                final ULocale uLocale = ULocale.addLikelySubtags(ULocale.forLocale(fontLocale));
                final String script = uLocale.getScript();
                if (SCRIPT_QAAG.equals(script)) {
                    qaagFontExists = true;
                    break;
                }
            }
        }

        assertTrue(qaagFontExists);
    }

    private static boolean isUnicodeRendersCorrectly(Context context, LocaleList localeList) {
        final Bitmap bitmapCorrect = CaptureTextView.capture(context, localeList,
                UNICODE_CORRECT_ORDER);
        final Bitmap bitmapWrong = CaptureTextView.capture(context, localeList,
                UNICODE_WRONG_ORDER);

        return !bitmapCorrect.sameAs(bitmapWrong);
    }

    private static boolean isZawgyiRendersCorrectly(Context context, LocaleList localeList) {
        final Bitmap bitmapCorrect = CaptureTextView.capture(context, localeList,
                UNICODE_CORRECT_ORDER);
        final Bitmap bitmapWrong = CaptureTextView.capture(context, localeList,
                UNICODE_WRONG_ORDER);

        return bitmapCorrect.sameAs(bitmapWrong);
    }

    private static String[] getSupportedLocales() {
        return Resources.getSystem().getStringArray(
                Resources.getSystem().getIdentifier("supported_locales", "array", "android"));
    }

    private static class CaptureTextView extends TextView {

        CaptureTextView(Context context) {
            super(context);
            final float textSize = TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_SP, 8,
                    context.getResources().getDisplayMetrics());
            setTextSize(textSize);
        }

        private Bitmap capture(String text) {
            setText(text);

            invalidate();

            setDrawingCacheEnabled(true);
            measure(MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED),
                    MeasureSpec.makeMeasureSpec(0, MeasureSpec.UNSPECIFIED));
            layout(0, 0, 200, 200);

            Bitmap bitmap = Bitmap.createBitmap(getDrawingCache());
            setDrawingCacheEnabled(false);
            return bitmap;
        }

        static Bitmap capture(Context context, LocaleList localeList, String string) {
            final CaptureTextView textView = new CaptureTextView(context);
            if (localeList != null) textView.setTextLocales(localeList);
            return textView.capture(string);
        }
    }

}
