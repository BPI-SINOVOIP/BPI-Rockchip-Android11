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

package android.net.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import androidx.annotation.NonNull;
import androidx.test.filters.SmallTest;
import androidx.test.runner.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.nio.charset.StandardCharsets;

/**
 * Tests for HostnameTransliterator class.
 *
 */
@RunWith(AndroidJUnit4.class)
@SmallTest
public class HostnameTransliteratorTest {
    private static final String TEST_HOST_NAME_EN = "AOSP on Crosshatch";
    private static final String TEST_HOST_NAME_EN_TRANSLITERATION = "AOSP-on-Crosshatch";
    private static final String TEST_HOST_NAME_JP = "AOSP on ほげほげ";
    private static final String TEST_HOST_NAME_JP_TRANSLITERATION = "AOSP-on-hogehoge";
    private static final String TEST_HOST_NAME_CN = "AOSP on 安卓";
    private static final String TEST_HOST_NAME_CN_TRANSLITERATION = "AOSP-on-an-zhuo";
    private static final String TEST_HOST_NAME_UNICODE = "AOSP on àéö→ūçŗ̊œ";
    private static final String TEST_HOST_NAME_UNICODE_TRANSLITERATION = "AOSP-on-aeo-ucroe";
    private static final String TEST_HOST_NAME_LONG =
            "AOSP on Ccccccrrrrrroooooosssssshhhhhhaaaaaattttttcccccchhhhhh3abcd";
    private static final String TEST_HOST_NAME_LONG_TRANSLITERATION =
            "AOSP-on-Ccccccrrrrrroooooosssssshhhhhhaaaaaattttttcccccchhhhhh3";
    private static final String TEST_HOST_NAME_LONG_TRUNCATED =
            "AOSP-on-Ccccccrrrrrroooooosssssshhhhhhaaaaaattttttcccccchhhhhh3";

    @NonNull
    private HostnameTransliterator mTransliterator;

    @Before
    public void setUp() throws Exception {
        mTransliterator = new HostnameTransliterator();
        assertNotNull(mTransliterator);
    }

    @Test
    public void testNullHostname() {
        assertNull(mTransliterator.transliterate(null));
    }

    private void assertHostnameTransliteration(final String hostnameAftertransliteration,
            final String hostname) {
        assertEquals(hostnameAftertransliteration, mTransliterator.transliterate(hostname));
    }

    @Test
    public void testEmptyHostname() {
        assertHostnameTransliteration(null, "");
    }

    @Test
    public void testHostnameOnlyTabs() {
        assertHostnameTransliteration(null, "\t\t");
    }

    @Test
    public void testHostnameOnlySpaces() {
        assertHostnameTransliteration(null, "    ");
    }

    @Test
    public void testHostnameOnlyUnsupportedAsciiSymbols() {
        final String symbol = new String(new byte[] { 0x00, 0x1b /* ESC */, 0x7f /* DEL */,
                0x10 /* backspace */}, StandardCharsets.US_ASCII);
        assertHostnameTransliteration(null, symbol);
    }

    @Test
    public void testHostnameMixedAsciiSymbols() {
        final String symbol = new String(new byte[] { 0x00, 'a', 0x1b /* ESC */, 0x7f /* DEL */,
                'b', 0x10 /* backspace */}, StandardCharsets.US_ASCII);
        assertHostnameTransliteration("a-b", symbol);
    }

    @Test
    public void testHostnames() {
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, TEST_HOST_NAME_EN);
        assertHostnameTransliteration(TEST_HOST_NAME_JP_TRANSLITERATION, TEST_HOST_NAME_JP);
        assertHostnameTransliteration(TEST_HOST_NAME_CN_TRANSLITERATION, TEST_HOST_NAME_CN);
        assertHostnameTransliteration(TEST_HOST_NAME_UNICODE_TRANSLITERATION,
                TEST_HOST_NAME_UNICODE);
    }

    @Test
    public void testHostnameWithMinusSign() {
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, "-AOSP on Crosshatch");
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, "AOSP on Crosshatch-");
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, "---AOSP on Crosshatch");
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, "AOSP on Crosshatch---");
        assertHostnameTransliteration(TEST_HOST_NAME_EN_TRANSLITERATION, "AOSP---on---Crosshatch");
    }

    @Test
    public void testLongHostname() {
        assertHostnameTransliteration(TEST_HOST_NAME_LONG_TRANSLITERATION, TEST_HOST_NAME_LONG);
    }

    @Test
    public void testNonAsciiHostname() {
        mTransliterator = new HostnameTransliterator(null);
        assertHostnameTransliteration(null, TEST_HOST_NAME_UNICODE);
    }

    @Test
    public void testAsciiLongHostname() {
        mTransliterator = new HostnameTransliterator(null);
        assertHostnameTransliteration(TEST_HOST_NAME_LONG_TRUNCATED, TEST_HOST_NAME_LONG);
    }
}
