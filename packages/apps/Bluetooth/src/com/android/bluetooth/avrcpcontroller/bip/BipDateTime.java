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

package com.android.bluetooth.avrcpcontroller;

import java.util.Calendar;
import java.util.Date;
import java.util.Locale;
import java.util.Objects;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * An object representing a DateTime sent over the Basic Imaging Profile
 *
 * Date-time format is as follows:
 *
 *    YYYYMMDDTHHMMSSZ, where
 *      Y/M/D/H/M/S - years, months, days, hours, minutes, seconds
 *      T           - A delimiter
 *      Z           - An optional, but recommended, character indicating the time is in UTC. If UTC
 *                    is not used then we're to assume "local timezone" instead.
 *
 * Example date-time values:
 *    20000101T000000Z
 *    20000101T235959Z
 *    20000101T000000
 */
public class BipDateTime {
    private static final String TAG = "avrcpcontroller.BipDateTime";

    private Date mDate = null;
    private boolean mIsUtc = false;

    public BipDateTime(String time) {
        try {
            /*
            * Match groups for the timestamp are numbered as follows:
            *
            *     YYYY MM DD T HH MM SS Z
            *     ^^^^ ^^ ^^   ^^ ^^ ^^ ^
            *     1    2  3    4  5  6  7
            */
            Pattern p = Pattern.compile("(\\d{4})(\\d{2})(\\d{2})T(\\d{2})(\\d{2})(\\d{2})([Z])?");
            Matcher m = p.matcher(time);

            if (m.matches()) {
                /* Default to system default and assume it knows best what our local timezone is */
                Calendar.Builder builder = new Calendar.Builder();

                /* Throw exceptions when given bad values */
                builder.setLenient(false);

                /* Note that Calendar months are zero-based in Java framework */
                builder.setDate(Integer.parseInt(m.group(1)), /* year */
                        Integer.parseInt(m.group(2)) - 1,     /* month */
                        Integer.parseInt(m.group(3)));        /* day of month */

                /* Note the timestamp doesn't have milliseconds and we're explicitly setting to 0 */
                builder.setTimeOfDay(Integer.parseInt(m.group(4)), /* hours */
                        Integer.parseInt(m.group(5)),              /* minutes */
                        Integer.parseInt(m.group(6)),              /* seconds */
                        0);                                        /* milliseconds */

                /* If the 7th group is matched then we have UTC based timestamp */
                if (m.group(7) != null) {
                    TimeZone tz = TimeZone.getTimeZone("UTC");
                    tz.setRawOffset(0);
                    builder.setTimeZone(tz);
                    mIsUtc = true;
                } else {
                    mIsUtc = false;
                }

                /* Note: Java dates are UTC and any date generated will be offset by the timezone */
                mDate = builder.build().getTime();
                return;
            }
        } catch (IllegalArgumentException e) {
            // Let calendar bad values be caught and fall through
        } catch (NullPointerException e) {
            // Let null strings while matching fall through
        }

        // If we reach here then we've failed to parse the input string into a time
        throw new ParseException("Failed to parse time '" + time + "'");
    }

    public BipDateTime(Date date) {
        mDate = Objects.requireNonNull(date, "Date cannot be null");
        mIsUtc = true; // All Java Date objects store timestamps as UTC
    }

    public boolean isUtc() {
        return mIsUtc;
    }

    public Date getTime() {
        return mDate;
    }

    @Override
    public boolean equals(Object o) {
        if (o == this) return true;
        if (!(o instanceof BipDateTime)) return false;

        BipDateTime d = (BipDateTime) o;
        return d.isUtc() == isUtc() && d.getTime() == getTime();
    }

    @Override
    public String toString() {
        Date d = getTime();
        if (d == null) {
            return null;
        }

        Calendar cal = Calendar.getInstance();
        cal.setTime(d);

        /* Note that months are numbered stating from 0 */
        if (isUtc()) {
            TimeZone utc = TimeZone.getTimeZone("UTC");
            utc.setRawOffset(0);
            cal.setTimeZone(utc);
            return String.format(Locale.US, "%04d%02d%02dT%02d%02d%02dZ", cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH) + 1, cal.get(Calendar.DATE),
                    cal.get(Calendar.HOUR_OF_DAY), cal.get(Calendar.MINUTE),
                    cal.get(Calendar.SECOND));
        } else {
            cal.setTimeZone(TimeZone.getDefault());
            return String.format(Locale.US, "%04d%02d%02dT%02d%02d%02d", cal.get(Calendar.YEAR),
                    cal.get(Calendar.MONTH) + 1, cal.get(Calendar.DATE),
                    cal.get(Calendar.HOUR_OF_DAY), cal.get(Calendar.MINUTE),
                    cal.get(Calendar.SECOND));
        }
    }
}
