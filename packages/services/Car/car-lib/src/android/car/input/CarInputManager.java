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

package android.car.input;

import android.annotation.IntDef;
import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.car.Car;
import android.car.CarManagerBase;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.SparseArray;
import android.view.KeyEvent;

import com.android.internal.annotations.GuardedBy;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.lang.ref.WeakReference;
import java.util.List;

/**
 * This API allows capturing selected input events.
 *
 * @hide
 */
public final class CarInputManager extends CarManagerBase {

    /**
     * Callback for capturing input events.
     */
    public interface CarInputCaptureCallback {
        /**
         * Key events were captured.
         */
        void onKeyEvents(int targetDisplayId, @NonNull List<KeyEvent> keyEvents);

        /**
         * Rotary events were captured.
         */
        void onRotaryEvents(int targetDisplayId, @NonNull List<RotaryEvent> events);

        /**
         * Capture state for the display has changed due to other client making requests or
         * releasing capture. Client should check {@code activeInputTypes} for which input types
         * are currently captured.
         */
        void onCaptureStateChanged(int targetDisplayId,
                @NonNull @InputTypeEnum int[] activeInputTypes);
    }

    /**
     * Represents main display for the system.
     */
    public static final int TARGET_DISPLAY_TYPE_MAIN = 0;

    /**
     * Represents cluster display.
     */
    public static final int TARGET_DISPLAY_TYPE_CLUSTER = 1;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "TARGET_DISPLAY_TYPE_", value = {
            TARGET_DISPLAY_TYPE_MAIN,
            TARGET_DISPLAY_TYPE_CLUSTER,
    })
    @Target({ElementType.TYPE_USE})
    public @interface TargetDisplayTypeEnum {}

    /**
     * Client will wait for grant if the request is failing due to higher priority client.
     */
    public static final int CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT = 0x1;

    /**
     * Client wants to capture the keys for the whole display. This is only allowed to system
     * process.
     *
     * @hide
     */
    public static final int CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY = 0x2;

    /** @hide */
    @IntDef(flag = true, prefix = { "CAPTURE_REQ_FLAGS_" }, value = {
            CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT,
            CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface CaptureRequestFlags {}

    /**
     * This is special type to cover all INPUT_TYPE_*. This is used for clients using
     * {@link #CAPTURE_REQ_FLAGS_TAKE_ALL_EVENTS_FOR_DISPLAY} flag.
     *
     * @hide
     */
    public static final int INPUT_TYPE_ALL_INPUTS = 1;

    /**
     * This covers rotary input device for navigation.
     */
    public static final int INPUT_TYPE_ROTARY_NAVIGATION = 10;

    /**
     * Volume knob.
     * TODO (b/151666020): This will be only allowed to system apps later.
     *
     * @hide
     */
    public static final int INPUT_TYPE_ROTARY_VOLUME = 11;

    /**
     * This is the group of keys for DPAD.
     * Included key events are: {@link KeyEvent#KEYCODE_DPAD_UP},
     * {@link KeyEvent#KEYCODE_DPAD_DOWN}, {@link KeyEvent#KEYCODE_DPAD_LEFT},
     * {@link KeyEvent#KEYCODE_DPAD_RIGHT}, {@link KeyEvent#KEYCODE_DPAD_CENTER},
     * {@link KeyEvent#KEYCODE_DPAD_DOWN_LEFT}, {@link KeyEvent#KEYCODE_DPAD_DOWN_RIGHT},
     * {@link KeyEvent#KEYCODE_DPAD_UP_LEFT}, {@link KeyEvent#KEYCODE_DPAD_UP_RIGHT}
     */
    public static final int INPUT_TYPE_DPAD_KEYS = 100;

    /**
     * This is for all KEYCODE_NAVIGATE_* keys.
     */
    public static final int INPUT_TYPE_NAVIGATE_KEYS = 101;

    /**
     * This is for all KEYCODE_SYSTEM_NAVIGATE_* keys.
     */
    public static final int INPUT_TYPE_SYSTEM_NAVIGATE_KEYS = 102;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "INPUT_TYPE_", value = {
            INPUT_TYPE_ALL_INPUTS,
            INPUT_TYPE_ROTARY_NAVIGATION,
            INPUT_TYPE_ROTARY_VOLUME,
            INPUT_TYPE_DPAD_KEYS,
            INPUT_TYPE_NAVIGATE_KEYS,
            INPUT_TYPE_SYSTEM_NAVIGATE_KEYS
    })
    @Target({ElementType.TYPE_USE})
    public @interface InputTypeEnum {}

    /**
     * The client's request has succeeded and capture will start.
     */
    public static final int INPUT_CAPTURE_RESPONSE_SUCCEEDED = 0;

    /**
     * The client's request has failed due to higher priority client already capturing. If priority
     * for the clients are the same, last client making request will be allowed to capture.
     */
    public static final int INPUT_CAPTURE_RESPONSE_FAILED = 1;

    /**
     * This is used when client has set {@link #CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT} in
     * {@code requestFlags} and capturing is blocked due to existing higher priority client.
     * When the higher priority client stops capturing, this client can capture events after
     * getting @link CarInputCaptureCallback#onCaptureStateChanged(int, int[])} call.
     */
    public static final int INPUT_CAPTURE_RESPONSE_DELAYED = 2;

    /** @hide */
    @Retention(RetentionPolicy.SOURCE)
    @IntDef(prefix = "INPUT_CAPTURE_RESPONSE_", value = {
            INPUT_CAPTURE_RESPONSE_SUCCEEDED,
            INPUT_CAPTURE_RESPONSE_FAILED,
            INPUT_CAPTURE_RESPONSE_DELAYED
    })
    @Target({ElementType.TYPE_USE})
    public @interface InputCaptureResponseEnum {}

    private final ICarInput mService;
    private final ICarInputCallback mServiceCallback = new ICarInputCallbackImpl(this);

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private final SparseArray<CarInputCaptureCallback> mCarInputCaptureCallbacks =
            new SparseArray<>(1);

    /**
     * @hide
     */
    public CarInputManager(Car car, IBinder service) {
        super(car);
        mService = ICarInput.Stub.asInterface(service);
    }

    /**
     * Requests capturing of input event for the specified display for all requested input types.
     *
     * <p>The request can fail if a high priority client is holding it. The client can set
     * {@link #CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT} in {@code requestFlags} to wait for the
     * current high priority client to release it.
     *
     * <p>If only some of the input types specified are available, the request will either:
     * <ul>
     * <li>fail, returning {@link #INPUT_CAPTURE_RESPONSE_FAILED}, or
     * <li>be deferred, returning {@link #INPUT_CAPTURE_RESPONSE_DELAYED}, if the
     * {@link #CAPTURE_REQ_FLAGS_ALLOW_DELAYED_GRANT} flag is used.
     * </ul>
     *
     * <p> After {@link #INPUT_CAPTURE_RESPONSE_DELAYED} is returned, no input types are captured
     * until the client receives a {@link CarInputCaptureCallback#onCaptureStateChanged(int, int[])}
     * call with valid input types.
     */
    @RequiresPermission(android.Manifest.permission.MONITOR_INPUT)
    @InputCaptureResponseEnum
    public int requestInputEventCapture(@NonNull CarInputCaptureCallback callback,
            @TargetDisplayTypeEnum int targetDisplayType,
            @NonNull @InputTypeEnum int[] inputTypes,
            @CaptureRequestFlags int requestFlags) {
        synchronized (mLock) {
            mCarInputCaptureCallbacks.put(targetDisplayType, callback);
        }
        try {
            return mService.requestInputEventCapture(mServiceCallback, targetDisplayType,
                    inputTypes, requestFlags);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, INPUT_CAPTURE_RESPONSE_FAILED);
        }
    }

    /**
     * Stops capturing of given display.
     */
    public void releaseInputEventCapture(@TargetDisplayTypeEnum int targetDisplayType) {
        CarInputCaptureCallback callback;
        synchronized (mLock) {
            callback = mCarInputCaptureCallbacks.removeReturnOld(targetDisplayType);
        }
        if (callback == null) {
            return;
        }
        try {
            mService.releaseInputEventCapture(mServiceCallback, targetDisplayType);
        } catch (RemoteException e) {
            // ignore
        }
    }

    @Override
    protected void onCarDisconnected() {
        synchronized (mLock) {
            mCarInputCaptureCallbacks.clear();
        }
    }

    private CarInputCaptureCallback getCallback(int targetDisplayType) {
        synchronized (mLock) {
            return mCarInputCaptureCallbacks.get(targetDisplayType);
        }
    }

    private void dispatchKeyEvents(int targetDisplayType, List<KeyEvent> keyEvents) {
        getEventHandler().post(() -> {
            CarInputCaptureCallback callback = getCallback(targetDisplayType);
            if (callback != null) {
                callback.onKeyEvents(targetDisplayType, keyEvents);
            }
        });

    }

    private void dispatchRotaryEvents(int targetDisplayType, List<RotaryEvent> events) {
        getEventHandler().post(() -> {
            CarInputCaptureCallback callback = getCallback(targetDisplayType);
            if (callback != null) {
                callback.onRotaryEvents(targetDisplayType, events);
            }
        });
    }

    private void dispatchOnCaptureStateChanged(int targetDisplayType, int[] activeInputTypes) {
        getEventHandler().post(() -> {
            CarInputCaptureCallback callback = getCallback(targetDisplayType);
            if (callback != null) {
                callback.onCaptureStateChanged(targetDisplayType, activeInputTypes);
            }
        });
    }

    private static final class ICarInputCallbackImpl extends ICarInputCallback.Stub {

        private final WeakReference<CarInputManager> mManager;

        private ICarInputCallbackImpl(CarInputManager manager) {
            mManager = new WeakReference<>(manager);
        }

        @Override
        public void onKeyEvents(int targetDisplayType, List<KeyEvent> keyEvents) {
            CarInputManager manager = mManager.get();
            if (manager == null) {
                return;
            }
            manager.dispatchKeyEvents(targetDisplayType, keyEvents);
        }

        @Override
        public void onRotaryEvents(int targetDisplayType, List<RotaryEvent> events) {
            CarInputManager manager = mManager.get();
            if (manager == null) {
                return;
            }
            manager.dispatchRotaryEvents(targetDisplayType, events);
        }

        @Override
        public void onCaptureStateChanged(int targetDisplayType, int[] activeInputTypes) {
            CarInputManager manager = mManager.get();
            if (manager == null) {
                return;
            }
            manager.dispatchOnCaptureStateChanged(targetDisplayType, activeInputTypes);
        }
    }
}
