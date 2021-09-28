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

package com.android.libcore.java.text;

import static org.junit.Assert.assertEquals;

import android.icu.testsharding.MainTestShard;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.JUnit4;

import java.text.DecimalFormat;
import java.text.DecimalFormatSymbols;
import java.text.NumberFormat;
import java.util.Locale;

/**
 * Test for libcore APIs implemented by ICU.
 * This test isn't put into CtsLibocreTestCases due to ART/libcore unbundling. http://b/164511892
 */
@MainTestShard
@RunWith(JUnit4.class)
public class NumberFormatTest {

    // http://b/162744366
    @Test
    public void testMonetarySeparator() {
        Locale locale = Locale.forLanguageTag("en-BE");
        DecimalFormat df = (DecimalFormat) NumberFormat.getCurrencyInstance(locale);
        DecimalFormatSymbols dfs = df.getDecimalFormatSymbols();
        // Check the assumption on the en-BE locale data. If the data is changed, please fix the
        // below assert statement.
        assertEquals('.', dfs.getGroupingSeparator());
        assertEquals(',', dfs.getDecimalSeparator());
        assertEquals(',', dfs.getMonetaryDecimalSeparator());
        // Assert that the separators in DecimalFormatSymbols are used.
        assertEquals("€9.876,66",df.format(9876.66));
    }
}
