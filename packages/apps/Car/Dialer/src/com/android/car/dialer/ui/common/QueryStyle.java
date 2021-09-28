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

import android.content.Context;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.TextUtils;
import android.text.style.TextAppearanceSpan;
import android.util.Pair;

import androidx.annotation.NonNull;

import java.util.HashMap;
import java.util.Map;

/**
 * A util class to add special style for the query string in a phone number.
 */
public class QueryStyle {

    private TextAppearanceSpan mTextAppearanceSpan;

    /**
     * The constructor for {@link QueryStyle}.
     *
     * @param context UI context used to access resources.
     * @param appearance is to specify TextAppearance resource to determine the text appearance.
     */
    public QueryStyle(Context context, int appearance) {
        mTextAppearanceSpan = new TextAppearanceSpan(context, appearance);
    }

    /**
     * Returns the TextAppearanceSpan.
     */
    public TextAppearanceSpan getTextAppearanceSpan() {
        return mTextAppearanceSpan;
    }

    /**
     * Returns a SpannableString, which is a similar string as the phone number string, but has a
     * special style for the query substring.
     */
    public SpannableString getStringWithQueryInSpecialStyle(@NonNull String phoneNumber,
            @NonNull String query) {
        SpannableString spannableString = new SpannableString(phoneNumber);

        String digitsOnlyQuery = getDigitsOnlyString(query);
        if (TextUtils.isEmpty(digitsOnlyQuery)) {
            return spannableString;
        }

        Pair<String, Map<Integer, Integer>> digitsOnlyNumberInfo = getDigitsOnlyNumberInfo(
                phoneNumber);
        int digitsOnlyStartIndex = digitsOnlyNumberInfo.first.indexOf(digitsOnlyQuery);

        if (digitsOnlyStartIndex == -1) {
            return spannableString;
        }

        int startIndex = digitsOnlyNumberInfo.second.get(digitsOnlyStartIndex);
        int digitsOnlyEndIndex = digitsOnlyStartIndex + digitsOnlyQuery.length() - 1;
        int endIndex = digitsOnlyNumberInfo.second.get(digitsOnlyEndIndex);
        spannableString.setSpan(mTextAppearanceSpan, startIndex, endIndex + 1,
                Spanned.SPAN_INCLUSIVE_EXCLUSIVE);
        return spannableString;
    }

    /**
     * @return a pair of String and Map. The string is a digit-only string for the phone number. And
     * the Map maps the index of every number from the digit-only number to the original index in
     * the original phone number.
     */
    private Pair<String, Map<Integer, Integer>> getDigitsOnlyNumberInfo(String phoneNumber) {
        StringBuilder numberWithOnlyDigits = new StringBuilder();
        Map<Integer, Integer> indexMap = new HashMap<>();
        for (int i = 0; i < phoneNumber.length(); i++) {
            char c = phoneNumber.charAt(i);
            if (Character.isDigit(c)) {
                numberWithOnlyDigits.append(c);
                indexMap.put(numberWithOnlyDigits.length() - 1, i);
            }
        }
        return new Pair<>(numberWithOnlyDigits.toString(), indexMap);
    }

    private String getDigitsOnlyString(String string) {
        StringBuilder digitsOnlyString = new StringBuilder();
        for (char c : string.toCharArray()) {
            if (Character.isDigit(c)) {
                digitsOnlyString.append(c);
            }
        }
        return digitsOnlyString.toString();
    }
}
