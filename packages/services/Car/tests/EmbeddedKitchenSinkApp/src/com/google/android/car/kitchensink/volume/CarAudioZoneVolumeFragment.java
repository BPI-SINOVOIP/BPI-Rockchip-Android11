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

package com.google.android.car.kitchensink.volume;

import android.car.media.CarAudioManager;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.util.SparseIntArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ListView;

import androidx.fragment.app.Fragment;

import com.google.android.car.kitchensink.R;
import com.google.android.car.kitchensink.volume.VolumeTestFragment.CarAudioZoneVolumeInfo;

public final class CarAudioZoneVolumeFragment extends Fragment {
    private static final String TAG = "CarVolumeTest."
            + CarAudioZoneVolumeFragment.class.getSimpleName();
    private static final boolean DEBUG = true;

    private static final int MSG_VOLUME_CHANGED = 0;
    private static final int MSG_REQUEST_FOCUS = 1;
    private static final int MSG_FOCUS_CHANGED = 2;

    private final int mZoneId;
    private final CarAudioManager mCarAudioManager;
    private final AudioManager mAudioManager;
    private CarAudioZoneVolumeInfo[] mVolumeInfos =
            new CarAudioZoneVolumeInfo[0];
    private final Handler mHandler = new VolumeHandler();

    private CarAudioZoneVolumeAdapter mCarAudioZoneVolumeAdapter;
    private final SparseIntArray mGroupIdIndexMap = new SparseIntArray();

    public void sendChangeMessage() {
        mHandler.sendMessage(mHandler.obtainMessage(MSG_VOLUME_CHANGED));
    }

    private class VolumeHandler extends Handler {
        private AudioFocusListener mFocusListener;

        @Override
        public void handleMessage(Message msg) {
            if (DEBUG) {
                Log.d(TAG, "zone " + mZoneId + " handleMessage : " + getMessageName(msg));
            }
            switch (msg.what) {
                case MSG_VOLUME_CHANGED:
                    initVolumeInfo();
                    break;
                case MSG_REQUEST_FOCUS:
                    int groupId = msg.arg1;
                    if (mFocusListener != null) {
                        mAudioManager.abandonAudioFocus(mFocusListener);
                        mVolumeInfos[mGroupIdIndexMap.get(groupId)].mHasFocus = false;
                        mCarAudioZoneVolumeAdapter.notifyDataSetChanged();
                    }

                    mFocusListener = new AudioFocusListener(groupId);
                    mAudioManager.requestAudioFocus(mFocusListener, groupId,
                            AudioManager.AUDIOFOCUS_GAIN);
                    break;
                case MSG_FOCUS_CHANGED:
                    int focusGroupId = msg.arg1;
                    mVolumeInfos[mGroupIdIndexMap.get(focusGroupId)].mHasFocus = true;
                    mCarAudioZoneVolumeAdapter.refreshVolumes(mVolumeInfos);
                    break;
                default :
                    Log.wtf(TAG,"VolumeHandler handleMessage called with unknown message"
                            + msg.what);

            }
        }
    }

    public CarAudioZoneVolumeFragment(int zoneId, CarAudioManager carAudioManager,
            AudioManager audioManager) {
        mZoneId = zoneId;
        mCarAudioManager = carAudioManager;
        mAudioManager = audioManager;
    }

    @Override
    public View onCreateView(LayoutInflater inflater, ViewGroup container,
            Bundle savedInstanceState) {
        if (DEBUG) {
            Log.d(TAG, "onCreateView " + mZoneId);
        }
        View v = inflater.inflate(R.layout.zone_volume_tab, container, false);
        ListView volumeListView = v.findViewById(R.id.volume_list);
        mCarAudioZoneVolumeAdapter =
                new CarAudioZoneVolumeAdapter(getContext(), R.layout.volume_item, mVolumeInfos,
                        this);
        initVolumeInfo();
        volumeListView.setAdapter(mCarAudioZoneVolumeAdapter);
        return v;
    }

    void initVolumeInfo() {
        int volumeGroupCount = mCarAudioManager.getVolumeGroupCount(mZoneId);
        mVolumeInfos = new CarAudioZoneVolumeInfo[volumeGroupCount + 1];
        mGroupIdIndexMap.clear();
        CarAudioZoneVolumeInfo titlesInfo = new CarAudioZoneVolumeInfo();
        titlesInfo.mId = "Group id";
        titlesInfo.mCurrent = "Current";
        titlesInfo.mMax = "Max";
        mVolumeInfos[0] = titlesInfo;

        int i = 1;
        for (int groupId = 0; groupId < volumeGroupCount; groupId++) {
            CarAudioZoneVolumeInfo volumeInfo = new CarAudioZoneVolumeInfo();
            mGroupIdIndexMap.put(groupId, i);
            volumeInfo.mGroupId = groupId;
            volumeInfo.mId = String.valueOf(groupId);
            int current = mCarAudioManager.getGroupVolume(mZoneId, groupId);
            int max = mCarAudioManager.getGroupMaxVolume(mZoneId, groupId);
            volumeInfo.mCurrent = String.valueOf(current);
            volumeInfo.mMax = String.valueOf(max);

            mVolumeInfos[i] = volumeInfo;
            if (DEBUG)
            {
                Log.d(TAG, groupId + " max: " + volumeInfo.mMax + " current: "
                        + volumeInfo.mCurrent);
            }
            i++;
        }
        mCarAudioZoneVolumeAdapter.refreshVolumes(mVolumeInfos);
    }

    public void adjustVolumeByOne(int groupId, boolean up) {
        if (mCarAudioManager == null) {
            Log.e(TAG, "CarAudioManager is null");
            return;
        }
        int current = mCarAudioManager.getGroupVolume(mZoneId, groupId);
        int volume = current + (up ? 1 : -1);
        mCarAudioManager.setGroupVolume(mZoneId, groupId, volume,
                AudioManager.FLAG_SHOW_UI | AudioManager.FLAG_PLAY_SOUND);
        if (DEBUG) {
            Log.d(TAG, "Set group " + groupId + " volume " + volume + " in audio zone "
                    + mZoneId);
        }
        mHandler.sendMessage(mHandler.obtainMessage(MSG_VOLUME_CHANGED));
    }

    public void requestFocus(int groupId) {
        // Automatic volume change only works for primary audio zone.
        if (mZoneId == CarAudioManager.PRIMARY_AUDIO_ZONE) {
            mHandler.sendMessage(mHandler.obtainMessage(MSG_REQUEST_FOCUS, groupId));
        }
    }

    private class AudioFocusListener implements AudioManager.OnAudioFocusChangeListener {
        private final int mGroupId;
        AudioFocusListener(int groupId) {
            mGroupId = groupId;
        }
        @Override
        public void onAudioFocusChange(int focusChange) {
            if (focusChange == AudioManager.AUDIOFOCUS_GAIN) {
                mHandler.sendMessage(mHandler.obtainMessage(MSG_FOCUS_CHANGED, mGroupId, 0));
            } else {
                Log.e(TAG, "Audio focus request failed");
            }
        }
    }
}
