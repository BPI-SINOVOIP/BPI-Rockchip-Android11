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

package com.android.bluetooth.mapclient;

import androidx.test.runner.AndroidJUnit4;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Date;
import java.util.TimeZone;

@RunWith(AndroidJUnit4.class)
public class ObexTimeTest {
    private static final String TAG = ObexTimeTest.class.getSimpleName();

    private static final String VALID_TIME_STRING = "20190101T121314";
    private static final String VALID_TIME_STRING_WITH_OFFSET_POS = "20190101T121314+0130";
    private static final String VALID_TIME_STRING_WITH_OFFSET_NEG = "20190101T121314-0130";

    private static final String INVALID_TIME_STRING_OFFSET_EXTRA_DIGITS = "20190101T121314-99999";
    private static final String INVALID_TIME_STRING_BAD_DELIMITER = "20190101Q121314";

    // MAP message listing times, per spec, use "local time basis" if UTC offset isn't given. The
    // ObexTime class parses using the current default timezone (assumed to be the "local timezone")
    // in the case that UTC isn't provided. However, the Date class assumes UTC ALWAYS when
    // initializing off of a long value. We have to take that into account when determining our
    // expected results for time strings that don't have an offset.
    private static final long LOCAL_TIMEZONE_OFFSET = TimeZone.getDefault().getRawOffset();

    // If you are a positive offset from GMT then GMT is in the "past" and you need to subtract that
    // offset from the time. If you are negative then GMT is in the future and you need to add that
    // offset to the time.
    private static final long VALID_TS = 1546344794000L; // Jan 01, 2019 at 12:13:14 GMT
    private static final long TS_OFFSET = 5400000L; // 1 Hour, 30 minutes -> milliseconds
    private static final long VALID_TS_LOCAL_TZ = VALID_TS - LOCAL_TIMEZONE_OFFSET;
    private static final long VALID_TS_OFFSET_POS = VALID_TS - TS_OFFSET;
    private static final long VALID_TS_OFFSET_NEG = VALID_TS + TS_OFFSET;

    private static final Date VALID_DATE_LOCAL_TZ = new Date(VALID_TS_LOCAL_TZ);
    private static final Date VALID_DATE_WITH_OFFSET_POS = new Date(VALID_TS_OFFSET_POS);
    private static final Date VALID_DATE_WITH_OFFSET_NEG = new Date(VALID_TS_OFFSET_NEG);

    @Test
    public void createWithValidDateTimeString_TimestampCorrect() {
        ObexTime timestamp = new ObexTime(VALID_TIME_STRING);
        Assert.assertEquals("Parsed timestamp must match expected", VALID_DATE_LOCAL_TZ,
                timestamp.getTime());
    }

    @Test
    public void createWithValidDateTimeStringWithPosOffset_TimestampCorrect() {
        ObexTime timestamp = new ObexTime(VALID_TIME_STRING_WITH_OFFSET_POS);
        Assert.assertEquals("Parsed timestamp must match expected", VALID_DATE_WITH_OFFSET_POS,
                timestamp.getTime());
    }

    @Test
    public void createWithValidDateTimeStringWithNegOffset_TimestampCorrect() {
        ObexTime timestamp = new ObexTime(VALID_TIME_STRING_WITH_OFFSET_NEG);
        Assert.assertEquals("Parsed timestamp must match expected", VALID_DATE_WITH_OFFSET_NEG,
                timestamp.getTime());
    }

    @Test
    public void createWithValidDate_TimestampCorrect() {
        ObexTime timestamp = new ObexTime(VALID_DATE_LOCAL_TZ);
        Assert.assertEquals("ObexTime created with a date must return the same date",
                VALID_DATE_LOCAL_TZ, timestamp.getTime());
    }

    @Test
    public void printValidTime_TimestampMatchesInput() {
        ObexTime timestamp = new ObexTime(VALID_TIME_STRING);
        Assert.assertEquals("Timestamp as a string must match the input string", VALID_TIME_STRING,
                timestamp.toString());
    }

    @Test
    public void createWithInvalidDelimiterString_TimestampIsNull() {
        ObexTime timestamp = new ObexTime(INVALID_TIME_STRING_BAD_DELIMITER);
        Assert.assertEquals("Parsed timestamp was invalid and must result in a null object", null,
                timestamp.getTime());
    }

    @Test
    public void createWithInvalidOffsetString_TimestampIsNull() {
        ObexTime timestamp = new ObexTime(INVALID_TIME_STRING_OFFSET_EXTRA_DIGITS);
        Assert.assertEquals("Parsed timestamp was invalid and must result in a null object", null,
                timestamp.getTime());
    }

    @Test
    public void printInvalidTime_ReturnsNull() {
        ObexTime timestamp = new ObexTime(INVALID_TIME_STRING_BAD_DELIMITER);
        Assert.assertEquals("Invalid timestamps must return null for toString()", null,
                timestamp.toString());
    }
}
