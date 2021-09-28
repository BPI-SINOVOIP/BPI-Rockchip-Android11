/*
 * Copyright (C) 2018 The Android Open Source Project
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

package android.car.cluster;

import android.car.Car;
import android.car.CarOccupantZoneManager;
import android.car.CarOccupantZoneManager.OccupantZoneInfo;
import android.content.Context;
import android.hardware.display.DisplayManager.DisplayListener;
import android.util.Log;
import android.view.Display;

import com.android.internal.util.Preconditions;

import java.util.List;

/**
 * This class provides a display for instrument cluster renderer.
 * <p>
 * By default it will try to provide physical secondary display if it is connected, if secondary
 * display is not connected during creation of this class then it will wait for the display will
 * be added.
 */
public class ClusterDisplayProvider {
    private static final String TAG = "Cluster.DisplayProvider";
    private static final boolean DEBUG = false;

    private final DisplayListener mListener;
    private final Car mCar;
    private CarOccupantZoneManager mOccupantZoneManager;

    private int mClusterDisplayId = Display.INVALID_DISPLAY;

    ClusterDisplayProvider(Context context, DisplayListener clusterDisplayListener) {
        mListener = clusterDisplayListener;
        mCar = Car.createCar(context, null, Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                (car, ready) -> {
                    if (!ready) return;
                    initClusterDisplayProvider(context, (CarOccupantZoneManager) car.getCarManager(
                            Car.CAR_OCCUPANT_ZONE_SERVICE));
                });
    }

    void release() {
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
        }
    }

    private void initClusterDisplayProvider(
            Context context, CarOccupantZoneManager occupantZoneManager) {
        Preconditions.checkArgument(
                occupantZoneManager != null,"Can't get CarOccupantZoneManager");
        mOccupantZoneManager = occupantZoneManager;
        checkClusterDisplayAdded();
        mOccupantZoneManager.registerOccupantZoneConfigChangeListener(
                new ClusterDisplayChangeListener());
    }

    private void checkClusterDisplayAdded() {
        Display clusterDisplay = getClusterDisplay();
        if (clusterDisplay != null) {
            Log.i(TAG, String.format("Found display: %s (id: %d, owner: %s)",
                    clusterDisplay.getName(), clusterDisplay.getDisplayId(),
                    clusterDisplay.getOwnerPackageName()));
            mClusterDisplayId = clusterDisplay.getDisplayId();
            mListener.onDisplayAdded(clusterDisplay.getDisplayId());
        }
    }

    private Display getClusterDisplay() {
        List<OccupantZoneInfo> zones = mOccupantZoneManager.getAllOccupantZones();
        int zones_size = zones.size();
        for (int i = 0; i < zones_size; ++i) {
            OccupantZoneInfo zone = zones.get(i);
            // Assumes that a Car has only one driver.
            if (zone.occupantType == CarOccupantZoneManager.OCCUPANT_TYPE_DRIVER) {
                return mOccupantZoneManager.getDisplayForOccupant(
                        zone, CarOccupantZoneManager.DISPLAY_TYPE_INSTRUMENT_CLUSTER);
            }
        }
        Log.e(TAG, "Can't find the OccupantZoneInfo for driver");
        return null;
    }

    private final class ClusterDisplayChangeListener implements
            CarOccupantZoneManager.OccupantZoneConfigChangeListener {
        @Override
        public void onOccupantZoneConfigChanged(int changeFlags) {
            if (DEBUG) Log.d(TAG, "onOccupantZoneConfigChanged changeFlags=" + changeFlags);
            if ((changeFlags & CarOccupantZoneManager.ZONE_CONFIG_CHANGE_FLAG_DISPLAY) == 0) {
                return;
            }
            if (mClusterDisplayId == Display.INVALID_DISPLAY) {
                checkClusterDisplayAdded();
            } else {
                Display clusterDisplay = getClusterDisplay();
                if (clusterDisplay == null) {
                    mListener.onDisplayRemoved(mClusterDisplayId);
                    mClusterDisplayId = Display.INVALID_DISPLAY;
                }
            }
        }
    }

    @Override
    public String toString() {
        return getClass().getSimpleName() + "{"
                + " clusterDisplayId = " + mClusterDisplayId
                + "}";
    }
}