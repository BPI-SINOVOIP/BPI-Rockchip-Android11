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

import static com.google.common.base.Preconditions.checkState;

import static java.time.temporal.ChronoUnit.DAYS;
import static java.time.temporal.ChronoUnit.MINUTES;

import android.content.ContentResolver;
import android.content.ContentUris;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.provider.CalendarContract;
import android.provider.CalendarContract.Instances;
import android.util.Log;

import androidx.lifecycle.LiveData;

import com.google.common.collect.ImmutableList;
import com.google.common.collect.Iterables;

import java.time.Clock;
import java.time.Instant;
import java.time.ZoneId;
import java.time.ZonedDateTime;
import java.time.temporal.ChronoUnit;
import java.util.ArrayList;
import java.util.Comparator;
import java.util.List;

import javax.annotation.Nullable;

/**
 * An observable source of calendar events coming from the <a
 * href="https://developer.android.com/guide/topics/providers/calendar-provider">Calendar
 * Provider</a>.
 *
 * <p>While in the active state the content provider is observed for changes.
 */
public class EventsLiveData extends LiveData<ImmutableList<Event>> {

    private static final String TAG = "CarCalendarEventsLiveData";
    private static final boolean DEBUG = Log.isLoggable(TAG, Log.DEBUG);

    // Sort events by start date and title.
    private static final Comparator<Event> EVENT_COMPARATOR =
            Comparator.comparing(Event::getDayStartInstant).thenComparing(Event::getTitle);

    private final Clock mClock;
    private final Handler mBackgroundHandler;
    private final ContentResolver mContentResolver;
    private final EventDescriptions mEventDescriptions;
    private final EventLocations mLocations;

    /** The event instances cursor is a field to allow observers to be managed. */
    @Nullable private Cursor mEventsCursor;

    @Nullable private ContentObserver mEventInstancesObserver;

    public EventsLiveData(
            Clock clock,
            Handler backgroundHandler,
            ContentResolver contentResolver,
            EventDescriptions eventDescriptions,
            EventLocations locations) {
        super(ImmutableList.of());
        mClock = clock;
        mBackgroundHandler = backgroundHandler;
        mContentResolver = contentResolver;
        mEventDescriptions = eventDescriptions;
        mLocations = locations;
    }

    /** Refreshes the event instances and sets the new value which notifies observers. */
    private void update() {
        postValue(getEventsUntilTomorrow());
    }

    /** Queries the content provider for event instances. */
    @Nullable
    private ImmutableList<Event> getEventsUntilTomorrow() {
        // Check we are running on our background thread.
        checkState(mBackgroundHandler.getLooper().isCurrentThread());

        if (mEventsCursor != null) {
            tearDownCursor();
        }

        ZonedDateTime now = ZonedDateTime.now(mClock);

        // Find all events in the current day to include any all-day events.
        ZonedDateTime startDateTime = now.truncatedTo(DAYS);
        ZonedDateTime endDateTime = startDateTime.plusDays(2).truncatedTo(ChronoUnit.DAYS);

        // Always create the cursor so we can observe it for changes to events.
        mEventsCursor = createEventsCursor(startDateTime, endDateTime);

        // If there are no calendars we return null
        if (!hasCalendars()) {
            return null;
        }

        List<Event> events = new ArrayList<>();
        while (mEventsCursor.moveToNext()) {
            List<Event> eventsForRow = createEventsForRow(mEventsCursor, mEventDescriptions);
            for (Event event : eventsForRow) {
                // Filter out any events that do not overlap the time window.
                if (event.getDayEndInstant().isBefore(now.toInstant())
                        || !event.getDayStartInstant().isBefore(endDateTime.toInstant())) {
                    continue;
                }
                events.add(event);
            }
        }
        events.sort(EVENT_COMPARATOR);
        return ImmutableList.copyOf(events);
    }

    private boolean hasCalendars() {
        try (Cursor cursor =
                mContentResolver.query(CalendarContract.Calendars.CONTENT_URI, null, null, null)) {
            return cursor == null || cursor.getCount() > 0;
        }
    }

    /** Creates a new {@link Cursor} over event instances with an updated time range. */
    private Cursor createEventsCursor(ZonedDateTime startDateTime, ZonedDateTime endDateTime) {
        Uri.Builder eventInstanceUriBuilder = CalendarContract.Instances.CONTENT_URI.buildUpon();
        if (DEBUG) Log.d(TAG, "Reading from " + startDateTime + " to " + endDateTime);

        ContentUris.appendId(eventInstanceUriBuilder, startDateTime.toInstant().toEpochMilli());
        ContentUris.appendId(eventInstanceUriBuilder, endDateTime.toInstant().toEpochMilli());
        Uri eventInstanceUri = eventInstanceUriBuilder.build();
        Cursor cursor =
                mContentResolver.query(
                        eventInstanceUri,
                        /* projection= */ null,
                        /* selection= */ null,
                        /* selectionArgs= */ null,
                        Instances.BEGIN);

        // Set an observer on the Cursor, not the ContentResolver so it can be mocked for tests.
        mEventInstancesObserver =
                new ContentObserver(mBackgroundHandler) {
                    @Override
                    public boolean deliverSelfNotifications() {
                        return true;
                    }

                    @Override
                    public void onChange(boolean selfChange) {
                        if (DEBUG) Log.d(TAG, "Events changed");
                        update();
                    }
                };
        cursor.setNotificationUri(mContentResolver, eventInstanceUri);
        cursor.registerContentObserver(mEventInstancesObserver);

        return cursor;
    }

    /** Can return multiple events for a single cursor row when an event spans multiple days. */
    private List<Event> createEventsForRow(
            Cursor eventInstancesCursor, EventDescriptions eventDescriptions) {
        String titleText = text(eventInstancesCursor, Instances.TITLE);

        boolean allDay = integer(eventInstancesCursor, CalendarContract.Events.ALL_DAY) == 1;
        String descriptionText = text(eventInstancesCursor, Instances.DESCRIPTION);

        long startTimeMs = integer(eventInstancesCursor, Instances.BEGIN);
        long endTimeMs = integer(eventInstancesCursor, Instances.END);

        Instant startInstant = Instant.ofEpochMilli(startTimeMs);
        Instant endInstant = Instant.ofEpochMilli(endTimeMs);

        // If an event is all-day then the times are stored in UTC and must be adjusted.
        if (allDay) {
            startInstant = utcToDefaultTimeZone(startInstant);
            endInstant = utcToDefaultTimeZone(endInstant);
        }

        String locationText = text(eventInstancesCursor, Instances.EVENT_LOCATION);
        if (!mLocations.isValidLocation(locationText)) {
            locationText = null;
        }

        List<Dialer.NumberAndAccess> numberAndAccesses =
                eventDescriptions.extractNumberAndPins(descriptionText);
        Dialer.NumberAndAccess numberAndAccess = Iterables.getFirst(numberAndAccesses, null);
        long calendarColor = integer(eventInstancesCursor, Instances.CALENDAR_COLOR);
        String calendarName = text(eventInstancesCursor, Instances.CALENDAR_DISPLAY_NAME);
        int selfAttendeeStatus =
                (int) integer(eventInstancesCursor, Instances.SELF_ATTENDEE_STATUS);

        Event.Status status;
        switch (selfAttendeeStatus) {
            case CalendarContract.Attendees.ATTENDEE_STATUS_ACCEPTED:
                status = Event.Status.ACCEPTED;
                break;
            case CalendarContract.Attendees.ATTENDEE_STATUS_DECLINED:
                status = Event.Status.DECLINED;
                break;
            default:
                status = Event.Status.NONE;
        }

        // Add an Event for each day of events that span multiple days.
        List<Event> events = new ArrayList<>();
        Instant dayStartInstant =
                startInstant.atZone(mClock.getZone()).truncatedTo(DAYS).toInstant();
        Instant dayEndInstant;
        do {
            dayEndInstant = dayStartInstant.plus(1, DAYS);
            events.add(
                    new Event(
                            allDay,
                            startInstant,
                            dayStartInstant.isAfter(startInstant) ? dayStartInstant : startInstant,
                            endInstant,
                            dayEndInstant.isBefore(endInstant) ? dayEndInstant : endInstant,
                            titleText,
                            status,
                            locationText,
                            numberAndAccess,
                            new Event.CalendarDetails(calendarName, (int) calendarColor)));
            dayStartInstant = dayEndInstant;
        } while (dayStartInstant.isBefore(endInstant));
        return events;
    }

    private Instant utcToDefaultTimeZone(Instant instant) {
        return instant.atZone(ZoneId.of("UTC")).withZoneSameLocal(mClock.getZone()).toInstant();
    }

    @Override
    protected void onActive() {
        super.onActive();
        if (DEBUG) Log.d(TAG, "Live data active");
        mBackgroundHandler.post(this::updateAndScheduleNext);
    }

    @Override
    protected void onInactive() {
        super.onInactive();
        if (DEBUG) Log.d(TAG, "Live data inactive");
        mBackgroundHandler.post(this::cancelScheduledUpdate);
        mBackgroundHandler.post(this::tearDownCursor);
    }

    /** Calls {@link #update()} every minute to keep the displayed time range correct. */
    private void updateAndScheduleNext() {
        if (DEBUG) Log.d(TAG, "Update and schedule");
        if (hasActiveObservers()) {
            update();
            ZonedDateTime now = ZonedDateTime.now(mClock);
            ZonedDateTime truncatedNowTime = now.truncatedTo(MINUTES);
            ZonedDateTime updateTime = truncatedNowTime.plus(1, MINUTES);
            long delayMs = updateTime.toInstant().toEpochMilli() - now.toInstant().toEpochMilli();
            if (DEBUG) Log.d(TAG, "Scheduling in " + delayMs);
            mBackgroundHandler.postDelayed(this::updateAndScheduleNext, this, delayMs);
        }
    }

    private void cancelScheduledUpdate() {
        mBackgroundHandler.removeCallbacksAndMessages(this);
    }

    private void tearDownCursor() {
        if (mEventsCursor != null) {
            if (DEBUG) Log.d(TAG, "Closing cursor and unregistering observer");
            mEventsCursor.unregisterContentObserver(mEventInstancesObserver);
            mEventsCursor.close();
            mEventsCursor = null;
        } else {
            // Should not happen as the cursor should have been created first on the same handler.
            Log.w(TAG, "Expected cursor");
        }
    }

    private static String text(Cursor cursor, String columnName) {
        return cursor.getString(cursor.getColumnIndex(columnName));
    }

    /** An integer for the content provider is actually a Java long. */
    private static long integer(Cursor cursor, String columnName) {
        return cursor.getLong(cursor.getColumnIndex(columnName));
    }
}
