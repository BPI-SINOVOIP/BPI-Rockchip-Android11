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

package android.car.experimental;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.car.Car;
import android.car.CarManagerBase;
import android.car.annotation.RequiredFeature;
import android.os.Handler;
import android.os.IBinder;
import android.os.Looper;
import android.os.Message;
import android.os.RemoteException;
import android.util.Log;

import java.lang.ref.WeakReference;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * Experimental APIs that allow clients to consume information about a driver's current distraction
 * level.
 *
 * @hide
 */
@RequiredFeature(ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE)
public final class CarDriverDistractionManager extends CarManagerBase {

    private static final String TAG = "CarDriverDistractionManager";
    private static final int MSG_HANDLE_DISTRACTION_CHANGE = 0;

    private final IDriverDistractionManager mService;
    private final EventHandler mEventHandler;

    private final IDriverDistractionChangeListenerImpl mBinderCallback;

    /**
     * Listener to monitor any changes to the driver's distraction level.
     */
    public interface OnDriverDistractionChangeListener {
        /**
         * Called when the driver's distraction level has changed.
         *
         * @param event the new driver distraction information.
         */
        void onDriverDistractionChange(DriverDistractionChangeEvent event);
    }

    private final CopyOnWriteArrayList<OnDriverDistractionChangeListener> mListeners =
            new CopyOnWriteArrayList<>();

    /**
     * Constructor parameters should remain this way for Car library to construct this.
     */
    public CarDriverDistractionManager(Car car, IBinder service) {
        super(car);
        mService = IDriverDistractionManager.Stub.asInterface(service);
        mEventHandler = new EventHandler(this, getEventHandler().getLooper());
        mBinderCallback = new IDriverDistractionChangeListenerImpl(this);
    }

    /**
     * Gets the current driver distraction level based on the last change event that is not stale.
     *
     * Return {@code null} if there is an internal error.
     */
    @RequiresPermission(value = ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION)
    @Nullable
    public DriverDistractionChangeEvent getLastDistractionEvent() {
        try {
            return mService.getLastDistractionEvent();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Adds a {@link OnDriverDistractionChangeListener} to be notified for changes to the driver's
     * distraction level.
     *
     * <p>The provided listener will immediately receive a callback for the most recent distraction
     * event.
     *
     * @param listener to be added
     */
    @RequiresPermission(value = ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION)
    public void addDriverDistractionChangeListener(
            @NonNull OnDriverDistractionChangeListener listener) {
        if (mListeners.addIfAbsent(listener)) {
            if (mListeners.size() == 1) {
                try {
                    mService.addDriverDistractionChangeListener(mBinderCallback);
                } catch (RemoteException e) {
                    handleRemoteExceptionFromCarService(e);
                }
            }
        }
    }

    /**
     * Removes the listener. Listeners not added before will be ignored.
     */
    @RequiresPermission(value = ExperimentalCar.PERMISSION_READ_CAR_DRIVER_DISTRACTION)
    public void removeDriverDistractionChangeListener(
            @NonNull OnDriverDistractionChangeListener listener) {
        if (mListeners.remove(listener)) {
            if (mListeners.size() == 0) {
                try {
                    mService.removeDriverDistractionChangeListener(mBinderCallback);
                } catch (RemoteException ignored) {
                    // ignore for unregistering
                }
            }
        }
    }

    @Override
    protected void onCarDisconnected() {
        // nothing to do
    }

    private void dispatchDistractionChangeToClient(
            DriverDistractionChangeEvent distractionChangeEvent) {
        if (distractionChangeEvent == null) {
            return;
        }
        for (OnDriverDistractionChangeListener listener : mListeners) {
            listener.onDriverDistractionChange(distractionChangeEvent);
        }
    }

    private static final class EventHandler extends Handler {
        private final WeakReference<CarDriverDistractionManager> mDistractionManager;

        EventHandler(CarDriverDistractionManager manager, Looper looper) {
            super(looper);
            mDistractionManager = new WeakReference<>(manager);
        }

        @Override
        public void handleMessage(Message msg) {
            CarDriverDistractionManager mgr = mDistractionManager.get();
            if (mgr == null) {
                Log.e(TAG,
                        "handleMessage: null reference to distraction manager when handling "
                                + "message for " + msg.what);
                return;
            }
            switch (msg.what) {
                case MSG_HANDLE_DISTRACTION_CHANGE:
                    mgr.dispatchDistractionChangeToClient((DriverDistractionChangeEvent) msg.obj);
                    break;
                default:
                    Log.e(TAG, "Unknown msg not handled:" + msg.what);
            }
        }

        private void dispatchOnDistractionChangedEvent(DriverDistractionChangeEvent event) {
            sendMessage(obtainMessage(MSG_HANDLE_DISTRACTION_CHANGE, event));
        }
    }

    private static class IDriverDistractionChangeListenerImpl extends
            IDriverDistractionChangeListener.Stub {
        private final WeakReference<CarDriverDistractionManager> mDistractionManager;

        IDriverDistractionChangeListenerImpl(CarDriverDistractionManager manager) {
            mDistractionManager = new WeakReference<>(manager);
        }

        @Override
        public void onDriverDistractionChange(DriverDistractionChangeEvent event) {
            CarDriverDistractionManager manager = mDistractionManager.get();
            if (manager != null) {
                manager.mEventHandler.dispatchOnDistractionChangedEvent(event);
            }
        }
    }
}
