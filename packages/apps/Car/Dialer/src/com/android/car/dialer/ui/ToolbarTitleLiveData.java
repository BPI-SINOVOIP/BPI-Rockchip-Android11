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

package com.android.car.dialer.ui;

import android.bluetooth.BluetoothDevice;
import android.content.Context;

import androidx.annotation.IntDef;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;

import com.android.car.dialer.R;
import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.livedata.HfpDeviceListLiveData;

import java.util.List;

/**
 * LiveData for the toolbar title of the  {@link com.android.car.dialer.ui.TelecomActivity}. The
 * attribute {@link R.attr#toolbarTitleMode} is activity scope attribute. Supported values are:
 * <ul>
 *     <li> app_name: 0
 *     <li> none: 1
 *     <li> device_name: 2
 */
class ToolbarTitleLiveData extends MediatorLiveData<String> {

    @IntDef({
            ToolbarTitleMode.APP_NAME,
            ToolbarTitleMode.NONE,
            ToolbarTitleMode.DEVICE_NAME
    })
    private @interface ToolbarTitleMode {
        int APP_NAME = 0;
        int NONE = 1;
        int DEVICE_NAME = 2;
    }

    private final HfpDeviceListLiveData mHfpDeviceListLiveData;
    private final LiveData<Integer> mToolbarTitleModeLiveData;
    private final Context mContext;

    ToolbarTitleLiveData(Context context, LiveData<Integer> toolbarTitleModeLiveData) {
        mContext = context;
        mToolbarTitleModeLiveData = toolbarTitleModeLiveData;
        mHfpDeviceListLiveData = UiBluetoothMonitor.get().getHfpDeviceListLiveData();

        addSource(mToolbarTitleModeLiveData, this::updateToolbarTitle);
        addSource(mHfpDeviceListLiveData, this::updateDeviceName);
    }

    private void updateToolbarTitle(int toolbarTitleMode) {
        switch (toolbarTitleMode) {
            case ToolbarTitleMode.NONE:
                setValue(null);
                return;
            case ToolbarTitleMode.DEVICE_NAME:
                updateDeviceName(mHfpDeviceListLiveData.getValue());
                return;
            case ToolbarTitleMode.APP_NAME:
            default:
                setValue(mContext.getString(R.string.phone_app_name));
                return;
        }
    }

    private void updateDeviceName(List<BluetoothDevice> hfpDeviceList) {
        Integer toolbarTitleMode = mToolbarTitleModeLiveData.getValue();
        if (toolbarTitleMode != null
                && ToolbarTitleMode.DEVICE_NAME == toolbarTitleMode.intValue()) {
            // When there is no hfp device connected, use the app name as title.
            if (hfpDeviceList == null || hfpDeviceList.isEmpty()) {
                setValue(mContext.getString(R.string.phone_app_name));
                return;
            }

            // TODO: handle multi-HFP cases. Right now return the first connected device in list.
            BluetoothDevice bluetoothDevice = hfpDeviceList.get(0);
            setValue(bluetoothDevice.getName());
        }
    }
}
