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

import static android.provider.CalendarContract.Calendars.ACCOUNT_NAME;
import static android.provider.CalendarContract.Calendars.ACCOUNT_TYPE;

import static com.android.car.connecteddevice.util.SafeLog.loge;

import android.content.ContentProviderOperation;
import android.content.ContentResolver;
import android.content.OperationApplicationException;
import android.database.Cursor;
import android.os.RemoteException;
import android.provider.CalendarContract;

import java.util.ArrayList;

/** A helper class that deals with cleaning up the stored calendar data. */
class CalendarCleaner {
    private static final String TAG = "CalendarCleaner";

    private static final String CALENDARS_ACCOUNT_NAME_TYPE_SELECTION =
            "((" + ACCOUNT_NAME + " = ?) AND (" + ACCOUNT_TYPE + " = ?))";
    private static final String CALENDARS_ID_SELECTION = CalendarContract.Calendars._ID + " = ?";
    private static final String EVENTS_CALENDAR_ID_SELECTION =
            CalendarContract.Events.CALENDAR_ID + " = ?";
    private static final String ATTENDEES_EVENT_ID_SELECTION =
            CalendarContract.Attendees.EVENT_ID + " = ?";

    private final ContentResolver mContentResolver;

    CalendarCleaner(ContentResolver contentResolver) {
        mContentResolver = contentResolver;
    }

    /** Erases the locally imported calendars. */
    void eraseCalendars() {
        Cursor cursor =
                mContentResolver.query(
                        CalendarContract.Calendars.CONTENT_URI,
                        new String[]{CalendarContract.Calendars._ID},
                        CALENDARS_ACCOUNT_NAME_TYPE_SELECTION,
                        new String[]{
                                CalendarImporter.DEFAULT_ACCOUNT_NAME,
                                CalendarContract.ACCOUNT_TYPE_LOCAL
                        },
                        null);
        if (cursor == null) {
            // This means the content provider crashed and the Activity Manager will kill this
            // process shortly afterwards.
            return;
        }

        ArrayList<ContentProviderOperation> deleteOps = new ArrayList<>();
        while (cursor.moveToNext()) {
            buildCalendarDeletionOps(cursor.getString(0), deleteOps);
        }
        executeBatchOperations(deleteOps);
    }

    void eraseCalendar(String calId) {
        ArrayList<ContentProviderOperation> deleteOps = new ArrayList<>();
        buildCalendarDeletionOps(calId, deleteOps);
        executeBatchOperations(deleteOps);
    }

    private void buildCalendarDeletionOps(
            String calId, ArrayList<ContentProviderOperation> operations) {
        // Order matters! To make sure that all data for a calendar is erased it is necessary to
        // 1. erase all attendees for all events,
        // 2. erase all events, and
        // 3. erase the calendar

        // Prepare attendees for deletion:
        String[] calendarIdArgs = new String[]{calId};
        Cursor cursor =
                mContentResolver.query(
                        CalendarContract.Events.CONTENT_URI,
                        new String[]{CalendarContract.Events._ID},
                        EVENTS_CALENDAR_ID_SELECTION,
                        calendarIdArgs,
                        null);
        if (cursor == null) {
            // This means the content provider crashed and the Activity Manager will kill this
            // process shortly afterwards.
            return;
        }

        while (cursor.moveToNext()) {
            operations.add(
                    ContentProviderOperation.newDelete(CalendarContract.Attendees.CONTENT_URI)
                            .withSelection(ATTENDEES_EVENT_ID_SELECTION,
                                    new String[]{cursor.getString(0)})
                            .build());
        }

        // Prepare events for deletion:
        operations.add(
                ContentProviderOperation.newDelete(CalendarContract.Events.CONTENT_URI)
                        .withSelection(EVENTS_CALENDAR_ID_SELECTION, calendarIdArgs)
                        .build());
        // Prepare calendar for deletion:
        operations.add(
                ContentProviderOperation.newDelete(CalendarContract.Calendars.CONTENT_URI)
                        .withSelection(CALENDARS_ID_SELECTION, calendarIdArgs)
                        .build());
    }

    private void executeBatchOperations(ArrayList<ContentProviderOperation> operations) {
        try {
            mContentResolver.applyBatch(CalendarContract.AUTHORITY, operations);
        } catch (RemoteException | OperationApplicationException e) {
            loge(TAG, "Batch deletion of calendar failed.", e);
        }
    }
}
