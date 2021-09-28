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
import android.app.AlertDialog;
import android.car.Car;
import android.car.diagnostic.CarDiagnosticEvent;
import android.car.diagnostic.CarDiagnosticManager;
import android.os.Bundle;
import android.os.Handler;
import android.util.Log;
import android.view.View;
import android.widget.Button;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toolbar;

import androidx.recyclerview.widget.DividerItemDecoration;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.google.android.car.diagnostictools.utils.MetadataProcessing;

import java.util.ArrayList;
import java.util.List;

/**
 * Displays detailed information about a specific DTC. Opened through clicking a DTC in
 * DTCListActivity
 */
public class DTCDetailActivity extends Activity {

    private static final String TAG = "DTCDetailActivity";
    private final Handler mHandler = new Handler();
    private Car mCar;
    private Toolbar mDTCTitle;
    private TextView mDTCDetails;
    private DTC mDTC;
    private ProgressBar mFreezeFrameLoading;
    private TextView mFreezeFrameTitle;
    private Button mFreezeFrameClear;
    private RecyclerView mFreezeFrameData;
    private CarDiagnosticManager mCarDiagnosticManager;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_dtc_details);

        mDTCTitle = findViewById(R.id.toolbar);
        mDTCTitle.setNavigationIcon(R.drawable.ic_arrow_back_black_24dp);
        mDTCTitle.setNavigationOnClickListener(
                view -> {
                    finish();
                    mHandler.removeCallbacksAndMessages(null);
                });
        mDTCDetails = findViewById(R.id.dtc_details);
        mFreezeFrameLoading = findViewById(R.id.freeze_frame_loading);
        mFreezeFrameTitle = findViewById(R.id.freeze_frame_title);
        mFreezeFrameClear = findViewById(R.id.freeze_frame_clear);
        mFreezeFrameData = findViewById(R.id.freeze_frame_data);

        hideFreezeFrameFields();

        // Set up RecyclerView
        mFreezeFrameData.setHasFixedSize(false);
        mFreezeFrameData.setLayoutManager(new LinearLayoutManager(DTCDetailActivity.this));
        final List<LiveDataAdapter.SensorDataWrapper> input = new ArrayList<>();
        // Add test data
        for (int i = 0; i < 100; i++) {
            input.add(new LiveDataAdapter.SensorDataWrapper("Test " + i, "%", i));
        }
        LiveDataAdapter adapter = new LiveDataAdapter(input);
        mFreezeFrameData.setAdapter(adapter);
        mFreezeFrameData.addItemDecoration(
                new DividerItemDecoration(
                        mFreezeFrameData.getContext(), DividerItemDecoration.VERTICAL));

        loadingFreezeFrame();

        mCar = Car.createCar(this);
        mCarDiagnosticManager = (CarDiagnosticManager) mCar.getCarManager(Car.DIAGNOSTIC_SERVICE);

        // Runnable function that checks to see if there is a Freeze Frame available or not.
        // Repeats
        // until there is a Freeze Frame available
        mHandler.postDelayed(
                new Runnable() {
                    @Override
                    public void run() {
                        long[] timestamps = mCarDiagnosticManager.getFreezeFrameTimestamps();
                        Log.e(TAG, "onCreate: Number of timestamps" + timestamps.length);
                        if (timestamps.length > 0) {
                            CarDiagnosticEvent freezeFrame =
                                    mCarDiagnosticManager.getFreezeFrame(
                                            timestamps[timestamps.length - 1]);
                            adapter.update(
                                    LiveDataActivity.processSensorInfoIntoWrapper(
                                            freezeFrame,
                                            MetadataProcessing.getInstance(
                                                    DTCDetailActivity.this)));
                            loadedFreezeFrame();
                            mDTC.setTimestamp(timestamps[timestamps.length - 1]);
                        } else {
                            hideFreezeFrameFields();
                            mHandler.postDelayed(this, 2000);
                        }
                    }
                },
                0);

        getIncomingIntent();
    }

    /** Removes all callbacks from mHandler when DTCDetailActivity is destroyed */
    @Override
    protected void onDestroy() {
        if (mHandler != null) {
            mHandler.removeCallbacksAndMessages(null);
        }
        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroy();
    }

    /** Handle incoming intent to extract extras. */
    private void getIncomingIntent() {
        Log.d(TAG, "getIncomingIntent: extras: " + getIntent().toString());
        if (getIntent().hasExtra("dtc")) {
            mDTC = getIntent().getParcelableExtra("dtc");
            setUpDetailPage();
        }
    }

    /** Assuming dtc has been set to a DTC. */
    private void setUpDetailPage() {
        if (mDTC != null) {
            mDTCTitle.setTitle(mDTC.getCode() + ": " + mDTC.getDescription());
            mDTCDetails.setText(mDTC.getLongDescription());
        } else {
            mDTCTitle.setTitle("No DTC input");
            mDTCDetails.setText("No DTC long description");
        }
    }

    /** Hide all Freeze Frame associated elements. */
    private void hideFreezeFrameFields() {
        mFreezeFrameData.setVisibility(View.INVISIBLE);
        mFreezeFrameClear.setVisibility(View.INVISIBLE);
        mFreezeFrameTitle.setVisibility(View.INVISIBLE);
        mFreezeFrameLoading.setVisibility(View.INVISIBLE);
    }

    /** Hide most Freeze Frame associated elements and tell user that there isn't one available. */
    private void noFreezeFrameAvailable() {
        hideFreezeFrameFields();
        mFreezeFrameTitle.setVisibility(View.VISIBLE);
        mFreezeFrameTitle.setText(
                "No Freeze Frame Data Available Right Now. Data will appear if becomes available");
    }

    /** Indicate to the user that a freeze frame is being loaded with a spinning progress bar */
    private void loadingFreezeFrame() {
        mFreezeFrameTitle.setVisibility(View.VISIBLE);
        mFreezeFrameTitle.setText("Freeze Frame Loading...");
        mFreezeFrameLoading.setVisibility(View.VISIBLE);
    }

    /**
     * Displays freeze frame and conditionally displays button to clear it based on if functionality
     * is supported
     */
    private void loadedFreezeFrame() {
        mFreezeFrameLoading.setVisibility(View.INVISIBLE);
        if (mCarDiagnosticManager.isClearFreezeFramesSupported()
                && mCarDiagnosticManager.isSelectiveClearFreezeFramesSupported()) {
            mFreezeFrameClear.setVisibility(View.VISIBLE);
        }
        mFreezeFrameData.setVisibility(View.VISIBLE);
        mFreezeFrameTitle.setText("Freeze Frame");
    }

    /**
     * Handles button press of the clear Freeze Frame button. Confirms that the user would like to
     * do this action and then clears Frames with Manager methods
     *
     * @param v View that triggered the function
     */
    public void clearFreezeFrameButtonPress(View v) {
        new AlertDialog.Builder(this)
                .setTitle("Confirm Freeze Frame Clear")
                .setMessage(
                        String.format(
                                "Do you really want to clear the freeze frame for DTC %s?",
                                mDTC.getCode()))
                .setIcon(android.R.drawable.ic_dialog_alert)
                .setPositiveButton(
                        android.R.string.yes,
                        (dialog, whichButton) -> {
                            mCarDiagnosticManager.clearFreezeFrames(mDTC.getTimestamp());
                            hideFreezeFrameFields();
                        })
                .setNegativeButton(android.R.string.no, null)
                .show();
    }
}
