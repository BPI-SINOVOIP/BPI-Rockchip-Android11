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

package android.car.occupantawareness;

import android.annotation.NonNull;
import android.annotation.RequiresPermission;
import android.car.Car;
import android.car.CarManagerBase;
import android.car.annotation.RequiredFeature;
import android.car.occupantawareness.OccupantAwarenessDetection.VehicleOccupantRole;
import android.car.occupantawareness.SystemStatusEvent.DetectionTypeFlags;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;

import java.lang.ref.WeakReference;

/**
 * API exposing Occupant Awareness System data.
 *
 * <p>Supported detections include: presence detection, {@link GazeDetection} and {@link
 * DriverMonitoringDetection}.
 *
 * @hide
 */
@RequiredFeature(Car.OCCUPANT_AWARENESS_SERVICE)
public class OccupantAwarenessManager extends CarManagerBase {
    private static final String TAG = "OAS.Manager";
    private static final boolean DBG = false;

    private static final int MSG_HANDLE_SYSTEM_STATUS_CHANGE = 0;
    private static final int MSG_HANDLE_DETECTION_EVENT = 1;

    private final android.car.occupantawareness.IOccupantAwarenessManager mOccupantAwarenessService;

    private final Object mLock = new Object();

    @GuardedBy("mLock")
    private ChangeCallback mChangeCallback;

    private final EventCallbackHandler mEventCallbackHandler;

    @GuardedBy("mLock")
    private ChangeListenerToService mListenerToService;

    /** @hide */
    public OccupantAwarenessManager(Car car, IBinder service) {
        super(car);
        mOccupantAwarenessService =
                android.car.occupantawareness.IOccupantAwarenessManager.Stub.asInterface(service);

        mEventCallbackHandler = new EventCallbackHandler(this, getEventHandler().getLooper());
    }

    /** @hide */
    @Override
    public void onCarDisconnected() {
        synchronized (mLock) {
            mChangeCallback = null;
            mListenerToService = null;
        }
    }

    /**
     * Gets the detection capabilities for a given {@link VehicleOccupantRole} in this vehicle.
     *
     * <p>There may be different detection capabilities for different {@link VehicleOccupantRole}.
     * Each role should be queried independently.
     *
     * <p>Capabilities are static for a given vehicle configuration and need only be queried once
     * per vehicle. Once capability is determined, clients should query system status via {@link
     * SystemStatusEvent} to see if the subsystem is currently online and ready to serve.
     *
     * @param role {@link VehicleOccupantRole} to query for.
     * @return {@link SystemStatusEvent.DetectionTypeFlags} indicating supported capabilities for
     *     the role.
     */
    @RequiresPermission(value = Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE)
    public @DetectionTypeFlags int getCapabilityForRole(@VehicleOccupantRole int role) {
        try {
            return mOccupantAwarenessService.getCapabilityForRole(role);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Callbacks for listening to changes to {@link SystemStatusEvent} and {@link
     * OccupantAwarenessDetection}.
     */
    public abstract static class ChangeCallback {
        /**
         * Called when the system state changes changes.
         *
         * @param systemStatus The new system state as a {@link SystemStatusEvent}.
         */
        public abstract void onSystemStateChanged(@NonNull SystemStatusEvent systemStatus);

        /**
         * Called when a detection event is generated.
         *
         * @param event The new detection state as a {@link OccupantAwarenessDetection}.
         */
        public abstract void onDetectionEvent(@NonNull OccupantAwarenessDetection event);
    }

    /**
     * Registers a {@link ChangeCallback} for listening for events.
     *
     * <p>If a listener has already been registered, it has to be unregistered before registering
     * the new one.
     *
     * @param callback {@link ChangeCallback} to register.
     * @throws IllegalStateException if an existing callback is already registered.
     */
    @RequiresPermission(value = Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE)
    public void registerChangeCallback(@NonNull ChangeCallback callback) {
        if (DBG) {
            Log.d(TAG, "Registering change listener");
        }

        synchronized (mLock) {
            // Check if the listener has been already registered.
            if (mChangeCallback != null) {
                throw new IllegalStateException(
                        "Attempting to register a new listener when an existing listener has"
                                + " already been registered.");
            }

            mChangeCallback = callback;

            try {
                if (mListenerToService == null) {
                    mListenerToService = new ChangeListenerToService(this);
                }

                mOccupantAwarenessService.registerEventListener(mListenerToService);
            } catch (RemoteException e) {
                handleRemoteExceptionFromCarService(e);
            }
        }
    }

    /** Unregisters a previously registered {@link ChangeCallback}. */
    @RequiresPermission(value = Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE)
    public void unregisterChangeCallback() {
        if (DBG) {
            Log.d(TAG, "Unregistering change listener");
        }

        synchronized (mLock) {
            if (mChangeCallback == null) {
                Log.e(TAG, "No listener exists to unregister.");
                return;
            }
            mChangeCallback = null;
        }

        synchronized (mLock) {
            try {
                mOccupantAwarenessService.unregisterEventListener(mListenerToService);
            } catch (RemoteException e) {
                handleRemoteExceptionFromCarService(e);
            }

            mListenerToService = null;
        }
    }

    /**
     * Class that implements the listener interface and gets called back from the {@link
     * com.android.car.IOccupantAwarenessEventCallback} across the binder interface.
     */
    private static class ChangeListenerToService extends IOccupantAwarenessEventCallback.Stub {
        private final WeakReference<OccupantAwarenessManager> mOccupantAwarenessManager;

        ChangeListenerToService(OccupantAwarenessManager manager) {
            mOccupantAwarenessManager = new WeakReference<>(manager);
        }

        @Override
        public void onStatusChanged(SystemStatusEvent systemStatus) {
            OccupantAwarenessManager manager = mOccupantAwarenessManager.get();
            if (manager != null) {
                manager.handleSystemStatusChanged(systemStatus);
            }
        }

        @Override
        public void onDetectionEvent(OccupantAwarenessDetection event) {
            OccupantAwarenessManager manager = mOccupantAwarenessManager.get();
            if (manager != null) {
                manager.handleDetectionEvent(event);
            }
        }
    }

    /**
     * Gets the {@link SystemStatusEvent} from the service listener {@link
     * SystemStateChangeListenerToService} and dispatches it to a handler provided to the manager.
     *
     * @param systemStatus {@link SystemStatusEvent} that has been registered to listen on
     */
    private void handleSystemStatusChanged(SystemStatusEvent systemStatus) {
        // Send a message via the handler.
        mEventCallbackHandler.sendMessage(
                mEventCallbackHandler.obtainMessage(MSG_HANDLE_SYSTEM_STATUS_CHANGE, systemStatus));
    }

    /**
     * Checks for the listeners to list of {@link SystemStatusEvent} and calls them back in the
     * callback handler thread.
     *
     * @param systemStatus {@link SystemStatusEvent}
     */
    private void dispatchSystemStatusToClient(SystemStatusEvent systemStatus) {
        if (systemStatus == null) {
            return;
        }

        synchronized (mLock) {
            if (mChangeCallback != null) {
                mChangeCallback.onSystemStateChanged(systemStatus);
            }
        }
    }

    /**
     * Gets the {@link OccupantAwarenessDetection} from the service listener {@link
     * DetectionEventListenerToService} and dispatches it to a handler provided to the manager.
     *
     * @param detectionEvent {@link OccupantAwarenessDetection} that has been registered to listen
     *     on
     */
    private void handleDetectionEvent(OccupantAwarenessDetection detectionEvent) {
        // Send a message via the handler.
        mEventCallbackHandler.sendMessage(
                mEventCallbackHandler.obtainMessage(MSG_HANDLE_DETECTION_EVENT, detectionEvent));
    }

    /**
     * Checks for the listeners to list of {@link
     * android.car.occupantawareness.OccupantAwarenessDetection} and calls them back in the callback
     * handler thread.
     *
     * @param detectionEvent {@link OccupantAwarenessDetection}
     */
    private void dispatchDetectionEventToClient(OccupantAwarenessDetection detectionEvent) {
        if (detectionEvent == null) {
            return;
        }

        ChangeCallback callback;

        synchronized (mLock) {
            callback = mChangeCallback;
        }

        if (callback != null) {

            callback.onDetectionEvent(detectionEvent);
        }
    }

    /** Callback handler to dispatch system status changes to the corresponding listeners. */
    private static final class EventCallbackHandler extends Handler {
        private final WeakReference<OccupantAwarenessManager> mOccupantAwarenessManager;

        EventCallbackHandler(OccupantAwarenessManager manager, Looper looper) {
            super(looper);
            mOccupantAwarenessManager = new WeakReference<>(manager);
        }

        @Override
        public void handleMessage(Message msg) {
            OccupantAwarenessManager mgr = mOccupantAwarenessManager.get();
            if (mgr != null) {

                switch (msg.what) {
                    case MSG_HANDLE_SYSTEM_STATUS_CHANGE:
                        mgr.dispatchSystemStatusToClient((SystemStatusEvent) msg.obj);
                        break;

                    case MSG_HANDLE_DETECTION_EVENT:
                        mgr.dispatchDetectionEventToClient((OccupantAwarenessDetection) msg.obj);
                        break;

                    default:
                        throw new RuntimeException("Unknown message " + msg.what);
                }
            }
        }
    }
}
