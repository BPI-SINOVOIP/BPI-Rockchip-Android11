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

import android.car.media.CarAudioManager;
import android.car.media.CarAudioPatchHandle;
import android.media.AudioAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.annotation.NonNull;
import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;

import javax.annotation.concurrent.GuardedBy;

public final class CarAudioZoneInputFragment extends Fragment {
    private static final String TAG = "CAR.AUDIO.INPUT."
            + CarAudioZoneInputFragment.class.getSimpleName();
    private static final boolean DEBUG = true;

    static final int PLAYING_STATE = 0;
    static final int PAUSED_STATE = 1;
    static final int STOPPED_STATE = 2;
    static final int DELAYED_STATE = 3;

    private final Object mLock = new Object();
    private final int mAudioZoneId;
    private final CarAudioManager mCarAudioManager;
    private final AudioManager mAudioManager;
    @GuardedBy("mLock")
    private final Map<String, CarAudioAudioInputInfo> mAudioDeviceInfoMap = new HashMap<>();
    @GuardedBy("mLock")
    private String mActiveInputAddress;
    @GuardedBy("mLock")
    private CarAudioPatchHandle mAudioPatch;
    @GuardedBy("mLock")
    private AudioFocusRequest mAudioFocusRequest;
    @GuardedBy("mLock")
    private InputAudioFocusListener mAudioFocusListener;

    private CarAudioInputAdapter mCarAudioInputAdapter;
    private CarAudioAudioInputInfo[] mCarInputDevicesInfos = new CarAudioAudioInputInfo[0];
    private final AudioAttributes mAudioAttributes;

    public CarAudioZoneInputFragment(int audioZoneId, CarAudioManager carAudioManager,
            AudioManager audioManager) {
        mAudioZoneId = audioZoneId;
        mCarAudioManager = carAudioManager;
        mAudioManager = audioManager;
        mAudioAttributes = new AudioAttributes.Builder()
                .setUsage(AudioAttributes.USAGE_MEDIA)
                .build();
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        if (DEBUG) {
            Log.d(TAG, "onCreateView " + mAudioZoneId);
        }
        View view = inflater.inflate(R.layout.zone_input_tab, container, false);
        ListView audiInputListView = view.findViewById(R.id.input_list);
        mCarAudioInputAdapter =
                new CarAudioInputAdapter(getContext(), R.layout.audio_input_item,
                        mCarInputDevicesInfos, this);
        initAudioInputInfo(mAudioZoneId);
        audiInputListView.setAdapter(mCarAudioInputAdapter);
        return view;
    }

    @Override
    public void onDestroyView() {
        synchronized (mLock) {
            if (mActiveInputAddress != null) {
                stopAudio(mActiveInputAddress);
            }
        }
        super.onDestroyView();
    }

    void initAudioInputInfo(int audioZoneId) {
        List<AudioDeviceInfo> inputDevices =
                mCarAudioManager.getInputDevicesForZoneId(audioZoneId);
        mCarInputDevicesInfos = new CarAudioAudioInputInfo[inputDevices.size() + 1];
        CarAudioAudioInputInfo titlesInfo = new CarAudioAudioInputInfo();
        titlesInfo.mDeviceAddress = "Device Address";
        titlesInfo.mPlayerState = STOPPED_STATE;
        mCarInputDevicesInfos[0] = titlesInfo;

        synchronized (mLock) {
            for (int index = 0; index < inputDevices.size(); index++) {
                AudioDeviceInfo info = inputDevices.get(index);
                CarAudioAudioInputInfo audioInput = new CarAudioAudioInputInfo();
                audioInput.mCarAudioZone = mAudioZoneId;
                audioInput.mDeviceAddress = info.getAddress();
                audioInput.mPlayerState = STOPPED_STATE;
                mCarInputDevicesInfos[index + 1] = audioInput;

                if (DEBUG) {
                    Log.d(TAG, audioInput.toString());
                }
                mAudioDeviceInfoMap.put(info.getAddress(), audioInput);
            }
        }
        mCarAudioInputAdapter.refreshAudioInputs(mCarInputDevicesInfos);
    }

    public void playAudio(@NonNull String deviceAddress) {
        Objects.requireNonNull(deviceAddress);
        synchronized (mLock) {
            if (deviceAddress.equals(mActiveInputAddress)) {
                if (DEBUG) {
                    Log.d(TAG, "Audio already playing");
                }
                return;
            }
            if (mActiveInputAddress != null) {
                mAudioManager.abandonAudioFocusRequest(mAudioFocusRequest);
                stopAudioLocked();
            }

            mAudioFocusListener = new InputAudioFocusListener();
            mAudioFocusRequest = new AudioFocusRequest
                    .Builder(AudioManager.AUDIOFOCUS_GAIN)
                    .setAudioAttributes(mAudioAttributes)
                    .setOnAudioFocusChangeListener(mAudioFocusListener)
                    .setForceDucking(false)
                    .setWillPauseWhenDucked(false)
                    .setAcceptsDelayedFocusGain(true)
                    .build();

            int audioFocusRequestResults = mAudioManager.requestAudioFocus(mAudioFocusRequest);
            if (audioFocusRequestResults == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
                mActiveInputAddress = deviceAddress;
                startAudioLocked();
                return;
            } else if (audioFocusRequestResults == AudioManager.AUDIOFOCUS_REQUEST_DELAYED) {
                // Keep the device but will start the audio once we get the focus gain
                mActiveInputAddress = deviceAddress;
                mAudioDeviceInfoMap.get(deviceAddress).mPlayerState = DELAYED_STATE;
                mCarAudioInputAdapter.notifyDataSetChanged();
                return;
            }

            // Focus was rejected
            mAudioFocusRequest = null;
            mAudioFocusListener = null;
        }
        mCarAudioInputAdapter.notifyDataSetChanged();
    }

    public void stopAudio(@NonNull String deviceAddress) {
        Objects.requireNonNull(deviceAddress);
        synchronized (mLock) {
            if (deviceAddress.equals(mActiveInputAddress)) {
                mAudioManager.abandonAudioFocusRequest(mAudioFocusRequest);
                stopAudioLocked();
            }
        }
    }

    private void startAudioLocked() {
        if (mAudioPatch != null) {
            if (DEBUG) {
                Log.d(TAG, "Audio already playing");
            }
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "Starting audio input " + mActiveInputAddress);
        }
        mAudioPatch = mCarAudioManager.createAudioPatch(mActiveInputAddress,
                AudioAttributes.USAGE_MEDIA, 0);
        mAudioDeviceInfoMap.get(mActiveInputAddress).mPlayerState = PLAYING_STATE;
        mCarAudioInputAdapter.notifyDataSetChanged();
    }

    private void pauseAudioLocked() {
        if (mAudioPatch == null) {
            if (DEBUG) {
                Log.d(TAG, "Audio already paused");
            }
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "Pausing audio input " + mActiveInputAddress);
        }
        mCarAudioManager.releaseAudioPatch(mAudioPatch);
        mAudioPatch = null;
        mAudioDeviceInfoMap.get(mActiveInputAddress).mPlayerState = PAUSED_STATE;
        mCarAudioInputAdapter.notifyDataSetChanged();
    }

    private void stopAudioLocked() {
        if (mAudioPatch == null) {
            if (DEBUG) {
                Log.d(TAG, "Audio already stopped");
            }
            return;
        }
        if (DEBUG) {
            Log.d(TAG, "Stopping audio input " + mActiveInputAddress);
        }
        mCarAudioManager.releaseAudioPatch(mAudioPatch);
        mAudioDeviceInfoMap.get(mActiveInputAddress).mPlayerState = STOPPED_STATE;
        mAudioPatch = null;
        mAudioFocusRequest = null;
        mAudioFocusListener = null;
        mActiveInputAddress = null;

        mCarAudioInputAdapter.notifyDataSetChanged();
    }

    private final class InputAudioFocusListener implements AudioManager.OnAudioFocusChangeListener {

        @Override
        public void onAudioFocusChange(int focusChange) {
            if (DEBUG) {
                Log.d(TAG, "InputAudioFocusListener focus change:" + focusChange);
            }
            synchronized (mLock) {
                switch (focusChange) {
                    case AudioManager.AUDIOFOCUS_GAIN:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE:
                    case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
                        startAudioLocked();
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
                    case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
                        pauseAudioLocked();
                        break;
                    case AudioManager.AUDIOFOCUS_LOSS:
                    default:
                        stopAudioLocked();
                        break;
                }
            }
        }
    }

    public static class CarAudioAudioInputInfo {
        public int mCarAudioZone;
        public String mDeviceAddress;
        public int mPlayerState;

        @Override
        public String toString() {
            return "Device address " + mDeviceAddress + ", Audio zone id " + mCarAudioZone;
        }
    }
}
