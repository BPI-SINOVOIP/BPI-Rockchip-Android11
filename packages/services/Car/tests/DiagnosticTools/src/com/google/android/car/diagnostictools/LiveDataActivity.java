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

package com.google.android.car.diagnostictools;

import android.app.Activity;
import android.car.Car;
import android.car.diagnostic.CarDiagnosticEvent;
import android.car.diagnostic.CarDiagnosticManager;
import android.car.diagnostic.FloatSensorIndex;
import android.car.diagnostic.IntegerSensorIndex;
import android.car.hardware.property.CarPropertyManager;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.Toast;
import android.widget.Toolbar;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;
import androidx.recyclerview.widget.RecyclerView.LayoutManager;

import com.google.android.car.diagnostictools.utils.MetadataProcessing;
import com.google.android.car.diagnostictools.utils.SensorMetadata;

import java.util.ArrayList;
import java.util.List;

public class LiveDataActivity extends Activity {
    private static final String TAG = "LiveDataActivity";

    private Car mCar;
    private MetadataProcessing mMetadataProcessing;
    private RecyclerView mRecyclerView;
    private LayoutManager mLayoutManager;
    private LiveDataAdapter mAdapter;

    /**
     * Convert CarDiagnosticEvent into a list of SensorDataWrapper objects to be displayed
     *
     * @param event CarDiagnosticEvent with live(freeze) frame data in it.
     * @param mMetadataProcessing MetadataProcessing object to associate sensor data with
     * @return List of LiveDataWrappers to be displayed
     */
    static List<LiveDataAdapter.SensorDataWrapper> processSensorInfoIntoWrapper(
            CarDiagnosticEvent event, MetadataProcessing mMetadataProcessing) {
        List<LiveDataAdapter.SensorDataWrapper> sensorData = new ArrayList<>();
        for (int i = 0; i <= FloatSensorIndex.LAST_SYSTEM; i++) {
            Float sensor_value = event.getSystemFloatSensor(i);
            if (sensor_value != null) {
                SensorMetadata metadata = mMetadataProcessing.getFloatMetadata(i);
                if (metadata != null) {
                    Log.d(TAG, "Float metadata" + metadata.toString());
                    sensorData.add(metadata.toLiveDataWrapper(sensor_value));
                } else {
                    sensorData.add(
                            new LiveDataAdapter.SensorDataWrapper(
                                    "Float Sensor " + i, "", sensor_value));
                }
            }
        }
        for (int i = 0; i <= IntegerSensorIndex.LAST_SYSTEM; i++) {
            Integer sensor_value = event.getSystemIntegerSensor(i);
            if (sensor_value != null) {
                SensorMetadata metadata = mMetadataProcessing.getIntegerMetadata(i);
                if (metadata != null) {
                    Log.d(TAG, "Sensor metadata" + metadata.toString());
                    sensorData.add(metadata.toLiveDataWrapper(sensor_value));
                } else {
                    sensorData.add(
                            new LiveDataAdapter.SensorDataWrapper(
                                    "Integer Sensor " + i, "", sensor_value));
                }
            }
        }
        return sensorData;
    }

    /**
     * Overloaded version of processSensorInfoIntoWrapper that uses the metadata member variable
     *
     * @param event CarDiagnosticEvent with live(freeze) frame data in it.
     * @return List of LiveDataWrappers to be displayed
     */
    private List<LiveDataAdapter.SensorDataWrapper> processSensorInfoIntoWrapper(
            CarDiagnosticEvent event) {
        return processSensorInfoIntoWrapper(event, mMetadataProcessing);
    }

    /** Called with the activity is first created. */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        View view = getLayoutInflater().inflate(R.layout.activity_live_data, null);
        setContentView(view);

        mCar = Car.createCar(this);
        mMetadataProcessing = MetadataProcessing.getInstance(this);

        CarDiagnosticManager diagnosticManager =
                (CarDiagnosticManager) mCar.getCarManager(Car.DIAGNOSTIC_SERVICE);

        CarDiagnosticListener listener = new CarDiagnosticListener(diagnosticManager);

        if (diagnosticManager != null && diagnosticManager.isLiveFrameSupported()) {
            diagnosticManager.registerListener(
                    listener,
                    CarDiagnosticManager.FRAME_TYPE_LIVE,
                    (int) CarPropertyManager.SENSOR_RATE_NORMAL);
        } else if (diagnosticManager == null) {
            Toast.makeText(this, "Error reading manager, please reload", Toast.LENGTH_LONG).show();
        } else if (!diagnosticManager.isLiveFrameSupported()) {
            Toast.makeText(this, "Live Frame data not supported", Toast.LENGTH_LONG).show();
        }

        Toolbar toolbar = findViewById(R.id.toolbar);
        setActionBar(toolbar);
        toolbar.setNavigationIcon(R.drawable.ic_arrow_back_black_24dp);
        toolbar.setNavigationOnClickListener(view1 -> finish());

        mRecyclerView = findViewById(R.id.live_data_list);

        mRecyclerView.setHasFixedSize(false);

        mLayoutManager = new LinearLayoutManager(this);
        mRecyclerView.setLayoutManager(mLayoutManager);
        mAdapter = new LiveDataAdapter();
        mRecyclerView.setAdapter(mAdapter);
        mRecyclerView.addItemDecoration(
                new DividerItemDecoration(
                        mRecyclerView.getContext(), DividerItemDecoration.VERTICAL));
    }

    @Override
    public void onDestroy() {
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroy();
    }

    /** Listener which updates live frame data when it is available */
    private class CarDiagnosticListener implements CarDiagnosticManager.OnDiagnosticEventListener {

        private final CarDiagnosticManager mManager;

        CarDiagnosticListener(CarDiagnosticManager manager) {
            this.mManager = manager;
        }

        @Override
        public void onDiagnosticEvent(CarDiagnosticEvent event) {
            if (mManager.isLiveFrameSupported()) {
                mAdapter.update(processSensorInfoIntoWrapper(event));
            }
        }
    }
}
