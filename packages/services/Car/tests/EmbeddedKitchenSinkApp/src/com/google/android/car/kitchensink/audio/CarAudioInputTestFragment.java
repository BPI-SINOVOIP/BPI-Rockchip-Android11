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

package com.google.android.car.kitchensink.audio;

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.car.Car;
import android.car.media.CarAudioManager;
import android.content.Context;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;

import androidx.fragment.app.Fragment;
import androidx.viewpager.widget.ViewPager;

import com.google.android.car.kitchensink.R;
import com.google.android.material.tabs.TabLayout;

import java.util.List;

public class CarAudioInputTestFragment extends Fragment {
    private static final String TAG = "CAR.AUDIO.INPUT.KS";
    private static final boolean DEBUG = true;


    private Handler mHandler;
    private Context mContext;

    private Car mCar;
    private AudioManager mAudioManager;
    private CarAudioManager mCarAudioManager;
    private TabLayout mZonesTabLayout;
    private CarAudioZoneInputTabAdapter mInputAudioZoneAdapter;
    private ViewPager mViewPager;

    private void connectCar() {
        mContext = getContext();
        mHandler = new Handler(Looper.getMainLooper());
        mCar = Car.createCar(mContext, /* handler= */ null,
                Car.CAR_WAIT_TIMEOUT_WAIT_FOREVER, mCarServiceLifecycleListener);
    }

    private Car.CarServiceLifecycleListener mCarServiceLifecycleListener = (car, ready) -> {
        if (!ready) {
            if (DEBUG) {
                Log.d(TAG, "Disconnect from Car Service");
            }
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "Connected to Car Service");
        }
        mCarAudioManager = (CarAudioManager) car.getCarManager(Car.AUDIO_SERVICE);

        mAudioManager = mContext.getSystemService(AudioManager.class);
    };


    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container, Bundle bundle) {
        Log.i(TAG, "onCreateView");
        View view = inflater.inflate(R.layout.audio_input, container, false);
        connectCar();
        return view;
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        Log.i(TAG, "onViewCreated ");
        mZonesTabLayout = view.findViewById(R.id.zones_input_tab);
        mViewPager = (ViewPager) view.findViewById(R.id.zones_input_view_pager);

        mInputAudioZoneAdapter = new CarAudioZoneInputTabAdapter(getChildFragmentManager());
        mViewPager.setAdapter(mInputAudioZoneAdapter);
        initInputInfo();
        mZonesTabLayout.setupWithViewPager(mViewPager);
    }

    @Override
    public void onDestroyView() {
        Log.i(TAG, "onDestroyView");

        if (mCar != null && mCar.isConnected()) {
            mCar.disconnect();
            mCar = null;
        }
        super.onDestroyView();
    }

    private void initInputInfo() {
        if (!mCarAudioManager.isDynamicRoutingEnabled()) {
            return;
        }
        List<Integer> audioZoneList = mCarAudioManager.getAudioZoneIds();
        for (int audioZoneId : audioZoneList) {
            if (mCarAudioManager.getInputDevicesForZoneId(audioZoneId).isEmpty()) {
                if (DEBUG) {
                    Log.d(TAG, "Audio Zone " + audioZoneId + " has no input devices");
                }
                continue;
            }
            addAudioZoneInputDevices(audioZoneId);
        }
        mInputAudioZoneAdapter.notifyDataSetChanged();
    }

    private void addAudioZoneInputDevices(int audioZoneId) {
        String title = "Audio Zone " + audioZoneId;
        if (DEBUG) {
            Log.d(TAG, title + " adding devices");
        }
        CarAudioZoneInputFragment fragment = new CarAudioZoneInputFragment(audioZoneId,
                mCarAudioManager, mAudioManager);

        mZonesTabLayout.addTab(mZonesTabLayout.newTab().setText(title));
        mInputAudioZoneAdapter.addFragment(fragment, title);
    }
}
