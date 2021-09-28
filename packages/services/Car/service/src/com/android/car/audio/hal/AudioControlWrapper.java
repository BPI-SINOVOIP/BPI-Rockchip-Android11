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

import android.hardware.automotive.audiocontrol.V2_0.IFocusListener;
import android.media.AudioAttributes.AttributeUsage;

import androidx.annotation.Nullable;

import java.io.PrintWriter;

/**
 * AudioControlWrapper wraps IAudioControl HAL interface, handling version specific support so that
 * the rest of CarAudioService doesn't need to know about it.
 */
public interface AudioControlWrapper {

    /**
     * Closes the focus listener that's registered on the AudioControl HAL
     */
    void unregisterFocusListener();

    /**
     * Indicates if HAL can support making and abandoning audio focus requests.
     */
    boolean supportsHalAudioFocus();

    /**
     * Registers listener for HAL audio focus requests with IAudioControl. Only works if
     * {@code supportsHalAudioFocus} returns true.
     *
     * @param focusListener the listener to register on the IAudioControl HAL.
     */
    void registerFocusListener(IFocusListener focusListener);

    /**
     * Notifies HAL of change in audio focus for a request it has made.
     *
     * @param usage that the request is associated with.
     * @param zoneId for the audio zone that the request is associated with.
     * @param focusChange the new status of the request.
     */
    void onAudioFocusChange(@AttributeUsage int usage, int zoneId, int focusChange);

    /**
     * dumps the current state of the AudioControlWrapper
     *
     * @param indent indent to append to each new line
     * @param writer stream to write current state
     */
    void dump(String indent, PrintWriter writer);

    /**
     * Sets the fade for the vehicle.
     *
     * @param value to set for the fade. Positive is towards front.
     */
    void setFadeTowardFront(float value);

    /**
     * Sets the balance value for the vehicle.
     *
     * @param value to set for the balance. Positive is towards the right.
     */
    void setBalanceTowardRight(float value);

    /**
     * Registers recipient to be notified if AudioControl HAL service dies.
     * @param deathRecipient to be notified upon HAL service death.
     */
    void linkToDeath(@Nullable AudioControlDeathRecipient deathRecipient);

    /**
     * Unregisters recipient for AudioControl HAL service death.
     */
    void unlinkToDeath();

    /**
     * Recipient to be notified upon death of AudioControl HAL.
     */
    interface AudioControlDeathRecipient {
        /**
         * Called if AudioControl HAL dies.
         */
        void serviceDied();
    }
}
