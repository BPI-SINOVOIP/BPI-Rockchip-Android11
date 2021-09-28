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

package com.android.car.dialer.ui.common;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;
import android.text.SpannableString;
import android.text.style.TextAppearanceSpan;

import com.android.car.dialer.CarDialerRobolectricTestRunner;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;

@RunWith(CarDialerRobolectricTestRunner.class)
public class QueryStyleTest {
    private static final String NUMBER_1 = "+1-234-567-8901";
    private static final String NUMBER_2 = "+1 234 567 8901";
    private static final String NUMBER_3 = "+1(234)-567-8901";
    private static final String NUMBER_4 = "234-567-8901";
    private static final String NUMBER_5 = "1234567890";
    private static final String NUMBER_6 = "+67212345678";
    private static final String NUMBER_7 = "+672-1234-5678";
    private static final String NUMBER_8 = "+44 1234 567890";
    private static final String QUERY_1 = "1";
    private static final String QUERY_2 = "890";
    private static final String QUERY_3 = "45";
    private static final String QUERY_4 = "987";
    private static final String QUERY_EMPTY = "";

    private static final int STYLE = android.R.style.TextAppearance_Large;

    private QueryStyle mQueryStyle;

    @Before
    public void setUp() {
        Context context = RuntimeEnvironment.application;
        mQueryStyle = new QueryStyle(context, STYLE);
    }

    @Test
    public void test_spannableString1WithQuery1() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_1,
                QUERY_1);

        int startIndex = 1;
        checkCorrectness(testString, startIndex, QUERY_1.length(), 0);
    }

    @Test
    public void test_spannableString1WithQuery2() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_1,
                QUERY_2);

        checkCorrectness(testString, 11, QUERY_2.length(), 0);
    }

    @Test
    public void test_spannableString1WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_1,
                QUERY_3);

        checkCorrectness(testString, 5, QUERY_3.length(), 1);
    }

    @Test
    public void test_spannableString1WithQuery4_queryIsNotPartOfTheString() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_1,
                QUERY_4);

        checkCorrectness(testString, -1, QUERY_4.length(), 0);
    }

    @Test
    public void test_spannableString1WithEmptyQuery() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_1,
                QUERY_EMPTY);

        checkCorrectness(testString, -1, QUERY_EMPTY.length(), 0);
    }

    @Test
    public void test_spannableString2WithQuery1() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_2,
                QUERY_1);

        int startIndex = 1;
        checkCorrectness(testString, startIndex, QUERY_1.length(), 0);
    }

    @Test
    public void test_spannableString2WithQuery2() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_2,
                QUERY_2);

        checkCorrectness(testString, 11, QUERY_2.length(), 0);
    }

    @Test
    public void test_spannableString2WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_2,
                QUERY_3);

        checkCorrectness(testString, 5, QUERY_3.length(), 1);
    }

    @Test
    public void test_spannableString3WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_3,
                QUERY_3);

        checkCorrectness(testString, 5, QUERY_3.length(), 2);
    }

    @Test
    public void test_spannableString4WithQuery2() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_4,
                QUERY_2);

        checkCorrectness(testString, 8, QUERY_2.length(), 0);
    }

    @Test
    public void test_spannableString4WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_4,
                QUERY_3);

        checkCorrectness(testString, 2, QUERY_3.length(), 1);
    }

    @Test
    public void test_spannableString5WithQuery2() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_5,
                QUERY_2);

        checkCorrectness(testString, 7, QUERY_2.length(), 0);
    }

    @Test
    public void test_spannableString5WithQuery4() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_5,
                QUERY_4);

        checkCorrectness(testString, -1, QUERY_4.length(), 0);
    }

    @Test
    public void test_spannableString6WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_6,
                QUERY_3);

        checkCorrectness(testString, 7, QUERY_3.length(), 0);
    }

    @Test
    public void test_spannableString6WithQuery4() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_6,
                QUERY_4);

        checkCorrectness(testString, -1, QUERY_4.length(), 0);
    }

    @Test
    public void test_spannableString7WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_7,
                QUERY_3);

        checkCorrectness(testString, 8, QUERY_3.length(), 1);
    }

    @Test
    public void test_spannableString8WithQuery3() {
        SpannableString testString = mQueryStyle.getStringWithQueryInSpecialStyle(NUMBER_8,
                QUERY_3);

        checkCorrectness(testString, 7, QUERY_3.length(), 1);
    }

    private void checkCorrectness(SpannableString testString, int startIndex, int length,
            int specialCharNumber) {
        int endIndex = startIndex == -1 ? -1 : startIndex + length + specialCharNumber;
        TextAppearanceSpan textAppearanceSpan = mQueryStyle.getTextAppearanceSpan();
        assertThat(testString.getSpanStart(textAppearanceSpan)).isEqualTo(startIndex);
        assertThat(testString.getSpanEnd(textAppearanceSpan)).isEqualTo(endIndex);
        if (startIndex != -1) {
            assertThat(testString.getSpanFlags(textAppearanceSpan)).isEqualTo(
                    SpannableString.SPAN_INCLUSIVE_EXCLUSIVE);
        }
    }
}
