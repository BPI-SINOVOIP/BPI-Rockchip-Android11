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

import android.media.AudioAttributes;
import android.media.AudioAttributes.AttributeUsage;
import android.util.SparseArray;
import android.util.SparseIntArray;

import androidx.annotation.IntDef;

import com.android.internal.util.Preconditions;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Arrays;

/**
 * Groupings of {@link AttributeUsage}s to simplify configuration of car audio routing, volume
 * groups, and focus interactions for similar usages.
 */
public final class CarAudioContext {
    /*
     * Shouldn't be used
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.INVALID
     */
    static final int INVALID = 0;
    /*
     * Music playback
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.INVALID implicitly + 1
     */
    static final int MUSIC = 1;
    /*
     * Navigation directions
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.MUSIC implicitly + 1
     */
    static final int NAVIGATION = 2;
    /*
     * Voice command session
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.NAVIGATION implicitly + 1
     */
    static final int VOICE_COMMAND = 3;
    /*
     * Voice call ringing
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber
     *     .VOICE_COMMAND implicitly + 1
     */
    static final int CALL_RING = 4;
    /*
     * Voice call
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.CALL_RING implicitly + 1
     */
    static final int CALL = 5;
    /*
     * Alarm sound from Android
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.CALL implicitly + 1
     */
    static final int ALARM = 6;
    /*
     * Notifications
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber.ALARM implicitly + 1
     */
    static final int NOTIFICATION = 7;
    /*
     * System sounds
     * ::android::hardware::automotive::audiocontrol::V1_0::ContextNumber
     *     .NOTIFICATION implicitly + 1
     */
    static final int SYSTEM_SOUND = 8;
    /*
     * Emergency related sounds such as collision warnings
     */
    static final int EMERGENCY = 9;
    /*
     * Safety sounds such as obstacle detection when backing up or when changing lanes
     */
    static final int SAFETY = 10;
    /*
     * Vehicle Status related sounds such as check engine light or seat belt chimes
     */
    static final int VEHICLE_STATUS = 11;
    /*
     * Announcement such as traffic announcements
     */
    static final int ANNOUNCEMENT = 12;

    static final int[] CONTEXTS = {
            MUSIC,
            NAVIGATION,
            VOICE_COMMAND,
            CALL_RING,
            CALL,
            ALARM,
            NOTIFICATION,
            SYSTEM_SOUND,
            EMERGENCY,
            SAFETY,
            VEHICLE_STATUS,
            ANNOUNCEMENT
    };

    private static final SparseArray<String> CONTEXT_NAMES = new SparseArray<>(CONTEXTS.length + 1);
    static {
        CONTEXT_NAMES.append(INVALID, "INVALID");
        CONTEXT_NAMES.append(MUSIC, "MUSIC");
        CONTEXT_NAMES.append(NAVIGATION, "NAVIGATION");
        CONTEXT_NAMES.append(VOICE_COMMAND, "VOICE_COMMAND");
        CONTEXT_NAMES.append(CALL_RING, "CALL_RING");
        CONTEXT_NAMES.append(CALL, "CALL");
        CONTEXT_NAMES.append(ALARM, "ALARM");
        CONTEXT_NAMES.append(NOTIFICATION, "NOTIFICATION");
        CONTEXT_NAMES.append(SYSTEM_SOUND, "SYSTEM_SOUND");
        CONTEXT_NAMES.append(EMERGENCY, "EMERGENCY");
        CONTEXT_NAMES.append(SAFETY, "SAFETY");
        CONTEXT_NAMES.append(VEHICLE_STATUS, "VEHICLE_STATUS");
        CONTEXT_NAMES.append(ANNOUNCEMENT, "ANNOUNCEMENT");
    }

    private static final SparseArray<int[]> CONTEXT_TO_USAGES = new SparseArray<>();

    static {
        CONTEXT_TO_USAGES.put(MUSIC,
                new int[]{
                        AudioAttributes.USAGE_UNKNOWN,
                        AudioAttributes.USAGE_GAME,
                        AudioAttributes.USAGE_MEDIA
                });

        CONTEXT_TO_USAGES.put(NAVIGATION,
                new int[]{
                        AudioAttributes.USAGE_ASSISTANCE_NAVIGATION_GUIDANCE
                });

        CONTEXT_TO_USAGES.put(VOICE_COMMAND,
                new int[]{
                        AudioAttributes.USAGE_ASSISTANCE_ACCESSIBILITY,
                        AudioAttributes.USAGE_ASSISTANT
                });

        CONTEXT_TO_USAGES.put(CALL_RING,
                new int[]{
                        AudioAttributes.USAGE_NOTIFICATION_RINGTONE
                });

        CONTEXT_TO_USAGES.put(CALL,
                new int[]{
                        AudioAttributes.USAGE_VOICE_COMMUNICATION,
                        AudioAttributes.USAGE_VOICE_COMMUNICATION_SIGNALLING
                });

        CONTEXT_TO_USAGES.put(ALARM,
                new int[]{
                        AudioAttributes.USAGE_ALARM
                });

        CONTEXT_TO_USAGES.put(NOTIFICATION,
                new int[]{
                        AudioAttributes.USAGE_NOTIFICATION,
                        AudioAttributes.USAGE_NOTIFICATION_COMMUNICATION_REQUEST,
                        AudioAttributes.USAGE_NOTIFICATION_COMMUNICATION_INSTANT,
                        AudioAttributes.USAGE_NOTIFICATION_COMMUNICATION_DELAYED,
                        AudioAttributes.USAGE_NOTIFICATION_EVENT
                });

        CONTEXT_TO_USAGES.put(SYSTEM_SOUND,
                new int[]{
                        AudioAttributes.USAGE_ASSISTANCE_SONIFICATION
                });

        CONTEXT_TO_USAGES.put(EMERGENCY,
                new int[]{
                        AudioAttributes.USAGE_EMERGENCY
                });

        CONTEXT_TO_USAGES.put(SAFETY,
                new int[]{
                        AudioAttributes.USAGE_SAFETY
                });

        CONTEXT_TO_USAGES.put(VEHICLE_STATUS,
                new int[]{
                        AudioAttributes.USAGE_VEHICLE_STATUS
                });

        CONTEXT_TO_USAGES.put(ANNOUNCEMENT,
                new int[]{
                        AudioAttributes.USAGE_ANNOUNCEMENT
                });

        CONTEXT_TO_USAGES.put(INVALID,
                new int[]{
                        AudioAttributes.USAGE_VIRTUAL_SOURCE
                });
    }

    private static final SparseIntArray USAGE_TO_CONTEXT = new SparseIntArray();

    static {
        for (int i = 0; i < CONTEXT_TO_USAGES.size(); i++) {
            @AudioContext int audioContext = CONTEXT_TO_USAGES.keyAt(i);
            @AttributeUsage int[] usages = CONTEXT_TO_USAGES.valueAt(i);
            for (@AttributeUsage int usage : usages) {
                USAGE_TO_CONTEXT.put(usage, audioContext);
            }
        }
    }

    /**
     * Checks if the audio context is within the valid range from MUSIC to SYSTEM_SOUND
     */
    static void preconditionCheckAudioContext(@AudioContext int audioContext) {
        Preconditions.checkArgument(Arrays.binarySearch(CONTEXTS, audioContext) >= 0,
                "audioContext %d is invalid", audioContext);
    }

    static @AttributeUsage int[] getUsagesForContext(@AudioContext int carAudioContext) {
        preconditionCheckAudioContext(carAudioContext);
        return CONTEXT_TO_USAGES.get(carAudioContext);
    }

    /**
     * @return Context number for a given audio usage, {@code INVALID} if the given usage is
     * unrecognized.
     */
    static @AudioContext int getContextForUsage(@AttributeUsage int audioUsage) {
        return USAGE_TO_CONTEXT.get(audioUsage, INVALID);
    }

    static String toString(@AudioContext int audioContext) {
        String name = CONTEXT_NAMES.get(audioContext);
        if (name != null) {
            return name;
        }
        return "Unsupported Context 0x" + Integer.toHexString(audioContext);
    }

    @IntDef({
            INVALID,
            MUSIC,
            NAVIGATION,
            VOICE_COMMAND,
            CALL_RING,
            CALL,
            ALARM,
            NOTIFICATION,
            SYSTEM_SOUND,
            EMERGENCY,
            SAFETY,
            VEHICLE_STATUS,
            ANNOUNCEMENT
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AudioContext {
    }
};
