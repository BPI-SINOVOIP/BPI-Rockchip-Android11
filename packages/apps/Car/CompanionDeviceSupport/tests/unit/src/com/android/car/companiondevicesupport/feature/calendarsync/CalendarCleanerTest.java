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

import static android.provider.CalendarContract.AUTHORITY;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.content.ContentProvider;
import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Bundle;
import android.provider.CalendarContract.Attendees;
import android.provider.CalendarContract.Calendars;
import android.provider.CalendarContract.Events;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatcher;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.ArrayList;
import java.util.Arrays;

@RunWith(AndroidJUnit4.class)
public class CalendarCleanerTest {

    private static final String CALENDAR_IDENTIFIER = "test identifier";
    private static final String TEST_EVENT_ID_1 = "e1";
    private static final String TEST_EVENT_ID_2 = "e2";
    private static final int NUMBER_OF_TEST_EVENTS = 2;

    @Mock
    private ContentProvider mContentProvider;
    @Mock
    private ContentResolver mContentResolver;
    @Mock
    private Cursor mEventsCursor;
    @Mock
    private Cursor mCalendarsCursor;

    private CalendarCleaner mCalendarCleaner;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mCalendarCleaner = new CalendarCleaner(mContentResolver);

        when(mContentResolver.query(eq(Events.CONTENT_URI), any(), any(), any(),
                eq(null))).thenReturn(mEventsCursor);
        when(mEventsCursor.moveToNext()).thenReturn(true, true, false);
        when(mEventsCursor.getString(0)).thenReturn(TEST_EVENT_ID_1, TEST_EVENT_ID_2);
    }

    @Test
    public void eraseCalendarDeleteOpsHaveCorrectOrder() throws Exception {
        ArgumentCaptor<ArrayList<ContentProviderOperation>> batchOperationCaptor =
                ArgumentCaptor.forClass(ArrayList.class);

        mCalendarCleaner.eraseCalendar(CALENDAR_IDENTIFIER);

        verify(mContentResolver).query(
                eq(Events.CONTENT_URI),
                argThat(createStringArrayMatcher(Calendars._ID)),
                any(),
                any(),
                any());
        verify(mContentResolver).applyBatch(eq(AUTHORITY), batchOperationCaptor.capture());

        verify(mEventsCursor, times(NUMBER_OF_TEST_EVENTS)).getString(0);
        // moveToNext() is called an additional time, as the cursor starts before the first element.
        verify(mEventsCursor, times(NUMBER_OF_TEST_EVENTS + 1)).moveToNext();

        ArrayList<ContentProviderOperation> capturedOps = batchOperationCaptor.getValue();
        assertEquals(4, capturedOps.size());

        // Order matters: deletion has to happen as follows: attendees, events, calendar.
        assertEquals(Attendees.CONTENT_URI, capturedOps.get(0).getUri());
        assertEquals(Attendees.CONTENT_URI, capturedOps.get(1).getUri());
        assertEquals(Events.CONTENT_URI, capturedOps.get(2).getUri());
        assertEquals(Calendars.CONTENT_URI, capturedOps.get(3).getUri());
    }

    @Test
    public void eraseCalendar() throws Exception {
        ArgumentCaptor<ArrayList<ContentProviderOperation>> batchOpsCaptor =
                ArgumentCaptor.forClass(ArrayList.class);

        mCalendarCleaner.eraseCalendar(CALENDAR_IDENTIFIER);

        verify(mContentResolver).applyBatch(eq(AUTHORITY), batchOpsCaptor.capture());

        for (ContentProviderOperation operation : batchOpsCaptor.getValue()) {
            // Every operation has to be a delete operation.
            assertTrue(operation.isDelete());
            operation.apply(mContentProvider, null, 0);
        }

        verifyDelete(Attendees.CONTENT_URI, Attendees.EVENT_ID, TEST_EVENT_ID_1);
        verifyDelete(Attendees.CONTENT_URI, Attendees.EVENT_ID, TEST_EVENT_ID_2);
        verifyDelete(Events.CONTENT_URI, Events.CALENDAR_ID, CALENDAR_IDENTIFIER);
        verifyDelete(Calendars.CONTENT_URI, Calendars._ID, CALENDAR_IDENTIFIER);
    }

    @Test
    public void eraseCalendars() throws Exception {
        String secondCalendarId = "other cal id";

        when(mContentResolver.query(eq(Calendars.CONTENT_URI), any(), any(), any(), any()))
                .thenReturn(mCalendarsCursor);
        when(mCalendarsCursor.moveToNext()).thenReturn(true, true, false);
        when(mCalendarsCursor.getString(0)).thenReturn(CALENDAR_IDENTIFIER, secondCalendarId);

        mCalendarCleaner.eraseCalendars();

        ArgumentCaptor<ArrayList<ContentProviderOperation>> batchOpsCaptor =
                ArgumentCaptor.forClass(ArrayList.class);
        verify(mContentResolver).applyBatch(eq(AUTHORITY), batchOpsCaptor.capture());

        verify(mCalendarsCursor, times(2)).getString(0);

        for (ContentProviderOperation operation : batchOpsCaptor.getValue()) {
            // Every operation has to be a delete operation.
            assertTrue(operation.isDelete());
            operation.apply(mContentProvider, null, 0);
        }

        verifyDelete(Attendees.CONTENT_URI, Attendees.EVENT_ID, TEST_EVENT_ID_1);
        verifyDelete(Attendees.CONTENT_URI, Attendees.EVENT_ID, TEST_EVENT_ID_2);
        verifyDelete(Events.CONTENT_URI, Events.CALENDAR_ID, CALENDAR_IDENTIFIER);
        verifyDelete(Calendars.CONTENT_URI, Calendars._ID, CALENDAR_IDENTIFIER);
        // Second calendar has no events, so no attendees entry need to be deleted.
        verifyDelete(Events.CONTENT_URI, Events.CALENDAR_ID, secondCalendarId);
        verifyDelete(Calendars.CONTENT_URI, Calendars._ID, secondCalendarId);
        verifyNoMoreInteractions(mContentProvider);
    }

    @Test
    public void eraseCalendar_failingQuery() {
        when(mContentResolver.query(eq(Events.CONTENT_URI), any(), any(), any(),
                eq(null))).thenReturn(null);
        try {
            mCalendarCleaner.eraseCalendar(CALENDAR_IDENTIFIER);
        } catch (NullPointerException e) {
            fail();
        }
    }

    @Test
    public void eraseCalendars_failingQuery() {
        when(mContentResolver.query(eq(Events.CONTENT_URI), any(), any(), any(),
                eq(null))).thenReturn(null);
        try {
            mCalendarCleaner.eraseCalendars();
        } catch (NullPointerException e) {
            fail();
        }
    }

    // --- Helpers ---

    private ArgumentMatcher<String[]> createStringArrayMatcher(String expectedArg) {
        return argument -> argument.length == 1 && argument[0].equals(expectedArg);
    }

    private ArgumentMatcher<Bundle> selectionBundleMatcher(String selection,
            String[] selectionArgs) {
        return argument -> selection.equals(argument.get(ContentResolver.QUERY_ARG_SQL_SELECTION))
                && Arrays.equals(selectionArgs,
                argument.getStringArray(ContentResolver.QUERY_ARG_SQL_SELECTION_ARGS));
    }

    private void verifyDelete(Uri uri, String selection, String selectionArg) {
        verify(mContentProvider).delete(
                eq(uri),
                argThat(selectionBundleMatcher(
                        String.format("%s = ?", selection), new String[]{selectionArg})));
    }
}
