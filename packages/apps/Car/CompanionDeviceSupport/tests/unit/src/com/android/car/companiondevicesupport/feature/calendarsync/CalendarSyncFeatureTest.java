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

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.verifyZeroInteractions;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.ServiceConnection;
import android.os.IBinder;

import androidx.test.ext.junit.runners.AndroidJUnit4;

import com.android.car.companiondevicesupport.R;
import com.android.car.companiondevicesupport.api.external.CompanionDevice;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendar;
import com.android.car.companiondevicesupport.feature.calendarsync.proto.Calendars;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import java.util.UUID;

@RunWith(AndroidJUnit4.class)
public class CalendarSyncFeatureTest {
//    @Rule
//    public GrantPermissionRule mGrantPermissionRule = GrantPermissionRule.grant(
//            Manifest.permission.INTERACT_ACROSS_USERS,
//            Manifest.permission.INTERACT_ACROSS_USERS_FULL
//    );

    @Mock
    private Context mContext;
    @Mock
    private IBinder mIBinder;
    @Mock
    private CalendarCleaner mCalendarCleaner;
    @Mock
    private CalendarImporter mCalendarImporter;
    @Mock
    private CompanionDevice mCompanionDevice;

    private CalendarSyncFeature mCalendarSyncFeature;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mContext.getString(eq(R.string.calendar_sync_feature_id))).thenReturn(
                UUID.randomUUID().toString());
        when(mContext.bindServiceAsUser(any(), any(), anyInt(), any())).thenReturn(true);

        mCalendarSyncFeature = new CalendarSyncFeature(
                mContext, mCalendarImporter,
                mCalendarCleaner);
    }

    @Test
    public void start() {
        mCalendarSyncFeature.start();

        verify(mCalendarCleaner).eraseCalendars();
        verifyNoMoreInteractions(mCalendarCleaner);
        verifyZeroInteractions(mCalendarImporter);
    }

    @Test
    public void stop() {
        mCalendarSyncFeature.start();

        ArgumentCaptor<ServiceConnection> serviceConnectionCaptor =
                ArgumentCaptor.forClass(ServiceConnection.class);
        verify(mContext).bindServiceAsUser(any(), serviceConnectionCaptor.capture(), anyInt(),
                any());
        serviceConnectionCaptor.getValue().onServiceConnected(null, mIBinder);

        reset(mCalendarCleaner);

        mCalendarSyncFeature.stop();

        verify(mCalendarCleaner).eraseCalendars();
        verifyNoMoreInteractions(mCalendarCleaner);
        verifyZeroInteractions(mCalendarImporter);
    }

    @Test
    public void onDeviceError() {
        mCalendarSyncFeature.onDeviceError(mCompanionDevice, 0);

        verify(mCalendarCleaner).eraseCalendars();
        verifyNoMoreInteractions(mCalendarCleaner);
        verifyZeroInteractions(mCalendarImporter);
    }

    @Test
    public void onDeviceDisconnected() {
        mCalendarSyncFeature.onDeviceDisconnected(mCompanionDevice);

        verify(mCalendarCleaner).eraseCalendars();
        verifyNoMoreInteractions(mCalendarCleaner);
        verifyZeroInteractions(mCalendarImporter);
    }

    @Test
    public void onMessageReceived() {
        when(mCompanionDevice.hasSecureChannel()).thenReturn(true);

        Calendars expectedCalendars = sampleCalendars();
        mCalendarSyncFeature.onMessageReceived(mCompanionDevice, expectedCalendars.toByteArray());

        verify(mCalendarImporter).importCalendars(eq(expectedCalendars));
    }

    // --- Helper ---

    private Calendars sampleCalendars() {
        return Calendars.newBuilder()
                .addCalendar(newCalendar("calId_1", "Calendar One"))
                .addCalendar(newCalendar("calId_2", "Other Calendar"))
                .build();
    }

    private Calendar.Builder newCalendar(String uid, String title) {
        return Calendar.newBuilder()
                .setUuid(uid)
                .setTitle(title);
    }
}
