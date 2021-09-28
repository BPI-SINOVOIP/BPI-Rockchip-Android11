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
package com.android.car.audio;

import android.car.Car;
import android.car.media.CarAudioManager;
import android.content.pm.PackageManager;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.os.Bundle;

import androidx.annotation.NonNull;

import com.android.car.audio.CarAudioContext.AudioContext;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.List;
import java.util.Objects;

final class FocusEntry {
    private final AudioFocusInfo mAudioFocusInfo;
    private final int mAudioContext;

    private final List<FocusEntry> mBlockers;
    private final PackageManager mPackageManager;
    private boolean mIsDucked;

    FocusEntry(@NonNull AudioFocusInfo audioFocusInfo, @AudioContext int context,
            @NonNull PackageManager packageManager) {
        Objects.requireNonNull(audioFocusInfo, "AudioFocusInfo cannot be null");
        Objects.requireNonNull(packageManager, "PackageManager cannot be null");
        mAudioFocusInfo = audioFocusInfo;
        mAudioContext = context;
        mBlockers = new ArrayList<>();
        mPackageManager = packageManager;
    }

    @AudioContext
    int getAudioContext() {
        return mAudioContext;
    }

    AudioFocusInfo getAudioFocusInfo() {
        return mAudioFocusInfo;
    }

    boolean isUnblocked() {
        return mBlockers.isEmpty();
    }

    void addBlocker(FocusEntry blocker) {
        mBlockers.add(blocker);
    }

    void removeBlocker(FocusEntry blocker) {
        mBlockers.remove(blocker);
    }

    String getClientId() {
        return mAudioFocusInfo.getClientId();
    }

    boolean isDucked() {
        return mIsDucked;
    }

    void setDucked(boolean ducked) {
        mIsDucked = ducked;
    }

    boolean wantsPauseInsteadOfDucking() {
        return (mAudioFocusInfo.getFlags() & AudioManager.AUDIOFOCUS_FLAG_PAUSES_ON_DUCKABLE_LOSS)
                != 0;
    }

    boolean receivesDuckEvents() {
        Bundle bundle = mAudioFocusInfo.getAttributes().getBundle();

        if (bundle == null) {
            return false;
        }

        if (!bundle.getBoolean(CarAudioManager.AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS)) {
            return false;
        }

        return (mPackageManager.checkPermission(
                Car.PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS,
                mAudioFocusInfo.getPackageName())
                == PackageManager.PERMISSION_GRANTED);
    }

    String getUsageName() {
        return mAudioFocusInfo.getAttributes().usageToString();
    }

    public void dump(String indent, PrintWriter writer) {
        writer.printf("%s%s - %s\n", indent, getClientId(), getUsageName());
        // Prints in single line
        writer.printf("%s\tReceives Duck Events: %b, ", indent, receivesDuckEvents());
        writer.printf("Wants Pause Instead of Ducking: %b, ", wantsPauseInsteadOfDucking());
        writer.printf("Is Ducked: %b\n", isDucked());
    }
}
