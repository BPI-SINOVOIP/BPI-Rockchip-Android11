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

package android.text.cts;

import static android.text.TextUtils.SAFE_STRING_FLAG_FIRST_LINE;
import static android.text.TextUtils.SAFE_STRING_FLAG_SINGLE_LINE;
import static android.text.TextUtils.SAFE_STRING_FLAG_TRIM;
import static android.text.TextUtils.makeSafeForPresentation;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Test;

public class MakeSafeForPresentationTest {
    private static final String LONG_STRING = "Lorem ipsum dolor sit amet, consectetur adipiscing "
            + "elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim "
            + "ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
            + "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse "
            + "cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non "
            + "proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

    private static final String VERY_LONG_STRING = LONG_STRING + LONG_STRING + LONG_STRING
            + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING
            + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING + LONG_STRING;

    private static final int DEFAULT_MAX_LABEL_SIZE_PX = 500;

    /**
     * Convert a string into a list of code-points
     */
    private static String strToCodePoints(String in) {
        StringBuilder out = new StringBuilder();
        int offset = 0;
        while (offset < in.length()) {
            if (out.length() != 0) {
                out.append(", ");
            }
            int charCount = Character.charCount(in.codePointAt(offset));
            out.append(Integer.toHexString(in.codePointAt(offset))).append("=\"")
                    .append(in.substring(offset, offset + charCount)).append("\"");
            offset += charCount;
        }

        return out.toString();
    }

    /**
     * Assert that {@code a.equals(b)} and if not throw a nicely formatted error.
     *
     * @param a First string to compare
     * @param b String to compare first string to
     */
    private static void assertStringEquals(String a, CharSequence b) {
        if (a.equals(b.toString())) {
            return;
        }

        throw new AssertionError("[" + strToCodePoints(a) + "] != ["
                + strToCodePoints(b.toString()) + "]");
    }

    /**
     * Make sure {@code abbreviated} is an abbreviation of {@code full}.
     *
     * @param full        The non-abbreviated string
     * @param abbreviated The abbreviated string
     * @return {@code true} iff the string was abbreviated
     */
    private static boolean isAbbreviation(String full, CharSequence abbreviated) {
        String abbrStr = abbreviated.toString();
        int abbrLength = abbrStr.length();
        int offset = 0;

        while (offset < abbrLength) {
            final int abbrCodePoint = abbrStr.codePointAt(offset);
            final int fullCodePoint = full.codePointAt(offset);
            final int codePointLen = Character.charCount(abbrCodePoint);

            if (abbrCodePoint != fullCodePoint) {
                return (abbrLength == offset + codePointLen)
                        && (/* ellipses */8230 == (abbrStr.codePointAt(offset)));
            }

            offset += codePointLen;
        }

        return false;
    }

    @Test
    public void removeIllegal() {
        assertStringEquals("ab", makeSafeForPresentation("a&#x8;b", 0, 0, 0));
    }

    @Test
    public void convertHTMLCharacters() {
        assertStringEquals("a&b", makeSafeForPresentation("a&amp;b", 0, 0, 0));
    }

    @Test
    public void convertHTMLGTAndLT() {
        assertStringEquals("a<br>b", makeSafeForPresentation("a&lt;br&gt;b", 0, 0, 0));
    }

    @Test
    public void removeHTMLFormatting() {
        assertStringEquals("ab", makeSafeForPresentation("a<b>b</>", 0, 0, 0));
    }

    @Test
    public void replaceImgTags() {
        assertStringEquals("a\ufffcb", makeSafeForPresentation("a<img src=\"myimg.jpg\" />b", 0, 0,
                0));
    }

    @Test
    public void replacePTags() {
        assertStringEquals("a\n\nb\n\n", makeSafeForPresentation("a<p>b", 0, 0, 0));
    }

    @Test
    public void aTagIsIgnored() {
        assertStringEquals("ab", makeSafeForPresentation("a<a href=\"foo.bar\">b</a>", 0, 0, 0));
    }

    @Test
    public void leadingAndMoreThanOneTrailingSpacesAreAlwaysTrimmed() {
        assertStringEquals("ab ", makeSafeForPresentation("    ab    ", 0, 0, 0));
    }

    @Test
    public void preIsIgnored() {
        assertStringEquals("a b ", makeSafeForPresentation("<pre>  a\n b   </pre>", 0, 0, 0));
    }

    @Test
    public void removeSpecialNotHtmlCharacters() {
        assertStringEquals("ab", makeSafeForPresentation("a\bb", 0, 0, 0));
    }

    @Test
    public void collapseInternalConsecutiveSpaces() {
        assertStringEquals("a b", makeSafeForPresentation("a  b", 0, 0, 0));
    }

    @Test
    public void replaceNonHtmlNewLineWithSpace() {
        assertStringEquals("a b", makeSafeForPresentation("a\nb", 0, 0, 0));
    }

    @Test
    public void keepNewLines() {
        assertStringEquals("a\nb", makeSafeForPresentation("a<br/>b", 0, 0, 0));
    }

    @Test
    public void removeNewLines() {
        assertStringEquals("ab", makeSafeForPresentation("a<br/>b", 0, 0,
                SAFE_STRING_FLAG_SINGLE_LINE));
    }

    @Test
    public void truncateAtNewLine() {
        assertStringEquals("a", makeSafeForPresentation("a<br/>b", 0, 0,
                SAFE_STRING_FLAG_FIRST_LINE));
    }

    @Test
    public void nbspIsTrimmed() {
        /*
         * A non breaking white space "&nbsp;" in HTML is not a white space according to
         * Character.isWhitespace, but for clean string it is treated as shite-space
         */
        assertStringEquals("a b", makeSafeForPresentation("&nbsp; a b", 0, 0,
                SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void trimFront() {
        assertStringEquals("a b", makeSafeForPresentation("<br/> <br/> a b", 0, 0,
                SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void trimAll() {
        assertStringEquals("", makeSafeForPresentation(" <br/> <br/> ", 0, 0,
                SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void trimEnd() {
        assertStringEquals("a b", makeSafeForPresentation("a b <br/> <br/>", 0, 0,
                SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void trimBoth() {
        assertStringEquals("a b", makeSafeForPresentation("<br/> <br/> a b <br/> <br/>", 0, 0,
                SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void ellipsizeShort() {
        assertTrue(isAbbreviation(VERY_LONG_STRING, makeSafeForPresentation(VERY_LONG_STRING, 0,
                DEFAULT_MAX_LABEL_SIZE_PX, 0)));
    }

    @Test
    public void ellipsizeLong() {
        assertTrue(isAbbreviation(VERY_LONG_STRING, makeSafeForPresentation(VERY_LONG_STRING, 0,
                DEFAULT_MAX_LABEL_SIZE_PX * 20, 0)));
    }

    @Test
    public void thousandDipLengthIs50Characters() {
        assertEquals(50, makeSafeForPresentation(VERY_LONG_STRING, 0, 1000, 0).length(), 10);
    }

    @Test
    public void ellipsizeShouldBeProportional() {
        assertEquals(20, (float) makeSafeForPresentation(VERY_LONG_STRING, 0,
                DEFAULT_MAX_LABEL_SIZE_PX * 20, 0).length() / makeSafeForPresentation(
                VERY_LONG_STRING, 0, DEFAULT_MAX_LABEL_SIZE_PX, 0).length(), 3);
    }

    @Test
    public void ignoreLastCharacters() {
        assertEquals(42, makeSafeForPresentation(VERY_LONG_STRING, 42, 0, 0).length());
    }

    @Test
    public void ignoreLastCharactersShortStirng() {
        assertEquals("abc", makeSafeForPresentation("abc", 42, 0, 0));
    }

    @Test
    public void ignoreLastCharactersWithTrim() {
        assertEquals("abc", makeSafeForPresentation("abc ", 4, 0, SAFE_STRING_FLAG_TRIM));
    }

    @Test
    public void ignoreLastCharactersWithHtml() {
        assertEquals("a\nbcd", makeSafeForPresentation("a<br />bcdef", 10, 0, 0));
    }

    @Test
    public void ignoreLastCharactersWithTrailingWhitespace() {
        assertEquals("abc ", makeSafeForPresentation("abc ", 4, 0, 0));
    }

    @Test(expected = IllegalArgumentException.class)
    public void cannotKeepAndTruncateNewLines() {
        makeSafeForPresentation("", 0, 0,
                SAFE_STRING_FLAG_SINGLE_LINE | SAFE_STRING_FLAG_FIRST_LINE);
    }

    @Test(expected = IllegalArgumentException.class)
    public void invalidFlags() {
        makeSafeForPresentation("", 0, 0, 0xffff);
    }

    @Test(expected = IllegalArgumentException.class)
    public void invalidEllipsizeDip() {
        makeSafeForPresentation("", 0, -1, 0);
    }

    @Test(expected = IllegalArgumentException.class)
    public void invalidMaxCharacters() {
        makeSafeForPresentation("", -1, 0, 0);
    }

    @Test(expected = NullPointerException.class)
    public void nullString() {
        makeSafeForPresentation(null, 0, 0, 0);
    }
}
