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

import android.hardware.automotive.audiocontrol.V1_0.IAudioControl;
import android.hardware.automotive.audiocontrol.V2_0.IFocusListener;
import android.os.RemoteException;
import android.util.Log;

import androidx.annotation.Nullable;

import com.android.car.audio.CarAudioContext;
import com.android.internal.annotations.VisibleForTesting;

import java.io.PrintWriter;
import java.util.NoSuchElementException;
import java.util.Objects;

/**
 * Wrapper for IAudioControl@1.0.
 */
public final class AudioControlWrapperV1 implements AudioControlWrapper {
    private static final String TAG = AudioControlWrapperV1.class.getSimpleName();

    private IAudioControl mAudioControlV1;
    private AudioControlDeathRecipient mDeathRecipient;

    /**
     * Gets IAudioControl@1.0 service if registered.
     */
    public static @Nullable IAudioControl getService() {
        try {
            return IAudioControl.getService(true);
        } catch (RemoteException e) {
            throw new IllegalStateException("Failed to get IAudioControl@1.0 service", e);
        } catch (NoSuchElementException e) {
            return null;
        }
    }

    @VisibleForTesting
    AudioControlWrapperV1(IAudioControl audioControlV1) {
        mAudioControlV1 = Objects.requireNonNull(audioControlV1);
    }

    @Override
    public boolean supportsHalAudioFocus() {
        return false;
    }

    @Override
    public void registerFocusListener(IFocusListener focusListener) {
        throw new UnsupportedOperationException(
                "Focus listener is unsupported for IAudioControl@1.0");
    }

    @Override
    public void unregisterFocusListener() {
        throw new UnsupportedOperationException(
                "Focus listener is unsupported for IAudioControl@1.0");
    }

    @Override
    public void onAudioFocusChange(int usage, int zoneId, int focusChange) {
        throw new UnsupportedOperationException(
                "Focus listener is unsupported for IAudioControl@1.0");
    }

    @Override
    public void dump(String indent, PrintWriter writer) {
        writer.printf("%s*AudioControlWrapperV1*\n", indent);
    }

    @Override
    public void setFadeTowardFront(float value) {
        try {
            mAudioControlV1.setFadeTowardFront(value);
        } catch (RemoteException e) {
            Log.e(TAG, "setFadeTowardFront failed", e);
        }
    }

    @Override
    public void setBalanceTowardRight(float value) {
        try {
            mAudioControlV1.setBalanceTowardRight(value);
        } catch (RemoteException e) {
            Log.e(TAG, "setBalanceTowardRight failed", e);
        }
    }

    /**
     * Gets the bus associated with CarAudioContext.
     *
     * <p>This API is used along with car_volume_groups.xml to configure volume groups and routing.
     *
     * @param audioContext {@code CarAudioContext} to get a context for.
     * @return int bus number. Should be part of the prefix for the device's address. For example,
     * bus001_media would be bus 1.
     * @deprecated Volume and routing configuration has been replaced by
     * car_audio_configuration.xml. Starting with IAudioControl@V2.0, getBusForContext is no longer
     * supported.
     */
    @Deprecated
    public int getBusForContext(@CarAudioContext.AudioContext int audioContext) {
        try {
            return mAudioControlV1.getBusForContext(audioContext);
        } catch (RemoteException e) {
            Log.e(TAG, "Failed to query IAudioControl HAL to get bus for context", e);
            throw new IllegalStateException("Failed to query IAudioControl#getBusForContext", e);
        }
    }

    @Override
    public void linkToDeath(@Nullable AudioControlDeathRecipient deathRecipient) {
        try {
            mAudioControlV1.linkToDeath(this::serviceDied, 0);
            mDeathRecipient = deathRecipient;
        } catch (RemoteException e) {
            throw new IllegalStateException("Call to IAudioControl@1.0#linkToDeath failed", e);
        }
    }

    @Override
    public void unlinkToDeath() {
        try {
            mAudioControlV1.unlinkToDeath(this::serviceDied);
            mDeathRecipient = null;
        } catch (RemoteException e) {
            throw new IllegalStateException("Call to IAudioControl@1.0#unlinkToDeath failed", e);
        }
    }

    private void serviceDied(long cookie) {
        Log.w(TAG, "IAudioControl@1.0 died. Fetching new handle");
        mAudioControlV1 = AudioControlWrapperV1.getService();
        linkToDeath(mDeathRecipient);
        if (mDeathRecipient != null) {
            mDeathRecipient.serviceDied();
        }
    }


}
