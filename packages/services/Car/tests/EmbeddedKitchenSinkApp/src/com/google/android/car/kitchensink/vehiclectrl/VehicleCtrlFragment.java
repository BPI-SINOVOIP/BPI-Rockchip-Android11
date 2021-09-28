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

package com.google.android.car.kitchensink.vehiclectrl;

import android.annotation.IdRes;
import android.annotation.Nullable;
import android.annotation.SuppressLint;
import android.car.VehicleAreaWindow;
import android.car.VehiclePropertyIds;
import android.car.hardware.CarPropertyValue;
import android.car.hardware.property.CarPropertyManager;
import android.hardware.automotive.vehicle.V2_0.VehicleArea;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyGroup;
import android.hardware.automotive.vehicle.V2_0.VehiclePropertyType;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.fragment.app.Fragment;
import androidx.viewpager.widget.ViewPager;

import com.google.android.car.kitchensink.KitchenSinkActivity;
import com.google.android.car.kitchensink.R;
import com.google.android.car.kitchensink.SimplePagerAdapter;

import java.util.HashMap;
import java.util.Map;

@SuppressLint("SetTextI18n")
public final class VehicleCtrlFragment extends Fragment {
    private static final String TAG = VehicleCtrlFragment.class.getSimpleName();

    public static final class CustomVehicleProperty {
        public static final int PROTOCAN_TEST = (
                0x0ABC
                | VehiclePropertyGroup.VENDOR
                | VehiclePropertyType.BOOLEAN
                | VehicleArea.GLOBAL);

        private CustomVehicleProperty() {}
    };

    private CarPropertyManager mPropMgr;
    private final Map<Integer, TextView> mWindowPosWidgets = new HashMap<>();

    private final CarPropertyManager.CarPropertyEventCallback mPropCb =
            new CarPropertyManager.CarPropertyEventCallback() {
        @Override
        public void onChangeEvent(CarPropertyValue value) {
            onPropertyEvent(value);
        }

        @Override
        public void onErrorEvent(int propId, int zone) {}
    };

    @Override
    public View onCreateView(LayoutInflater inflater, @Nullable ViewGroup container,
            @Nullable Bundle savedInstanceState) {
        View view = inflater.inflate(R.layout.vehicle_ctrl_fragment, container, false);

        ViewPager pager = view.findViewById(R.id.vehicle_ctrl_pager);
        pager.setAdapter(new SimplePagerAdapter(pager));

        KitchenSinkActivity activity = (KitchenSinkActivity) getHost();
        mPropMgr = activity.getPropertyManager();

        subscribeProps();

        initWindowBtns(view, VehicleAreaWindow.WINDOW_ROW_1_LEFT,
                R.id.winFLOpen, R.id.winFLClose, R.id.winFLPos);
        initWindowBtns(view, VehicleAreaWindow.WINDOW_ROW_1_RIGHT,
                R.id.winFROpen, R.id.winFRClose, R.id.winFRPos);
        initWindowBtns(view, VehicleAreaWindow.WINDOW_ROW_2_LEFT,
                R.id.winRLOpen, R.id.winRLClose, R.id.winRLPos);
        initWindowBtns(view, VehicleAreaWindow.WINDOW_ROW_2_RIGHT,
                R.id.winRROpen, R.id.winRRClose, R.id.winRRPos);

        initTestBtn(view, R.id.protocan_test_on, true);
        initTestBtn(view, R.id.protocan_test_off, false);

        return view;
    }

    @Override
    public void onDestroyView() {
        super.onDestroyView();
        unsubscribeProps();
    }

    private void subscribeProps() {
        mPropMgr.registerCallback(mPropCb, VehiclePropertyIds.WINDOW_POS, 10);
    }

    private void unsubscribeProps() {
        mPropMgr.unregisterCallback(mPropCb);
    }

    public void onPropertyEvent(CarPropertyValue prop) {
        if (prop.getPropertyId() == VehiclePropertyIds.WINDOW_POS) {
            Log.i(TAG, "Window pos: " + prop.getValue());
            updateWindowPos(prop.getAreaId(), (int) prop.getValue());
        }
    }

    private void initWindowBtns(View view, int winId, @IdRes int openId, @IdRes int closeId,
            @IdRes int posId) {
        // TODO(twasilczyk): fetch the actual min/max values
        view.findViewById(openId).setOnClickListener(v -> moveWindow(winId, 1));
        view.findViewById(closeId).setOnClickListener(v -> moveWindow(winId, -1));
        mWindowPosWidgets.put(winId, view.findViewById(posId));
    }

    private void moveWindow(int windowId, int speed) {
        Log.i(TAG, "Moving window " + windowId + " with speed " + speed);
        mPropMgr.setIntProperty(VehiclePropertyIds.WINDOW_MOVE, windowId, speed);
    }

    private void updateWindowPos(int windowId, int pos) {
        TextView view = mWindowPosWidgets.get(windowId);
        view.post(() -> view.setText(pos + "%"));
    }

    private void initTestBtn(View view, @IdRes int btnId, boolean on) {
        view.findViewById(btnId).setOnClickListener(v -> onTestBtnClicked(on));
    }

    private void onTestBtnClicked(boolean on) {
        Log.i(TAG, "onTestBtnClicked " + on);
        mPropMgr.setBooleanProperty(CustomVehicleProperty.PROTOCAN_TEST, VehicleArea.GLOBAL, on);
    }
}
