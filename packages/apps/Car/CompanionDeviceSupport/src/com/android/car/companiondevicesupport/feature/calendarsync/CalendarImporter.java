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

package com.android.car.companiondevicesupport.feature.calendarsync;

import static com.android.car.connecteddevice.util.SafeLog.logd;
import static com.android.car.connecteddevice.util.SafeLog.loge;
import static com.android.car.connecteddevice.util.SafeLog.logw;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.annotation.NonNull;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.net.Uri;
import android.os.RemoteException;
import android.provider.CalendarContract;

import com.android.car.companiondevicesupport.feature.calendarsync.proto.Attendee;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendar;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendars;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Event;
import com.android.internal.annotations.VisibleForTesting;

import java.util.ArrayList;
import java.util.List;
import java.util.TimeZone;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * A helper class dealing with the import and deletion of calendar event data that was sent from a
 * mobile device to the head unit.
 */
class CalendarImporter {
    private static final String TAG = "CalendarImporter";

    static final int INVALID_CALENDAR_ID = -1;

    private static final Pattern CALENDAR_ID_PATTERN = Pattern.compile(".*/calendars/(\\d+)\\?.*");
    private static final int CALENDAR_ID_GROUP = 1;
    private static final Pattern EVENT_ID_PATTERN = Pattern.compile(".*/events/(\\d+)\\?.*");
    private static final int EVENT_ID_GROUP = 1;

    static final String DEFAULT_ACCOUNT_NAME = "CloudlessCalSync";

    private final ContentResolver mContentResolver;
    private final CalendarCleaner mCalendarCleaner;

    CalendarImporter(@NonNull ContentResolver contentResolver) {
        mContentResolver = contentResolver;
        mCalendarCleaner = new CalendarCleaner(mContentResolver);
    }

    /**
     * Imports the provided calendars into the calendar database.
     *
     * @param calendars The calendars to import.
     */
    void importCalendars(@NonNull Calendars calendars) {
        for (Calendar calendar : calendars.getCalendarList()) {
            logd(TAG, String.format("Import Calendar[title=%s, uuid=%s] with %d events",
                    calendar.getTitle(), calendar.getUuid(), calendar.getEventCount()));
            if (calendar.getEventCount() == 0) {
                logd(TAG, "Ignore calendar- has no events");
                continue;
            }
            int calId = findOrCreateCalendar(calendar);
            logd(TAG, "Importing into calendar with id: " + calId);
            for (Event event : calendar.getEventList()) {
                insertEvent(event, calId);
            }
        }
    }

    private int findOrCreateCalendar(@NonNull Calendar calendar) {
        int calendarId = findCalendar(calendar.getUuid());
        if (calendarId != INVALID_CALENDAR_ID) {
            return calendarId;
        }
        return createCalendar(calendar);
    }

    /**
     * Provides the calendar identifier used by the system.
     * <p>
     * This identifier is system-specific and is used to know to which calendar an events belongs.
     *
     * @param uuid The UUID of the calendar to find.
     * @return The identifier of the calendar or {@link #INVALID_CALENDAR_ID} if nothing was found.
     */
    int findCalendar(@NonNull final String uuid) {
        Cursor cursor = mContentResolver.query(
                CalendarContract.Calendars.CONTENT_URI,
                new String[]{
                        CalendarContract.Calendars._ID
                },
                CalendarContract.Calendars._SYNC_ID + " = ?",
                new String[]{
                        uuid
                },
                null);

        if (cursor.getCount() == 0) {
            return INVALID_CALENDAR_ID;
        }

        cursor.moveToFirst();
        final int calId = Integer.valueOf(cursor.getString(0));

        // Just in case there are more calendars with the same uuid, erase all of them.
        while (cursor.moveToNext()) {
            mCalendarCleaner.eraseCalendar(cursor.getString(0));
        }

        return calId;
    }

    private int createCalendar(@NonNull Calendar calendar) {
        ContentValues values = new ContentValues();
        // TODO: maybe use the name of the logged in user instead.
        values.put(CalendarContract.Calendars.ACCOUNT_NAME, DEFAULT_ACCOUNT_NAME);
        values.put(CalendarContract.Calendars.OWNER_ACCOUNT, calendar.getAccountName());
        values.put(CalendarContract.Calendars.ACCOUNT_TYPE, CalendarContract.ACCOUNT_TYPE_LOCAL);
        values.put(CalendarContract.Calendars.NAME, calendar.getTitle());
        values.put(CalendarContract.Calendars.CALENDAR_DISPLAY_NAME, calendar.getTitle());
        values.put(CalendarContract.Calendars._SYNC_ID, calendar.getUuid());
        if (calendar.getColor() != null) {
            values.put(CalendarContract.Calendars.CALENDAR_COLOR, calendar.getColor().getArgb());
        }
        values.put(CalendarContract.Calendars.VISIBLE, 1);
        values.put(CalendarContract.Calendars.SYNC_EVENTS, 1);

        Uri uri = insertContent(CalendarContract.Calendars.CONTENT_URI, values);
        Matcher matcher = CALENDAR_ID_PATTERN.matcher(uri.toString());
        return matcher.matches() ? Integer.valueOf(matcher.group(CALENDAR_ID_GROUP))
                : findCalendar(calendar.getUuid());
    }

    private void insertEvent(@NonNull Event event, int calId) {
        logd(TAG, "insert(calId=" + calId + ", event=" + event.getTitle() + ")");

        ContentValues values = new ContentValues();
        values.put(CalendarContract.Events.CALENDAR_ID, calId);
        values.put(CalendarContract.Events.TITLE, event.getTitle());
        values.put(CalendarContract.Events.DESCRIPTION, event.getDescription());
        values.put(CalendarContract.Events._SYNC_ID, event.getExternalIdentifier());
        values.put(CalendarContract.Events.DTSTART,
                SECONDS.toMillis(event.getStartDate().getSeconds()));
        values.put(CalendarContract.Events.DTEND,
                SECONDS.toMillis(event.getEndDate().getSeconds()));
        values.put(CalendarContract.Events.EVENT_LOCATION, event.getLocation());
        values.put(CalendarContract.Events.ORGANIZER, event.getOrganizer());
        values.put(CalendarContract.Events.ALL_DAY, event.getIsAllDay() ? 1 : 0);

        if (event.hasColor()) {
            values.put(CalendarContract.Events.EVENT_COLOR, event.getColor().getArgb());
        }

        TimeZone timeZone = TimeZone.getDefault();
        values.put(CalendarContract.Events.EVENT_TIMEZONE, timeZone.getID());

        // Insert event to calendar
        Uri eventRowUri = insertContent(CalendarContract.Events.CONTENT_URI, values);

        if (event.getAttendeeCount() == 0) {
            return;
        }

        if (eventRowUri == null) {
            logw(TAG, "Cannot add attendees. Missing new event row URL.");
            return;
        }

        Matcher matcher = EVENT_ID_PATTERN.matcher(eventRowUri.toString());
        if (!matcher.matches()) {
            logw(TAG,
                    "Cannot add attendees. Unable to match event id in: " + eventRowUri.toString());
            return;
        }

        String eventId = matcher.group(EVENT_ID_GROUP);
        insertAttendees(event.getAttendeeList(), eventId);
    }

    private void insertAttendees(@NonNull List<Attendee> attendees, @NonNull String eventId) {
        ArrayList<ContentProviderOperation> operations = new ArrayList<>();
        for (Attendee attendee : attendees) {
            ContentValues values = new ContentValues();
            values.put(CalendarContract.Attendees.ATTENDEE_NAME, attendee.getName());
            values.put(CalendarContract.Attendees.ATTENDEE_EMAIL, attendee.getEmail());
            values.put(CalendarContract.Attendees.ATTENDEE_TYPE,
                    convertAttendeeType(attendee.getType()));
            values.put(CalendarContract.Attendees.ATTENDEE_STATUS,
                    convertAttendeeStatus(attendee.getStatus()));
            values.put(CalendarContract.Attendees.EVENT_ID, eventId);

            operations.add(ContentProviderOperation.newInsert(
                    appendQueryParameters(CalendarContract.Attendees.CONTENT_URI))
                    .withValues(values)
                    .build());
        }

        try {
            mContentResolver.applyBatch(CalendarContract.AUTHORITY, operations);
        } catch (RemoteException | OperationApplicationException e) {
            loge(TAG, "Failed to insert attendees", e);
        }
    }

    private Uri insertContent(@NonNull Uri contentUri, @NonNull ContentValues values) {
        return mContentResolver.insert(appendQueryParameters(contentUri), values);
    }

    private Uri appendQueryParameters(@NonNull Uri contentUri) {
        Uri.Builder builder = contentUri.buildUpon();
        builder.appendQueryParameter(CalendarContract.Calendars.ACCOUNT_NAME, DEFAULT_ACCOUNT_NAME);
        builder.appendQueryParameter(CalendarContract.Calendars.ACCOUNT_TYPE,
                CalendarContract.ACCOUNT_TYPE_LOCAL);
        builder.appendQueryParameter(CalendarContract.CALLER_IS_SYNCADAPTER, "true");
        return builder.build();
    }

    @VisibleForTesting
    static int convertAttendeeStatus(Attendee.Status status) {
        switch (status) {
            case NONE_STATUS:
                return CalendarContract.Attendees.ATTENDEE_STATUS_NONE;
            case ACCEPTED:
                return CalendarContract.Attendees.ATTENDEE_STATUS_ACCEPTED;
            case DECLINED:
                return CalendarContract.Attendees.ATTENDEE_STATUS_DECLINED;
            case INVITED:
                return CalendarContract.Attendees.ATTENDEE_STATUS_INVITED;
            case TENTATIVE:
                return CalendarContract.Attendees.ATTENDEE_STATUS_TENTATIVE;
            default:
                loge(TAG, "Cannot match attendee status " + status, null);
                return CalendarContract.Attendees.ATTENDEE_STATUS_NONE;
        }
    }

    @VisibleForTesting
    static int convertAttendeeType(Attendee.Type type) {
        switch (type) {
            case NONE_TYPE:
                return CalendarContract.Attendees.TYPE_NONE;
            case OPTIONAL:
                return CalendarContract.Attendees.TYPE_OPTIONAL;
            case REQUIRED:
                return CalendarContract.Attendees.TYPE_REQUIRED;
            case RESOURCE:
                return CalendarContract.Attendees.TYPE_RESOURCE;
            default:
                loge(TAG, "Cannot match attendee type " + type, null);
                return CalendarContract.Attendees.TYPE_NONE;
        }
    }
}
