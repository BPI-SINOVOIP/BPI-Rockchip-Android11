/*
 * Copyright (C) 2018 The Android Open Source Project
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

package com.android.tv;

import android.content.Context;
import android.media.tv.TvTrackInfo;
import android.media.tv.TvView;
import android.util.AttributeSet;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.shadows.ShadowView;

// TODO(b/78304522): move this class to robolectric
/** Shadow class of {@link TvView}. */
@Implements(TvView.class)
public class ShadowTvView extends ShadowView {
    public Map<Integer, String> mSelectedTracks = new HashMap<>();
    public Map<Integer, List<TvTrackInfo>> mTracks = new HashMap<>();
    public MainActivity.MyOnTuneListener listener;
    public TvView.TvInputCallback mCallback;
    public boolean mAudioTrackCountChanged;

    @Implementation
    public void __constructor__(Context context) {
    }

    @Implementation
    public void __constructor__(Context context, AttributeSet attrs) {
    }

    @Override
    public void __constructor__(Context context, AttributeSet attrs, int defStyleAttr) {
    }

    @Implementation
    public List<TvTrackInfo> getTracks(int type) {
        return mTracks.get(type);
    }

    @Implementation
    public void selectTrack(int type, String trackId) {
        mSelectedTracks.put(type, trackId);
        int infoIndex = findTrackIndex(type, trackId);
        // for some drivers, audio track count is set to 0 until the corresponding track is
        // selected. Here we replace the track with another one whose audio track count is non-zero
        // to test this case.
        if (mAudioTrackCountChanged) {
            replaceTrack(type, infoIndex);
        }
        mCallback.onTrackSelected("fakeInputId", type, trackId);
    }

    @Implementation
    public String getSelectedTrack(int type) {
        return mSelectedTracks.get(type);
    }

    @Implementation
    public void setCallback(TvView.TvInputCallback callback) {
        mCallback = callback;
    }

    private int findTrackIndex(int type, String trackId) {
        List<TvTrackInfo> tracks = mTracks.get(type);
        if (tracks == null) {
            return -1;
        }
        for (int i = 0; i < tracks.size(); i++) {
            TvTrackInfo info = tracks.get(i);
            if (info.getId().equals(trackId)) {
                return i;
            }
        }
        return -1;
    }

    private void replaceTrack(int type, int trackIndex) {
        if (trackIndex >= 0) {
            TvTrackInfo info = mTracks.get(type).get(trackIndex);
            info = new TvTrackInfo
                    .Builder(info.getType(), info.getId())
                    .setLanguage(info.getLanguage())
                    .setAudioChannelCount(info.getAudioChannelCount() + 2)
                    .build();
            mTracks.get(type).set(trackIndex, info);
        }
    }
}
