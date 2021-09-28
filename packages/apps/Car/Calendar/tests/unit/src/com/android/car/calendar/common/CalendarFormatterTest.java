/*
 * Copyright 2020 The Android Open Source Project
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

package com.android.car.calendar.common;

import static com.google.common.truth.Truth.assertThat;

import android.content.Context;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;

import java.time.Clock;
import java.time.LocalDateTime;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.util.Locale;

public class CalendarFormatterTest {

    private static final ZoneId BERLIN_ZONE_ID = ZoneId.of("Europe/Berlin");
    private static final ZonedDateTime CURRENT_DATE_TIME =
            LocalDateTime.of(2019, 12, 10, 10, 10, 10, 500500).atZone(BERLIN_ZONE_ID);
    private static final Locale LOCALE = Locale.ENGLISH;
    private CalendarFormatter mFormatter;

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getInstrumentation().getTargetContext();
        Clock clock = Clock.fixed(CURRENT_DATE_TIME.toInstant(), BERLIN_ZONE_ID);
        mFormatter = new CalendarFormatter(context, LOCALE, clock);
    }

    @Test
    public void getDateText_today() {
        String dateText = mFormatter.getDateText(CURRENT_DATE_TIME.toLocalDate());

        assertThat(dateText).startsWith("Today");
        assertThat(dateText).endsWith("Tue, Dec 10");
    }

    @Test
    public void getDateText_tomorrow() {
        String dateText = mFormatter.getDateText(CURRENT_DATE_TIME.plusDays(1).toLocalDate());

        assertThat(dateText).startsWith("Tomorrow");
        assertThat(dateText).endsWith("Wed, Dec 11");
    }

    @Test
    public void getDateText_nextWeek_onlyShowsDate() {
        String dateText = mFormatter.getDateText(CURRENT_DATE_TIME.plusDays(7).toLocalDate());

        assertThat(dateText).isEqualTo("Tue, Dec 17");
    }

    @Test
    public void getTimeRangeText_sameAmPm() {
        String dateText =
                mFormatter.getTimeRangeText(
                        CURRENT_DATE_TIME.toInstant(), CURRENT_DATE_TIME.plusHours(1).toInstant());

        assertThat(dateText).isEqualTo("10:10 – 11:10 AM");
    }

    @Test
    public void getTimeRangeText_differentAmPm() {
        String dateText =
                mFormatter.getTimeRangeText(
                        CURRENT_DATE_TIME.toInstant(), CURRENT_DATE_TIME.plusHours(3).toInstant());

        assertThat(dateText).isEqualTo("10:10 AM – 1:10 PM");
    }

    @Test
    public void getTimeRangeText_differentDays() {
        String dateText =
                mFormatter.getTimeRangeText(
                        CURRENT_DATE_TIME.toInstant(), CURRENT_DATE_TIME.plusDays(1).toInstant());

        assertThat(dateText).isEqualTo("Dec 10, 10:10 AM – Dec 11, 10:10 AM");
    }
}
