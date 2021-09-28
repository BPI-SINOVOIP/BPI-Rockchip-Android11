/*
 * Copyright (C) 2017 The Android Open Source Project
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

package android.car.diagnostic;

import android.annotation.IntDef;
import android.annotation.Nullable;
import android.annotation.SystemApi;
import android.car.Car;
import android.car.CarLibLog;
import android.car.CarManagerBase;
import android.car.diagnostic.ICarDiagnosticEventListener.Stub;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;
import android.util.SparseArray;

import com.android.car.internal.CarPermission;
import com.android.car.internal.CarRatedListeners;
import com.android.car.internal.SingleMessageHandler;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;

/**
 * API for monitoring car diagnostic data.
 *
 * @hide
 */
@SystemApi
public final class CarDiagnosticManager extends CarManagerBase {
    public static final int FRAME_TYPE_LIVE = 0;
    public static final int FRAME_TYPE_FREEZE = 1;

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({FRAME_TYPE_LIVE, FRAME_TYPE_FREEZE})
    public @interface FrameType {}

    /** @hide */
    public static final @FrameType int[] FRAME_TYPES = {
        FRAME_TYPE_LIVE,
        FRAME_TYPE_FREEZE
    };

    private static final int MSG_DIAGNOSTIC_EVENTS = 0;

    private final ICarDiagnostic mService;
    private final SparseArray<CarDiagnosticListeners> mActiveListeners = new SparseArray<>();

    /** Handles call back into clients. */
    private final SingleMessageHandler<CarDiagnosticEvent> mHandlerCallback;

    private final CarDiagnosticEventListenerToService mListenerToService;

    private final CarPermission mVendorExtensionPermission;

    /** @hide */
    public CarDiagnosticManager(Car car, IBinder service) {
        super(car);
        mService = ICarDiagnostic.Stub.asInterface(service);
        mHandlerCallback = new SingleMessageHandler<CarDiagnosticEvent>(
                getEventHandler().getLooper(), MSG_DIAGNOSTIC_EVENTS) {
            @Override
            protected void handleEvent(CarDiagnosticEvent event) {
                CarDiagnosticListeners listeners;
                synchronized (mActiveListeners) {
                    listeners = mActiveListeners.get(event.frameType);
                }
                if (listeners != null) {
                    listeners.onDiagnosticEvent(event);
                }
            }
        };
        mVendorExtensionPermission = new CarPermission(getContext(),
                Car.PERMISSION_VENDOR_EXTENSION);
        mListenerToService = new CarDiagnosticEventListenerToService(this);
    }

    @Override
    public void onCarDisconnected() {
        synchronized (mActiveListeners) {
            mActiveListeners.clear();
        }
    }

    /** Listener for diagnostic events. Callbacks are called in the Looper context. */
    public interface OnDiagnosticEventListener {
        /**
         * Called when there is a diagnostic event from the car.
         *
         * @param carDiagnosticEvent
         */
        void onDiagnosticEvent(CarDiagnosticEvent carDiagnosticEvent);
    }

    // OnDiagnosticEventListener registration

    private void assertFrameType(@FrameType int frameType) {
        switch(frameType) {
            case FRAME_TYPE_FREEZE:
            case FRAME_TYPE_LIVE:
                return;
            default:
                throw new IllegalArgumentException(String.format(
                            "%d is not a valid diagnostic frame type", frameType));
        }
    }

    /**
     * Register a new listener for events of a given frame type and rate.
     * @param listener
     * @param frameType
     * @param rate
     * @return true if the registration was successful; false otherwise
     * @throws IllegalArgumentException
     */
    public boolean registerListener(
            OnDiagnosticEventListener listener, @FrameType int frameType, int rate) {
        assertFrameType(frameType);
        synchronized (mActiveListeners) {
            boolean needsServerUpdate = false;
            CarDiagnosticListeners listeners = mActiveListeners.get(frameType);
            if (listeners == null) {
                listeners = new CarDiagnosticListeners(rate);
                mActiveListeners.put(frameType, listeners);
                needsServerUpdate = true;
            }
            if (listeners.addAndUpdateRate(listener, rate)) {
                needsServerUpdate = true;
            }
            if (needsServerUpdate) {
                if (!registerOrUpdateDiagnosticListener(frameType, rate)) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Unregister a listener, causing it to stop receiving all diagnostic events.
     * @param listener
     */
    public void unregisterListener(OnDiagnosticEventListener listener) {
        synchronized (mActiveListeners) {
            for (@FrameType int frameType : FRAME_TYPES) {
                doUnregisterListenerLocked(listener, frameType);
            }
        }
    }

    private void doUnregisterListenerLocked(OnDiagnosticEventListener listener,
            @FrameType int frameType) {
        CarDiagnosticListeners listeners = mActiveListeners.get(frameType);
        if (listeners != null) {
            boolean needsServerUpdate = false;
            if (listeners.contains(listener)) {
                needsServerUpdate = listeners.remove(listener);
            }
            if (listeners.isEmpty()) {
                try {
                    mService.unregisterDiagnosticListener(frameType,
                            mListenerToService);
                } catch (RemoteException e) {
                    handleRemoteExceptionFromCarService(e);
                    // continue for local clean-up
                }
                mActiveListeners.remove(frameType);
            } else if (needsServerUpdate) {
                registerOrUpdateDiagnosticListener(frameType, listeners.getRate());
            }
        }
    }

    private boolean registerOrUpdateDiagnosticListener(@FrameType int frameType, int rate) {
        try {
            return mService.registerOrUpdateDiagnosticListener(frameType, rate, mListenerToService);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    // ICarDiagnostic forwards

    /**
     * Retrieve the most-recently acquired live frame data from the car.
     * @return A CarDiagnostic event for the most recently known live frame if one is present.
     *         null if no live frame has been recorded by the vehicle.
     */
    public @Nullable CarDiagnosticEvent getLatestLiveFrame() {
        try {
            return mService.getLatestLiveFrame();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Return the list of the timestamps for which a freeze frame is currently stored.
     * @return An array containing timestamps at which, at the current time, the vehicle has
     *         a freeze frame stored. If no freeze frames are currently stored, an empty
     *         array will be returned.
     * Because vehicles might have a limited amount of storage for frames, clients cannot
     * assume that a timestamp obtained via this call will be indefinitely valid for retrieval
     * of the actual diagnostic data, and must be prepared to handle a missing frame.
     */
    public long[] getFreezeFrameTimestamps() {
        try {
            return mService.getFreezeFrameTimestamps();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, new long[0]);
        }
    }

    /**
     * Retrieve the freeze frame event data for a given timestamp, if available.
     * @param timestamp
     * @return A CarDiagnostic event for the frame at the given timestamp, if one is
     *         available. null is returned otherwise.
     * Storage constraints might cause frames to be deleted from vehicle memory.
     * For this reason it cannot be assumed that a timestamp will yield a valid frame,
     * even if it was initially obtained via a call to getFreezeFrameTimestamps().
     */
    public @Nullable CarDiagnosticEvent getFreezeFrame(long timestamp) {
        try {
            return mService.getFreezeFrame(timestamp);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Clear the freeze frame information from vehicle memory at the given timestamps.
     * @param timestamps A list of timestamps to delete freeze frames at, or an empty array
     *                   to delete all freeze frames from vehicle memory.
     * @return true if all the required frames were deleted (including if no timestamps are
     *         provided and all frames were cleared); false otherwise.
     * Due to storage constraints, timestamps cannot be assumed to be indefinitely valid, and
     * a false return from this method should be used by the client as cause for invalidating
     * its local knowledge of the vehicle diagnostic state.
     */
    public boolean clearFreezeFrames(long... timestamps) {
        try {
            return mService.clearFreezeFrames(timestamps);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns true if this vehicle supports sending live frame information.
     * @return
     */
    public boolean isLiveFrameSupported() {
        try {
            return mService.isLiveFrameSupported();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns true if this vehicle supports supports sending notifications to
     * registered listeners when new freeze frames happen.
     */
    public boolean isFreezeFrameNotificationSupported() {
        try {
            return mService.isFreezeFrameNotificationSupported();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns whether the underlying HAL supports retrieving freeze frames
     * stored in vehicle memory using timestamp.
     */
    public boolean isGetFreezeFrameSupported() {
        try {
            return mService.isGetFreezeFrameSupported();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns true if this vehicle supports clearing all freeze frames.
     * This is only meaningful if freeze frame data is also supported.
     *
     * A return value of true for this method indicates that it is supported to call
     * carDiagnosticManager.clearFreezeFrames()
     * to delete all freeze frames stored in vehicle memory.
     *
     * @return
     */
    public boolean isClearFreezeFramesSupported() {
        try {
            return mService.isClearFreezeFramesSupported();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Returns true if this vehicle supports clearing specific freeze frames by timestamp.
     * This is only meaningful if freeze frame data is also supported.
     *
     * A return value of true for this method indicates that it is supported to call
     * carDiagnosticManager.clearFreezeFrames(timestamp1, timestamp2, ...)
     * to delete the freeze frames stored for the provided input timestamps, provided any exist.
     *
     * @return
     */
    public boolean isSelectiveClearFreezeFramesSupported() {
        try {
            return mService.isSelectiveClearFreezeFramesSupported();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    private static class CarDiagnosticEventListenerToService
            extends Stub {
        private final WeakReference<CarDiagnosticManager> mManager;

        CarDiagnosticEventListenerToService(CarDiagnosticManager manager) {
            mManager = new WeakReference<>(manager);
        }

        private void handleOnDiagnosticEvents(CarDiagnosticManager manager,
                List<CarDiagnosticEvent> events) {
            manager.mHandlerCallback.sendEvents(events);
        }

        @Override
        public void onDiagnosticEvents(List<CarDiagnosticEvent> events) {
            CarDiagnosticManager manager = mManager.get();
            if (manager != null) {
                handleOnDiagnosticEvents(manager, events);
            }
        }
    }

    private class CarDiagnosticListeners extends CarRatedListeners<OnDiagnosticEventListener> {
        CarDiagnosticListeners(int rate) {
            super(rate);
        }

        void onDiagnosticEvent(final CarDiagnosticEvent event) {
            // throw away old data as oneway binder call can change order.
            long updateTime = event.timestamp;
            if (updateTime < mLastUpdateTime) {
                Log.w(CarLibLog.TAG_DIAGNOSTIC, "dropping old data");
                return;
            }
            mLastUpdateTime = updateTime;
            final boolean hasVendorExtensionPermission = mVendorExtensionPermission.checkGranted();
            final CarDiagnosticEvent eventToDispatch = hasVendorExtensionPermission
                    ? event :
                    event.withVendorSensorsRemoved();
            List<OnDiagnosticEventListener> listeners;
            synchronized (mActiveListeners) {
                listeners = new ArrayList<>(getListeners());
            }
            listeners.forEach(new Consumer<OnDiagnosticEventListener>() {

                @Override
                public void accept(OnDiagnosticEventListener listener) {
                    listener.onDiagnosticEvent(eventToDispatch);
                }
            });
        }
    }
}
