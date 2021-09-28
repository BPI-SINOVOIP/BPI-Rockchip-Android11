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

package com.google.android.car.kitchensink;

import android.car.Car;
import android.content.ComponentName;
import android.content.ServiceConnection;
import android.os.Bundle;
import android.os.IBinder;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;

public class CarApiTestFragment extends Fragment {

    private static final String TAG = CarApiTestFragment.class.getSimpleName();

    private Car mCarForCreateAndConnect;
    private Car mCarForCreateCar;
    private Car mCarForStatusChange;

    private TextView mTextForCreateAndConnect;
    private TextView mTextForCreateCar;
    private TextView mTextForCreateCarWithStatusChangeListener;

    private int mConnectCountForStatusChange = 0;

    private final ServiceConnection mServiceConnectionForCreateAndConnect =
            new ServiceConnection() {

                private int mConnectCount = 0;

                @Override
                public void onServiceConnected(ComponentName name, IBinder service) {
                    mConnectCount++;
                    mTextForCreateAndConnect.setText("bound service connected, isConnected:"
                            + mCarForCreateAndConnect.isConnected()
                            + " connect count:" + mConnectCount);
                }

                @Override
                public void onServiceDisconnected(ComponentName name) {
                    mTextForCreateAndConnect.setText("bound service disconnected, isConnected:"
                            + mCarForCreateAndConnect.isConnected());
                }
            };

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle bundle) {
        View view = inflater.inflate(R.layout.carapi, container, false);
        mTextForCreateAndConnect = view.findViewById(R.id.carapi_createandconnect);
        mTextForCreateCar = view.findViewById(R.id.carapi_createcar);
        mTextForCreateCarWithStatusChangeListener = view.findViewById(
                R.id.carapi_createcar_with_status_change);
        view.findViewById(R.id.button_carapi_createandconnect).setOnClickListener(
                (View v) -> {
                    disconnectCar(mCarForCreateAndConnect);
                    mCarForCreateAndConnect = Car.createCar(getContext(),
                            mServiceConnectionForCreateAndConnect);
                    mCarForCreateAndConnect.connect();
                });
        view.findViewById(R.id.button_carapi_createcar).setOnClickListener(
                (View v) -> {
                    disconnectCar(mCarForCreateCar);
                    mCarForCreateCar = Car.createCar(getContext());
                    mTextForCreateCar.setText("isConnected:" + mCarForCreateCar.isConnected());
                });
        view.findViewById(R.id.button_carapi_createcar_with_status_change).setOnClickListener(
                (View v) -> {
                    disconnectCar(mCarForStatusChange);
                    mCarForStatusChange = Car.createCar(getContext(), null,
                            Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER,
                            (Car car, boolean ready) -> {
                                if (ready) {
                                    mConnectCountForStatusChange++;
                                    mTextForCreateCarWithStatusChangeListener.setText(
                                            "service ready, isConnected:"
                                                    + car.isConnected()
                                                    + " connect count:"
                                                    + mConnectCountForStatusChange);
                                } else {
                                    mTextForCreateCarWithStatusChangeListener.setText(
                                            "bound service crashed, isConnected:"
                                                    + car.isConnected());
                                }
                            });
                });
        return view;
    }

    @Override
    public void onDestroyView() {
        disconnectCar(mCarForCreateAndConnect);
        mCarForCreateAndConnect = null;
        disconnectCar(mCarForCreateCar);
        mCarForCreateCar = null;
        disconnectCar(mCarForStatusChange);
        mCarForStatusChange = null;
        super.onDestroyView();
    }

    private void disconnectCar(Car car) {
        if (car != null && car.isConnected()) {
            car.disconnect();
        }
    }
}
