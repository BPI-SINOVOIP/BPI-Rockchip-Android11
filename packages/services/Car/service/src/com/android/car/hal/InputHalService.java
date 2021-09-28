/*
 * Copyright (C) 2016 The Android Open Source Project
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
package com.android.car.hal;

import static android.hardware.automotive.vehicle.V2_0.RotaryInputType.ROTARY_INPUT_TYPE_AUDIO_VOLUME;
import static android.hardware.automotive.vehicle.V2_0.RotaryInputType.ROTARY_INPUT_TYPE_SYSTEM_NAVIGATION;
import static android.hardware.automotive.vehicle.V2_0.VehicleProperty.HW_KEY_INPUT;
import static android.hardware.automotive.vehicle.V2_0.VehicleProperty.HW_ROTARY_INPUT;

import android.car.input.CarInputManager;
import android.car.input.RotaryEvent;
import android.hardware.automotive.vehicle.V2_0.VehicleDisplay;
import android.hardware.automotive.vehicle.V2_0.VehicleHwKeyInputAction;
import android.hardware.automotive.vehicle.V2_0.VehiclePropConfig;
import android.hardware.automotive.vehicle.V2_0.VehiclePropValue;
import android.os.SystemClock;
import android.util.Log;
import android.util.SparseArray;
import android.view.Display;
import android.view.InputDevice;
import android.view.KeyEvent;

import com.android.car.CarLog;
import com.android.car.CarServiceUtils;
import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.Collection;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.function.LongSupplier;

public class InputHalService extends HalServiceBase {

    public static final int DISPLAY_MAIN = VehicleDisplay.MAIN;
    public static final int DISPLAY_INSTRUMENT_CLUSTER = VehicleDisplay.INSTRUMENT_CLUSTER;

    private static final int[] SUPPORTED_PROPERTIES = new int[] {
            HW_KEY_INPUT,
            HW_ROTARY_INPUT
    };

    private final VehicleHal mHal;

    /**
     * A function to retrieve the current system uptime in milliseconds - replaceable for testing.
     */
    private final LongSupplier mUptimeSupplier;

    public interface InputListener {
        /** Called for key event */
        void onKeyEvent(KeyEvent event, int targetDisplay);
        /** Called for rotary event */
        void onRotaryEvent(RotaryEvent event, int targetDisplay);
    }

    /** The current press state of a key. */
    private static class KeyState {
        /** The timestamp (uptimeMillis) of the last ACTION_DOWN event for this key. */
        public long mLastKeyDownTimestamp = -1;
        /** The number of ACTION_DOWN events that have been sent for this keypress. */
        public int mRepeatCount = 0;
    }

    private static final boolean DBG = false;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private boolean mKeyInputSupported = false;

    @GuardedBy("mLock")
    private boolean mRotaryInputSupported = false;

    @GuardedBy("mLock")
    private InputListener mListener;

    @GuardedBy("mKeyStates")
    private final SparseArray<KeyState> mKeyStates = new SparseArray<>();

    public InputHalService(VehicleHal hal) {
        this(hal, SystemClock::uptimeMillis);
    }

    @VisibleForTesting
    InputHalService(VehicleHal hal, LongSupplier uptimeSupplier) {
        mHal = hal;
        mUptimeSupplier = uptimeSupplier;
    }

    public void setInputListener(InputListener listener) {
        boolean keyInputSupported;
        boolean rotaryInputSupported;
        synchronized (mLock) {
            if (!mKeyInputSupported && !mRotaryInputSupported) {
                Log.w(CarLog.TAG_INPUT,
                        "input listener set while rotary and key input not supported");
                return;
            }
            mListener = listener;
            keyInputSupported = mKeyInputSupported;
            rotaryInputSupported = mRotaryInputSupported;
        }
        if (keyInputSupported) {
            mHal.subscribeProperty(this, HW_KEY_INPUT);
        }
        if (rotaryInputSupported) {
            mHal.subscribeProperty(this, HW_ROTARY_INPUT);
        }
    }

    /** Returns whether {@code HW_KEY_INPUT} is supported. */
    public boolean isKeyInputSupported() {
        synchronized (mLock) {
            return mKeyInputSupported;
        }
    }

    /** Returns whether {@code HW_ROTARY_INPUT} is supported. */
    public boolean isRotaryInputSupported() {
        synchronized (mLock) {
            return mRotaryInputSupported;
        }
    }

    @Override
    public void init() {
    }

    @Override
    public void release() {
        synchronized (mLock) {
            mListener = null;
            mKeyInputSupported = false;
            mRotaryInputSupported = false;
        }
    }

    @Override
    public int[] getAllSupportedProperties() {
        return SUPPORTED_PROPERTIES;
    }

    @Override
    public void takeProperties(Collection<VehiclePropConfig> properties) {
        for (VehiclePropConfig property : properties) {
            switch (property.prop) {
                case HW_KEY_INPUT:
                    synchronized (mLock) {
                        mKeyInputSupported = true;
                    }
                    break;
                case HW_ROTARY_INPUT:
                    synchronized (mLock) {
                        mRotaryInputSupported = true;
                    }
                    break;
            }
        }
    }

    @Override
    public void onHalEvents(List<VehiclePropValue> values) {
        InputListener listener;
        synchronized (mLock) {
            listener = mListener;
        }
        if (listener == null) {
            Log.w(CarLog.TAG_INPUT, "Input event while listener is null");
            return;
        }
        for (VehiclePropValue value : values) {
            switch (value.prop) {
                case HW_KEY_INPUT:
                    dispatchKeyInput(listener, value);
                    break;
                case HW_ROTARY_INPUT:
                    dispatchRotaryInput(listener, value);
                    break;
                default:
                    Log.e(CarLog.TAG_INPUT,
                            "Wrong event dispatched, prop:0x" + Integer.toHexString(value.prop));
                    break;
            }
        }
    }

    private void dispatchKeyInput(InputListener listener, VehiclePropValue value) {
        int action = (value.value.int32Values.get(0) == VehicleHwKeyInputAction.ACTION_DOWN)
                ? KeyEvent.ACTION_DOWN
                : KeyEvent.ACTION_UP;
        int code = value.value.int32Values.get(1);
        int display = value.value.int32Values.get(2);
        int indentsCount = value.value.int32Values.size() < 4 ? 1 : value.value.int32Values.get(3);
        if (DBG) {
            Log.i(CarLog.TAG_INPUT, new StringBuilder()
                    .append("hal event code:").append(code)
                    .append(", action:").append(action)
                    .append(", display: ").append(display)
                    .append(", number of indents: ").append(indentsCount)
                    .toString());
        }
        while (indentsCount > 0) {
            indentsCount--;
            dispatchKeyEvent(listener, action, code, display);
        }
    }

    private void dispatchRotaryInput(InputListener listener, VehiclePropValue value) {
        int timeValuesIndex = 3;  // remaining values are time deltas in nanoseconds
        if (value.value.int32Values.size() < timeValuesIndex) {
            Log.e(CarLog.TAG_INPUT, "Wrong int32 array size for RotaryInput from vhal:"
                    + value.value.int32Values.size());
            return;
        }
        int rotaryInputType = value.value.int32Values.get(0);
        int detentCount = value.value.int32Values.get(1);
        int display = value.value.int32Values.get(2);
        long timestamp = value.timestamp;  // for first detent, uptime nanoseconds
        if (DBG) {
            Log.i(CarLog.TAG_INPUT, new StringBuilder()
                    .append("hal rotary input type: ").append(rotaryInputType)
                    .append(", number of detents:").append(detentCount)
                    .append(", display: ").append(display)
                    .toString());
        }
        boolean clockwise = detentCount > 0;
        detentCount = Math.abs(detentCount);
        if (detentCount == 0) { // at least there should be one event
            Log.e(CarLog.TAG_INPUT, "Zero detentCount from vhal, ignore the event");
            return;
        }
        if (display != DISPLAY_MAIN && display != DISPLAY_INSTRUMENT_CLUSTER) {
            Log.e(CarLog.TAG_INPUT, "Wrong display type for RotaryInput from vhal:"
                    + display);
            return;
        }
        if (value.value.int32Values.size() != (timeValuesIndex + detentCount - 1)) {
            Log.e(CarLog.TAG_INPUT, "Wrong int32 array size for RotaryInput from vhal:"
                    + value.value.int32Values.size());
            return;
        }
        int carInputManagerType;
        switch (rotaryInputType) {
            case ROTARY_INPUT_TYPE_SYSTEM_NAVIGATION:
                carInputManagerType = CarInputManager.INPUT_TYPE_ROTARY_NAVIGATION;
                break;
            case ROTARY_INPUT_TYPE_AUDIO_VOLUME:
                carInputManagerType = CarInputManager.INPUT_TYPE_ROTARY_VOLUME;
                break;
            default:
                Log.e(CarLog.TAG_INPUT, "Unknown rotary input type: " + rotaryInputType);
                return;
        }

        long[] timestamps = new long[detentCount];
        // vhal returns elapsed time while rotary event is using uptime to be in line with KeyEvent.
        long uptimeToElapsedTimeDelta = CarServiceUtils.getUptimeToElapsedTimeDeltaInMillis();
        long startUptime = TimeUnit.NANOSECONDS.toMillis(timestamp) - uptimeToElapsedTimeDelta;
        timestamps[0] = startUptime;
        for (int i = 0; i < timestamps.length - 1; i++) {
            timestamps[i + 1] = timestamps[i] + TimeUnit.NANOSECONDS.toMillis(
                    value.value.int32Values.get(timeValuesIndex + i));
        }
        RotaryEvent event = new RotaryEvent(carInputManagerType, clockwise, timestamps);
        listener.onRotaryEvent(event, display);
    }

    /**
     * Dispatches a KeyEvent using {@link #mUptimeSupplier} for the event time.
     *
     * @param listener listener to dispatch the event to
     * @param action action for the KeyEvent
     * @param code keycode for the KeyEvent
     * @param display target display the event is associated with
     */
    private void dispatchKeyEvent(InputListener listener, int action, int code, int display) {
        dispatchKeyEvent(listener, action, code, display, mUptimeSupplier.getAsLong());
    }

    /**
     * Dispatches a KeyEvent.
     *
     * @param listener listener to dispatch the event to
     * @param action action for the KeyEvent
     * @param code keycode for the KeyEvent
     * @param display target display the event is associated with
     * @param eventTime uptime in milliseconds when the event occurred
     */
    private void dispatchKeyEvent(InputListener listener, int action, int code, int display,
            long eventTime) {
        long downTime;
        int repeat;

        synchronized (mKeyStates) {
            KeyState state = mKeyStates.get(code);
            if (state == null) {
                state = new KeyState();
                mKeyStates.put(code, state);
            }

            if (action == KeyEvent.ACTION_DOWN) {
                downTime = eventTime;
                repeat = state.mRepeatCount++;
                state.mLastKeyDownTimestamp = eventTime;
            } else {
                // Handle key up events without any matching down event by setting the down time to
                // the event time. This shouldn't happen in practice - keys should be pressed
                // before they can be released! - but this protects us against HAL weirdness.
                downTime =
                        (state.mLastKeyDownTimestamp == -1)
                                ? eventTime
                                : state.mLastKeyDownTimestamp;
                repeat = 0;
                state.mRepeatCount = 0;
            }
        }

        KeyEvent event = new KeyEvent(
                downTime,
                eventTime,
                action,
                code,
                repeat,
                0 /* meta state */,
                0 /* deviceId */,
                0 /* scancode */,
                0 /* flags */,
                InputDevice.SOURCE_CLASS_BUTTON);

        if (display == DISPLAY_MAIN) {
            event.setDisplayId(Display.DEFAULT_DISPLAY);
        }
        listener.onKeyEvent(event, display);
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*Input HAL*");
        writer.println("mKeyInputSupported:" + mKeyInputSupported);
        writer.println("mRotaryInputSupported:" + mRotaryInputSupported);
    }
}
