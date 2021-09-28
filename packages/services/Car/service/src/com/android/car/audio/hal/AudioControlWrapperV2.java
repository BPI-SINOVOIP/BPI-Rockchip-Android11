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

import android.hardware.automotive.audiocontrol.V2_0.IAudioControl;
import android.hardware.automotive.audiocontrol.V2_0.ICloseHandle;
import android.hardware.automotive.audiocontrol.V2_0.IFocusListener;
import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.Nullable;

import java.io.PrintWriter;
import java.util.NoSuchElementException;
import java.util.Objects;

/**
 * Wrapper for IAudioControl@2.0.
 */
public final class AudioControlWrapperV2 implements AudioControlWrapper {
    private static final String TAG = AudioControlWrapperV2.class.getSimpleName();

    private IAudioControl mAudioControlV2;

    private AudioControlDeathRecipient mDeathRecipient;
    private ICloseHandle mCloseHandle;

    public static @Nullable IAudioControl getService() {
        try {
            return IAudioControl.getService(true);
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to get IAudioControl@2.0 service", e);
        } catch (NoSuchElementException e) {
            return null;
        }
    }

    AudioControlWrapperV2(IAudioControl audioControlV2) {
        mAudioControlV2 = Objects.requireNonNull(audioControlV2);
    }

    @Override
    public void unregisterFocusListener() {
        if (mCloseHandle != null) {
            try {
                mCloseHandle.close();
            } catch (RemoteException e) {
                Log.e(TAG, "Failed to close focus listener", e);
            } finally {
                mCloseHandle = null;
            }
        }
    }

    @Override
    public boolean supportsHalAudioFocus() {
        return true;
    }

    @Override
    public void registerFocusListener(IFocusListener focusListener) {
        Log.d(TAG, "Registering focus listener on AudioControl HAL");
        try {
            mCloseHandle = mAudioControlV2.registerFocusListener(focusListener);
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to register focus listener");
            throw new IllegalStateException("IAudioControl#registerFocusListener failed", e);
        }
    }

    @Override
    public void onAudioFocusChange(@AttributeUsage int usage, int zoneId, int focusChange) {
        if (Log.isLoggable(TAG, Log.DEBUG)) {
            Log.d(TAG, "onAudioFocusChange: usage " + AudioAttributes.usageToString(usage)
                    + ", zoneId " + zoneId + ", focusChange " + focusChange);
        }
        try {
            mAudioControlV2.onAudioFocusChange(usage, zoneId, focusChange);
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to query IAudioControl#onAudioFocusChange", e);
        }
    }

    /**
     * Dumps the current state of the {@code AudioControlWrapperV2}.
     *
     * @param indent indent to append to each new line.
     * @param writer stream to write current state.
     */
    @Override
    public void dump(String indent, PrintWriter writer) {
        writer.printf("%s*AudioControlWrapperV2*\n", indent);
        writer.printf("%s\tFocus listener registered on HAL? %b", indent, (mCloseHandle != null));
    }

    @Override
    public void setFadeTowardFront(float value) {
        try {
            mAudioControlV2.setFadeTowardFront(value);
        } catch (RemoteException e) {
            Log.e(TAG, "setFadeTowardFront failed", e);
        }
    }

    @Override
    public void setBalanceTowardRight(float value) {
        try {
            mAudioControlV2.setBalanceTowardRight(value);
        } catch (RemoteException e) {
            Log.e(TAG, "setBalanceTowardRight failed", e);
        }
    }

    @Override
    public void linkToDeath(@Nullable AudioControlDeathRecipient deathRecipient) {
        try {
            mAudioControlV2.linkToDeath(this::serviceDied, 0);
            mDeathRecipient = deathRecipient;
        } catch (RemoteException e) {
            throw new IllegalStateException("Call to IAudioControl@2.0#linkToDeath failed", e);
        }
    }

    @Override
    public void unlinkToDeath() {
        try {
            mAudioControlV2.unlinkToDeath(this::serviceDied);
            mDeathRecipient = null;
        } catch (RemoteException e) {
            throw new IllegalStateException("Call to IAudioControl@2.0#unlinkToDeath failed", e);
        }
    }

    private void serviceDied(long cookie) {
        Log.w(TAG, "IAudioControl@2.0 died. Fetching new handle");
        mAudioControlV2 = AudioControlWrapperV2.getService();
        linkToDeath(mDeathRecipient);
        if (mDeathRecipient != null) {
            mDeathRecipient.serviceDied();
        }
    }
}
