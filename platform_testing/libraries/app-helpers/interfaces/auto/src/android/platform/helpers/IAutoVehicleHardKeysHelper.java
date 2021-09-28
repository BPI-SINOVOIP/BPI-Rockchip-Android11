/*
 * Copyright (C) 2019 The Android Open Source Project
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

package android.platform.helpers;

public interface IAutoVehicleHardKeysHelper extends IAppHelper {

    /** Enum class for volume types */
    enum VolumeType {
        Media,
        Ring,
        Notification,
        Navigation,
    }

    /** Enum class for driving state */
    enum DrivingState {
        UNKNOWN,
        MOVING,
        IDLING,
        PARKED,
    }

    /**
     * Setup expectations: incoming call in progress.
     *
     * Recieve phone call
     */
    void pressRecieveCallKey();

    /**
     * Setup expectations: The app is open and there is an ongoing call.
     *
     * End call using hardkey.
     */
    void pressEndCallKey();

    /**
     * Setup expectations: The Media app is active.
     *
     * Change media track using hardkey.
     */
    void pressMediaNextTrackKey();

    /**
     * Setup expectations: The Media app is active.
     *
     * Change media track using hardkey.
     */
    void pressMediaPreviousTrackKey();

    /**
     * Setup expectations: The Media app is active.
     *
     * Increase media volume.
     */
    void tuneVolumeUpKey();

    /**
     * Setup expectations: The Media app is active.
     *
     * Decrease media volume.
     */
    void tuneVolumeDownKey();

    /** Increase brightness. */
    void pressBrightnessUpKey();

    /** Decrease brighness. */
    void pressBrightnessDownKey();

    /** Launch assistant. */
    void pressAssistantKey();

    /**
     * Setup expectations: The media app is active.
     *
     * Tune volume knob to mute media.
     */
    void tuneMuteKey();

    /** Switch off screen. */
    void pressScreenOffKey();

    /** Select content. */
    void tuneKnobKey();

    /** Open selected content. */
    void pressKnobButtonKey();

    /** Increase/decrease volume */
    void tuneVolumeKnobKey();

    /** Mute media by pressing volume knob. */
    void pressVolumeKnobButtonKey();

    /**
     * Get current volume level for a specific type of volume. Eg: Media, In-call, Alarm, Navigation
     *
     * @param type type of volume to get current volume level from.
     */
    int getCurrentVolumeLevel(VolumeType type);

    /** Get driving state. */
    DrivingState getDrivingState();

    /**
     * Set driving state.
     *
     * @param state to be set.
     */
    void setDrivingState(DrivingState state);

    /**
     * Set vehicle speed.
     *
     * @param speed to be set.
     */
    void setSpeed(int speed);
}
