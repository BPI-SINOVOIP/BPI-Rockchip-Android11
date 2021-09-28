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

import static com.android.experimentalcar.DriverDistractionExperimentalFeatureService.DEFAULT_AWARENESS_PERCENTAGE;
import static com.android.experimentalcar.DriverDistractionExperimentalFeatureService.DISPATCH_THROTTLE_MS;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.spy;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.car.Car;
import android.car.VehiclePropertyIds;
import android.car.experimental.CarDriverDistractionManager;
import android.car.experimental.DriverAwarenessEvent;
import android.car.experimental.DriverAwarenessSupplierConfig;
import android.car.experimental.DriverAwarenessSupplierService;
import android.car.experimental.DriverDistractionChangeEvent;
import android.car.experimental.IDriverAwarenessSupplier;
import android.car.experimental.IDriverAwarenessSupplierCallback;
import android.car.hardware.CarPropertyValue;
import android.content.ComponentName;
import android.content.Context;
import android.content.ContextWrapper;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.hardware.input.InputManager;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.RemoteException;
import android.os.UserHandle;
import android.util.Pair;
import android.view.InputChannel;
import android.view.InputMonitor;

import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.rule.ServiceTestRule;

import com.android.internal.annotations.GuardedBy;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnitRunner;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;
import java.util.stream.Collectors;

@RunWith(MockitoJUnitRunner.class)
public class DriverDistractionExperimentalFeatureServiceTest {

    private static final String TAG = "Car.DriverDistractionServiceTest";

    private static final String SERVICE_BIND_GAZE_SUPPLIER =
            "com.android.experimentalcar/.GazeDriverAwarenessSupplier";

    private static final long INITIAL_TIME = 1000L;
    private static final long PREFERRED_SUPPLIER_STALENESS = 10L;

    private final IDriverAwarenessSupplier mFallbackSupplier =
            new IDriverAwarenessSupplier.Stub() {
                @Override
                public void onReady() throws RemoteException {
                }

                @Override
                public void setCallback(IDriverAwarenessSupplierCallback callback)
                        throws RemoteException {
                }
            };
    private final DriverAwarenessSupplierConfig mFallbackConfig = new DriverAwarenessSupplierConfig(
            DriverAwarenessSupplierService.NO_STALENESS);

    private final IDriverAwarenessSupplier mPreferredSupplier =
            new IDriverAwarenessSupplier.Stub() {
                @Override
                public void onReady() throws RemoteException {
                }

                @Override
                public void setCallback(IDriverAwarenessSupplierCallback callback)
                        throws RemoteException {
                }
            };
    private final DriverAwarenessSupplierConfig mPreferredSupplierConfig =
            new DriverAwarenessSupplierConfig(PREFERRED_SUPPLIER_STALENESS);

    // stores the last change event from OnDriverDistractionChange call
    private DriverDistractionChangeEvent mLastDistractionEvent;
    private List<DriverDistractionChangeEvent> mDistractionEventHistory = new ArrayList<>();
    private final Semaphore mChangeEventSignal = new Semaphore(0);

    private final CarDriverDistractionManager.OnDriverDistractionChangeListener mChangeListener =
            new CarDriverDistractionManager.OnDriverDistractionChangeListener() {
                @Override
                public void onDriverDistractionChange(DriverDistractionChangeEvent event) {
                    // should be dispatched to main thread.
                    assertThat(Looper.getMainLooper()).isEqualTo(Looper.myLooper());
                    mLastDistractionEvent = event;
                    mDistractionEventHistory.add(event);
                    mChangeEventSignal.release();
                }
            };

    @Mock
    private InputManager mInputManager;

    @Mock
    private InputMonitor mInputMonitor;

    @Mock
    private IBinder mIBinder;

    @Mock
    private Handler mHandler;

    @Rule
    public final ServiceTestRule serviceRule = new ServiceTestRule();

    private final Context mSpyContext = spy(
            InstrumentationRegistry.getInstrumentation().getContext());

    private DriverDistractionExperimentalFeatureService mService;
    private CarDriverDistractionManager mManager;
    private FakeTimeSource mTimeSource;
    private FakeTimer mExpiredAwarenessTimer;
    private List<Runnable> mQueuedRunnables;

    @Before
    public void setUp() throws Exception {
        mTimeSource = new FakeTimeSource(INITIAL_TIME);
        mExpiredAwarenessTimer = new FakeTimer();
        // execute all handler posts immediately
        when(mHandler.post(any())).thenAnswer(i -> {
            ((Runnable) i.getArguments()[0]).run();
            return true;
        });
        // keep track of runnables with delayed posts
        mQueuedRunnables = new ArrayList<>();
        when(mHandler.postDelayed(any(), anyLong())).thenAnswer(i -> {
            mQueuedRunnables.add(((Runnable) i.getArguments()[0]));
            return true;
        });
        mDistractionEventHistory = new ArrayList<>();
        doReturn(PackageManager.PERMISSION_GRANTED)
                .when(mSpyContext).checkCallingOrSelfPermission(any());
        mService = new DriverDistractionExperimentalFeatureService(mSpyContext, mTimeSource,
                mExpiredAwarenessTimer, Looper.myLooper(), mHandler);
        // Car must not be created with a mock context (otherwise CarService may crash)
        mManager = new CarDriverDistractionManager(Car.createCar(mSpyContext), mService);
    }

    @After
    public void tearDown() throws Exception {
        if (mService != null) {
            mService.release();
        }
        resetChangeEventWait();
    }

    @Test
    public void testConfig_servicesCanBeBound() throws Exception {
        Context realContext = InstrumentationRegistry.getInstrumentation().getTargetContext();

        // Get the actual suppliers defined in the config
        String[] preferredDriverAwarenessSuppliers = realContext.getResources().getStringArray(
                R.array.preferredDriverAwarenessSuppliers);

        for (String supplierStringName : preferredDriverAwarenessSuppliers) {
            ComponentName supplierComponent = ComponentName.unflattenFromString(supplierStringName);
            Class<?> supplerClass = Class.forName(supplierComponent.getClassName());
            Intent serviceIntent =
                    new Intent(ApplicationProvider.getApplicationContext(), supplerClass);

            // Bind the service and grab a reference to the binder.
            IBinder binder = serviceRule.bindService(serviceIntent);

            assertThat(binder instanceof DriverAwarenessSupplierService.SupplierBinder).isTrue();
        }
    }

    @Test
    public void testInit_bindsToServicesInXmlConfig() throws Exception {
        Context spyContext = spy(InstrumentationRegistry.getInstrumentation().getTargetContext());

        // Mock the config to load a gaze supplier
        Resources spyResources = spy(spyContext.getResources());
        doReturn(spyResources).when(spyContext).getResources();
        doReturn(new String[]{SERVICE_BIND_GAZE_SUPPLIER}).when(spyResources).getStringArray(
                anyInt());

        // Mock the InputManager that will be used by TouchDriverAwarenessSupplier
        doReturn(mInputManager).when(spyContext).getSystemService(Context.INPUT_SERVICE);
        when(mInputManager.monitorGestureInput(any(), anyInt())).thenReturn(mInputMonitor);
        // InputChannel cannot be mocked because it passes to InputEventReceiver.
        final InputChannel[] inputChannels = InputChannel.openInputChannelPair(TAG);
        inputChannels[0].dispose();
        when(mInputMonitor.getInputChannel()).thenReturn(inputChannels[1]);

        // Create a special context that allows binders to succeed and keeps track of them. Doesn't
        // actually start the intents / services.
        ServiceLauncherContext serviceLauncherContext = new ServiceLauncherContext(spyContext);
        mService = new DriverDistractionExperimentalFeatureService(serviceLauncherContext,
                mTimeSource, mExpiredAwarenessTimer,
                spyContext.getMainLooper(), mHandler);
        mService.init();

        serviceLauncherContext.assertBoundService(SERVICE_BIND_GAZE_SUPPLIER);

        serviceLauncherContext.reset();
        inputChannels[0].dispose();
    }

    @Test
    public void testHandleDriverAwarenessEvent_updatesCurrentValue_withLatestEvent()
            throws Exception {
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        float firstAwarenessValue = 0.7f;
        emitDriverAwarenessEvent(mFallbackSupplier, INITIAL_TIME + 1, firstAwarenessValue);

        assertThat(getCurrentAwarenessValue()).isEqualTo(firstAwarenessValue);
    }

    @Test
    public void testHandleDriverAwarenessEvent_hasPreferredEvent_ignoresFallbackEvent()
            throws Exception {
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig),
                new Pair<>(mPreferredSupplier, mPreferredSupplierConfig)));

        // emit an event from the preferred supplier before the fallback supplier
        float preferredValue = 0.6f;
        emitDriverAwarenessEvent(mPreferredSupplier, INITIAL_TIME + 1, preferredValue);
        float fallbackValue = 0.7f;
        emitDriverAwarenessEvent(mFallbackSupplier, INITIAL_TIME + 2, fallbackValue);

        // even though the fallback supplier has a more recent timestamp, it is not the current
        // since the event from the preferred supplier is still fresh
        assertThat(getCurrentAwarenessValue()).isEqualTo(preferredValue);
    }

    @Test
    public void testHandleDriverAwarenessEvent_ignoresOldEvents() throws Exception {
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        float firstAwarenessValue = 0.7f;
        emitDriverAwarenessEvent(mFallbackSupplier, INITIAL_TIME + 1, firstAwarenessValue);
        long oldTime = INITIAL_TIME - 100;
        emitDriverAwarenessEvent(mFallbackSupplier, oldTime, 0.6f);

        // the event with the old timestamp shouldn't overwrite the value with a more recent
        // timestamp
        assertThat(getCurrentAwarenessValue()).isEqualTo(firstAwarenessValue);
    }

    @Test
    public void testPreferredAwarenessEvent_becomesStale_fallsBackToFallbackEvent()
            throws Exception {
        setVehicleMoving();
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig),
                new Pair<>(mPreferredSupplier, mPreferredSupplierConfig)));

        // emit an event from the preferred supplier before the fallback supplier
        float preferredSupplierAwarenessValue = 0.6f;
        long preferredSupplierEventTime = INITIAL_TIME + 1;
        mTimeSource.setTimeMillis(preferredSupplierEventTime);
        emitDriverAwarenessEvent(mPreferredSupplier, preferredSupplierEventTime,
                preferredSupplierAwarenessValue);
        float fallbackSuppplierAwarenessValue = 0.7f;
        long fallbackSupplierEventTime = INITIAL_TIME + 2;
        mTimeSource.setTimeMillis(fallbackSupplierEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, fallbackSupplierEventTime,
                fallbackSuppplierAwarenessValue);

        // the preferred supplier still has a fresh event
        assertThat(getCurrentAwarenessValue()).isEqualTo(preferredSupplierAwarenessValue);

        // go into the future
        mTimeSource.setTimeMillis(
                preferredSupplierEventTime + PREFERRED_SUPPLIER_STALENESS + 1);
        mExpiredAwarenessTimer.executePendingTask();

        // the preferred supplier's data has become stale
        assertThat(getCurrentAwarenessValue()).isEqualTo(fallbackSuppplierAwarenessValue);
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        // time is when the event expired
                        .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                        .setAwarenessPercentage(fallbackSuppplierAwarenessValue)
                        .build());
    }


    @Test
    public void testGetLastDistractionEvent_noEvents_returnsDefault() throws Exception {
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME)
                        .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                        .build());
    }

    @Test
    public void testGetLastDistractionEvent_afterEventEmit_returnsLastEvent() throws Exception {
        setVehicleMoving();
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        float firstAwarenessValue = 0.7f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME + 1)
                        .setAwarenessPercentage(0.7f)
                        .build());
    }

    @Test
    public void testManagerRegister_returnsInitialEvent() throws Exception {
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();
        assertThat(mLastDistractionEvent).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                        .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                        .build());
    }

    @Test
    public void testManagerRegister_distractionValueUnchanged_doesNotEmitEvent() throws Exception {
        setVehicleMoving();
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();
        assertThat(mLastDistractionEvent).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                        .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                        .build());

        float firstAwarenessValue = 1.0f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        resetChangeEventWait();
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isFalse();
    }

    @Test
    public void testManagerRegister_eventInThrottleWindow_isQueued()
            throws Exception {
        setVehicleMoving();
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Collections.singletonList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);
        // 0th event: event from registering
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 1st event: driver awareness changes, and first event is emitted
        resetChangeEventWait();
        float firstAwarenessValue = 0.9f;
        long firstEventTime = INITIAL_TIME + 1;
        mTimeSource.setTimeMillis(firstEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 2nd event: driver awareness changes within throttle window, so dispatch is scheduled
        resetChangeEventWait();
        float secondAwarenessValue = 0.8f;
        long secondEventTime = INITIAL_TIME + 2;
        mTimeSource.setTimeMillis(secondEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                secondAwarenessValue);

        // delayed runnable should be posted to handler in 1 second less than the throttle delay
        // (since there was 1 second between the two events). No event should have been emitted.
        verify(mHandler).postDelayed(any(), eq(DISPATCH_THROTTLE_MS - 1));
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isFalse();

        // event is emitted once the pending task is run
        mQueuedRunnables.get(0).run();
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();
        assertThat(mLastDistractionEvent).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(secondEventTime)
                        .setAwarenessPercentage(secondAwarenessValue)
                        .build());
    }

    @Test
    public void testManagerRegister_multipleEventsInThrottleWindow_dropsExtraEvents()
            throws Exception {
        setVehicleMoving();
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Collections.singletonList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);
        // 0th event: event from registering
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 1st event: driver awareness changes, and first event is emitted
        resetChangeEventWait();
        float firstAwarenessValue = 0.9f;
        long firstEventTime = INITIAL_TIME + 1;
        mTimeSource.setTimeMillis(firstEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 2nd event: driver awareness changes within throttle window, so dispatch is scheduled
        resetChangeEventWait();
        float secondAwarenessValue = 0.8f;
        long secondEventTime = INITIAL_TIME + 2;
        mTimeSource.setTimeMillis(secondEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                secondAwarenessValue);
        // runnable will be posted in 1 second less than the throttle delay
        verify(mHandler).postDelayed(any(), eq(DISPATCH_THROTTLE_MS - 1));

        // 3rd event: driver awareness changes within throttle window again, no new schedule
        float thirdAwarenessValue = 0.7f;
        long thirdEventTime = INITIAL_TIME + 3;
        mTimeSource.setTimeMillis(thirdEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                thirdAwarenessValue);
        // verify that this was still only called once
        verify(mHandler).postDelayed(any(), eq(DISPATCH_THROTTLE_MS - 1));

        // neither 2nd or 3rd events should trigger a callback
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isFalse();
    }

    @Test
    public void testManagerRegister_multipleEventsOutsideThrottleWindow_emitsAllEvents()
            throws Exception {
        setVehicleMoving();
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Collections.singletonList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);
        // 0th event: event from registering
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 1st event: driver awareness changes, and first event is emitted
        resetChangeEventWait();
        float firstAwarenessValue = 0.9f;
        long firstEventTime = INITIAL_TIME + DISPATCH_THROTTLE_MS;
        mTimeSource.setTimeMillis(firstEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 2nd event: outside throttle window, so dispatched
        resetChangeEventWait();
        float secondAwarenessValue = 0.8f;
        long secondEventTime = INITIAL_TIME + DISPATCH_THROTTLE_MS * 2;
        mTimeSource.setTimeMillis(secondEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                secondAwarenessValue);
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // 3rd event: outside throttle window, so dispatched
        resetChangeEventWait();
        float thirdAwarenessValue = 0.7f;
        long thirdEventTime = INITIAL_TIME + DISPATCH_THROTTLE_MS * 3;
        mTimeSource.setTimeMillis(thirdEventTime);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                thirdAwarenessValue);
        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();

        // all events should be in history
        assertThat(mDistractionEventHistory).containsExactly(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME)
                        .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                        .build(),
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(firstEventTime)
                        .setAwarenessPercentage(firstAwarenessValue)
                        .build(),
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(secondEventTime)
                        .setAwarenessPercentage(secondAwarenessValue)
                        .build(),
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(thirdEventTime)
                        .setAwarenessPercentage(thirdAwarenessValue)
                        .build());
    }

    @Test
    public void testManagerRegister_receivesChangeEvents() throws Exception {
        setVehicleMoving();
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();
        assertThat(mLastDistractionEvent).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                        .setAwarenessPercentage(DEFAULT_AWARENESS_PERCENTAGE)
                        .build());

        float firstAwarenessValue = 0.7f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        resetChangeEventWait();
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isTrue();
        assertThat(mLastDistractionEvent).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(mTimeSource.elapsedRealtime())
                        .setAwarenessPercentage(firstAwarenessValue)
                        .build());
    }

    @Test
    public void testManagerRegisterUnregister_stopsReceivingEvents() throws Exception {
        long eventWaitTimeMs = 300;

        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));
        resetChangeEventWait();
        mManager.addDriverDistractionChangeListener(mChangeListener);
        mManager.removeDriverDistractionChangeListener(mChangeListener);

        float firstAwarenessValue = 0.8f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        resetChangeEventWait();
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        assertThat(waitForCallbackEvent(eventWaitTimeMs)).isFalse();
    }

    @Test
    public void testManagerUnregister_beforeRegister_doesNothing() throws Exception {
        mManager.removeDriverDistractionChangeListener(mChangeListener);
    }

    @Test
    public void testDistractionEvent_noSpeedEventsReceived_awarenessPercentageRemainsFull()
            throws Exception {
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        float firstAwarenessValue = 0.7f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        // No new distraction event since required awareness is 0 until a speed change occurs
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME)
                        .setAwarenessPercentage(1.0f)
                        .build());
    }

    @Test
    public void testRequiredAwareness_multipleSpeedChanges_emitsSingleEvent()
            throws Exception {
        setVehicleStopped();
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        float firstAwarenessValue = 0.7f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);
        mService.handleSpeedEventLocked(
                new CarPropertyValue<>(VehiclePropertyIds.PERF_VEHICLE_SPEED, 0, 30.0f));

        // Receive the first speed change event
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME + 1)
                        .setAwarenessPercentage(0.7f)
                        .build());

        mTimeSource.setTimeMillis(INITIAL_TIME + 2);
        mService.handleSpeedEventLocked(
                new CarPropertyValue<>(VehiclePropertyIds.PERF_VEHICLE_SPEED, 0, 20.0f));
        mTimeSource.setTimeMillis(INITIAL_TIME + 3);
        mService.handleSpeedEventLocked(
                new CarPropertyValue<>(VehiclePropertyIds.PERF_VEHICLE_SPEED, 0, 40.0f));

        // Speed changes when already in a moving state don't trigger a new distraction event
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME + 1)
                        .setAwarenessPercentage(0.7f)
                        .build());
    }

    @Test
    public void testDistractionEvent_vehicleBecomesStopped_emitsFullAwareness() throws Exception {
        mService.setDriverAwarenessSuppliers(Arrays.asList(
                new Pair<>(mFallbackSupplier, mFallbackConfig)));

        setVehicleMoving();
        float firstAwarenessValue = 0.7f;
        mTimeSource.setTimeMillis(INITIAL_TIME + 1);
        emitDriverAwarenessEvent(mFallbackSupplier, mTimeSource.elapsedRealtime(),
                firstAwarenessValue);

        mTimeSource.setTimeMillis(INITIAL_TIME + 2);
        setVehicleStopped();

        // Awareness percentage is 1.0 since the vehicle is stopped, even though driver awareness
        // is 0.7
        assertThat(mService.getLastDistractionEvent()).isEqualTo(
                new DriverDistractionChangeEvent.Builder()
                        .setElapsedRealtimeTimestamp(INITIAL_TIME + 2)
                        .setAwarenessPercentage(1.0f)
                        .build());
    }

    private void setVehicleMoving() {
        mService.handleSpeedEventLocked(
                new CarPropertyValue<>(VehiclePropertyIds.PERF_VEHICLE_SPEED, 0, 30.0f));
    }

    private void setVehicleStopped() {
        mService.handleSpeedEventLocked(
                new CarPropertyValue<>(VehiclePropertyIds.PERF_VEHICLE_SPEED, 0, 0.0f));
    }

    private void resetChangeEventWait() {
        mLastDistractionEvent = null;
        mChangeEventSignal.drainPermits();
    }

    private boolean waitForCallbackEvent(long timeoutMs) {
        boolean acquired = false;
        try {
            acquired = mChangeEventSignal.tryAcquire(timeoutMs, TimeUnit.MILLISECONDS);
        } catch (Exception ignored) {

        }
        return acquired;
    }

    private float getCurrentAwarenessValue() {
        return mService.getCurrentDriverAwareness().mAwarenessEvent.getAwarenessValue();
    }

    /**
     * Handle an event as if it were emitted from the specified supplier with the specified time and
     * value.
     */
    private void emitDriverAwarenessEvent(IDriverAwarenessSupplier supplier, long time, float value)
            throws RemoteException {
        long maxStaleness;
        if (supplier == mFallbackSupplier) {
            maxStaleness = DriverAwarenessSupplierService.NO_STALENESS;
        } else {
            maxStaleness = PREFERRED_SUPPLIER_STALENESS;
        }
        mService.handleDriverAwarenessEvent(
                new DriverDistractionExperimentalFeatureService.DriverAwarenessEventWrapper(
                        new DriverAwarenessEvent(time, value),
                        supplier,
                        maxStaleness));
    }


    /** Overrides framework behavior to succeed on binding/starting processes. */
    public class ServiceLauncherContext extends ContextWrapper {
        private final Object mLock = new Object();

        @GuardedBy("mLock")
        private List<Intent> mBoundIntents = new ArrayList<>();

        ServiceLauncherContext(Context base) {
            super(base);
        }

        @Override
        public boolean bindServiceAsUser(Intent service, ServiceConnection conn, int flags,
                Handler handler, UserHandle user) {
            synchronized (mLock) {
                mBoundIntents.add(service);
            }
            conn.onServiceConnected(service.getComponent(), mIBinder);
            return true;
        }

        @Override
        public boolean bindServiceAsUser(Intent service, ServiceConnection conn,
                int flags, UserHandle user) {
            return bindServiceAsUser(service, conn, flags, null, user);
        }

        @Override
        public void unbindService(ServiceConnection conn) {
            // do nothing
        }

        void assertBoundService(String service) {
            synchronized (mLock) {
                assertThat(mBoundIntents.stream().map(Intent::getComponent).collect(
                        Collectors.toList())).contains(ComponentName.unflattenFromString(service));
            }
        }

        void reset() {
            synchronized (mLock) {
                mBoundIntents.clear();
            }
        }
    }
}
