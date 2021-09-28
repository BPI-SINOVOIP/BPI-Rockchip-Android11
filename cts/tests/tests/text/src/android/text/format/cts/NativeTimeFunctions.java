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

package android.text.format.cts;

import static junit.framework.Assert.assertEquals;

import java.time.LocalDateTime;

/**
 * A test helper class for calling localtime_r() and mktime() for a given time zone.
 */
class NativeTimeFunctions {
    static {
        System.loadLibrary("ctstext_jni");
    }

    private NativeTimeFunctions() {}

    private static native StructTm localtime_tz(int timep, String tzId);
    private static native int mktime_tz(StructTm tm, String tzId);

    /**
     * Tries native time functions to see if they produce expected results.
     *
     * <p>localtime() is called with {tzId + localDateTime} and the result is checked against
     * timeMillis. mktime() is called with {tzId + timeMillis} and the result is checked against
     * localDateTime.
     *
     * <p>This method only works to second precision in the range of times that Android's
     * localtime() / mktime() work across all architectures; signed int32 seconds (because time_t
     * is dependent on word-size on Android). It may not work for ambiguous local times, i.e. local
     * times that don't exist (because of a "skip forward") or are duplicated (because of a "fall
     * back"). This method is not threadsafe as it uses the TZ environment variable.
     */
    static void assertNativeTimeResults(String tzId, LocalDateTime localDateTime, long timeMillis) {
        assertLocaltimeResult(tzId, timeMillis, localDateTime);
        assertMktimeResult(tzId, localDateTime, timeMillis);
    }

    private static void assertLocaltimeResult(String tzId, long timeMillis,
            LocalDateTime expected) {
        StructTm structTm = localtime_tz((int) (timeMillis / 1000), tzId);

        LocalDateTime actual = LocalDateTime.of(
                structTm.tm_year + 1900,
                structTm.tm_mon + 1,
                structTm.tm_mday,
                structTm.tm_hour,
                structTm.tm_min,
                structTm.tm_sec);
        assertEquals(timeMillis + " in " + tzId, expected, actual);
    }

    private static void assertMktimeResult(String tzId, LocalDateTime localDateTime,
            long expectedTimeMillis) {

        // Create a StructTm from the (second precision) information held in the LocalDateTime.
        StructTm tm = new StructTm();
        tm.tm_sec = localDateTime.getSecond();
        tm.tm_min = localDateTime.getMinute();
        tm.tm_hour = localDateTime.getHour();
        tm.tm_mday = localDateTime.getDayOfMonth();
        tm.tm_mon = localDateTime.getMonthValue() - 1;
        tm.tm_year = localDateTime.getYear() - 1900;
        tm.tm_isdst = -1;
        int actualTimeMillis = mktime_tz(tm, tzId);
        assertEquals(localDateTime + " in " + tzId,
                expectedTimeMillis, actualTimeMillis * 1000L);
    }

    /**
     * A basic Java representation of bionic's struct tm.
     */
    static class StructTm {
        public int tm_sec;    /* Seconds (0-60) */
        public int tm_min;    /* Minutes (0-59) */
        public int tm_hour;   /* Hours (0-23) */
        public int tm_mday;   /* Day of the month (1-31) */
        public int tm_mon;    /* Month (0-11) */
        public int tm_year;   /* Year - 1900 */
        public int tm_wday;   /* Day of the week (0-6, Sunday = 0) */
        public int tm_yday;   /* Day in the year (0-365, 1 Jan = 0) */
        public int tm_isdst;  /* Daylight saving time */
        public long tm_gmtoff;
        public String tm_zone;
    }
}
