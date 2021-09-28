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

package com.android.car.audio;

import static com.android.car.audio.CarAudioService.DEFAULT_AUDIO_CONTEXT;

import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.media.AudioPlaybackConfiguration;
import android.telephony.Annotation.CallState;
import android.telephony.TelephonyManager;
import android.util.Log;
import android.util.SparseIntArray;

import com.android.car.audio.CarAudioContext.AudioContext;

import java.util.List;

/**
 * CarVolume is responsible for determining which audio contexts to prioritize when adjusting volume
 */
final class CarVolume {
    private static final String TAG = CarVolume.class.getSimpleName();
    private static final int CONTEXT_NOT_PRIORITIZED = -1;

    private static final int[] AUDIO_CONTEXT_VOLUME_PRIORITY = {
            CarAudioContext.NAVIGATION,
            CarAudioContext.CALL,
            CarAudioContext.MUSIC,
            CarAudioContext.ANNOUNCEMENT,
            CarAudioContext.VOICE_COMMAND,
            CarAudioContext.CALL_RING,
            CarAudioContext.SYSTEM_SOUND,
            CarAudioContext.SAFETY,
            CarAudioContext.ALARM,
            CarAudioContext.NOTIFICATION,
            CarAudioContext.VEHICLE_STATUS,
            CarAudioContext.EMERGENCY,
            // CarAudioContext.INVALID is intentionally not prioritized as it is not routed by
            // CarAudioService and is not expected to be used.
    };

    private static final SparseIntArray VOLUME_PRIORITY_BY_AUDIO_CONTEXT = new SparseIntArray();

    static {
        for (int priority = 0; priority < AUDIO_CONTEXT_VOLUME_PRIORITY.length; priority++) {
            VOLUME_PRIORITY_BY_AUDIO_CONTEXT.append(AUDIO_CONTEXT_VOLUME_PRIORITY[priority],
                    priority);
        }
    }

    /**
     * Suggests a {@link AudioContext} that should be adjusted based on the current
     * {@link AudioPlaybackConfiguration}s and {@link CallState}.
     */
    static @AudioContext int getSuggestedAudioContext(
            List<AudioPlaybackConfiguration> configurations, @CallState int callState) {
        int currentContext = DEFAULT_AUDIO_CONTEXT;
        int currentPriority = AUDIO_CONTEXT_VOLUME_PRIORITY.length;

        if (callState == TelephonyManager.CALL_STATE_RINGING) {
            currentContext = CarAudioContext.CALL_RING;
            currentPriority = VOLUME_PRIORITY_BY_AUDIO_CONTEXT.get(CarAudioContext.CALL_RING);
        } else if (callState == TelephonyManager.CALL_STATE_OFFHOOK) {
            currentContext = CarAudioContext.CALL;
            currentPriority = VOLUME_PRIORITY_BY_AUDIO_CONTEXT.get(CarAudioContext.CALL);
        }

        for (AudioPlaybackConfiguration configuration : configurations) {
            if (!configuration.isActive()) {
                continue;
            }

            @AttributeUsage int usage = configuration.getAudioAttributes().getSystemUsage();
            @AudioContext int context = CarAudioContext.getContextForUsage(usage);
            int priority = VOLUME_PRIORITY_BY_AUDIO_CONTEXT.get(context, CONTEXT_NOT_PRIORITIZED);
            if (priority == CONTEXT_NOT_PRIORITIZED) {
                Log.w(TAG, "Usage " + AudioAttributes.usageToString(usage) + " mapped to context "
                        + CarAudioContext.toString(context) + " which is not prioritized");
                continue;
            }

            if (priority < currentPriority) {
                currentContext = context;
                currentPriority = priority;
            }
        }

        return currentContext;
    }
}
