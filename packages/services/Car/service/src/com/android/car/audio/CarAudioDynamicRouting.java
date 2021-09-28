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

import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.audiopolicy.AudioMix;
import android.media.audiopolicy.AudioMixingRule;
import android.media.audiopolicy.AudioPolicy;
import android.util.Log;
import android.util.SparseArray;

import com.android.car.CarLog;

import java.util.Arrays;

/**
 * Builds dynamic audio routing in a car from audio zone configuration.
 */
final class CarAudioDynamicRouting {
    // For legacy stream type based volume control.
    // Values in STREAM_TYPES and STREAM_TYPE_USAGES should be aligned.
    static final int[] STREAM_TYPES = new int[] {
            AudioManager.STREAM_MUSIC,
            AudioManager.STREAM_ALARM,
            AudioManager.STREAM_RING
    };
    static final int[] STREAM_TYPE_USAGES = new int[] {
            AudioAttributes.USAGE_MEDIA,
            AudioAttributes.USAGE_ALARM,
            AudioAttributes.USAGE_NOTIFICATION_RINGTONE
    };

    static void setupAudioDynamicRouting(AudioPolicy.Builder builder,
            SparseArray<CarAudioZone> carAudioZones) {
        for (int i = 0; i < carAudioZones.size(); i++) {
            CarAudioZone zone = carAudioZones.valueAt(i);
            for (CarVolumeGroup group : zone.getVolumeGroups()) {
                setupAudioDynamicRoutingForGroup(group, builder);
            }
        }
    }

    /**
     * Enumerates all physical buses in a given volume group and attach the mixing rules.
     * @param group {@link CarVolumeGroup} instance to enumerate the buses with
     * @param builder {@link AudioPolicy.Builder} to attach the mixing rules
     */
    private static void setupAudioDynamicRoutingForGroup(CarVolumeGroup group,
            AudioPolicy.Builder builder) {
        // Note that one can not register audio mix for same bus more than once.
        for (String address : group.getAddresses()) {
            boolean hasContext = false;
            CarAudioDeviceInfo info = group.getCarAudioDeviceInfoForAddress(address);
            AudioFormat mixFormat = new AudioFormat.Builder()
                    .setSampleRate(info.getSampleRate())
                    .setEncoding(info.getEncodingFormat())
                    .setChannelMask(info.getChannelCount())
                    .build();
            AudioMixingRule.Builder mixingRuleBuilder = new AudioMixingRule.Builder();
            for (int carAudioContext : group.getContextsForAddress(address)) {
                hasContext = true;
                int[] usages = CarAudioContext.getUsagesForContext(carAudioContext);
                for (int usage : usages) {
                    AudioAttributes attributes = buildAttributesWithUsage(usage);
                    mixingRuleBuilder.addRule(attributes,
                            AudioMixingRule.RULE_MATCH_ATTRIBUTE_USAGE);
                }
                if (Log.isLoggable(CarLog.TAG_AUDIO, Log.DEBUG)) {
                    Log.d(CarLog.TAG_AUDIO, String.format(
                            "Address: %s AudioContext: %s sampleRate: %d channels: %d usages: %s",
                            address, carAudioContext, info.getSampleRate(), info.getChannelCount(),
                            Arrays.toString(usages)));
                }
            }
            if (hasContext) {
                // It's a valid case that an audio output address is defined in
                // audio_policy_configuration and no context is assigned to it.
                // In such case, do not build a policy mix with zero rules.
                AudioMix audioMix = new AudioMix.Builder(mixingRuleBuilder.build())
                        .setFormat(mixFormat)
                        .setDevice(info.getAudioDeviceInfo())
                        .setRouteFlags(AudioMix.ROUTE_FLAG_RENDER)
                        .build();
                builder.addMix(audioMix);
            }
        }
    }

    private static AudioAttributes buildAttributesWithUsage(@AttributeUsage int usage) {
        AudioAttributes.Builder attributesBuilder = new AudioAttributes.Builder();
        if (AudioAttributes.isSystemUsage(usage)) {
            attributesBuilder.setSystemUsage(usage);
        } else {
            attributesBuilder.setUsage(usage);
        }
        return attributesBuilder.build();
    }
}
