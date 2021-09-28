/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright (c) 2003, 2004, Oracle and/or its affiliates. All rights reserved.
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 *
 * This code is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 only, as
 * published by the Free Software Foundation.  Oracle designates this
 * particular file as subject to the "Classpath" exception as provided
 * by Oracle in the LICENSE file that accompanied this code.
 *
 * This code is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * version 2 for more details (a copy is included in the LICENSE file that
 * accompanied this code).
 *
 * You should have received a copy of the GNU General Public License version
 * 2 along with this work; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * Please contact Oracle, 500 Oracle Parkway, Redwood Shores, CA 94065 USA
 * or visit www.oracle.com if you need additional information or have any
 * questions.
 */

package sun.util.calendar;

@SuppressWarnings({"unchecked", "deprecation", "all"})
public abstract class AbstractCalendar extends sun.util.calendar.CalendarSystem {

    protected AbstractCalendar() {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.Era getEra(java.lang.String eraName) {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public sun.util.calendar.Era[] getEras() {
        throw new RuntimeException("Stub!");
    }

    public void setEra(sun.util.calendar.CalendarDate date, java.lang.String eraName) {
        throw new RuntimeException("Stub!");
    }

    protected void setEras(sun.util.calendar.Era[] eras) {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.CalendarDate getCalendarDate() {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.CalendarDate getCalendarDate(long millis) {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.CalendarDate getCalendarDate(long millis, java.util.TimeZone zone) {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.CalendarDate getCalendarDate(
            long millis, sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    public long getTime(sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    protected long getTimeOfDay(sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public long getTimeOfDayValue(sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    public sun.util.calendar.CalendarDate setTimeOfDay(
            sun.util.calendar.CalendarDate cdate, int fraction) {
        throw new RuntimeException("Stub!");
    }

    public int getWeekLength() {
        throw new RuntimeException("Stub!");
    }

    protected abstract boolean isLeapYear(sun.util.calendar.CalendarDate date);

    public sun.util.calendar.CalendarDate getNthDayOfWeek(
            int nth, int dayOfWeek, sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    static long getDayOfWeekDateBefore(long fixedDate, int dayOfWeek) {
        throw new RuntimeException("Stub!");
    }

    static long getDayOfWeekDateAfter(long fixedDate, int dayOfWeek) {
        throw new RuntimeException("Stub!");
    }

    @android.compat.annotation.UnsupportedAppUsage
    public static long getDayOfWeekDateOnOrBefore(long fixedDate, int dayOfWeek) {
        throw new RuntimeException("Stub!");
    }

    protected abstract long getFixedDate(sun.util.calendar.CalendarDate date);

    protected abstract void getCalendarDateFromFixedDate(
            sun.util.calendar.CalendarDate date, long fixedDate);

    public boolean validateTime(sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    int normalizeTime(sun.util.calendar.CalendarDate date) {
        throw new RuntimeException("Stub!");
    }

    static final int DAY_IN_MILLIS = 86400000; // 0x5265c00

    static final int EPOCH_OFFSET = 719163; // 0xaf93b

    static final int HOUR_IN_MILLIS = 3600000; // 0x36ee80

    static final int MINUTE_IN_MILLIS = 60000; // 0xea60

    static final int SECOND_IN_MILLIS = 1000; // 0x3e8

    private sun.util.calendar.Era[] eras;
}
