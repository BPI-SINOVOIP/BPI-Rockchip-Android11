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

package com.google.android.car.kitchensink.experimental;

import android.car.Car;
import android.car.experimental.CarTestDemoExperimentalFeatureManager;
import android.car.experimental.ExperimentalCar;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.KitchenSinkActivity;
import com.google.android.car.kitchensink.R;

public class ExperimentalFeatureTestFragment extends Fragment {

    private static final String TAG = "ExperimentalFeature";

    private static final String[] PING_MSGS = {
            "Hello, world",
            "This is 1st experimental feature",
    };

    private int mCurrentMsgIndex = 0;
    private TextView mPingMsgTextView;
    private Button mPingButton;

    private CarTestDemoExperimentalFeatureManager mDemoManager;

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle savedInstance) {
        View view = inflater.inflate(R.layout.experimental_feature_test, container, false);
        mPingMsgTextView = view.findViewById(R.id.experimental_ping_msg);
        mPingButton = view.findViewById(R.id.button_experimental_ping);
        Car car = ((KitchenSinkActivity) getHost()).getCar();
        if (car.isFeatureEnabled(ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE)) {
            mDemoManager = (CarTestDemoExperimentalFeatureManager) car.getCarManager(
                    ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE);
            mPingMsgTextView.setText("feature enabled");
        } else {
            Log.w(TAG, "ExperimentalCar.TEST_EXPERIMENTAL_FEATURE_SERVICE not enabled");
            mPingButton.setActivated(false);
            mPingMsgTextView.setText("feature disabled");
        }
        view.findViewById(R.id.button_experimental_ping).setOnClickListener(
                (View v) -> {
                    if (mDemoManager == null) {
                        return;
                    }
                    String msg = pickMsg();
                    mPingMsgTextView.setText(mDemoManager.ping(msg));
                });
        return view;
    }

    private String pickMsg() {
        String msg = PING_MSGS[mCurrentMsgIndex];
        mCurrentMsgIndex++;
        if (mCurrentMsgIndex >= PING_MSGS.length) {
            mCurrentMsgIndex = 0;
        }
        return msg;
    }
}
