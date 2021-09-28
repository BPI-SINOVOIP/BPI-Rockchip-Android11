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

import android.annotation.NonNull;
import android.content.Context;
import android.os.ParcelUuid;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.feature.RemoteFeature;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendar;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendars;
import com.android.car.protobuf.InvalidProtocolBufferException;

import com.android.internal.annotations.VisibleForTesting;

import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;

/**
 * An implementation of {@link RemoteFeature} that registers the CalendarSync feature and handles
 * the import and deletion of calendar event data sent from trusted mobile devices.
 *
 * <p>The calendar data the feature is handling will be cleared every time an error or disconnect
 * happens, or the feature is stopped. As an additional safety measure calendar data is furthermore
 * cleared every time the feature is started.
 */
final class CalendarSyncFeature extends RemoteFeature {
    private static final String TAG = "CalendarSyncFeature";

    private final CalendarImporter mCalendarImporter;
    private final CalendarCleaner mCalendarCleaner;

    // Holds the UUIDs for the synced calendars.
    private final Set<String> mSyncedCalendars = new HashSet<>();

    CalendarSyncFeature(@NonNull Context context) {
        this(context, new CalendarImporter(context.getContentResolver()),
                new CalendarCleaner(context.getContentResolver()));
    }

    @VisibleForTesting
    CalendarSyncFeature(@NonNull Context context, @NonNull CalendarImporter calendarImporter,
            @NonNull CalendarCleaner calendarCleaner) {
        super(context, ParcelUuid.fromString(context.getString(R.string.calendar_sync_feature_id)));
        mCalendarImporter = calendarImporter;
        mCalendarCleaner = calendarCleaner;
    }

    @Override
    public void start() {
        // For safety in case something went wrong and the feature couldn't terminate correctly we
        // clear all locally synchronized calendars on start of the feature.
        mCalendarCleaner.eraseCalendars();
        super.start();
    }

    @Override
    public void stop() {
        // Erase all the locally synchronized calendars, so that no user data stays on the device
        // after the feature is stopped.
        mCalendarCleaner.eraseCalendars();
        super.stop();
    }

    @Override
    protected void onMessageReceived(CompanionDevice device, byte[] message) {
        if (!device.hasSecureChannel()) {
            logd(TAG, device + ": skipped message from unsecure channel");
            return;
        }

        logd(TAG, device + ": received message over secure channel");
        try {
            Calendars calendars = Calendars.parseFrom(message);

            maybeEraseSynchronizedCalendars(calendars.getCalendarList());
            mCalendarImporter.importCalendars(calendars);
            storeSynchronizedCalendars(calendars.getCalendarList());
        } catch (InvalidProtocolBufferException e) {
            loge(TAG, device + ": error parsing calendar events protobuf", e);
        }
    }

    @Override
    protected void onDeviceError(CompanionDevice device, int error) {
        loge(TAG, device + ": received device error " + error, null);
        mCalendarCleaner.eraseCalendars();
    }

    @Override
    protected void onDeviceDisconnected(CompanionDevice device) {
        logw(TAG, device + ": disconnected");
        mCalendarCleaner.eraseCalendars();
    }

    private void storeSynchronizedCalendars(@NonNull List<Calendar> calendars) {
        mSyncedCalendars.addAll(
                calendars.stream().map(cal -> cal.getUuid()).collect(Collectors.toSet()));
    }

    private void maybeEraseSynchronizedCalendars(@NonNull List<Calendar> calendars) {
        if (calendars.isEmpty()) {
            mCalendarCleaner.eraseCalendars();
            mSyncedCalendars.clear();
            return;
        }
        for (Calendar calendar : calendars) {
            if (!mSyncedCalendars.contains(calendar.getUuid())) {
                continue;
            }
            logw(TAG, String.format("remove calendar: %s", calendar.getUuid()));
            int calId = mCalendarImporter.findCalendar(calendar.getUuid());
            if (calId == CalendarImporter.INVALID_CALENDAR_ID) {
                loge(TAG, "Cannot find calendar to erase: " + calendar.getUuid(), null);
                continue;
            }
            mCalendarCleaner.eraseCalendar(String.valueOf(calId));
            mSyncedCalendars.remove(calendar.getUuid());
        }
    }
}
