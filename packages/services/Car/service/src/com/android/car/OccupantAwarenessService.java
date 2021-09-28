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

package com.android.car;

import android.annotation.NonNull;
import android.car.Car;
import android.car.occupantawareness.IOccupantAwarenessEventCallback;
import android.car.occupantawareness.OccupantAwarenessDetection;
import android.car.occupantawareness.OccupantAwarenessDetection.VehicleOccupantRole;
import android.car.occupantawareness.SystemStatusEvent;
import android.car.occupantawareness.SystemStatusEvent.DetectionTypeFlags;
import android.content.Context;
import android.hardware.automotive.occupant_awareness.IOccupantAwareness;
import android.hardware.automotive.occupant_awareness.IOccupantAwarenessClientCallback;
import android.os.RemoteCallbackList;
import android.os.RemoteException;
import android.os.ServiceManager;
import android.util.Log;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.lang.ref.WeakReference;

/**
 * A service that listens to an Occupant Awareness Detection system across a HAL boundary and
 * exposes the data to system clients in Android via a {@link
 * android.car.occupantawareness.OccupantAwarenessManager}.
 *
 * <p>The service exposes the following detections types:
 *
 * <h1>Presence Detection</h1>
 *
 * Detects whether a person is present for each seat location.
 *
 * <h1>Gaze Detection</h1>
 *
 * Detects where an occupant is looking and for how long they have been looking at the specified
 * target.
 *
 * <h1>Driver Monitoring</h1>
 *
 * Detects whether a driver is looking on or off-road and for how long they have been looking there.
 */
public class OccupantAwarenessService
        extends android.car.occupantawareness.IOccupantAwarenessManager.Stub
        implements CarServiceBase {
    private static final String TAG = CarLog.TAG_OAS;
    private static final boolean DBG = false;

    // HAL service identifier name.
    @VisibleForTesting
    static final String OAS_SERVICE_ID =
            "android.hardware.automotive.occupant_awareness.IOccupantAwareness/default";

    private final Object mLock = new Object();
    private final Context mContext;

    @GuardedBy("mLock")
    private IOccupantAwareness mOasHal;

    private final ChangeListenerToHalService mHalListener = new ChangeListenerToHalService(this);

    private class ChangeCallbackList extends RemoteCallbackList<IOccupantAwarenessEventCallback> {
        private final WeakReference<OccupantAwarenessService> mOasService;

        ChangeCallbackList(OccupantAwarenessService oasService) {
            mOasService = new WeakReference<>(oasService);
        }

        /** Handle callback death. */
        @Override
        public void onCallbackDied(IOccupantAwarenessEventCallback listener) {
            Log.i(TAG, "binderDied: " + listener.asBinder());

            OccupantAwarenessService service = mOasService.get();
            if (service != null) {
                service.handleClientDisconnected();
            }
        }
    }

    @GuardedBy("mLock")
    private final ChangeCallbackList mListeners = new ChangeCallbackList(this);

    /** Creates an OccupantAwarenessService instance given a {@link Context}. */
    public OccupantAwarenessService(Context context) {
        mContext = context;
    }

    /** Creates an OccupantAwarenessService instance given a {@link Context}. */
    @VisibleForTesting
    OccupantAwarenessService(Context context, IOccupantAwareness oasInterface) {
        mContext = context;
        mOasHal = oasInterface;
    }

    @Override
    public void init() {
        logd("Initializing service");
        connectToHalServiceIfNotConnected();
    }

    @Override
    public void release() {
        logd("Will stop detection and disconnect listeners");
        stopDetectionGraph();
        mListeners.kill();
    }

    @Override
    public void dump(PrintWriter writer) {
        writer.println("*OccupantAwarenessService*");
        writer.println(
                String.format(
                        "%s to HAL service", mOasHal == null ? "NOT connected" : "Connected"));
        writer.println(
                String.format(
                        "%d change listeners subscribed.",
                        mListeners.getRegisteredCallbackCount()));
    }

    /** Attempts to connect to the HAL service if it is not already connected. */
    private void connectToHalServiceIfNotConnected() {
        logd("connectToHalServiceIfNotConnected()");

        synchronized (mLock) {
            // If already connected, nothing more needs to be done.
            if (mOasHal != null) {
                logd("Client is already connected, nothing more to do");
                return;
            }

            // Attempt to find the HAL service.
            logd("Attempting to connect to client at: " + OAS_SERVICE_ID);
            mOasHal =
                    android.hardware.automotive.occupant_awareness.IOccupantAwareness.Stub
                            .asInterface(ServiceManager.getService(OAS_SERVICE_ID));

            if (mOasHal == null) {
                Log.e(TAG, "Failed to find OAS hal_service at: [" + OAS_SERVICE_ID + "]");
                return;
            }

            // Register for callbacks.
            try {
                mOasHal.setCallback(mHalListener);
            } catch (RemoteException e) {
                mOasHal = null;
                Log.e(TAG, "Failed to set callback: " + e);
                return;
            }

            logd("Successfully connected to hal_service at: [" + OAS_SERVICE_ID + "]");
        }
    }

    /** Sends a message via the HAL to start the detection graph. */
    private void startDetectionGraph() {
        logd("Attempting to start detection graph");

        // Grab a copy of 'mOasHal' to avoid sitting on the lock longer than is necessary.
        IOccupantAwareness hal;
        synchronized (mLock) {
            hal = mOasHal;
        }

        if (hal != null) {
            try {
                hal.startDetection();
            } catch (RemoteException e) {
                Log.e(TAG, "startDetection() HAL invocation failed: " + e, e);

                synchronized (mLock) {
                    mOasHal = null;
                }
            }
        } else {
            Log.e(TAG, "No HAL is connected. Cannot request graph start");
        }
    }

    /** Sends a message via the HAL to stop the detection graph. */
    private void stopDetectionGraph() {
        logd("Attempting to stop detection graph.");

        // Grab a copy of 'mOasHal' to avoid sitting on the lock longer than is necessary.
        IOccupantAwareness hal;
        synchronized (mLock) {
            hal = mOasHal;
        }

        if (hal != null) {
            try {
                hal.stopDetection();
            } catch (RemoteException e) {
                Log.e(TAG, "stopDetection() HAL invocation failed: " + e, e);

                synchronized (mLock) {
                    mOasHal = null;
                }
            }
        } else {
            Log.e(TAG, "No HAL is connected. Cannot request graph stop");
        }
    }

    /**
     * Gets the vehicle capabilities for a given role.
     *
     * <p>Capabilities are static for a given vehicle configuration and need only be queried once
     * per vehicle. Once capability is determined, clients should query system status to see if the
     * subsystem is currently ready to serve.
     *
     * <p>Requires {@link android.car.Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE} read
     * permissions to access.
     *
     * @param role {@link VehicleOccupantRole} to query for.
     * @return Flags indicating supported capabilities for the role.
     */
    public @DetectionTypeFlags int getCapabilityForRole(@VehicleOccupantRole int role) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE);

        connectToHalServiceIfNotConnected();

        // Grab a copy of 'mOasHal' to avoid sitting on the lock longer than is necessary.
        IOccupantAwareness hal;
        synchronized (mLock) {
            hal = mOasHal;
        }

        if (hal != null) {
            try {
                return hal.getCapabilityForRole(role);
            } catch (RemoteException e) {

                Log.e(TAG, "getCapabilityForRole() HAL invocation failed: " + e, e);

                synchronized (mLock) {
                    mOasHal = null;
                }

                return SystemStatusEvent.DETECTION_TYPE_NONE;
            }
        } else {
            Log.e(
                    TAG,
                    "getCapabilityForRole(): No HAL interface has been provided. Cannot get"
                            + " capabilities");
            return SystemStatusEvent.DETECTION_TYPE_NONE;
        }
    }

    /**
     * Registers a {@link IOccupantAwarenessEventCallback} to be notified for changes in the system
     * state.
     *
     * <p>Requires {@link android.car.Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE} read
     * permissions to access.
     *
     * @param listener {@link IOccupantAwarenessEventCallback} listener to register.
     */
    @Override
    public void registerEventListener(@NonNull IOccupantAwarenessEventCallback listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE);

        connectToHalServiceIfNotConnected();

        synchronized (mLock) {
            if (mOasHal == null) {
                Log.e(TAG, "Attempting to register a listener, but could not connect to HAL.");
                return;
            }

            logd("Registering a new listener");
            mListeners.register(listener);

            // After the first client connects, request that the detection graph start.
            if (mListeners.getRegisteredCallbackCount() == 1) {
                startDetectionGraph();
            }
        }
    }

    /**
     * Unregister the given {@link IOccupantAwarenessEventCallback} listener from receiving events.
     *
     * <p>Requires {@link android.car.Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE} read
     * permissions to access.
     *
     * @param listener {@link IOccupantAwarenessEventCallback} client to unregister.
     */
    @Override
    public void unregisterEventListener(@NonNull IOccupantAwarenessEventCallback listener) {
        ICarImpl.assertPermission(mContext, Car.PERMISSION_READ_CAR_OCCUPANT_AWARENESS_STATE);

        connectToHalServiceIfNotConnected();

        synchronized (mLock) {
            mListeners.unregister(listener);
        }

        // When the last client disconnects, request that the detection graph stop.
        handleClientDisconnected();
    }

    /** Processes a detection event and propagates it to registered clients. */
    @VisibleForTesting
    void processStatusEvent(@NonNull SystemStatusEvent statusEvent) {
        int idx = mListeners.beginBroadcast();
        while (idx-- > 0) {
            IOccupantAwarenessEventCallback listener = mListeners.getBroadcastItem(idx);
            try {
                listener.onStatusChanged(statusEvent);
            } catch (RemoteException e) {
                // It's likely the connection snapped. Let binder death handle the situation.
                Log.e(TAG, "onStatusChanged() invocation failed: " + e, e);
            }
        }
        mListeners.finishBroadcast();
    }

    /** Processes a detection event and propagates it to registered clients. */
    @VisibleForTesting
    void processDetectionEvent(@NonNull OccupantAwarenessDetection detection) {
        int idx = mListeners.beginBroadcast();
        while (idx-- > 0) {
            IOccupantAwarenessEventCallback listener = mListeners.getBroadcastItem(idx);
            try {
                listener.onDetectionEvent(detection);
            } catch (RemoteException e) {
                // It's likely the connection snapped. Let binder death handle the situation.
                Log.e(TAG, "onDetectionEvent() invocation failed: " + e, e);
            }
        }
        mListeners.finishBroadcast();
    }

    /** Handle client disconnections, possibly stopping the detection graph. */
    void handleClientDisconnected() {
        // If the last client disconnects, requests that the graph stops.
        synchronized (mLock) {
            if (mListeners.getRegisteredCallbackCount() == 0) {
                stopDetectionGraph();
            }
        }
    }

    private static void logd(String msg) {
        if (DBG) {
            Log.d(TAG, msg);
        }
    }

    /**
     * Class that implements the listener interface and gets called back from the {@link
     * android.hardware.automotive.occupant_awareness.IOccupantAwarenessClientCallback} across the
     * binder interface.
     */
    private static class ChangeListenerToHalService extends IOccupantAwarenessClientCallback.Stub {
        private final WeakReference<OccupantAwarenessService> mOasService;

        ChangeListenerToHalService(OccupantAwarenessService oasService) {
            mOasService = new WeakReference<>(oasService);
        }

        @Override
        public void onSystemStatusChanged(int inputDetectionFlags, byte inputStatus) {
            OccupantAwarenessService service = mOasService.get();
            if (service != null) {
                service.processStatusEvent(
                        OccupantAwarenessUtils.convertToStatusEvent(
                                inputDetectionFlags, inputStatus));
            }
        }

        @Override
        public void onDetectionEvent(
                android.hardware.automotive.occupant_awareness.OccupantDetections detections) {
            OccupantAwarenessService service = mOasService.get();
            if (service != null) {
                for (android.hardware.automotive.occupant_awareness.OccupantDetection detection :
                        detections.detections) {
                    service.processDetectionEvent(
                            OccupantAwarenessUtils.convertToDetectionEvent(
                                    detections.timeStampMillis, detection));
                }
            }
        }

        @Override
        public int getInterfaceVersion() {
            return this.VERSION;
        }

        @Override
        public String getInterfaceHash() {
            return this.HASH;
        }
    }
}
