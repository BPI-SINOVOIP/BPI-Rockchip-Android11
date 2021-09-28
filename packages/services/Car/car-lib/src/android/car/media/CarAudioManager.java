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

import android.annotation.NonNull;
import android.annotation.Nullable;
import android.annotation.RequiresPermission;
import android.annotation.SystemApi;
import android.annotation.TestApi;
import android.car.Car;
import android.car.CarLibLog;
import android.car.CarManagerBase;
import android.media.AudioAttributes;
import android.media.AudioDeviceAttributes;
import android.media.AudioDeviceInfo;
import android.media.AudioManager;
import android.media.AudioManager.AudioDeviceRole;
import android.os.Bundle;
import android.os.IBinder;
import android.os.RemoteException;
import android.util.Log;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;
import java.util.Objects;
import java.util.Set;
import java.util.concurrent.CopyOnWriteArrayList;

/**
 * APIs for handling audio in a car.
 *
 * In a car environment, we introduced the support to turn audio dynamic routing on /off by
 * setting the "audioUseDynamicRouting" attribute in config.xml
 *
 * When audio dynamic routing is enabled:
 * - Audio devices are grouped into zones
 * - There is at least one primary zone, and extra secondary zones such as RSE
 *   (Reat Seat Entertainment)
 * - Within each zone, audio devices are grouped into volume groups for volume control
 * - Audio is assigned to an audio device based on its AudioAttributes usage
 *
 * When audio dynamic routing is disabled:
 * - There is exactly one audio zone, which is the primary zone
 * - Each volume group represents a controllable STREAM_TYPE, same as AudioManager
 */
public final class CarAudioManager extends CarManagerBase {

    /**
     * Zone id of the primary audio zone.
     * @hide
     */
    @SystemApi
    public static final int PRIMARY_AUDIO_ZONE = 0x0;

    /**
     * Zone id of the invalid audio zone.
     * @hide
     */
    @SystemApi
    public static final int INVALID_AUDIO_ZONE = 0xffffffff;

    /**
     * Volume Group ID when volume group not found.
     * @hide
     */
    public static final int INVALID_VOLUME_GROUP_ID = -1;

    /**
     * Extra for {@link android.media.AudioAttributes.Builder#addBundle(Bundle)}: when used in an
     * {@link android.media.AudioFocusRequest}, the requester should receive all audio focus events,
     * including {@link android.media.AudioManager#AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK}.
     * The requester must hold {@link Car#PERMISSION_RECEIVE_CAR_AUDIO_DUCKING_EVENTS}; otherwise,
     * this extra is ignored.
     *
     * @hide
     */
    @SystemApi
    public static final String AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS =
            "android.car.media.AUDIOFOCUS_EXTRA_RECEIVE_DUCKING_EVENTS";

    /**
     * Extra for {@link android.media.AudioAttributes.Builder#addBundle(Bundle)}: when used in an
     * {@link android.media.AudioFocusRequest}, the requester should receive all audio focus for the
     * the zone. If the zone id is not defined: the audio focus request will default to the
     * currently mapped zone for the requesting uid or {@link CarAudioManager.PRIMARY_AUDIO_ZONE}
     * if no uid mapping currently exist.
     *
     * @hide
     */
    public static final String AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID =
            "android.car.media.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID";

    private final ICarAudio mService;
    private final CopyOnWriteArrayList<CarVolumeCallback> mCarVolumeCallbacks;
    private final AudioManager mAudioManager;

    private final ICarVolumeCallback mCarVolumeCallbackImpl = new ICarVolumeCallback.Stub() {
        @Override
        public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {
            for (CarVolumeCallback callback : mCarVolumeCallbacks) {
                callback.onGroupVolumeChanged(zoneId, groupId, flags);
            }
        }

        @Override
        public void onMasterMuteChanged(int zoneId, int flags) {
            for (CarVolumeCallback callback : mCarVolumeCallbacks) {
                callback.onMasterMuteChanged(zoneId, flags);
            }
        }
    };

    /**
     * @return Whether dynamic routing is enabled or not.
     * @hide
     */
    @TestApi
    public boolean isDynamicRoutingEnabled() {
        try {
            return mService.isDynamicRoutingEnabled();
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Sets the volume index for a volume group in primary zone.
     *
     * @see {@link #setGroupVolume(int, int, int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public void setGroupVolume(int groupId, int index, int flags) {
        setGroupVolume(PRIMARY_AUDIO_ZONE, groupId, index, flags);
    }

    /**
     * Sets the volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is affected.
     * @param groupId The volume group id whose volume index should be set.
     * @param index The volume index to set. See
     *            {@link #getGroupMaxVolume(int, int)} for the largest valid value.
     * @param flags One or more flags (e.g., {@link android.media.AudioManager#FLAG_SHOW_UI},
     *              {@link android.media.AudioManager#FLAG_PLAY_SOUND})
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public void setGroupVolume(int zoneId, int groupId, int index, int flags) {
        try {
            mService.setGroupVolume(zoneId, groupId, index, flags);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Returns the maximum volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupMaxVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupMaxVolume(int groupId) {
        return getGroupMaxVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the maximum volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose maximum volume index is returned.
     * @return The maximum valid volume index for the given group.
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupMaxVolume(int zoneId, int groupId) {
        try {
            return mService.getGroupMaxVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Returns the minimum volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupMinVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupMinVolume(int groupId) {
        return getGroupMinVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the minimum volume index for a volume group.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose minimum volume index is returned.
     * @return The minimum valid volume index for the given group, non-negative
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupMinVolume(int zoneId, int groupId) {
        try {
            return mService.getGroupMinVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Returns the current volume index for a volume group in primary zone.
     *
     * @see {@link #getGroupVolume(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupVolume(int groupId) {
        return getGroupVolume(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Returns the current volume index for a volume group.
     *
     * @param zoneId The zone id whose volume groups is queried.
     * @param groupId The volume group id whose volume index is returned.
     * @return The current volume index for the given group.
     *
     * @see #getGroupMaxVolume(int, int)
     * @see #setGroupVolume(int, int, int, int)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getGroupVolume(int zoneId, int groupId) {
        try {
            return mService.getGroupVolume(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Adjust the relative volume in the front vs back of the vehicle cabin.
     *
     * @param value in the range -1.0 to 1.0 for fully toward the back through
     *              fully toward the front.  0.0 means evenly balanced.
     *
     * @see #setBalanceTowardRight(float)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public void setFadeTowardFront(float value) {
        try {
            mService.setFadeTowardFront(value);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Adjust the relative volume on the left vs right side of the vehicle cabin.
     *
     * @param value in the range -1.0 to 1.0 for fully toward the left through
     *              fully toward the right.  0.0 means evenly balanced.
     *
     * @see #setFadeTowardFront(float)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public void setBalanceTowardRight(float value) {
        try {
            mService.setBalanceTowardRight(value);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Queries the system configuration in order to report the available, non-microphone audio
     * input devices.
     *
     * @return An array of strings representing the available input ports.
     * Each port is identified by it's "address" tag in the audioPolicyConfiguration xml file.
     * Empty array if we find nothing.
     *
     * @see #createAudioPatch(String, int, int)
     * @see #releaseAudioPatch(CarAudioPatchHandle)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public @NonNull String[] getExternalSources() {
        try {
            return mService.getExternalSources();
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
            return new String[0];
        }
    }

    /**
     * Given an input port identified by getExternalSources(), request that it's audio signal
     * be routed below the HAL to the output port associated with the given usage.  For example,
     * The output of a tuner might be routed directly to the output buss associated with
     * AudioAttributes.USAGE_MEDIA while the tuner is playing.
     *
     * @param sourceAddress the input port name obtained from getExternalSources().
     * @param usage the type of audio represented by this source (usually USAGE_MEDIA).
     * @param gainInMillibels How many steps above the minimum value defined for the source port to
     *                       set the gain when creating the patch.
     *                       This may be used for source balancing without affecting the user
     *                       controlled volumes applied to the destination ports.  A value of
     *                       0 indicates no gain change is requested.
     * @return A handle for the created patch which can be used to later remove it.
     *
     * @see #getExternalSources()
     * @see #releaseAudioPatch(CarAudioPatchHandle)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public CarAudioPatchHandle createAudioPatch(String sourceAddress,
            @AudioAttributes.AttributeUsage int usage, int gainInMillibels) {
        try {
            return mService.createAudioPatch(sourceAddress, usage, gainInMillibels);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Removes the association between an input port and an output port identified by the provided
     * handle.
     *
     * @param patch CarAudioPatchHandle returned from createAudioPatch().
     *
     * @see #getExternalSources()
     * @see #createAudioPatch(String, int, int)
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public void releaseAudioPatch(CarAudioPatchHandle patch) {
        try {
            mService.releaseAudioPatch(patch);
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    /**
     * Gets the count of available volume groups in primary zone.
     *
     * @see {@link #getVolumeGroupCount(int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getVolumeGroupCount() {
        return getVolumeGroupCount(PRIMARY_AUDIO_ZONE);
    }

    /**
     * Gets the count of available volume groups in the system.
     *
     * @param zoneId The zone id whois count of volume groups is queried.
     * @return Count of volume groups
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getVolumeGroupCount(int zoneId) {
        try {
            return mService.getVolumeGroupCount(zoneId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Gets the volume group id for a given {@link AudioAttributes} usage in primary zone.
     *
     * @see {@link #getVolumeGroupIdForUsage(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getVolumeGroupIdForUsage(@AudioAttributes.AttributeUsage int usage) {
        return getVolumeGroupIdForUsage(PRIMARY_AUDIO_ZONE, usage);
    }

    /**
     * Gets the volume group id for a given {@link AudioAttributes} usage.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param usage The {@link AudioAttributes} usage to get a volume group from.
     * @return The volume group id where the usage belongs to
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public int getVolumeGroupIdForUsage(int zoneId, @AudioAttributes.AttributeUsage int usage) {
        try {
            return mService.getVolumeGroupIdForUsage(zoneId, usage);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Gets array of {@link AudioAttributes} usages for a volume group in primary zone.
     *
     * @see {@link #getUsagesForVolumeGroupId(int, int)}
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public @NonNull int[] getUsagesForVolumeGroupId(int groupId) {
        return getUsagesForVolumeGroupId(PRIMARY_AUDIO_ZONE, groupId);
    }

    /**
     * Gets array of {@link AudioAttributes} usages for a volume group in a zone.
     *
     * @param zoneId The zone id whose volume group is queried.
     * @param groupId The volume group id whose associated audio usages is returned.
     * @return Array of {@link AudioAttributes} usages for a given volume group id
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME)
    public @NonNull int[] getUsagesForVolumeGroupId(int zoneId, int groupId) {
        try {
            return mService.getUsagesForVolumeGroupId(zoneId, groupId);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, new int[0]);
        }
    }

    /**
     * Gets the audio zones currently available
     *
     * @return audio zone ids
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public @NonNull List<Integer> getAudioZoneIds() {
        try {
            int[] zoneIdArray = mService.getAudioZoneIds();
            List<Integer> zoneIdList = new ArrayList<Integer>(zoneIdArray.length);
            for (int zoneIdValue : zoneIdArray) {
                zoneIdList.add(zoneIdValue);
            }
            return zoneIdList;
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, Collections.emptyList());
        }
    }

    /**
     * Gets the audio zone id currently mapped to uId,
     * defaults to PRIMARY_AUDIO_ZONE if no mapping exist
     *
     * @param uid The uid to map
     * @return zone id mapped to uid
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public int getZoneIdForUid(int uid) {
        try {
            return mService.getZoneIdForUid(uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, 0);
        }
    }

    /**
     * Maps the audio zone id to uid
     *
     * @param zoneId The audio zone id
     * @param uid The uid to map
     * @return true if the uid is successfully mapped
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean setZoneIdForUid(int zoneId, int uid) {
        try {
            return mService.setZoneIdForUid(zoneId, uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Clears the current zone mapping of the uid
     *
     * @param uid The uid to clear
     * @return true if the zone was successfully cleared
     * @hide
     */
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public boolean clearZoneIdForUid(int uid) {
        try {
            return mService.clearZoneIdForUid(uid);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, false);
        }
    }

    /**
     * Gets the output device for a given {@link AudioAttributes} usage in zoneId.
     *
     * <p><b>Note:</b> To be used for routing to a specific device. Most applications should
     * use the regular routing mechanism, which is to set audio attribute usage to
     * an audio track.
     *
     * @param zoneId zone id to query for device
     * @param usage usage where audio is routed
     * @return Audio device info, returns {@code null} if audio device usage fails to map to
     * an active audio device. This is different from the using an invalid value for
     * {@link AudioAttributes} usage. In the latter case the query will fail with a
     * RuntimeException indicating the issue.
     *
     * @hide
     */
    @SystemApi
    @Nullable
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public AudioDeviceInfo getOutputDeviceForUsage(int zoneId,
            @AudioAttributes.AttributeUsage int usage) {
        try {
            String deviceAddress = mService.getOutputDeviceAddressForUsage(zoneId, usage);
            if (deviceAddress == null) {
                return null;
            }
            AudioDeviceInfo[] outputDevices =
                    mAudioManager.getDevices(AudioManager.GET_DEVICES_OUTPUTS);
            for (AudioDeviceInfo info : outputDevices) {
                if (info.getAddress().equals(deviceAddress)) {
                    return info;
                }
            }
            return null;
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, null);
        }
    }

    /**
     * Gets the input devices for an audio zone
     *
     * @return list of input devices
     * @hide
     */
    @SystemApi
    @RequiresPermission(Car.PERMISSION_CAR_CONTROL_AUDIO_SETTINGS)
    public @NonNull List<AudioDeviceInfo> getInputDevicesForZoneId(int zoneId) {
        try {
            return convertInputDevicesToDeviceInfos(
                    mService.getInputDevicesForZoneId(zoneId),
                    AudioManager.GET_DEVICES_INPUTS);
        } catch (RemoteException e) {
            return handleRemoteExceptionFromCarService(e, new ArrayList<>());
        }
    }

    /** @hide */
    @Override
    public void onCarDisconnected() {
        if (mService != null) {
            unregisterVolumeCallback();
        }
    }

    /** @hide */
    public CarAudioManager(Car car, IBinder service) {
        super(car);
        mService = ICarAudio.Stub.asInterface(service);
        mAudioManager = getContext().getSystemService(AudioManager.class);
        mCarVolumeCallbacks = new CopyOnWriteArrayList<>();
    }

    /**
     * Registers a {@link CarVolumeCallback} to receive volume change callbacks
     * @param callback {@link CarVolumeCallback} instance, can not be null
     * <p>
     * Requires permission Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME
     */
    public void registerCarVolumeCallback(@NonNull CarVolumeCallback callback) {
        Objects.requireNonNull(callback);

        if (mCarVolumeCallbacks.isEmpty()) {
            registerVolumeCallback();
        }

        mCarVolumeCallbacks.add(callback);
    }

    /**
     * Unregisters a {@link CarVolumeCallback} from receiving volume change callbacks
     * @param callback {@link CarVolumeCallback} instance previously registered, can not be null
     * <p>
     * Requires permission Car.PERMISSION_CAR_CONTROL_AUDIO_VOLUME
     */
    public void unregisterCarVolumeCallback(@NonNull CarVolumeCallback callback) {
        Objects.requireNonNull(callback);
        if (mCarVolumeCallbacks.remove(callback) && mCarVolumeCallbacks.isEmpty()) {
            unregisterVolumeCallback();
        }
    }

    private void registerVolumeCallback() {
        try {
            mService.registerVolumeCallback(mCarVolumeCallbackImpl.asBinder());
        } catch (RemoteException e) {
            Log.e(CarLibLog.TAG_CAR, "registerVolumeCallback failed", e);
        }
    }

    private void unregisterVolumeCallback() {
        try {
            mService.unregisterVolumeCallback(mCarVolumeCallbackImpl.asBinder());
        } catch (RemoteException e) {
            handleRemoteExceptionFromCarService(e);
        }
    }

    private List<AudioDeviceInfo> convertInputDevicesToDeviceInfos(
            List<AudioDeviceAttributes> devices, @AudioDeviceRole int flag) {
        int addressesSize = devices.size();
        Set<String> deviceAddressMap = new HashSet<>(addressesSize);
        for (int i = 0; i < addressesSize; ++i) {
            AudioDeviceAttributes device = devices.get(i);
            deviceAddressMap.add(device.getAddress());
        }
        List<AudioDeviceInfo> deviceInfoList = new ArrayList<>(devices.size());
        AudioDeviceInfo[] inputDevices = mAudioManager.getDevices(flag);
        for (int i = 0; i < inputDevices.length; ++i) {
            AudioDeviceInfo info = inputDevices[i];
            if (info.isSource() && deviceAddressMap.contains(info.getAddress())) {
                deviceInfoList.add(info);
            }
        }
        return deviceInfoList;
    }

    /**
     * Callback interface to receive volume change events in a car.
     * Extend this class and register it with {@link #registerCarVolumeCallback(CarVolumeCallback)}
     * and unregister it via {@link #unregisterCarVolumeCallback(CarVolumeCallback)}
     */
    public abstract static class CarVolumeCallback {
        /**
         * This is called whenever a group volume is changed.
         * The changed-to volume index is not included, the caller is encouraged to
         * get the current group volume index via CarAudioManager.
         *
         * @param zoneId Id of the audio zone that volume change happens
         * @param groupId Id of the volume group that volume is changed
         * @param flags see {@link android.media.AudioManager} for flag definitions
         */
        public void onGroupVolumeChanged(int zoneId, int groupId, int flags) {}

        /**
         * This is called whenever the master mute state is changed.
         * The changed-to master mute state is not included, the caller is encouraged to
         * get the current master mute state via AudioManager.
         *
         * @param zoneId Id of the audio zone that master mute state change happens
         * @param flags see {@link android.media.AudioManager} for flag definitions
         */
        public void onMasterMuteChanged(int zoneId, int flags) {}
    }
}
