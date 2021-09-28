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

package com.android.experimentalcar;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;

import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.IDriverAwarenessSupplierCallback;
import android.content.Context;
import android.content.res.Resources;
import android.os.Looper;

import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.concurrent.ScheduledExecutorService;

@RunWith(MockitoJUnitRunner.class)
public class TouchDriverAwarenessSupplierTest {

    private static final long INITIAL_TIME = 1000L;

    @Mock
    private ScheduledExecutorService mScheduledExecutorService;

    @Mock
    private Looper mLooper;

    @Mock
    private IDriverAwarenessSupplierCallback mSupplierCallback;

    private TouchDriverAwarenessSupplier mTouchSupplier;
    private int mMaxPermitsConfig;
    private int mPermitThrottleMs;
    private FakeTimeSource mTimeSource;
    private Context mSpyContext;

    @Before
    public void setUp() throws Exception {
        mSpyContext = spy(InstrumentationRegistry.getInstrumentation().getTargetContext());
        mMaxPermitsConfig = mSpyContext.getResources().getInteger(
                R.integer.driverAwarenessTouchModelMaxPermits);
        mPermitThrottleMs = mSpyContext.getResources().getInteger(
                R.integer.driverAwarenessTouchModelThrottleMs);
        mTimeSource = new FakeTimeSource(INITIAL_TIME);
        mTouchSupplier = spy(
                new TouchDriverAwarenessSupplier(mSpyContext, mSupplierCallback,
                        mScheduledExecutorService, mLooper,
                        mTimeSource));
        doNothing().when(mTouchSupplier).startTouchMonitoring();
    }

    @Test
    public void testReady_initializesPermitsToMaxFromConfig() throws Exception {
        mTouchSupplier.onReady();

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME,
                        TouchDriverAwarenessSupplier.INITIAL_DRIVER_AWARENESS_VALUE));
    }

    @Test
    public void testconsumePermitLocked_decrementsPermitCount() throws Exception {
        mTouchSupplier.onReady();

        mTouchSupplier.consumePermitLocked(INITIAL_TIME);

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME,
                        awarenessValueForPermitCount(mMaxPermitsConfig)));
    }

    @Test
    public void testconsumePermitLocked_decrementIgnoredInThrottleWindow() throws Exception {
        mTouchSupplier.onReady();

        mTouchSupplier.consumePermitLocked(INITIAL_TIME);
        mTouchSupplier.consumePermitLocked(INITIAL_TIME + 1);

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME,
                        awarenessValueForPermitCount(mMaxPermitsConfig)));
    }

    @Test
    public void testconsumePermitLocked_decrementsToMinOf0() throws Exception {
        int lowMaxPermits = 1;

        Resources spyResources = spy(mSpyContext.getResources());
        doReturn(spyResources).when(mSpyContext).getResources();
        doReturn(lowMaxPermits).when(spyResources).getInteger(
                eq(R.integer.driverAwarenessTouchModelMaxPermits));
        mTouchSupplier.onReady();

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME, 1f));
        long firstEventTimestamp = INITIAL_TIME + 1 + mPermitThrottleMs;
        mTouchSupplier.consumePermitLocked(firstEventTimestamp);
        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(firstEventTimestamp, 0f));
        long secondEventTimestamp = INITIAL_TIME + 1 + mPermitThrottleMs;
        mTouchSupplier.consumePermitLocked(secondEventTimestamp);

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(secondEventTimestamp, 0f));
    }

    @Test
    public void testhandlePermitRefreshLocked_incrementsPermitCount() throws Exception {
        mTouchSupplier.onReady();

        mTouchSupplier.consumePermitLocked(INITIAL_TIME);
        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME,
                        awarenessValueForPermitCount(mMaxPermitsConfig - 1)));

        long refreshTimestamp = INITIAL_TIME + 1;
        mTouchSupplier.handlePermitRefreshLocked(refreshTimestamp);
        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(refreshTimestamp,
                        awarenessValueForPermitCount(mMaxPermitsConfig)));
    }

    @Test
    public void testhandlePermitRefreshLocked_incrementsToMaxOfConfig() throws Exception {
        mTouchSupplier.onReady();

        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(INITIAL_TIME,
                        TouchDriverAwarenessSupplier.INITIAL_DRIVER_AWARENESS_VALUE));

        long refreshTimestamp = INITIAL_TIME + 1;
        mTouchSupplier.handlePermitRefreshLocked(refreshTimestamp);
        verify(mSupplierCallback).onDriverAwarenessUpdated(
                new DriverAwarenessEvent(refreshTimestamp,
                        awarenessValueForPermitCount(mMaxPermitsConfig)));
    }

    private float awarenessValueForPermitCount(int count) {
        return count / (float) mMaxPermitsConfig;
    }
}
