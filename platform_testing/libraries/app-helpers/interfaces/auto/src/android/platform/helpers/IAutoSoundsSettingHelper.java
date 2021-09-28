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

package android.platform.helpers;

public interface IAutoSoundsSettingHelper extends IAppHelper {

    /** enum class for volume types. */
    enum VolumeType {
        MEDIA,
        ALARM,
        NAVIGATION,
        INCALL,
        NOTIFICATION,
        RINGTONE
    }

    /** enum class for sound types. */
    enum SoundType {
        RINGTONE,
        NOTIFICATION,
        ALARM
    }

    /**
     * Setup expectation: Sound setting is open.
     *
     * <p>get volume value.
     *
     * @param volumeType type selected.
     */
    int getVolume(VolumeType volumeType);

    /**
     * Setup expectation: Sound setting is open.
     *
     * <p>set volume
     *
     * @param volumeType type selected
     * @param index index of sound value.
     */
    void setVolume(VolumeType volumeType, int index);

    /**
     * Setup expectation: Sound setting is open.
     *
     * <p>get sound.
     *
     * @param soundType type selected.
     */
    String getSound(SoundType soundType);

    /**
     * Setup expectation: Sound setting is open.
     *
     * <p>set sound
     *
     * @param soundType type selected.
     * @param sound sound selected.
     */
    void setSound(SoundType soundType, String sound);
}
