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

package com.android.car.input;

import static com.google.common.truth.Truth.assertThat;

import static org.testng.Assert.assertThrows;

import android.annotation.NonNull;
import android.car.Car;
import android.car.input.CarInputManager;
import android.car.input.RotaryEvent;
import android.hardware.automotive.vehicle.V2_0.VehicleProperty;
import android.util.Log;
import android.util.Pair;
import android.view.KeyEvent;

import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.filters.MediumTest;

import com.android.car.CarServiceUtils;
import com.android.car.MockedCarTestBase;
import com.android.car.vehiclehal.VehiclePropValueBuilder;
import com.android.internal.annotations.GuardedBy;

import org.junit.Test;
import org.junit.runner.RunWith;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.List;
import java.util.concurrent.Semaphore;
import java.util.concurrent.TimeUnit;


@RunWith(AndroidJUnit4.class)
@MediumTest
public final class CarInputManagerTest extends MockedCarTestBase {
    private static final String TAG = CarInputManagerTest.class.getSimpleName();

    private static final int INVALID_DISPLAY_TYPE = -1;
    private static final int INVALID_INPUT_TYPE = -1;

    private CarInputManager mCarInputManager;

    private final class CaptureCallback implements CarInputManager.CarInputCaptureCallback {

        private static final long EVENT_WAIT_TIME = 500;

        private final Object mLock = new Object();

        // Stores passed events. Last one in front
        @GuardedBy("mLock")
        private final LinkedList<Pair<Integer, List<KeyEvent>>> mKeyEvents = new LinkedList<>();

        // Stores passed events. Last one in front
        @GuardedBy("mLock")
        private final LinkedList<Pair<Integer, List<RotaryEvent>>> mRotaryEvents =
                new LinkedList<>();

        // Stores passed state changes. Last one in front
        @GuardedBy("mLock")
        private final LinkedList<Pair<Integer, int[]>> mStateChanges = new LinkedList<>();

        private final Semaphore mKeyEventWait = new Semaphore(0);
        private final Semaphore mRotaryEventWait = new Semaphore(0);
        private final Semaphore mStateChangeWait = new Semaphore(0);

        @Override
        public void onKeyEvents(int targetDisplayId, List<KeyEvent> keyEvents) {
            Log.i(TAG, "onKeyEvents event:" + keyEvents.get(0) + " this:" + this);
            synchronized (mLock) {
                mKeyEvents.addFirst(new Pair<Integer, List<KeyEvent>>(targetDisplayId, keyEvents));
            }
            mKeyEventWait.release();
        }

        @Override
        public void onRotaryEvents(int targetDisplayId, List<RotaryEvent> events) {
            Log.i(TAG, "onRotaryEvents event:" + events.get(0) + " this:" + this);
            synchronized (mLock) {
                mRotaryEvents.addFirst(new Pair<Integer, List<RotaryEvent>>(targetDisplayId,
                        events));
            }
            mRotaryEventWait.release();
        }

        @Override
        public void onCaptureStateChanged(int targetDisplayId,
                @NonNull @CarInputManager.InputTypeEnum int[] activeInputTypes) {
            Log.i(TAG, "onCaptureStateChanged types:" + Arrays.toString(activeInputTypes)
                    + " this:" + this);
            synchronized (mLock) {
                mStateChanges.addFirst(new Pair<Integer, int[]>(targetDisplayId, activeInputTypes));
            }
            mStateChangeWait.release();
        }

        private void resetAllEventsWaiting() {
            mStateChangeWait.drainPermits();
            mKeyEventWait.drainPermits();
            mRotaryEventWait.drainPermits();
        }

        private void waitForStateChange() throws Exception {
            mStateChangeWait.tryAcquire(EVENT_WAIT_TIME, TimeUnit.MILLISECONDS);
        }

        private void waitForKeyEvent() throws Exception  {
            mKeyEventWait.tryAcquire(EVENT_WAIT_TIME, TimeUnit.MILLISECONDS);
        }

        private void waitForRotaryEvent() throws Exception {
            mRotaryEventWait.tryAcquire(EVENT_WAIT_TIME, TimeUnit.MILLISECONDS);
        }

        private LinkedList<Pair<Integer, List<KeyEvent>>> getkeyEvents() {
            synchronized (mLock) {
                LinkedList<Pair<Integer, List<KeyEvent>>> r =
                        new LinkedList<Pair<Integer, List<KeyEvent>>>(mKeyEvents);
                Log.i(TAG, "getKeyEvents size:" + r.size() + ",this:" + this);
                return r;
            }
        }

        private LinkedList<Pair<Integer, List<RotaryEvent>>> getRotaryEvents() {
            synchronized (mLock) {
                LinkedList<Pair<Integer, List<RotaryEvent>>> r =
                        new LinkedList<Pair<Integer, List<RotaryEvent>>>(mRotaryEvents);
                Log.i(TAG, "getRotaryEvents size:" + r.size() + ",this:" + this);
                return r;
            }
        }

        private LinkedList<Pair<Integer, int[]>> getStateChanges() {
            synchronized (mLock) {
                return new LinkedList<Pair<Integer, int[]>>(mStateChanges);
            }
        }
    }

    private final CaptureCallback mCallback0 = new CaptureCallback();
    private final CaptureCallback mCallback1 = new CaptureCallback();

    @Override
    protected synchronized void configureMockedHal() {
        addProperty(VehicleProperty.HW_KEY_INPUT,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.HW_KEY_INPUT)
                        .addIntValue(0, 0, 0)
                        .build());
        addProperty(VehicleProperty.HW_ROTARY_INPUT,
                VehiclePropValueBuilder.newBuilder(VehicleProperty.HW_ROTARY_INPUT)
                        .addIntValue(0, 1, 0)
                        .build());
    }

    @Override
    public void setUp() throws Exception {
        super.setUp();
        mCarInputManager = (CarInputManager) getCar().getCarManager(Car.CAR_INPUT_SERVICE);
        assertThat(mCarInputManager).isNotNull();
    }

    @Test
    public void testInvalidArgs() {
        // TODO(b/150818155) Need to migrate cluster code to use this to enable it.
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0,
                        CarInputManager.TARGET_DISPLAY_TYPE_CLUSTER,
                        new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0));

        // Invalid display
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0, INVALID_DISPLAY_TYPE,
                        new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0));

        // Invalid input types
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0,
                        CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                        new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, INVALID_INPUT_TYPE},
                        0));
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0,
                        CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                        new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION},
                        CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY));
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0,
                        CarInputManager.TARGET_DISPLAY_TYPE_MAIN, new int[]{INVALID_INPUT_TYPE},
                        0));
        assertThrows(IllegalArgumentException.class,
                () -> mCarInputManager.requestInputEventCapture(mCallback0,
                        CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                        new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, INVALID_INPUT_TYPE},
                        0));
    }

    private CarInputManager createAnotherCarInputManager() {
        return (CarInputManager) createNewCar().getCarManager(Car.CAR_INPUT_SERVICE);
    }

    @Test
    public void testFailWithFullCaptureHigherPriority() throws Exception {
        CarInputManager carInputManager0 = createAnotherCarInputManager();
        int r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        //TODO test event

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_FAILED);

        carInputManager0.releaseInputEventCapture(CarInputManager.TARGET_DISPLAY_TYPE_MAIN);

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        //TODO test event
    }

    @Test
    public void testDelayedGrantWithFullCapture() throws Exception {
        mCallback1.resetAllEventsWaiting();
        CarInputManager carInputManager0 = createAnotherCarInputManager();
        int r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        injectKeyEvent(true, KeyEvent.KEYCODE_NAVIGATE_NEXT);
        mCallback0.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_NAVIGATE_NEXT, mCallback0);

        injectKeyEvent(true, KeyEvent.KEYCODE_DPAD_CENTER);
        mCallback0.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_DPAD_CENTER, mCallback0);

        int numClicks = 3;
        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback0.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback0);

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION},
                CarInputManager.CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_DELAYED);

        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        waitForDispatchToMain();
        assertNumberOfOnRotaryEvents(0, mCallback1);

        carInputManager0.releaseInputEventCapture(CarInputManager.TARGET_DISPLAY_TYPE_MAIN);

        // Now capture should be granted back
        mCallback1.waitForStateChange();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION},
                mCallback1);
        assertNoStateChange(mCallback0);

        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback1.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback1);
    }

    @Test
    public void testOneClientTransitionFromFullToNonFull() throws Exception {
        CarInputManager carInputManager0 = createAnotherCarInputManager();
        CarInputManager carInputManager1 = createAnotherCarInputManager();

        int r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION,
                        CarInputManager.INPUT_TYPE_NAVIGATE_KEYS},
                CarInputManager.CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_DELAYED);

        r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION,
                        CarInputManager.INPUT_TYPE_DPAD_KEYS}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_NAVIGATE_KEYS},
                mCallback1);
        assertNoStateChange(mCallback0);
    }

    @Test
    public void testSwitchFromFullCaptureToPerTypeCapture() throws Exception {
        int r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);
    }

    @Test
    public void testIndependentTwoCaptures() throws Exception {
        int r = createAnotherCarInputManager().requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        int numClicks = 3;
        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback0.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback0);

        r = mCarInputManager.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_NAVIGATE_KEYS}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        injectKeyEvent(true, KeyEvent.KEYCODE_NAVIGATE_NEXT);
        mCallback1.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_NAVIGATE_NEXT, mCallback1);
    }

    @Test
    public void testTwoClientsOverwrap() throws Exception {
        CarInputManager carInputManager0 = createAnotherCarInputManager();
        CarInputManager carInputManager1 = createAnotherCarInputManager();

        mCallback0.resetAllEventsWaiting();
        int r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        injectKeyEvent(true, KeyEvent.KEYCODE_DPAD_CENTER);
        mCallback0.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_DPAD_CENTER, mCallback0);

        int numClicks = 3;
        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback0.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback0);

        r = carInputManager1.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION,
                        CarInputManager.INPUT_TYPE_NAVIGATE_KEYS}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        mCallback1.waitForStateChange();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_DPAD_KEYS},
                mCallback0);
        assertNoStateChange(mCallback1);

        injectKeyEvent(true, KeyEvent.KEYCODE_NAVIGATE_NEXT);
        mCallback1.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_NAVIGATE_NEXT, mCallback1);
        assertNumberOfOnKeyEvents(1, mCallback0);

        injectKeyEvent(true, KeyEvent.KEYCODE_DPAD_CENTER);
        mCallback0.waitForKeyEvent();
        assertLastKeyEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, true,
                KeyEvent.KEYCODE_DPAD_CENTER, mCallback0);
        assertNumberOfOnKeyEvents(2, mCallback0);

        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback1.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback1);
        assertNumberOfOnRotaryEvents(1, mCallback0);



        mCallback0.resetAllEventsWaiting();
        carInputManager1.releaseInputEventCapture(CarInputManager.TARGET_DISPLAY_TYPE_MAIN);

        mCallback0.waitForStateChange();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION},
                mCallback0);
        assertNoStateChange(mCallback1);

        injectRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN, numClicks);
        mCallback0.waitForRotaryEvent();
        assertLastRotaryEvent(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION, numClicks, mCallback0);
    }

    @Test
    public void testInteractionWithFullCapturer() throws Exception {
        CarInputManager carInputManager0 = createAnotherCarInputManager();
        CarInputManager carInputManager1 = createAnotherCarInputManager();

        Log.i(TAG, "requestInputEventCapture callback 0");

        int r = carInputManager0.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        Log.i(TAG, "requestInputEventCapture callback 1");
        r = carInputManager1.requestInputEventCapture(mCallback1,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[0], mCallback0);
        assertNoStateChange(mCallback1);

        Log.i(TAG, "releaseInputEventCapture callback 1");
        carInputManager1.releaseInputEventCapture(CarInputManager.TARGET_DISPLAY_TYPE_MAIN);

        waitForDispatchToMain();
        assertLastStateChange(CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION},
                mCallback0);
        assertNoStateChange(mCallback1);
    }

    @Test
    public void testSingleClientUpdates() throws Exception {
        int r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertNoStateChange(mCallback0);

        r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{
                        CarInputManager.INPUT_TYPE_DPAD_KEYS,
                        CarInputManager.INPUT_TYPE_NAVIGATE_KEYS}, 0);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertNoStateChange(mCallback0);

        r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertNoStateChange(mCallback0);

        r = mCarInputManager.requestInputEventCapture(mCallback0,
                CarInputManager.TARGET_DISPLAY_TYPE_MAIN,
                new int[]{CarInputManager.INPUT_TYPE_ALL_INPUTS},
                CarInputManager.CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY);
        assertThat(r).isEqualTo(CarInputManager.INPUT_CAPTURE_RESPONSE_SUCCEEDED);

        waitForDispatchToMain();
        assertNoStateChange(mCallback0);
    }

    /**
     * Events dispatched to main, so this should guarantee that all event dispatched are completed.
     */
    private void waitForDispatchToMain() {
        // Needs to twice as it is dispatched to main inside car service once and it is
        // dispatched to main inside CarInputManager once.
        CarServiceUtils.runOnMainSync(() -> { });
        CarServiceUtils.runOnMainSync(() -> { });
    }

    private void assertLastKeyEvent(int displayId, boolean down, int keyCode,
            CaptureCallback callback) {
        LinkedList<Pair<Integer, List<KeyEvent>>> events = callback.getkeyEvents();
        assertThat(events).isNotEmpty();
        Pair<Integer, List<KeyEvent>> lastEvent = events.getFirst();
        assertThat(lastEvent.first).isEqualTo(displayId);
        assertThat(lastEvent.second).hasSize(1);
        KeyEvent keyEvent = lastEvent.second.get(0);
        assertThat(keyEvent.getAction()).isEqualTo(
                down ? KeyEvent.ACTION_DOWN : KeyEvent.ACTION_UP);
        assertThat(keyEvent.getKeyCode()).isEqualTo(keyCode);
    }

    private void assertNumberOfOnKeyEvents(int expectedNumber, CaptureCallback callback) {
        LinkedList<Pair<Integer, List<KeyEvent>>> events = callback.getkeyEvents();
        assertThat(events).hasSize(expectedNumber);
    }

    private void assertLastRotaryEvent(int displayId, int rotaryType, int numberOfClicks,
            CaptureCallback callback) {
        LinkedList<Pair<Integer, List<RotaryEvent>>> rotaryEvents = callback.getRotaryEvents();
        assertThat(rotaryEvents).isNotEmpty();
        Pair<Integer, List<RotaryEvent>> lastEvent = rotaryEvents.getFirst();
        assertThat(lastEvent.first).isEqualTo(displayId);
        assertThat(lastEvent.second).hasSize(1);
        RotaryEvent rotaryEvent = lastEvent.second.get(0);
        assertThat(rotaryEvent.getInputType()).isEqualTo(rotaryType);
        assertThat(rotaryEvent.getNumberOfClicks()).isEqualTo(numberOfClicks);
        // TODO(b/151225008) Test timestamp
    }

    private void assertNumberOfOnRotaryEvents(int expectedNumber, CaptureCallback callback) {
        LinkedList<Pair<Integer, List<RotaryEvent>>> rotaryEvents = callback.getRotaryEvents();
        assertThat(rotaryEvents).hasSize(expectedNumber);
    }

    private void assertLastStateChange(int expectedTargetDisplayId, int[] expectedInputTypes,
            CaptureCallback callback) {
        LinkedList<Pair<Integer, int[]>> changes = callback.getStateChanges();
        assertThat(changes).isNotEmpty();
        Pair<Integer, int[]> lastChange = changes.getFirst();
        assertStateChange(expectedTargetDisplayId, expectedInputTypes, lastChange);
    }

    private void assertNoStateChange(CaptureCallback callback) {
        assertThat(callback.getStateChanges()).isEmpty();
    }

    private void assertStateChange(int expectedTargetDisplayId, int[] expectedInputTypes,
            Pair<Integer, int[]> actual) {
        Arrays.sort(expectedInputTypes);
        assertThat(actual.first).isEqualTo(expectedTargetDisplayId);
        assertThat(actual.second).isEqualTo(expectedInputTypes);
    }

    private void injectKeyEvent(boolean down, int keyCode) {
        getMockedVehicleHal().injectEvent(
                VehiclePropValueBuilder.newBuilder(VehicleProperty.HW_KEY_INPUT)
                .addIntValue(down ? 0 : 1, keyCode, 0)
                .build());
    }

    private void injectRotaryEvent(int displayId, int numClicks) {
        VehiclePropValueBuilder builder = VehiclePropValueBuilder.newBuilder(
                VehicleProperty.HW_ROTARY_INPUT)
                .addIntValue(0 /* navigation oly for now */, numClicks, displayId);
        for (int i = 0; i < numClicks - 1; i++) {
            builder.addIntValue(0);
        }
        getMockedVehicleHal().injectEvent(builder.build());
    }
}
