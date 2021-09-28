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

import android.content.Context;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.TextView;

import com.google.android.car.kitchensink.R;

public final class CarAudioInputAdapter
        extends ArrayAdapter<CarAudioZoneInputFragment.CarAudioAudioInputInfo> {

    private static final String TAG = "AUDIO.INPUT."
            + CarAudioInputAdapter.class.getSimpleName();
    private static final boolean DEBUG = true;
    private final Context mContext;
    private CarAudioZoneInputFragment.CarAudioAudioInputInfo[] mAudioDeviceInfos;
    private final int mLayoutResourceId;
    private CarAudioZoneInputFragment mFragment;

    public CarAudioInputAdapter(Context context,
            int layoutResourceId, CarAudioZoneInputFragment.CarAudioAudioInputInfo[]
            carInputDevicesInfos, CarAudioZoneInputFragment fragment) {
        super(context, layoutResourceId, carInputDevicesInfos);
        mFragment = fragment;
        mContext = context;
        mLayoutResourceId = layoutResourceId;
        mAudioDeviceInfos = carInputDevicesInfos;
    }

    @Override
    public View getView(int position, View convertView, ViewGroup parent) {
        if (DEBUG) {
            Log.d(TAG, "getView position " + position);
        }
        ViewHolder vh = new ViewHolder();
        if (convertView == null) {
            LayoutInflater inflater = LayoutInflater.from(mContext);
            convertView = inflater.inflate(mLayoutResourceId, parent, false);
            vh.mDeviceAddress = convertView.findViewById(R.id.input_device_address);
            vh.mPlayButton = convertView.findViewById(R.id.play_audio_input);
            vh.mStopButton = convertView.findViewById(R.id.stop_audio_input);
            vh.mPlayerState = convertView.findViewById(R.id.input_device_state);
            convertView.setTag(vh);
        } else {
            vh = (ViewHolder) convertView.getTag();
        }
        if (mAudioDeviceInfos[position] != null) {
            String deviceAddress = mAudioDeviceInfos[position].mDeviceAddress;
            vh.mDeviceAddress.setText(deviceAddress);
            if (position == 0) {
                vh.mPlayButton.setVisibility(View.INVISIBLE);
                vh.mStopButton.setVisibility(View.INVISIBLE);
                vh.mPlayerState.setVisibility(View.INVISIBLE);
            } else {
                vh.mPlayButton.setVisibility(View.VISIBLE);
                vh.mPlayButton.setOnClickListener((View v) -> playAudio(mAudioDeviceInfos[position]
                        .mDeviceAddress));
                vh.mStopButton.setVisibility(View.VISIBLE);
                vh.mStopButton.setOnClickListener((View v) -> stopAudio(mAudioDeviceInfos[position]
                        .mDeviceAddress));
                vh.mPlayerState.setVisibility(View.VISIBLE);
                vh.mPlayerState.setText(getPlayerStateStringResource(mAudioDeviceInfos[position]
                        .mPlayerState));
            }
        }
        return convertView;
    }

    private void playAudio(String deviceAddress) {
        mFragment.playAudio(deviceAddress);
    }

    private void stopAudio(String deviceAddress) {
        mFragment.stopAudio(deviceAddress);
    }

    private int getPlayerStateStringResource(int state) {
        switch (state) {
            case CarAudioZoneInputFragment.PLAYING_STATE:
                return R.string.player_started;
            case CarAudioZoneInputFragment.PAUSED_STATE:
                return R.string.player_paused;
            case CarAudioZoneInputFragment.DELAYED_STATE:
                return R.string.player_delayed;
            case CarAudioZoneInputFragment.STOPPED_STATE:
            default:
                return R.string.player_stopped;
        }
    }

    @Override
    public int getCount() {
        return mAudioDeviceInfos.length;
    }

    public void refreshAudioInputs(CarAudioZoneInputFragment.CarAudioAudioInputInfo[]
            carInputDevicesInfos) {
        mAudioDeviceInfos = carInputDevicesInfos;
    }

    private static final class ViewHolder {
        public TextView mDeviceAddress;
        public Button mPlayButton;
        public Button mStopButton;
        public TextView mPlayerState;
    }
}
