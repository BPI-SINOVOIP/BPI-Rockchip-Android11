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

package com.google.android.car.kitchensink.sensor;

import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.location.Location;
import android.location.LocationListener;
import android.location.LocationManager;
import android.os.Bundle;
import android.util.Log;

import java.util.ArrayList;
import java.util.function.Consumer;


public class LocationListeners {

    private static final String TAG = "CAR.SENSOR.KS.location";

    SensorsTestFragment.LocationInfoTextUpdateListener mTextUpdateHandler;

    LocationManager mLocationMgr;
    SensorManager   mSensorMgr;

    private class SensorHelper implements SensorEventListener {
        private static final String TAG = "CAR.SENSOR.KS";
        private static final String LOC_SENSOR_FORMAT = "%12.8f, %12.8f, %12.8f";

        private final SensorManager mSensorMgr;
        private final int mSensorType;
        private final String mSensorUnits;
        private final String mSensorName;
        private final Sensor mSensor;
        private final Consumer<String> mUpdate;

        SensorHelper(SensorManager mgr, int type, String unit,
                String name, Consumer<String> update) {
            mSensorMgr = mgr;
            mSensorType = type;
            mSensorUnits = unit;
            mSensorName = name;
            mSensor = (mSensorMgr != null) ? mSensorMgr.getDefaultSensor(type) : null;
            mUpdate = update;

            if (mSensor == null) {
                Log.w(TAG, "sensor " + mSensorName + " is not available");
            } else {
                Log.d(TAG, "sensor " + mSensorName + " is available");
            }
        }

        void startListening() {
            if (mSensorMgr == null) {
                mUpdate.accept(mSensorName + ": SensorManager not available");
                return;
            }

            if (mSensor == null) {
                mUpdate.accept(mSensorName + ": sensor not available");
                return;
            }
            if (!mSensorMgr.registerListener(this, mSensor,
                    SensorManager.SENSOR_DELAY_FASTEST)) {
                mUpdate.accept(mSensorName + ": failed to register listener.");
                Log.w(TAG, "sensor " + mSensorName + " cannot be listened to");
            } else {
                mUpdate.accept(mSensorName + ": waiting to hear from SensorManager.");
                Log.d(TAG, "sensor " + mSensorName + " is being listened to");
            }
        }

        void stopListening() {
            if (mSensor != null) {
                mSensorMgr.unregisterListener(this);
                mUpdate.accept(mSensorName + ": SensorManager stopped");
                Log.d(TAG, "sensor " + mSensorName + " is not being listened to anymore");
            }
        }

        @Override
        public void onSensorChanged(SensorEvent event) {
            if (event.sensor.getType() != mSensorType) {
                Log.w(TAG, "unexpected event: " + event);
                return;
            }

            final String es = String.format("%s %s: (" + LOC_SENSOR_FORMAT + ")",
                    mSensorName, mSensorUnits,
                    event.values[0], event.values[1], event.values[2]);

            mUpdate.accept(es);
        }

        @Override
        public void onAccuracyChanged(Sensor sensor, int accuracy) {
        }
    }

    ArrayList<SensorHelper> mSensors = new ArrayList<>();

    public LocationListeners(Context context,
                             SensorsTestFragment.LocationInfoTextUpdateListener listener) {
        mTextUpdateHandler = listener;

        mLocationMgr = (LocationManager) context.getSystemService(Context.LOCATION_SERVICE);
        mSensorMgr = (SensorManager) context.getSystemService(Context.SENSOR_SERVICE);

        mSensors.add(new SensorHelper(mSensorMgr, Sensor.TYPE_ACCELEROMETER,
                "m/s2", "Accelerometer", mTextUpdateHandler::setAccelField));
        mSensors.add(new SensorHelper(mSensorMgr, Sensor.TYPE_MAGNETIC_FIELD,
                "uT", "Magnetometer", mTextUpdateHandler::setMagField));
        mSensors.add(new SensorHelper(mSensorMgr, Sensor.TYPE_GYROSCOPE,
                "Rad/s", "Gyroscope", mTextUpdateHandler::setGyroField));
    }

    public void startListening() {
        if (mLocationMgr != null) {
            if (mLocationMgr.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                mLocationMgr.requestLocationUpdates(LocationManager.GPS_PROVIDER, 0, 0,
                        mLocationListener);
                mTextUpdateHandler.setLocationField("waiting to hear from GPS");
            } else {
                mTextUpdateHandler.setLocationField("GPS_PROVIDER not available");
            }
        } else {
            mTextUpdateHandler.setLocationField("LocationManager not available");
        }

        mSensors.forEach(SensorHelper::startListening);
    }

    public void stopListening() {
        if (mLocationMgr != null) {
            if (mLocationMgr.isProviderEnabled(LocationManager.GPS_PROVIDER)) {
                mLocationMgr.removeUpdates(mLocationListener);
                mTextUpdateHandler.setMagField("GPS stopped");
            }
        }

        mSensors.forEach(SensorHelper::stopListening);
    }


    private final LocationListener mLocationListener = new LocationListener() {
        @Override
        public void onLocationChanged(Location location) {
            String s = String.format("Location: lat=%10.6f, lon=%10.6f, altitude=%5.0f, "
                                   + "speed=%5.1f, bearing=%3.0f, accuracy=%5.1f",
                    location.getLatitude(),
                    location.getLongitude(),
                    location.getAltitude(),
                    location.getSpeed(),
                    location.getBearing(),
                    location.getAccuracy());

            mTextUpdateHandler.setLocationField(s);
        }

        @Override
        public void onProviderEnabled(String provider) {
        }

        @Override
        public void onProviderDisabled(String provider) {
        }

        @Override
        public void onStatusChanged(String provider, int status, Bundle extras) {
        }
    };
}
