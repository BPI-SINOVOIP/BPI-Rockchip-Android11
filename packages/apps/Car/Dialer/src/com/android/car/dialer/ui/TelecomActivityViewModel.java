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

import android.app.Application;
import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.content.Context;
import android.telecom.Call;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.AndroidViewModel;
import androidx.lifecycle.LiveData;
import androidx.lifecycle.MediatorLiveData;
import androidx.lifecycle.MutableLiveData;

import com.android.car.dialer.bluetooth.UiBluetoothMonitor;
import com.android.car.dialer.log.L;
import com.android.car.dialer.telecom.LocalCallHandler;
import com.android.car.dialer.ui.common.SingleLiveEvent;

import java.util.List;

/**
 * View model for {@link TelecomActivity}.
 */
public class TelecomActivityViewModel extends AndroidViewModel {
    private static final String TAG = "CD.TelecomActivityViewModel";

    private final Context mApplicationContext;
    private RefreshUiEvent mRefreshTabsLiveData;

    private final ToolbarTitleLiveData mToolbarTitleLiveData;
    private final MutableLiveData<Integer> mToolbarTitleMode;

    private final LiveData<Boolean> mHasHfpDeviceConnectedLiveData;

    private final LocalCallHandler mLocalCallHandler;

    public TelecomActivityViewModel(Application application) {
        super(application);
        mApplicationContext = application.getApplicationContext();

        mToolbarTitleMode = new MediatorLiveData<>();
        mToolbarTitleLiveData = new ToolbarTitleLiveData(mApplicationContext, mToolbarTitleMode);

        if (BluetoothAdapter.getDefaultAdapter() != null) {
            mRefreshTabsLiveData = new RefreshUiEvent(
                    UiBluetoothMonitor.get().getHfpDeviceListLiveData());
        }

        mHasHfpDeviceConnectedLiveData = UiBluetoothMonitor.get().hasHfpDeviceConnected();

        mLocalCallHandler = new LocalCallHandler(mApplicationContext);
    }

    /**
     * Returns the {@link LiveData} for the toolbar title, which provides the toolbar title
     * depending on the {@link com.android.car.dialer.R.attr#toolbarTitleMode}.
     */
    public LiveData<String> getToolbarTitle() {
        return mToolbarTitleLiveData;
    }

    /**
     * Returns the {@link MutableLiveData} of the toolbar title mode. The value should be set by the
     * {@link TelecomActivity}.
     */
    public MutableLiveData<Integer> getToolbarTitleMode() {
        return mToolbarTitleMode;
    }

    /**
     * Returns the live data which monitors whether to refresh Dialer.
     */
    public LiveData<Boolean> getRefreshTabsLiveData() {
        return mRefreshTabsLiveData;
    }

    /** Returns a {@link LiveData} which monitors if there are any connected HFP devices. */
    public LiveData<Boolean> hasHfpDeviceConnected() {
        return mHasHfpDeviceConnectedLiveData;
    }

    /** Returns the live data which monitors the ongoing call list. */
    public LiveData<List<Call>> getOngoingCallListLiveData() {
        return mLocalCallHandler.getOngoingCallListLiveData();
    }

    @Override
    protected void onCleared() {
        mLocalCallHandler.tearDown();
    }

    /**
     * This is an event live data to determine if the Ui needs to be refreshed.
     */
    @VisibleForTesting
    static class RefreshUiEvent extends SingleLiveEvent<Boolean> {
        private BluetoothDevice mBluetoothDevice;

        @VisibleForTesting
        RefreshUiEvent(LiveData<List<BluetoothDevice>> hfpDeviceListLiveData) {
            addSource(hfpDeviceListLiveData, v -> update(v));
        }

        private void update(List<BluetoothDevice> hfpDeviceList) {
            L.v(TAG, "HfpDeviceList update");
            if (mBluetoothDevice != null && !listContainsDevice(hfpDeviceList, mBluetoothDevice)) {
                setValue(true);
            }
            mBluetoothDevice = getFirstDevice(hfpDeviceList);
        }

        private boolean deviceListIsEmpty(@Nullable List<BluetoothDevice> hfpDeviceList) {
            return hfpDeviceList == null || hfpDeviceList.isEmpty();
        }

        private boolean listContainsDevice(@Nullable List<BluetoothDevice> hfpDeviceList,
                @NonNull BluetoothDevice device) {
            if (!deviceListIsEmpty(hfpDeviceList) && hfpDeviceList.contains(device)) {
                return true;
            }

            return false;
        }

        @Nullable
        private BluetoothDevice getFirstDevice(@Nullable List<BluetoothDevice> hfpDeviceList) {
            if (deviceListIsEmpty(hfpDeviceList)) {
                return null;
            } else {
                return hfpDeviceList.get(0);
            }
        }
    }
}
