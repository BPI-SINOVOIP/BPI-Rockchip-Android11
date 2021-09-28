/*
 * Copyright (C) 2020 The Android Open Source Project
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

import static android.provider.CalendarContract.AUTHORITY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import static java.util.concurrent.TimeUnit.SECONDS;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.database.Cursor;
import android.net.Uri;
import android.provider.CalendarContract;
import android.provider.CalendarContract.Attendees;
import android.provider.CalendarContract.Events;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.companiondevicesupport.feature.calendarsync.proto.Attendee;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendar;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendars;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Color;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Event;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.TimeZone;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Timestamp;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;

@RunWith(AndroidJUnit4.class)
public class CalendarImporterTest {

    private static final String CALENDAR_UNIQUE_ID = "test identifier";
    private static final String CALENDAR_ID = "42";

    @Mock
    private ContentProvider mContentProvider;
    @Mock
    private ContentResolver mContentResolver;
    @Mock
    private Cursor mCursor;

    private CalendarImporter mCalendarImporter;

    private Calendars mCalendarsProto;
    private Event.Builder mEventProtoBuilder1;
    private Event.Builder mEventProtoBuilder2;
    private Attendee.Builder mAttendeeProtoBuilder1;
    private Attendee.Builder mAttendeeProtoBuilder2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mCalendarImporter = new CalendarImporter(mContentResolver);

        mEventProtoBuilder1 = newEvent("UID_1", "Event A", "", "here", 1, 2, "Europe/Berlin", null);
        mEventProtoBuilder2 = newEvent("UID_2", "Event B", "there", "somwhere", 4, 6,
                "Europe/Berlin", null);
        mAttendeeProtoBuilder1 = newAttendee("James Bond", "007@gmail.com",
                Attendee.Status.ACCEPTED, Attendee.Type.REQUIRED);
        mAttendeeProtoBuilder2 = newAttendee("Q", "q@gmail.com", Attendee.Status.DECLINED,
                Attendee.Type.OPTIONAL);

        mCalendarsProto = Calendars.newBuilder()
                .addCalendar(Calendar.newBuilder()
                        .setUuid(CALENDAR_UNIQUE_ID)
                        .setTitle("calendar one")
                        .addEvent(mEventProtoBuilder1)
                        .addEvent(mEventProtoBuilder2
                                .addAttendee(mAttendeeProtoBuilder1)
                                .addAttendee(mAttendeeProtoBuilder2)))
                .build();

        when(mContentResolver.query(eq(CalendarContract.Calendars.CONTENT_URI), any(), any(), any(),
                any())).thenReturn(mCursor);

        when(mContentResolver.insert(
                argThat(startsWithUriMatcher(CalendarContract.Events.CONTENT_URI)),
                any())).thenReturn(
                CalendarContract.Events.CONTENT_URI.buildUpon().path(
                        "events/42").appendQueryParameter("a", "b").build());

        when(mContentProvider.insert(
                argThat(startsWithUriMatcher(CalendarContract.Attendees.CONTENT_URI)),
                any(),
                any()))
                .thenReturn(CalendarContract.Attendees.CONTENT_URI);

        when(mCursor.getCount()).thenReturn(1);
        when(mCursor.getString(eq(0))).thenReturn(CALENDAR_ID);
    }

    @Test
    public void findCalendar() {
        assertEquals(CALENDAR_ID,
                String.valueOf(mCalendarImporter.findCalendar(CALENDAR_UNIQUE_ID)));

        verify(mContentResolver).query(eq(CalendarContract.Calendars.CONTENT_URI),
                argThat(stringArrayMatcher(CalendarContract.Calendars._ID)),
                eq(String.format("%s = ?", CalendarContract.Calendars._SYNC_ID)),
                argThat(stringArrayMatcher(CALENDAR_UNIQUE_ID)),
                eq(null));
    }

    @Test
    public void findCalendarWithUnknownIdentifier() {
        when(mCursor.getCount()).thenReturn(0);
        assertEquals(CalendarImporter.INVALID_CALENDAR_ID,
                mCalendarImporter.findCalendar(CALENDAR_UNIQUE_ID));
    }

    @Test
    public void importCalendarsWithExistingCalendar() throws Exception {
        ArgumentCaptor<ArrayList<ContentProviderOperation>> batchOpsCaptor =
                ArgumentCaptor.forClass(ArrayList.class);

        mCalendarImporter.importCalendars(mCalendarsProto);

        verify(mContentResolver).query(eq(CalendarContract.Calendars.CONTENT_URI),
                argThat(stringArrayMatcher(CalendarContract.Calendars._ID)),
                eq(String.format("%s = ?", CalendarContract.Calendars._SYNC_ID)),
                argThat(stringArrayMatcher(CALENDAR_UNIQUE_ID)),
                eq(null));
        // Verify event insertion.
        verify(mContentResolver).insert(
                argThat(startsWithUriMatcher(CalendarContract.Events.CONTENT_URI)),
                argThat(eventMatcher(CALENDAR_ID, mEventProtoBuilder1.build())));
        verify(mContentResolver).insert(
                argThat(startsWithUriMatcher(CalendarContract.Events.CONTENT_URI)),
                argThat(eventMatcher(CALENDAR_ID, mEventProtoBuilder2.build())));

        // Verify attendee insertion.
        verify(mContentResolver).applyBatch(eq(AUTHORITY), batchOpsCaptor.capture());

        ArrayList<ContentProviderOperation> capturedOps = batchOpsCaptor.getValue();
        assertEquals(2, capturedOps.size());
        for (ContentProviderOperation operation : capturedOps) {
            assertTrue(operation.isInsert());
            operation.apply(mContentProvider, null, 0);
        }
        verifyAttendeeInsert(mAttendeeProtoBuilder1.build());
        verifyAttendeeInsert(mAttendeeProtoBuilder2.build());
    }

    @Test
    public void importCalendarAndVerifyAllDayEvent() throws Exception {
        Event.Builder allDayEventBuilder = newEvent("UID_1", "Event A", "", "here", 1, 2,
                "Europe/Berlin", null);
        allDayEventBuilder.setIsAllDay(true);

        mCalendarsProto = Calendars.newBuilder()
                .addCalendar(Calendar.newBuilder()
                        .setUuid(CALENDAR_UNIQUE_ID)
                        .setTitle("calendar one")
                        .addEvent(allDayEventBuilder))
                .build();

        mCalendarImporter.importCalendars(mCalendarsProto);

        // Verify event insertion.
        verify(mContentResolver).insert(
                argThat(startsWithUriMatcher(CalendarContract.Events.CONTENT_URI)),
                argThat(argument -> argument.getAsInteger(Events.ALL_DAY) == 1));
    }

    @Test
    public void convertAttendeeStatus() {
        assertEquals(Attendees.ATTENDEE_STATUS_NONE,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.NONE_STATUS));
        assertEquals(Attendees.ATTENDEE_STATUS_ACCEPTED,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.ACCEPTED));
        assertEquals(Attendees.ATTENDEE_STATUS_DECLINED,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.DECLINED));
        assertEquals(Attendees.ATTENDEE_STATUS_INVITED,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.INVITED));
        assertEquals(Attendees.ATTENDEE_STATUS_TENTATIVE,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.TENTATIVE));
        assertEquals(Attendees.ATTENDEE_STATUS_NONE,
                CalendarImporter.convertAttendeeStatus(Attendee.Status.UNSPECIFIED_STATUS));
    }

    @Test
    public void convertAttendeeType() {
        assertEquals(Attendees.TYPE_NONE,
                CalendarImporter.convertAttendeeType(Attendee.Type.NONE_TYPE));
        assertEquals(Attendees.TYPE_REQUIRED,
                CalendarImporter.convertAttendeeType(Attendee.Type.REQUIRED));
        assertEquals(Attendees.TYPE_OPTIONAL,
                CalendarImporter.convertAttendeeType(Attendee.Type.OPTIONAL));
        assertEquals(Attendees.TYPE_RESOURCE,
                CalendarImporter.convertAttendeeType(Attendee.Type.RESOURCE));
        assertEquals(Attendees.TYPE_NONE,
                CalendarImporter.convertAttendeeType(Attendee.Type.UNSPECIFIED_TYPE));
    }

    // --- Helpers ---

    private ArgumentMatcher<String[]> stringArrayMatcher(String expectedArg) {
        return argument -> argument.length == 1 && argument[0].equals(expectedArg);
    }

    private ArgumentMatcher<ContentValues> eventMatcher(String calendarId,
            Event event) {
        return argument -> {
            if (argument.containsKey(Events.EVENT_COLOR) &&
                    !argument.getAsInteger(Events.EVENT_COLOR).equals(
                            event.getColor().getArgb())) {
                return false;
            }

            return argument.getAsString(Events.CALENDAR_ID).equals(calendarId) &&
                    argument.getAsString(Events.TITLE).equals(event.getTitle()) &&
                    argument.getAsString(Events.DESCRIPTION).equals(event.getDescription()) &&
                    argument.getAsString(Events._SYNC_ID).equals(
                            event.getExternalIdentifier()) &&
                    argument.getAsLong(Events.DTSTART).equals(
                            SECONDS.toMillis(event.getStartDate().getSeconds())) &&
                    argument.getAsLong(Events.DTEND).equals(
                            SECONDS.toMillis(event.getEndDate().getSeconds())) &&
                    argument.getAsString(Events.EVENT_LOCATION).equals(event.getLocation()) &&
                    argument.getAsString(Events.ORGANIZER).equals(event.getOrganizer()) &&
                    argument.getAsInteger(Events.ALL_DAY) == (event.getIsAllDay() ? 1 : 0);
        };
    }

    private ArgumentMatcher<ContentValues> attendeeMatcher(Attendee attendee) {
        return argument -> argument.getAsString(Attendees.ATTENDEE_NAME).equals(
                attendee.getName()) &&
                argument.getAsString(Attendees.ATTENDEE_EMAIL).equals(attendee.getEmail()) &&
                argument.getAsInteger(Attendees.ATTENDEE_TYPE).equals(
                        CalendarImporter.convertAttendeeType(attendee.getType())) &&
                argument.getAsInteger(Attendees.ATTENDEE_STATUS).equals(
                        CalendarImporter.convertAttendeeStatus(attendee.getStatus())) &&
                argument.containsKey(Attendees.EVENT_ID);
    }

    private ArgumentMatcher<Uri> startsWithUriMatcher(Uri expectedUri) {
        return argument -> argument.toString().startsWith(expectedUri.toString());
    }

    private Event.Builder newEvent(String uid, String title, String description, String location,
            int startTimeSecs, int endTimeSecs, String timeZone, Color color) {
        Event.Builder builder = Event.newBuilder()
                .setExternalIdentifier(uid)
                .setTitle(title)
                .setDescription(description)
                .setLocation(location)
                .setStartDate(Timestamp.newBuilder().setSeconds(startTimeSecs))
                .setEndDate(Timestamp.newBuilder().setSeconds(endTimeSecs))
                .setTimeZone(TimeZone.newBuilder().setName(timeZone));
        if (color != null) {
            builder.setColor(color);
        }
        return builder;
    }

    private Attendee.Builder newAttendee(String name, String email, Attendee.Status status,
            Attendee.Type type) {
        return Attendee.newBuilder()
                .setName(name)
                .setEmail(email)
                .setStatus(status)
                .setType(type);
    }

    private void verifyAttendeeInsert(Attendee attendee) {
        verify(mContentProvider).insert(
                argThat(startsWithUriMatcher(CalendarContract.Attendees.CONTENT_URI)),
                argThat(attendeeMatcher(attendee)),
                any());
    }
}
