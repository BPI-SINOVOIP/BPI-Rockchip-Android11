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

package com.android.car.audio.hal;

import static android.media.AudioManager.AUDIOFOCUS_LOSS;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_FAILED;
import static android.media.AudioManager.AUDIOFOCUS_REQUEST_GRANTED;

import android.car.media.CarAudioManager;
import android.hardware.automotive.audiocontrol.V2_0.IFocusListener;
import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
import android.os.Bundle;
import android.os.RemoteException;
import android.util.Log;
import android.util.SparseArray;

import androidx.annotation.NonNull;

import com.android.internal.annotations.GuardedBy;
import com.android.internal.util.Preconditions;

import java.io.PrintWriter;
import java.util.Objects;

/**
 * Manages focus requests from the HAL on a per-zone per-usage basis
 */
public final class HalAudioFocus extends IFocusListener.Stub {
    private static final String TAG = HalAudioFocus.class.getSimpleName();

    private final AudioManager mAudioManager;
    private final AudioControlWrapper mAudioControlWrapper;

    private final Object mLock = new Object();

    // Map of Maps. Top level keys are ZoneIds. Second level keys are usages.
    // Values are HalAudioFocusRequests
    @GuardedBy("mImplLock")
    private final SparseArray<SparseArray<HalAudioFocusRequest>> mHalFocusRequestsByZoneAndUsage;

    public HalAudioFocus(@NonNull AudioManager audioManager,
            @NonNull AudioControlWrapper audioControlWrapper,
            @NonNull int[] audioZoneIds) {
        mAudioManager = Objects.requireNonNull(audioManager);
        mAudioControlWrapper = Objects.requireNonNull(audioControlWrapper);
        Objects.requireNonNull(audioZoneIds);

        mHalFocusRequestsByZoneAndUsage = new SparseArray<>(audioZoneIds.length);
        for (int zoneId : audioZoneIds) {
            mHalFocusRequestsByZoneAndUsage.append(zoneId, new SparseArray<>());
        }
    }

    /**
     * Registers {@code IFocusListener} on {@code AudioControlWrapper} to receive HAL audio focus
     * request and abandon calls.
     */
    public void registerFocusListener() {
        mAudioControlWrapper.registerFocusListener(this);
    }

    /**
     * Unregisters {@code IFocusListener} from {@code AudioControlWrapper}.
     */
    public void unregisterFocusListener() {
        mAudioControlWrapper.unregisterFocusListener();
    }

    @Override
    public void requestAudioFocus(@AttributeUsage int usage, int zoneId, int focusGain) {
        Preconditions.checkArgument(mHalFocusRequestsByZoneAndUsage.contains(zoneId),
                "Invalid zoneId %d provided in requestAudioFocus", zoneId);
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Requesting focus gain " + focusGain + " with usage "
                    + AudioAttributes.usageToString(usage) + " and zoneId " + zoneId);
        }
        synchronized (mLock) {
            HalAudioFocusRequest currentRequest = mHalFocusRequestsByZoneAndUsage.get(zoneId).get(
                    usage);
            if (currentRequest != null) {
                if (Log.isLoggable(TAG, Log.DEBUG)) {
                    Log.d(TAG, "A request already exists for zoneId " + zoneId + " and usage "
                            + usage);
                }
                mAudioControlWrapper.onAudioFocusChange(usage, zoneId, currentRequest.mFocusStatus);
            } else {
                makeAudioFocusRequestLocked(usage, zoneId, focusGain);
            }
        }
    }

    @Override
    public void abandonAudioFocus(int usage, int zoneId) throws RemoteException {
        Preconditions.checkArgument(mHalFocusRequestsByZoneAndUsage.contains(zoneId),
                "Invalid zoneId %d provided in abandonAudioFocus", zoneId);
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "Abandoning focus with usage " + AudioAttributes.usageToString(usage)
                    + " for zoneId " + zoneId);
        }
        synchronized (mLock) {
            abandonAudioFocusLocked(usage, zoneId);
        }
    }

    /**
     * Clear out all existing focus requests. Called when HAL dies.
     */
    public void reset() {
        Log.d(TAG, "Resetting HAL Audio Focus requests");
        synchronized (mLock) {
            for (int i = 0; i < mHalFocusRequestsByZoneAndUsage.size(); i++) {
                int zoneId = mHalFocusRequestsByZoneAndUsage.keyAt(i);
                SparseArray<HalAudioFocusRequest> requestsByUsage =
                        mHalFocusRequestsByZoneAndUsage.valueAt(i);
                int usageCount = requestsByUsage.size();
                for (int j = 0; j < usageCount; j++) {
                    int usage = requestsByUsage.keyAt(j);
                    abandonAudioFocusLocked(usage, zoneId);
                }
            }
        }
    }

    /**
     * dumps the current state of the HalAudioFocus
     *
     * @param indent indent to append to each new line
     * @param writer stream to write current state
     */
    public void dump(String indent, PrintWriter writer) {
        writer.printf("%s*HalAudioFocus*\n", indent);

        writer.printf("%s\tCurrent focus requests:\n", indent);
        for (int i = 0; i < mHalFocusRequestsByZoneAndUsage.size(); i++) {
            int zoneId = mHalFocusRequestsByZoneAndUsage.keyAt(i);
            writer.printf("%s\t\tZone %s:\n", indent, zoneId);

            SparseArray<HalAudioFocusRequest> requestsByUsage =
                    mHalFocusRequestsByZoneAndUsage.valueAt(i);
            for (int j = 0; j < requestsByUsage.size(); j++) {
                int usage = requestsByUsage.keyAt(j);
                HalAudioFocusRequest request = requestsByUsage.valueAt(j);
                writer.printf("%s\t\t\t%s - focusGain: %s\n", indent,
                        AudioAttributes.usageToString(usage), request.mFocusStatus);
            }
        }
    }

    private void abandonAudioFocusLocked(int usage, int zoneId) {
        HalAudioFocusRequest currentRequest = mHalFocusRequestsByZoneAndUsage.get(zoneId)
                .removeReturnOld(usage);

        if (currentRequest == null) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "No focus to abandon for usage " + AudioAttributes.usageToString(usage)
                        + " and zoneId " + zoneId);
            }
            return;
        }

        int result = mAudioManager.abandonAudioFocusRequest(currentRequest.mAudioFocusRequest);
        if (result == AUDIOFOCUS_REQUEST_GRANTED) {
            if (Log.isLoggable(TAG, Log.DEBUG)) {
                Log.d(TAG, "Abandoned focus for usage " + AudioAttributes.usageToString(usage)
                        + "and zoneId " + zoneId);
            }
            mAudioControlWrapper.onAudioFocusChange(usage, zoneId, AUDIOFOCUS_LOSS);
        } else {
            Log.w(TAG,
                    "Failed to abandon focus for usage " + AudioAttributes.usageToString(usage)
                            + " and zoneId " + zoneId);
        }
    }

    private AudioAttributes generateAudioAttributes(int usage, int zoneId) {
        AudioAttributes.Builder builder = new AudioAttributes.Builder();
        Bundle bundle = new Bundle();
        bundle.putInt(CarAudioManager.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID, zoneId);
        builder.addBundle(bundle);

        if (AudioAttributes.isSystemUsage(usage)) {
            builder.setSystemUsage(usage);
        } else {
            builder.setUsage(usage);
        }
        return builder.build();
    }

    private AudioFocusRequest generateFocusRequestLocked(int usage, int zoneId, int focusGain) {
        AudioAttributes attributes = generateAudioAttributes(usage, zoneId);
        return new AudioFocusRequest.Builder(focusGain)
                .setAudioAttributes(attributes)
                .setOnAudioFocusChangeListener((int focusChange) -> {
                    onAudioFocusChange(usage, zoneId, focusChange);
                })
                .build();
    }

    private void onAudioFocusChange(int usage, int zoneId, int focusChange) {
        synchronized (mLock) {
            HalAudioFocusRequest currentRequest = mHalFocusRequestsByZoneAndUsage.get(zoneId).get(
                    usage);
            if (currentRequest != null) {
                if (focusChange == AUDIOFOCUS_LOSS) {
                    mHalFocusRequestsByZoneAndUsage.get(zoneId).remove(usage);
                } else {
                    currentRequest.mFocusStatus = focusChange;
                }
                mAudioControlWrapper.onAudioFocusChange(usage, zoneId, focusChange);
            }

        }
    }

    private void makeAudioFocusRequestLocked(@AttributeUsage int usage, int zoneId, int focusGain) {
        AudioFocusRequest audioFocusRequest = generateFocusRequestLocked(usage, zoneId, focusGain);

        int requestResult = mAudioManager.requestAudioFocus(audioFocusRequest);

        int resultingFocusGain = focusGain;

        if (requestResult == AUDIOFOCUS_REQUEST_GRANTED) {
            HalAudioFocusRequest halAudioFocusRequest = new HalAudioFocusRequest(audioFocusRequest,
                    focusGain);
            mHalFocusRequestsByZoneAndUsage.get(zoneId).append(usage, halAudioFocusRequest);
        } else if (requestResult == AUDIOFOCUS_REQUEST_FAILED) {
            resultingFocusGain = AUDIOFOCUS_LOSS;
        } else if (requestResult == AUDIOFOCUS_REQUEST_DELAYED) {
            Log.w(TAG, "Delayed result for request with usage "
                    + AudioAttributes.usageToString(usage) + ", zoneId " + zoneId
                    + ", and focusGain " + focusGain);
            resultingFocusGain = AUDIOFOCUS_LOSS;
        }

        mAudioControlWrapper.onAudioFocusChange(usage, zoneId, resultingFocusGain);
    }

    private final class HalAudioFocusRequest {
        final AudioFocusRequest mAudioFocusRequest;

        int mFocusStatus;

        HalAudioFocusRequest(AudioFocusRequest audioFocusRequest, int focusStatus) {
            mAudioFocusRequest = audioFocusRequest;
            mFocusStatus = focusStatus;
        }
    }
}
