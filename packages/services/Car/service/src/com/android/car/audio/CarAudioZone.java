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
package com.android.car.audio;

import android.car.media.CarAudioManager;
import android.media.AudioDeviceAttributes;
import android.media.AudioDeviceInfo;
import android.util.Log;

import com.android.car.CarLog;
import com.android.internal.util.Preconditions;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;

/**
 * A class encapsulates an audio zone in car.
 *
 * An audio zone can contain multiple {@link CarVolumeGroup}s, and each zone has its own
 * {@link CarAudioFocus} instance. Additionally, there may be dedicated hardware volume keys
 * attached to each zone.
 *
 * See also the unified car_audio_configuration.xml
 */
/* package */ class CarAudioZone {

    private final int mId;
    private final String mName;
    private final List<CarVolumeGroup> mVolumeGroups;
    private List<AudioDeviceAttributes> mInputAudioDevice;

    CarAudioZone(int id, String name) {
        mId = id;
        mName = name;
        mVolumeGroups = new ArrayList<>();
        mInputAudioDevice = new ArrayList<>();
    }

    int getId() {
        return mId;
    }

    String getName() {
        return mName;
    }

    boolean isPrimaryZone() {
        return mId == CarAudioManager.PRIMARY_AUDIO_ZONE;
    }

    void addVolumeGroup(CarVolumeGroup volumeGroup) {
        mVolumeGroups.add(volumeGroup);
    }

    CarVolumeGroup getVolumeGroup(int groupId) {
        Preconditions.checkArgumentInRange(groupId, 0, mVolumeGroups.size() - 1,
                "groupId(" + groupId + ") is out of range");
        return mVolumeGroups.get(groupId);
    }

    /**
     * @return Snapshot of available {@link AudioDeviceInfo}s in List.
     */
    List<AudioDeviceInfo> getAudioDeviceInfos() {
        final List<AudioDeviceInfo> devices = new ArrayList<>();
        for (CarVolumeGroup group : mVolumeGroups) {
            for (String address : group.getAddresses()) {
                devices.add(group.getCarAudioDeviceInfoForAddress(address).getAudioDeviceInfo());
            }
        }
        return devices;
    }

    int getVolumeGroupCount() {
        return mVolumeGroups.size();
    }

    /**
     * @return Snapshot of available {@link CarVolumeGroup}s in array.
     */
    CarVolumeGroup[] getVolumeGroups() {
        return mVolumeGroups.toArray(new CarVolumeGroup[0]);
    }

    /**
     * Constraints applied here:
     *
     * - One context should not appear in two groups
     * - All contexts are assigned
     * - One device should not appear in two groups
     * - All gain controllers in the same group have same step value
     *
     * Note that it is fine that there are devices which do not appear in any group. Those devices
     * may be reserved for other purposes.
     * Step value validation is done in {@link CarVolumeGroup#bind(int, CarAudioDeviceInfo)}
     */
    boolean validateVolumeGroups() {
        Set<Integer> contextSet = new HashSet<>();
        Set<String> addresses = new HashSet<>();
        for (CarVolumeGroup group : mVolumeGroups) {
            // One context should not appear in two groups
            for (int context : group.getContexts()) {
                if (!contextSet.add(context)) {
                    Log.e(CarLog.TAG_AUDIO, "Context appears in two groups: " + context);
                    return false;
                }
            }

            // One address should not appear in two groups
            for (String address : group.getAddresses()) {
                if (!addresses.add(address)) {
                    Log.e(CarLog.TAG_AUDIO, "Address appears in two groups: " + address);
                    return false;
                }
            }
        }

        // All contexts are assigned
        if (contextSet.size() != CarAudioContext.CONTEXTS.length) {
            Log.e(CarLog.TAG_AUDIO, "Some contexts are not assigned to group");
            Log.e(CarLog.TAG_AUDIO, "Assigned contexts " + contextSet);
            Log.e(CarLog.TAG_AUDIO,
                    "All contexts " + Arrays.toString(CarAudioContext.CONTEXTS));
            return false;
        }

        return true;
    }

    void synchronizeCurrentGainIndex() {
        for (CarVolumeGroup group : mVolumeGroups) {
            group.setCurrentGainIndex(group.getCurrentGainIndex());
        }
    }

    void dump(String indent, PrintWriter writer) {
        String internalIndent = indent + "\t";
        writer.printf("%sCarAudioZone(%s:%d) isPrimary? %b\n", indent, mName, mId, isPrimaryZone());

        for (CarVolumeGroup group : mVolumeGroups) {
            group.dump(internalIndent, writer);
        }

        writer.printf("%sInput Audio Device Addresses\n", internalIndent);
        String devicesIndent = internalIndent + "\t";
        for (AudioDeviceAttributes audioDevice : mInputAudioDevice) {
            writer.printf("%sDevice Address(%s)\n", devicesIndent,
                    audioDevice.getAddress());
        }
        writer.println();
    }

    String getAddressForContext(int audioContext) {
        CarAudioContext.preconditionCheckAudioContext(audioContext);
        String deviceAddress = null;
        for (CarVolumeGroup volumeGroup : getVolumeGroups()) {
            deviceAddress = volumeGroup.getAddressForContext(audioContext);
            if (deviceAddress != null) {
                return deviceAddress;
            }
        }
        // This should not happen unless something went wrong.
        // Device address are unique per zone and all contexts are assigned in a zone.
        throw new IllegalStateException("Could not find output device in zone " + mId
                + " for audio context " + audioContext);
    }

    /**
     * Update the volume groups for the new user
     * @param userId user id to update to
     */
    public void updateVolumeGroupsForUser(int userId) {
        for (CarVolumeGroup group : mVolumeGroups) {
            group.loadVolumesForUser(userId);
        }
    }

    void addInputAudioDevice(AudioDeviceAttributes device) {
        mInputAudioDevice.add(device);
    }

    List<AudioDeviceAttributes> getInputAudioDevices() {
        return mInputAudioDevice;
    }
}
