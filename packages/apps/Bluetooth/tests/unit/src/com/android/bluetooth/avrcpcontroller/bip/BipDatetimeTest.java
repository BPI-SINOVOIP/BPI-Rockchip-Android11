/*
 * Copyright 2019 The Android Open Source Project
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

package com.android.bluetooth.avrcpcontroller;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Calendar;
import java.util.Date;
import java.util.Locale;
import java.util.TimeZone;

/**
 * A test suite for the BipDateTime class
 */
@RunWith(AndroidJUnit4.class)
public class BipDatetimeTest {

    private Date makeDate(int month, int day, int year, int hours, int min, int sec, TimeZone tz) {
        Calendar.Builder builder = new Calendar.Builder();

        /* Note that Calendar months are zero-based in Java framework */
        builder.setDate(year, month - 1, day);
        builder.setTimeOfDay(hours, min, sec, 0);
        if (tz != null) builder.setTimeZone(tz);
        return builder.build().getTime();
    }

    private Date makeDate(int month, int day, int year, int hours, int min, int sec) {
        return makeDate(month, day, year, hours, min, sec, null);
    }

    private String makeTzAdjustedString(int month, int day, int year, int hours, int min,
            int sec) {
        Calendar cal = Calendar.getInstance();
        cal.setTime(makeDate(month, day, year, hours, min, sec));
        cal.setTimeZone(TimeZone.getDefault());
        return String.format(Locale.US, "%04d%02d%02dT%02d%02d%02d", cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH) + 1, cal.get(Calendar.DATE),
                    cal.get(Calendar.HOUR_OF_DAY), cal.get(Calendar.MINUTE),
                    cal.get(Calendar.SECOND));
    }

    private void testParse(String date, Date expectedDate, boolean isUtc, String expectedStr) {
        BipDateTime bipDateTime = new BipDateTime(date);
        Assert.assertEquals(expectedDate, bipDateTime.getTime());
        Assert.assertEquals(isUtc, bipDateTime.isUtc());
        Assert.assertEquals(expectedStr, bipDateTime.toString());
    }

    private void testCreate(Date date, String dateStr) {
        BipDateTime bipDate = new BipDateTime(date);
        Assert.assertEquals(date, bipDate.getTime());
        Assert.assertTrue(bipDate.isUtc());
        Assert.assertEquals(dateStr, bipDate.toString());
    }

    @Test
    public void testCreateFromValidString() {
        testParse("20000101T000000", makeDate(1, 1, 2000, 0, 0, 0), false,
                makeTzAdjustedString(1, 1, 2000, 0, 0, 0));
        testParse("20000101T060115", makeDate(1, 1, 2000, 6, 1, 15), false,
                makeTzAdjustedString(1, 1, 2000, 6, 1, 15));
        testParse("20000101T060000", makeDate(1, 1, 2000, 6, 0, 0), false,
                makeTzAdjustedString(1, 1, 2000, 6, 0, 0));
        testParse("20000101T071500", makeDate(1, 1, 2000, 7, 15, 0), false,
                makeTzAdjustedString(1, 1, 2000, 7, 15, 0));
        testParse("20000101T151700", makeDate(1, 1, 2000, 15, 17, 0), false,
                makeTzAdjustedString(1, 1, 2000, 15, 17, 0));
        testParse("20000101T235959", makeDate(1, 1, 2000, 23, 59, 59), false,
                makeTzAdjustedString(1, 1, 2000, 23, 59, 59));
        testParse("20501127T235959", makeDate(11, 27, 2050, 23, 59, 59), false,
                makeTzAdjustedString(11, 27, 2050, 23, 59, 59));
    }

    @Test
    public void testParseFromValidStringWithUtc() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        testParse("20000101T000000Z", makeDate(1, 1, 2000, 0, 0, 0, utc), true,
                "20000101T000000Z");
        testParse("20000101T060115Z", makeDate(1, 1, 2000, 6, 1, 15, utc), true,
                "20000101T060115Z");
        testParse("20000101T060000Z", makeDate(1, 1, 2000, 6, 0, 0, utc), true,
                "20000101T060000Z");
        testParse("20000101T071500Z", makeDate(1, 1, 2000, 7, 15, 0, utc), true,
                "20000101T071500Z");
        testParse("20000101T151700Z", makeDate(1, 1, 2000, 15, 17, 0, utc), true,
                "20000101T151700Z");
        testParse("20000101T235959Z", makeDate(1, 1, 2000, 23, 59, 59, utc), true,
                "20000101T235959Z");
        testParse("20501127T235959Z", makeDate(11, 27, 2050, 23, 59, 59, utc), true,
                "20501127T235959Z");
    }

    @Test(expected = ParseException.class)
    public void testParseNullString_throwsException() {
        testParse(null, null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseEmptyString_throwsException() {
        testParse("", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseTooFewCharacters_throwsException() {
        testParse("200011T61515", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadYear_throwsException() {
        testParse("00000101T000000", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadMonth_throwsException() {
        testParse("20000001T000000", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseDayTooSmall_throwsException() {
        testParse("20000100T000000", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseDayTooLarge_throwsException() {
        testParse("20000132T000000", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadHours_throwsException() {
        testParse("20000132T250000", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadMinutes_throwsException() {
        testParse("20000132T006100", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadSeconds_throwsException() {
        testParse("20000132T000061", null, false, null);
    }

    @Test(expected = ParseException.class)
    public void testParseBadCharacters_throwsException() {
        testParse("2ABC0101T000000", null, false, null);
    }

    @Test
    public void testCreateFromDateTime() {
        TimeZone utc = TimeZone.getTimeZone("UTC");
        utc.setRawOffset(0);
        // Note: All Java Date objects are stored as UTC timestamps so we expect all of these to be
        // UTC strings when formatted for BIP objects
        testCreate(makeDate(1, 1, 2000, 6, 1, 15, utc), "20000101T060115Z");
        testCreate(makeDate(1, 1, 2000, 6, 0, 0, utc), "20000101T060000Z");
        testCreate(makeDate(1, 1, 2000, 23, 59, 59, utc), "20000101T235959Z");
        testCreate(makeDate(11, 27, 2050, 23, 59, 59, utc), "20501127T235959Z");
    }
}
