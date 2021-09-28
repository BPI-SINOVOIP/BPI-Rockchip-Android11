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

package com.android.car.dialer.telecom;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Notification;
import android.app.NotificationManager;
import android.content.Context;
import android.content.Intent;
import android.telecom.Call;
import android.telecom.CallAudioState;

import com.android.car.dialer.CarDialerRobolectricTestRunner;
import com.android.car.dialer.ui.activecall.InCallActivity;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.android.controller.ServiceController;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowContextWrapper;
import org.robolectric.shadows.ShadowIntent;
import org.robolectric.shadows.ShadowLooper;

/**
 * Tests for {@link InCallServiceImpl}.
 */
@RunWith(CarDialerRobolectricTestRunner.class)
public class InCallServiceImplTest {
    private static final String TELECOM_CALL_ID = "TC@1234";

    private InCallServiceImpl mInCallServiceImpl;
    private Context mContext;

    @Mock
    private Call mMockTelecomCall;
    @Mock
    private Call.Details mMockCallDetails;
    @Mock
    private CallAudioState mMockCallAudioState;
    @Mock
    private InCallServiceImpl.CallAudioStateCallback mCallAudioStateCallback;
    @Mock
    private InCallServiceImpl.ActiveCallListChangedCallback mActiveCallListChangedCallback;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mContext = RuntimeEnvironment.application;

        ServiceController<InCallServiceImpl> inCallServiceController =
                Robolectric.buildService(InCallServiceImpl.class);
        inCallServiceController.create().bind();
        mInCallServiceImpl = inCallServiceController.get();

        mInCallServiceImpl.addActiveCallListChangedCallback(mActiveCallListChangedCallback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        when(mMockTelecomCall.getDetails()).thenReturn(mMockCallDetails);
        when(mMockCallDetails.getTelecomCallId()).thenReturn(TELECOM_CALL_ID);
    }

    @Test
    public void onActiveCallAdded_startInCallActivity() {
        when(mMockTelecomCall.getState()).thenReturn(Call.STATE_ACTIVE);
        mInCallServiceImpl.onCallAdded(mMockTelecomCall);

        ArgumentCaptor<Call> callCaptor = ArgumentCaptor.forClass(Call.class);

        verify(mActiveCallListChangedCallback).onTelecomCallAdded(callCaptor.capture());
        assertThat(callCaptor.getValue()).isEqualTo(mMockTelecomCall);

        ShadowContextWrapper shadowContextWrapper = shadowOf(RuntimeEnvironment.application);
        Intent intent = shadowContextWrapper.getNextStartedActivity();
        assertThat(intent).isNotNull();
    }

    @Test
    public void onCallRemoved() {
        when(mMockTelecomCall.getState()).thenReturn(Call.STATE_ACTIVE);
        mInCallServiceImpl.onCallRemoved(mMockTelecomCall);

        ArgumentCaptor<Call> callCaptor = ArgumentCaptor.forClass(Call.class);

        verify(mActiveCallListChangedCallback).onTelecomCallRemoved(callCaptor.capture());
        assertThat(callCaptor.getValue()).isEqualTo(mMockTelecomCall);
    }

    @Test
    public void onRingingCallAdded_showNotification() {
        when(mMockTelecomCall.getState()).thenReturn(Call.STATE_RINGING);
        mInCallServiceImpl.onCallAdded(mMockTelecomCall);

        ArgumentCaptor<Call> callCaptor = ArgumentCaptor.forClass(Call.class);

        verify(mActiveCallListChangedCallback).onTelecomCallAdded(callCaptor.capture());
        assertThat(callCaptor.getValue()).isEqualTo(mMockTelecomCall);

        ArgumentCaptor<Call.Callback> callbackListCaptor = ArgumentCaptor.forClass(
                Call.Callback.class);
        verify(mMockTelecomCall).registerCallback(callbackListCaptor.capture());

        NotificationManager notificationManager = (NotificationManager) mContext.getSystemService(
                Context.NOTIFICATION_SERVICE);
        verify(notificationManager).notify(eq(TELECOM_CALL_ID), anyInt(), any(Notification.class));
    }

    @Test
    public void testRemoveActiveCallListChangedCallback() {
        mInCallServiceImpl.removeActiveCallListChangedCallback(mActiveCallListChangedCallback);

        mInCallServiceImpl.onCallAdded(mMockTelecomCall);
        verify(mActiveCallListChangedCallback, never()).onTelecomCallAdded(any());

        mInCallServiceImpl.onCallRemoved(mMockTelecomCall);
        verify(mActiveCallListChangedCallback, never()).onTelecomCallRemoved(any());
    }

    @Test
    public void testAddCallAudioStateChangedCallback() {
        mInCallServiceImpl.addCallAudioStateChangedCallback(mCallAudioStateCallback);

        mInCallServiceImpl.onCallAudioStateChanged(mMockCallAudioState);
        verify(mCallAudioStateCallback).onCallAudioStateChanged(any());
    }

    @Test
    public void testOnBringToForeground() {
        ShadowApplication shadow = shadowOf(mInCallServiceImpl.getApplication());

        mInCallServiceImpl.onCallAdded(mMockTelecomCall);
        mInCallServiceImpl.onBringToForeground(false);

        Intent intent = shadow.getNextStartedActivity();
        ShadowIntent shadowIntent = shadowOf(intent);
        assertThat(InCallActivity.class).isEqualTo(shadowIntent.getIntentClass());
    }
}
