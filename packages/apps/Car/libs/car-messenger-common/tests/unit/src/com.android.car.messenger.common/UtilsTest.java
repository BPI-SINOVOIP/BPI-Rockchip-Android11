/*
 * Copyright (C) 2020 The Android Open Source Project
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

package com.android.car.messenger.common;

import static com.google.common.truth.Truth.assertThat;

import android.text.BidiFormatter;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.List;

@RunWith(AndroidJUnit4.class)
public class UtilsTest {

    private static final String ARABIC_NAME = "جﺗﺧ";
    private static final List<String> NAMES = Arrays.asList("+1-650-900-1234", "Logan.", "Emily",
            "Christopher", "!Sam", ARABIC_NAME);
    private static final String NAME_DELIMITER = "، ";
    private static final String TITLE_DELIMITER = " : ";
    private static final int TITLE_LENGTH = 30;
    private static final BidiFormatter RTL_FORMATTER = BidiFormatter.getInstance(/* rtlContext= */
            true);

    @Test
    public void testNameWithMultipleNumbers() {
        // Ensure that a group name with many phone numbers sorts the phone numbers correctly.
        List<String> senderNames = Arrays.asList("+1-650-900-1234", "+1-650-900-1111",
                "+1-100-200-1234");
        String actual = Utils.constructGroupConversationTitle(senderNames, NAME_DELIMITER,
                TITLE_LENGTH + 20);
        String expected = "+1-100-200-1234، +1-650-900-1111، +1-650-900-1234";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testNameWithInternationalNumbers() {
        // Ensure that a group name with many phone numbers sorts the phone numbers correctly.
        List<String> senderNames = Arrays.asList("+44-20-7183-8750", "+1-650-900-1111",
                "+1-100-200-1234");
        String actual = Utils.constructGroupConversationTitle(senderNames, NAME_DELIMITER,
                TITLE_LENGTH + 20);
        String expected = "+1-100-200-1234، +1-650-900-1111، +44-20-7183-8750";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testNameConstructorLtr() {
        String actual = Utils.constructGroupConversationTitle(NAMES, NAME_DELIMITER, TITLE_LENGTH);
        assertThat(actual).isEqualTo("!Sam، Christopher، Emily، Logan.");
    }

    @Test
    public void testNameConstructorLtr_longerTitle() {
        String actual = Utils.constructGroupConversationTitle(NAMES, NAME_DELIMITER,
                TITLE_LENGTH + 5);
        assertThat(actual).isEqualTo(
                "!Sam، Christopher، Emily، Logan.، \u200E\u202Bجﺗﺧ\u202C\u200E");

    }

    @Test
    public void testTitleConstructorLtr() {
        String actual = Utils.constructGroupConversationHeader("Christopher",
                "!Sam، Emily، Logan.، +1-650-900-1234", TITLE_DELIMITER);
        String expected = "Christopher : !Sam، Emily، Logan.، +1-650-900-1234";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testTitleConstructorLtr_with_rtlName() {
        String actual = Utils.constructGroupConversationHeader(ARABIC_NAME, "!Sam، Logan.، جﺗﺧ",
                TITLE_DELIMITER);
        // Note: the Group name doesn't have the RTL tag because in the function we format the
        // entire group name string, not each name in the string.
        String expected = "\u200E\u202Bجﺗﺧ\u202C\u200E : !Sam، Logan.، جﺗﺧ\u200E";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testTitleConstructorLtr_with_phoneNumber() {
        String actual = Utils.constructGroupConversationHeader("+1-650-900-1234",
                "!Sam، Logan.، جﺗﺧ",
                TITLE_DELIMITER);
        // Note: the Group name doesn't have the RTL tag because in the function we format the
        // entire group name string, not each name in the string.
        String expected = "+1-650-900-1234 : !Sam، Logan.، جﺗﺧ\u200E";
        assertThat(actual).isEqualTo(expected);
    }

    /**
     * NOTE for all the RTL tests done below: When BidiFormatter is unicode-wrapping strings, they
     * are actually adding invisible Unicode characters to denote whether a section is RTL, LTR,
     * etc. These invisible characters are NOT visible on the terminal output, or if you copy
     * paste the string to most HTML pages. They ARE visible when you paste them in certain
     * text editors like IntelliJ, or there are some online tools that provide this as well.
     *
     * Therefore, in most of these RTL tests (and some of the LTR tests) you will see the
     * invisible characters in the expected strings. Here's a couple of the characters, and what
     * they're used for:
     * \u200F is the RTL mark
     * \u200E is the LTR mark
     * \u202A marks the start of LTR embedding
     * \u202B marks the start of RTL embedding
     * \u202C pops the directional formatting - Must be used to end an embedding
     */
    @Test
    public void testNameWithInternationalNumbers_rtl() {
        // Ensure that a group name with many phone numbers sorts the phone numbers correctly.
        List<String> senderNames = Arrays.asList("+44-20-7183-8750", "+1-650-900-1111",
                "+1-100-200-1234");
        String actual = Utils.constructGroupConversationTitle(senderNames, NAME_DELIMITER,
                TITLE_LENGTH + 20, RTL_FORMATTER);
        String expected = "\u200F\u202A\u200F\u202A+1-100-200-1234\u202C\u200F\u200F\u202A، "
                + "\u202C\u200F\u200F\u202A+1-650-900-1111\u202C\u200F\u200F\u202A، "
                + "\u202C\u200F\u200F\u202A+44-20-7183-8750\u202C\u200F\u202C\u200F";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testNameConstructorRtl() {
        String actual = Utils.constructGroupConversationTitle(NAMES, NAME_DELIMITER, TITLE_LENGTH,
                /* isRtl */ RTL_FORMATTER);

        String expected =
                "\u200F\u202A\u200F\u202A!Sam\u202C\u200F\u200F\u202A، \u202C\u200F"
                        + "\u200F\u202AChristopher\u202C\u200F\u200F\u202A، \u202C\u200F"
                        + "\u200F\u202AEmily\u202C\u200F\u200F\u202A، "
                        + "\u202C\u200F\u200F\u202ALogan.\u202C\u200F\u202C\u200F";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testNameConstructorRtl_longerTitle() {
        String actual = Utils.constructGroupConversationTitle(NAMES, NAME_DELIMITER,
                TITLE_LENGTH + 5, /* isRtl */ RTL_FORMATTER);

        String expected =
                "\u200F\u202A\u200F\u202A!Sam\u202C\u200F\u200F\u202A، "
                        + "\u202C\u200F\u200F\u202AChristopher\u202C\u200F\u200F"
                        + "\u202A، \u202C\u200F\u200F\u202AEmily\u202C\u200F\u200F\u202A، "
                        + "\u202C\u200F\u200F\u202ALogan.\u202C\u200F\u200F\u202A، "
                        + "\u202C\u200Fجﺗﺧ\u202C\u200F";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testTitleConstructorRtl_with_rtlName() {
        String actual = Utils.constructGroupConversationHeader(ARABIC_NAME, "!Sam، Logan.، جﺗﺧ",
                TITLE_DELIMITER, RTL_FORMATTER);
        // Note: the Group name doesn't have the RTL tag because in the function we format the
        // entire group name string, not each name in the string.
        // Also, note that the sender's name, which is RTL still has LTR embedded because we wrap
        // it with FIRSTSTRONG_LTR.
        String expected = "\u200F\u202Aجﺗﺧ : \u200F\u202A!Sam، Logan.، جﺗﺧ\u202C\u200F\u202C"
                + "\u200F";
        assertThat(actual).isEqualTo(expected);
    }


    @Test
    public void testTitleConstructorRtl_with_phoneNumber() {
        String actual = Utils.constructGroupConversationHeader("+1-650-900-1234",
                "!Sam، Logan.، جﺗﺧ",
                TITLE_DELIMITER, RTL_FORMATTER);
        // Note: the Group name doesn't have the RTL tag because in the function we format the
        // entire group name string, not each name in the string.
        String expected = "\u200F\u202A\u200F\u202A+1-650-900-1234\u202C\u200F : "
                + "\u200F\u202A!Sam، Logan.، جﺗﺧ\u202C\u200F\u202C\u200F";
        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testTitleConstructorRtl() {
        String actual = Utils.constructGroupConversationHeader("Christopher",
                "+1-650-900-1234، Logan.، Emily، Christopher، !Sam", TITLE_DELIMITER, /* isRtl */
                RTL_FORMATTER).trim();

        String expected =
                "\u200F\u202A\u200F\u202AChristopher\u202C\u200F : \u200F\u202A+1-650-900-1234، "
                        + "Logan.، Emily، Christopher، !Sam\u202C\u200F\u202C\u200F";

        assertThat(actual).isEqualTo(expected);
    }

    @Test
    public void testTitleConstructorRtl_withTrailingPunctuation() {
        String actual = Utils.constructGroupConversationHeader("Christopher",
                "Abcd!!!", TITLE_DELIMITER, /* isRtl */
                RTL_FORMATTER).trim();

        String expected =
                "\u200F\u202A\u200F\u202AChristopher\u202C\u200F : \u200F\u202AAbcd!!!"
                        + "\u202C\u200F\u202C\u200F";

        assertThat(actual).isEqualTo(expected);
    }
}
