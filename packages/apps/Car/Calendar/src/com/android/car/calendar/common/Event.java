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

import com.android.car.calendar.common.Dialer.NumberAndAccess;

import java.time.Duration;
import java.time.Instant;

/**
 * An immutable value representing a calendar event. Should contain only details that are relevant
 * to the Car Calendar.
 */
public final class Event {

    /** The status of the current user for this event. */
    public enum Status {
        ACCEPTED,
        DECLINED,
        NONE,
    }

    /**
     * The details required for display of the calendar indicator.
     */
    public static class CalendarDetails {
        private final String mName;
        private final int mColor;

        CalendarDetails(String name, int color) {
            mName = name;
            mColor = color;
        }

        public int getColor() {
            return mColor;
        }

        public String getName() {
            return mName;
        }
    }

    private final boolean mAllDay;
    private final Instant mStartInstant;
    private final Instant mDayStartInstant;
    private final Instant mEndInstant;
    private final Instant mDayEndInstant;
    private final String mTitle;
    private final Status mStatus;
    private final String mLocation;
    private final NumberAndAccess mNumberAndAccess;
    private final CalendarDetails mCalendarDetails;

    Event(
            boolean allDay,
            Instant startInstant,
            Instant dayStartInstant,
            Instant endInstant,
            Instant dayEndInstant,
            String title,
            Status status,
            String location,
            NumberAndAccess numberAndAccess,
            CalendarDetails calendarDetails) {
        mAllDay = allDay;
        mStartInstant = startInstant;
        mDayStartInstant = dayStartInstant;
        mEndInstant = endInstant;
        mDayEndInstant = dayEndInstant;
        mTitle = title;
        mStatus = status;
        mLocation = location;
        mNumberAndAccess = numberAndAccess;
        mCalendarDetails = calendarDetails;
    }

    public Instant getStartInstant() {
        return mStartInstant;
    }

    public Instant getDayStartInstant() {
        return mDayStartInstant;
    }

    public Instant getDayEndInstant() {
        return mDayEndInstant;
    }

    public Instant getEndInstant() {
        return mEndInstant;
    }

    public String getTitle() {
        return mTitle;
    }

    public NumberAndAccess getNumberAndAccess() {
        return mNumberAndAccess;
    }

    public CalendarDetails getCalendarDetails() {
        return mCalendarDetails;
    }

    public String getLocation() {
        return mLocation;
    }

    public Status getStatus() {
        return mStatus;
    }

    public boolean isAllDay() {
        return mAllDay;
    }

    public Duration getDuration() {
        return Duration.between(getStartInstant(), getEndInstant());
    }
}
