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
 * limitations under the License
 */
package com.android.tv.data;

import static com.android.tv.data.ChannelNumber.parseChannelNumber;
import static com.android.tv.testing.ChannelNumberSubject.assertThat;

import static com.google.common.truth.Truth.assertThat;

import com.android.tv.testing.ComparableTester;
import com.android.tv.testing.constants.ConfigConstants;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.annotation.Config;

/** Tests for {@link ChannelNumber}. */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class ChannelNumberTest {

    @Test
    public void newChannelNumber() {
        assertThat(new ChannelNumber()).isEmpty();
    }

    @Test
    public void parseChannelNumber_empty() {
        assertThat(parseChannelNumber("")).isNull();
    }

    @Test
    public void parseChannelNumber_dash() {
        assertThat(parseChannelNumber("-")).isNull();
    }

    @Test
    public void parseChannelNumber_abcd12() {
        assertThat(parseChannelNumber("abcd12")).isNull();
    }

    @Test
    public void parseChannelNumber_12abcd() {
        assertThat(parseChannelNumber("12abcd")).isNull();
    }

    @Test
    public void parseChannelNumber_dash12() {
        assertThat(parseChannelNumber("-12")).isNull();
    }

    @Test
    public void parseChannelNumber_1() {
        assertThat(parseChannelNumber("1")).displaysAs(1);
    }

    @Test
    public void parseChannelNumber_1234dash4321() {
        assertThat(parseChannelNumber("1234-4321")).displaysAs(1234, 4321);
    }

    @Test
    public void parseChannelNumber_3dash4() {
        assertThat(parseChannelNumber("3-4")).displaysAs(3, 4);
    }

    @Test
    public void parseChannelNumber_5dash6() {
        assertThat(parseChannelNumber("5-6")).displaysAs(5, 6);
    }

    @Test
    public void compareTo() {
        new ComparableTester<ChannelNumber>()
                .addEquivalentGroup(parseChannelNumber("1"), parseChannelNumber("1"))
                .addEquivalentGroup(parseChannelNumber("2"))
                .addEquivalentGroup(parseChannelNumber("2-1"))
                .addEquivalentGroup(parseChannelNumber("2-2"))
                .addEquivalentGroup(parseChannelNumber("2-10"))
                .addEquivalentGroup(parseChannelNumber("3"))
                .addEquivalentGroup(parseChannelNumber("4"), parseChannelNumber("4-0"))
                .addEquivalentGroup(parseChannelNumber("10"))
                .addEquivalentGroup(parseChannelNumber("100"))
                .test();
    }

    @Test
    public void compare_null_null() {
        assertThat(ChannelNumber.compare(null, null)).isEqualTo(0);
    }

    @Test
    public void compare_1_1() {
        assertThat(ChannelNumber.compare("1", "1")).isEqualTo(0);
        ;
    }

    @Test
    public void compare_null_1() {
        assertThat(ChannelNumber.compare(null, "1")).isLessThan(0);
    }

    @Test
    public void compare_abcd_1() {
        assertThat(ChannelNumber.compare("abcd", "1")).isLessThan(0);
    }

    @Test
    public void compare_dash1_1() {
        assertThat(ChannelNumber.compare(".4", "1")).isLessThan(0);
    }

    @Test
    public void compare_1_null() {
        assertThat(ChannelNumber.compare("1", null)).isGreaterThan(0);
    }

    @Test
    public void equivalent_null_to_null() {
        assertThat(ChannelNumber.equivalent(null, null)).isTrue();
    }

    @Test
    public void equivalent_1_to_1() {
        assertThat(ChannelNumber.equivalent("1", "1")).isTrue();
    }

    @Test
    public void equivalent_1d2_to_1() {
        assertThat(ChannelNumber.equivalent("1-2", "1")).isTrue();
    }

    @Test
    public void equivalent_1_to_1d2() {
        assertThat(ChannelNumber.equivalent("1", "1-2")).isTrue();
    }

    @Test
    public void equivalent_1_to_2_isFalse() {
        assertThat(ChannelNumber.equivalent("1", "2")).isFalse();
    }

    @Test
    public void equivalent_1d1_to_1d1() {
        assertThat(ChannelNumber.equivalent("1-1", "1-1")).isTrue();
    }

    @Test
    public void equivalent_1d1_to_1d2_isFalse() {
        assertThat(ChannelNumber.equivalent("1-1", "1-2")).isFalse();
    }

    @Test
    public void equivalent_1_to_null_isFalse() {
        assertThat(ChannelNumber.equivalent("1", null)).isFalse();
    }

    @Test
    public void equivalent_null_to_1_isFalse() {
        assertThat(ChannelNumber.equivalent(null, "1")).isFalse();
    }
}
