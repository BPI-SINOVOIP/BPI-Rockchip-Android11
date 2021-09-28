/*
 * Copyright (C) 2015 The Android Open Source Project
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

package android.car.media;

import android.car.media.CarAudioPatchHandle;
import android.media.AudioDeviceAttributes;
/**
 * Binder interface for {@link android.car.media.CarAudioManager}.
 * Check {@link android.car.media.CarAudioManager} APIs for expected behavior of each call.
 *
 * @hide
 */
interface ICarAudio {
    boolean isDynamicRoutingEnabled();

    void setGroupVolume(int zoneId, int groupId, int index, int flags);
    int getGroupMaxVolume(int zoneId, int groupId);
    int getGroupMinVolume(int zoneId, int groupId);
    int getGroupVolume(int zoneId, int groupId);

    void setFadeTowardFront(float value);
    void setBalanceTowardRight(float value);

    String[] getExternalSources();
    CarAudioPatchHandle createAudioPatch(in String sourceAddress, int usage, int gainInMillibels);
    void releaseAudioPatch(in CarAudioPatchHandle patch);

    int getVolumeGroupCount(int zoneId);
    int getVolumeGroupIdForUsage(int zoneId, int usage);
    int[] getUsagesForVolumeGroupId(int zoneId, int groupId);

    int[] getAudioZoneIds();
    int getZoneIdForUid(int uid);
    boolean setZoneIdForUid(int zoneId, int uid);
    boolean clearZoneIdForUid(int uid);

    String getOutputDeviceAddressForUsage(int zoneId, int usage);

    List<AudioDeviceAttributes> getInputDevicesForZoneId(int zoneId);
    /**
     * IBinder is ICarVolumeCallback but passed as IBinder due to aidl hidden.
     */
    void registerVolumeCallback(in IBinder binder);
    void unregisterVolumeCallback(in IBinder binder);
}
