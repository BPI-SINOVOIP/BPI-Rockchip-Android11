/*
 * Copyright (C) 2017 The Android Open Source Project
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

package com.android.tv.tuner.util;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.common.util.PostalCodeUtils;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

import java.util.Locale;

/** Tests for {@link PostalCodeUtils} */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class PostalCodeUtilsTest {

    private static final String[] VALID_POSTCODES_US = {"94043", "94063", "90007"};
    private static final String[] INVALID_POSTCODES_US = {
        "", "9404", "ABC", "BCD8", "G777", "BT777", "OXX33", "E1WW", "SW1XX", "E11W", "SW10X",
        "GIR", "B8", "G77", "BT7", "OX33", "E1W", "SW1X"
    };
    private static final String[] VALID_POSTCODES_GB = {
        "GIR", "B8", "G77", "BT7", "OX33", "E1W", "SW1X", "GIR 0AA", "GIR0AA", "B8 2NE", "PR10BJ"
    };
    private static final String[] INVALID_POSTCODES_GB = {
        "", "9404", "ABC", "BCD8", "G777", "BT777", "OXX33", "E1WW", "SW1XX", "E11W", "SW10X",
        "94043", "94063", "90007", "B8 ", "OX331D"
    };

    @Test
    public void validPostcodesUs() {
        for (String postcode : VALID_POSTCODES_US) {
            assertThat(PostalCodeUtils.matches(postcode, Locale.US.getCountry())).isTrue();
        }
    }

    @Test
    public void validPostcodesGb() {
        for (String postcode : VALID_POSTCODES_GB) {
            assertThat(PostalCodeUtils.matches(postcode, Locale.UK.getCountry())).isTrue();
        }
    }

    @Test
    public void invalidPostcodesUs() {
        for (String postcode : INVALID_POSTCODES_US) {
            assertThat(PostalCodeUtils.matches(postcode, Locale.US.getCountry())).isFalse();
        }
    }

    @Test
    public void invalidPostcodesGb() {
        for (String postcode : INVALID_POSTCODES_GB) {
            assertThat(PostalCodeUtils.matches(postcode, Locale.UK.getCountry())).isFalse();
        }
    }

    @Test
    public void unsupportedRegion() {
        for (String postcode : INVALID_POSTCODES_US) {
            // {@link Locale.ROOT} is an empty Locale
            assertThat(PostalCodeUtils.matches(postcode, Locale.ROOT.getCountry())).isTrue();
        }
    }
}
