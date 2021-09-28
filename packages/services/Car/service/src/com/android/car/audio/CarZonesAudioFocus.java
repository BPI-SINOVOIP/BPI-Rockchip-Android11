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

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.car.media.CarAudioManager;
import android.content.pm.PackageManager;
import android.media.AudioAttributes;
import android.media.AudioFocusInfo;
import android.media.AudioManager;
import android.media.audiopolicy.AudioPolicy;
import android.os.Bundle;
import android.util.Log;
import android.util.SparseArray;

import com.android.car.CarLog;
import com.android.internal.util.Preconditions;

import java.io.PrintWriter;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Map;
import java.util.Objects;

/**
 * Implements {@link AudioPolicy.AudioPolicyFocusListener}
 *
 * <p><b>Note:</b> Manages audio focus on a per zone basis.
 */
class CarZonesAudioFocus extends AudioPolicy.AudioPolicyFocusListener {

    private final boolean mDelayedFocusEnabled;
    private CarAudioService mCarAudioService; // Dynamically assigned just after construction
    private AudioPolicy mAudioPolicy; // Dynamically assigned just after construction

    private final Map<Integer, CarAudioFocus> mFocusZones = new HashMap<>();

    CarZonesAudioFocus(@NonNull AudioManager audioManager,
            @NonNull PackageManager packageManager,
            @NonNull SparseArray<CarAudioZone> carAudioZones,
            @NonNull CarAudioSettings carAudioSettings,
            boolean enableDelayedAudioFocus) {
        //Create the zones here, the policy will be set setOwningPolicy,
        // which is called right after this constructor.
        Objects.requireNonNull(audioManager);
        Objects.requireNonNull(packageManager);
        Objects.requireNonNull(carAudioZones);
        Objects.requireNonNull(carAudioSettings);
        Preconditions.checkArgument(carAudioZones.size() != 0,
                "There must be a minimum of one audio zone");

        //Create focus for all the zones
        for (int i = 0; i < carAudioZones.size(); i++) {
            CarAudioZone audioZone = carAudioZones.valueAt(i);
            int audioZoneId = audioZone.getId();
            if (Log.isLoggable(CarLog.TAG_AUDIO, Log.DEBUG)) {
                Log.d(CarLog.TAG_AUDIO,
                        "CarZonesAudioFocus adding new zone " + audioZoneId);
            }
            CarAudioFocus zoneFocusListener =
                    new CarAudioFocus(audioManager, packageManager,
                            new FocusInteraction(carAudioSettings), enableDelayedAudioFocus);
            mFocusZones.put(audioZoneId, zoneFocusListener);
        }
        mDelayedFocusEnabled = enableDelayedAudioFocus;
    }


    /**
     * Query the current list of focus loser in zoneId for uid
     * @param uid uid to query for current focus losers
     * @param zoneId zone id to query for info
     * @return list of current focus losers for uid
     */
    ArrayList<AudioFocusInfo> getAudioFocusLosersForUid(int uid, int zoneId) {
        CarAudioFocus focus = mFocusZones.get(zoneId);
        return focus.getAudioFocusLosersForUid(uid);
    }

    /**
     * Query the current list of focus holders in zoneId for uid
     * @param uid uid to query for current focus holders
     * @param zoneId zone id to query
     * @return list of current focus holders that for uid
     */
    ArrayList<AudioFocusInfo> getAudioFocusHoldersForUid(int uid, int zoneId) {
        CarAudioFocus focus = mFocusZones.get(zoneId);
        return focus.getAudioFocusHoldersForUid(uid);
    }

    /**
     * For each entry in list, transiently lose focus
     * @param afiList list of audio focus entries
     * @param zoneId zone id where focus should should be lost
     */
    void transientlyLoseInFocusInZone(@NonNull ArrayList<AudioFocusInfo> afiList,
            int zoneId) {
        CarAudioFocus focus = mFocusZones.get(zoneId);

        for (AudioFocusInfo info : afiList) {
            focus.removeAudioFocusInfoAndTransientlyLoseFocus(info);
        }
    }

    int reevaluateAndRegainAudioFocus(AudioFocusInfo afi) {
        CarAudioFocus focus = getFocusForAudioFocusInfo(afi);
        return focus.reevaluateAndRegainAudioFocus(afi);
    }


    /**
     * Sets the owning policy of the audio focus
     *
     * <p><b>Note:</b> This has to happen after the construction to avoid a chicken and egg
     * problem when setting up the AudioPolicy which must depend on this object.

     * @param carAudioService owning car audio service
     * @param parentPolicy owning parent car audio policy
     */
    void setOwningPolicy(CarAudioService carAudioService, AudioPolicy parentPolicy) {
        mAudioPolicy = parentPolicy;
        mCarAudioService = carAudioService;

        for (int zoneId : mFocusZones.keySet()) {
            mFocusZones.get(zoneId).setOwningPolicy(mAudioPolicy);
        }
    }

    @Override
    public void onAudioFocusRequest(AudioFocusInfo afi, int requestResult) {
        CarAudioFocus focus = getFocusForAudioFocusInfo(afi);
        focus.onAudioFocusRequest(afi, requestResult);
    }

    /**
     * @see AudioManager#abandonAudioFocus(AudioManager.OnAudioFocusChangeListener, AudioAttributes)
     * Note that we'll get this call for a focus holder that dies while in the focus stack, so
     * we don't need to watch for death notifications directly.
     */
    @Override
    public void onAudioFocusAbandon(AudioFocusInfo afi) {
        CarAudioFocus focus = getFocusForAudioFocusInfo(afi);
        focus.onAudioFocusAbandon(afi);
    }

    private CarAudioFocus getFocusForAudioFocusInfo(AudioFocusInfo afi) {
        //getFocusForAudioFocusInfo defaults to returning default zoneId
        //if uid has not been mapped, thus the value returned will be
        //default zone focus
        int zoneId = mCarAudioService.getZoneIdForUid(afi.getClientUid());

        // If the bundle attribute for AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID has been assigned
        // Use zone id from that instead.
        Bundle bundle = afi.getAttributes().getBundle();

        if (bundle != null) {
            int bundleZoneId =
                    bundle.getInt(CarAudioManager.AUDIOFOCUS_EXTRA_REQUEST_ZONE_ID,
                            -1);
            // check if the zone id is within current zones bounds
            if (mCarAudioService.isAudioZoneIdValid(bundleZoneId)) {
                if (Log.isLoggable(CarLog.TAG_AUDIO, Log.DEBUG)) {
                    Log.d(CarLog.TAG_AUDIO,
                            "getFocusForAudioFocusInfo valid zoneId " + bundleZoneId
                                    + " with bundle request for client " + afi.getClientId());
                }
                zoneId = bundleZoneId;
            } else {
                Log.w(CarLog.TAG_AUDIO,
                        "getFocusForAudioFocusInfo invalid zoneId " + bundleZoneId
                                + " with bundle request for client " + afi.getClientId()
                                + ", dispatching focus request to zoneId " + zoneId);
            }
        }

        CarAudioFocus focus =  mFocusZones.get(zoneId);
        return focus;
    }

    /**
     * dumps the current state of the CarZoneAudioFocus
     *
     * @param indent indent to append to each new line
     * @param writer stream to write current state
     */
    void dump(String indent, PrintWriter writer) {
        writer.printf("%s*CarZonesAudioFocus*\n", indent);
        writer.printf("%s\tDelayed Focus Enabled: %b\n", indent, mDelayedFocusEnabled);
        writer.printf("%s\tCar Zones Audio Focus Listeners:\n", indent);
        Integer[] keys = mFocusZones.keySet().stream().sorted().toArray(Integer[]::new);
        for (Integer zoneId : keys) {
            writer.printf("%s\tZone Id: %s\n", indent, zoneId.toString());
            mFocusZones.get(zoneId).dump(indent + "\t", writer);
        }
    }

    public void updateUserForZoneId(int audioZoneId, @UserIdInt int userId) {
        Preconditions.checkArgument(mCarAudioService.isAudioZoneIdValid(audioZoneId),
                "Invalid zoneId %d", audioZoneId);
        mFocusZones.get(audioZoneId).getFocusInteraction().setUserIdForSettings(userId);
    }
}
