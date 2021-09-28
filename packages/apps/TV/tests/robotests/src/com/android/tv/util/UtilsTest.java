/*
 * Copyright (C) 2015 The Android Open Source Project
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
package com.android.tv.util;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.text.format.DateUtils;

import com.android.tv.testing.constants.ConfigConstants;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.annotation.Config;

import java.util.Calendar;
import java.util.GregorianCalendar;
import java.util.Locale;
import java.util.TimeZone;

/**
 * Tests for {@link com.android.tv.util.Utils#getDurationString}.
 *
 * <p>This test uses deprecated flags {@link DateUtils#FORMAT_12HOUR} and {@link
 * DateUtils#FORMAT_24HOUR} to run this test independent to system's 12/24h format.
 */
@RunWith(RobolectricTestRunner.class)
@Config(sdk = ConfigConstants.SDK)
public class UtilsTest {
    // TODO: Mock Context so we can specify current time and locale for test.
    private Locale mLocale;
    private static final long DATE_THIS_YEAR_2_1_MS = getFebOfThisYearInMillis(1, 0, 0);

    // All possible list for a parameter to test parameter independent result.
    private static final boolean[] PARAM_USE_SHORT_FORMAT = {false, true};

    @Before
    public void setUp() {
        // Set locale to US
        mLocale = Locale.getDefault();
        Locale.setDefault(Locale.US);
    }

    @After
    public void tearDown() {
        // Revive system locale.
        Locale.setDefault(mLocale);
    }

    /** Return time in millis assuming that whose year is this year and month is Jan. */
    private static long getJanOfThisYearInMillis(int date, int hour, int minutes) {
        return new GregorianCalendar(getThisYear(), Calendar.JANUARY, date, hour, minutes)
                .getTimeInMillis();
    }

    private static long getJanOfThisYearInMillis(int date, int hour) {
        return getJanOfThisYearInMillis(date, hour, 0);
    }

    /** Return time in millis assuming that whose year is this year and month is Feb. */
    private static long getFebOfThisYearInMillis(int date, int hour, int minutes) {
        return new GregorianCalendar(getThisYear(), Calendar.FEBRUARY, date, hour, minutes)
                .getTimeInMillis();
    }

    private static long getFebOfThisYearInMillis(int date, int hour) {
        return getFebOfThisYearInMillis(date, hour, 0);
    }

    private static int getThisYear() {
        return new GregorianCalendar().get(GregorianCalendar.YEAR);
    }

    @Test
    public void testSameDateAndTime() {
        assertEquals(
                "3:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(1, 3),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "03:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(1, 3),
                        false,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testDurationWithinToday() {
        assertEquals(
                "12:00 – 3:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "00:00 – 03:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        false,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testDurationFromYesterdayToToday() {
        assertEquals(
                "Jan 31, 3:00 AM – Feb 1, 4:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getJanOfThisYearInMillis(31, 3),
                        getFebOfThisYearInMillis(1, 4),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Jan 31, 03:00 – Feb 1, 04:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getJanOfThisYearInMillis(31, 3),
                        getFebOfThisYearInMillis(1, 4),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "1/31, 11:30 PM – 12:30 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getJanOfThisYearInMillis(31, 23, 30),
                        getFebOfThisYearInMillis(1, 0, 30),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "1/31, 23:30 – 00:30",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getJanOfThisYearInMillis(31, 23, 30),
                        getFebOfThisYearInMillis(1, 0, 30),
                        true,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testDurationFromTodayToTomorrow() {
        assertEquals(
                "Feb 1, 3:00 AM – Feb 2, 4:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(2, 4),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Feb 1, 03:00 – Feb 2, 04:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(2, 4),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "2/1, 3:00 AM – 2/2, 4:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(2, 4),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "2/1, 03:00 – 2/2, 04:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 3),
                        getFebOfThisYearInMillis(2, 4),
                        true,
                        DateUtils.FORMAT_24HOUR));

        assertEquals(
                "Feb 1, 11:30 PM – Feb 2, 12:30 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 23, 30),
                        getFebOfThisYearInMillis(2, 0, 30),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Feb 1, 23:30 – Feb 2, 00:30",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 23, 30),
                        getFebOfThisYearInMillis(2, 0, 30),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "11:30 PM – 12:30 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 23, 30),
                        getFebOfThisYearInMillis(2, 0, 30),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "23:30 – 00:30",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 23, 30),
                        getFebOfThisYearInMillis(2, 0, 30),
                        true,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testDurationWithinTomorrow() {
        assertEquals(
                "Feb 2, 2:00 – 4:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 2),
                        getFebOfThisYearInMillis(2, 4),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Feb 2, 02:00 – 04:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 2),
                        getFebOfThisYearInMillis(2, 4),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "2/2, 2:00 – 4:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 2),
                        getFebOfThisYearInMillis(2, 4),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "2/2, 02:00 – 04:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 2),
                        getFebOfThisYearInMillis(2, 4),
                        true,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testStartOfDay() {
        assertEquals(
                "12:00 – 1:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 1),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "00:00 – 01:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(1, 1),
                        false,
                        DateUtils.FORMAT_24HOUR));

        assertEquals(
                "Feb 2, 12:00 – 1:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(2, 1),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Feb 2, 00:00 – 01:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(2, 1),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "2/2, 12:00 – 1:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(2, 1),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "2/2, 00:00 – 01:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(2, 1),
                        true,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testEndOfDay() {
        for (boolean useShortFormat : PARAM_USE_SHORT_FORMAT) {
            assertEquals(
                    "11:00 PM – 12:00 AM",
                    Utils.getDurationString(
                            RuntimeEnvironment.application,
                            DATE_THIS_YEAR_2_1_MS,
                            getFebOfThisYearInMillis(1, 23),
                            getFebOfThisYearInMillis(2, 0),
                            useShortFormat,
                            DateUtils.FORMAT_12HOUR));
            assertEquals(
                    "23:00 – 00:00",
                    Utils.getDurationString(
                            RuntimeEnvironment.application,
                            DATE_THIS_YEAR_2_1_MS,
                            getFebOfThisYearInMillis(1, 23),
                            getFebOfThisYearInMillis(2, 0),
                            useShortFormat,
                            DateUtils.FORMAT_24HOUR));
        }

        assertEquals(
                "Feb 2, 11:00 PM – 12:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 23),
                        getFebOfThisYearInMillis(3, 0),
                        false,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "Feb 2, 23:00 – 00:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 23),
                        getFebOfThisYearInMillis(3, 0),
                        false,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "2/2, 11:00 PM – 12:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 23),
                        getFebOfThisYearInMillis(3, 0),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "2/2, 23:00 – 00:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 23),
                        getFebOfThisYearInMillis(3, 0),
                        true,
                        DateUtils.FORMAT_24HOUR));
        assertEquals(
                "2/2, 12:00 AM – 2/3, 12:00 AM",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(3, 0),
                        true,
                        DateUtils.FORMAT_12HOUR));
        assertEquals(
                "2/2, 00:00 – 2/3, 00:00",
                Utils.getDurationString(
                        RuntimeEnvironment.application,
                        DATE_THIS_YEAR_2_1_MS,
                        getFebOfThisYearInMillis(2, 0),
                        getFebOfThisYearInMillis(3, 0),
                        true,
                        DateUtils.FORMAT_24HOUR));
    }

    @Test
    public void testMidnight() {
        for (boolean useShortFormat : PARAM_USE_SHORT_FORMAT) {
            assertEquals(
                    "12:00 AM",
                    Utils.getDurationString(
                            RuntimeEnvironment.application,
                            DATE_THIS_YEAR_2_1_MS,
                            DATE_THIS_YEAR_2_1_MS,
                            DATE_THIS_YEAR_2_1_MS,
                            useShortFormat,
                            DateUtils.FORMAT_12HOUR));
            assertEquals(
                    "00:00",
                    Utils.getDurationString(
                            RuntimeEnvironment.application,
                            DATE_THIS_YEAR_2_1_MS,
                            DATE_THIS_YEAR_2_1_MS,
                            DATE_THIS_YEAR_2_1_MS,
                            useShortFormat,
                            DateUtils.FORMAT_24HOUR));
        }
    }

    @Test
    public void testIsInGivenDay() {
        assertTrue(
                Utils.isInGivenDay(
                        new GregorianCalendar(2015, Calendar.JANUARY, 1).getTimeInMillis(),
                        new GregorianCalendar(2015, Calendar.JANUARY, 1, 0, 30).getTimeInMillis()));
    }

    @Test
    public void testIsNotInGivenDay() {
        assertFalse(
                Utils.isInGivenDay(
                        new GregorianCalendar(2015, Calendar.JANUARY, 1).getTimeInMillis(),
                        new GregorianCalendar(2015, Calendar.JANUARY, 2).getTimeInMillis()));
    }

    @Test
    public void testIfTimeZoneApplied() {
        TimeZone timeZone = TimeZone.getDefault();

        TimeZone.setDefault(TimeZone.getTimeZone("Asia/Seoul"));

        // 2015.01.01 00:00 in KST = 2014.12.31 15:00 in UTC
        long date2015StartMs = new GregorianCalendar(2015, Calendar.JANUARY, 1).getTimeInMillis();

        // 2015.01.01 10:00 in KST = 2015.01.01 01:00 in UTC
        long date2015Start10AMMs =
                new GregorianCalendar(2015, Calendar.JANUARY, 1, 10, 0).getTimeInMillis();

        // Those two times aren't in the same day in UTC, but they are in KST.
        assertTrue(Utils.isInGivenDay(date2015StartMs, date2015Start10AMMs));

        TimeZone.setDefault(timeZone);
    }

    @Test
    public void testIsInternalTvInputInvalidInternalInputId() {
        String inputId = "tv.comp";
        assertFalse(Utils.isInternalTvInput(RuntimeEnvironment.application, inputId));
    }
}
