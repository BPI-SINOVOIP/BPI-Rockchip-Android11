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

import android.annotation.NonNull;
import android.annotation.UserIdInt;
import android.car.settings.CarSettings;
import android.database.ContentObserver;
import android.media.AudioManager;
import android.media.AudioManager.FocusRequestResult;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.os.UserHandle;
import android.provider.Settings;
import android.util.Log;

import androidx.annotation.VisibleForTesting;

import com.android.car.audio.CarAudioContext.AudioContext;
import com.android.internal.util.Preconditions;

import java.io.PrintWriter;
import java.util.List;
import java.util.Objects;

/**
 * FocusInteraction is responsible for evaluating how incoming focus requests should be handled
 * based on pre-defined interaction behaviors for each incoming {@link AudioContext} in relation to
 * a {@link AudioContext} that is currently holding focus.
 */
final class FocusInteraction {

    private static final String TAG = FocusInteraction.class.getSimpleName();

    @VisibleForTesting
    static final Uri AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL_URI =
            Settings.Secure.getUriFor(
                    CarSettings.Secure.KEY_AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL);

    // Values for the internal interaction matrix we use to make focus decisions
    @VisibleForTesting
    static final int INTERACTION_REJECT = 0; // Focus not granted
    @VisibleForTesting
    static final int INTERACTION_EXCLUSIVE = 1; // Focus granted, others loose focus
    @VisibleForTesting
    static final int INTERACTION_CONCURRENT = 2; // Focus granted, others keep focus

    private static final int[][] sInteractionMatrix = {
            // Each Row represents CarAudioContext of current focus holder
            // Each Column represents CarAudioContext of incoming request (labels along the right)
            // Cell value is one of INTERACTION_REJECT, INTERACTION_EXCLUSIVE,
            // or INTERACTION_CONCURRENT

            // Focus holder: INVALID
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_REJECT, // MUSIC
                    INTERACTION_REJECT, // NAVIGATION
                    INTERACTION_REJECT, // VOICE_COMMAND
                    INTERACTION_REJECT, // CALL_RING
                    INTERACTION_REJECT, // CALL
                    INTERACTION_REJECT, // ALARM
                    INTERACTION_REJECT, // NOTIFICATION
                    INTERACTION_REJECT, // SYSTEM_SOUND,
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_EXCLUSIVE, // SAFETY
                    INTERACTION_REJECT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: MUSIC
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_EXCLUSIVE, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_EXCLUSIVE, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_EXCLUSIVE, // ANNOUNCEMENT
            },
            // Focus holder: NAVIGATION
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_CONCURRENT, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_CONCURRENT, // ANNOUNCEMENT
            },
            // Focus holder: VOICE_COMMAND
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_REJECT, // NAVIGATION
                    INTERACTION_CONCURRENT, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_REJECT, // ALARM
                    INTERACTION_REJECT, // NOTIFICATION
                    INTERACTION_REJECT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: CALL_RING
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_REJECT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_CONCURRENT, // VOICE_COMMAND
                    INTERACTION_CONCURRENT, // CALL_RING
                    INTERACTION_CONCURRENT, // CALL
                    INTERACTION_REJECT, // ALARM
                    INTERACTION_REJECT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: CALL
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_REJECT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_REJECT, // VOICE_COMMAND
                    INTERACTION_CONCURRENT, // CALL_RING
                    INTERACTION_CONCURRENT, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_REJECT, // SYSTEM_SOUND
                    INTERACTION_CONCURRENT, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: ALARM
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: NOTIFICATION
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_CONCURRENT, // ANNOUNCEMENT
            },
            // Focus holder: SYSTEM_SOUND
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_CONCURRENT, // ANNOUNCEMENT
            },
            // Focus holder: EMERGENCY
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_REJECT, // MUSIC
                    INTERACTION_REJECT, // NAVIGATION
                    INTERACTION_REJECT, // VOICE_COMMAND
                    INTERACTION_REJECT, // CALL_RING
                    INTERACTION_CONCURRENT, // CALL
                    INTERACTION_REJECT, // ALARM
                    INTERACTION_REJECT, // NOTIFICATION
                    INTERACTION_REJECT, // SYSTEM_SOUND
                    INTERACTION_CONCURRENT, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_REJECT, // VEHICLE_STATUS
                    INTERACTION_REJECT, // ANNOUNCEMENT
            },
            // Focus holder: SAFETY
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_CONCURRENT, // VOICE_COMMAND
                    INTERACTION_CONCURRENT, // CALL_RING
                    INTERACTION_CONCURRENT, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_CONCURRENT, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_CONCURRENT, // ANNOUNCEMENT
            },
            // Focus holder: VEHICLE_STATUS
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_CONCURRENT, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_CONCURRENT, // VOICE_COMMAND
                    INTERACTION_CONCURRENT, // CALL_RING
                    INTERACTION_CONCURRENT, // CALL
                    INTERACTION_CONCURRENT, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_CONCURRENT, // ANNOUNCEMENT
            },
            // Focus holder: ANNOUNCEMENT
            {
                    INTERACTION_REJECT, // INVALID
                    INTERACTION_EXCLUSIVE, // MUSIC
                    INTERACTION_CONCURRENT, // NAVIGATION
                    INTERACTION_EXCLUSIVE, // VOICE_COMMAND
                    INTERACTION_EXCLUSIVE, // CALL_RING
                    INTERACTION_EXCLUSIVE, // CALL
                    INTERACTION_EXCLUSIVE, // ALARM
                    INTERACTION_CONCURRENT, // NOTIFICATION
                    INTERACTION_CONCURRENT, // SYSTEM_SOUND
                    INTERACTION_EXCLUSIVE, // EMERGENCY
                    INTERACTION_CONCURRENT, // SAFETY
                    INTERACTION_CONCURRENT, // VEHICLE_STATUS
                    INTERACTION_EXCLUSIVE, // ANNOUNCEMENT
            },
    };

    private final Object mLock = new Object();

    private final int[][] mInteractionMatrix;

    private ContentObserver mContentObserver;

    private final CarAudioSettings mCarAudioFocusSettings;

    private int mUserId;

    /**
     * Constructs a focus interaction instance.
     */
    FocusInteraction(@NonNull CarAudioSettings carAudioSettings) {
        mCarAudioFocusSettings = Objects.requireNonNull(carAudioSettings);
        mInteractionMatrix = cloneInteractionMatrix(sInteractionMatrix);
    }

    private void navigationOnCallSettingChanged() {
        synchronized (mLock) {
            if (mUserId != UserHandle.USER_NULL) {
                setRejectNavigationOnCallLocked(isRejectNavigationOnCallEnabledInSettings(mUserId));
            }
        }
    }

    public void setRejectNavigationOnCallLocked(boolean navigationRejectedWithCall) {
        mInteractionMatrix[CarAudioContext.CALL][CarAudioContext.NAVIGATION] =
                navigationRejectedWithCall ? INTERACTION_REJECT :
                sInteractionMatrix[CarAudioContext.CALL][CarAudioContext.NAVIGATION];
    }

    /**
     * Evaluates interaction between incoming focus {@link AudioContext} and the current focus
     * request based on interaction matrix.
     *
     * <p>Note: In addition to returning the {@link FocusRequestResult}
     * for the incoming request based on this interaction, this method also adds the current {@code
     * focusHolder} to the {@code focusLosers} list when appropriate.
     *
     * @param requestedContext CarAudioContextType of incoming focus request
     * @param focusHolder      {@link FocusEntry} for current focus holder
     * @param focusLosers      Mutable array to add focusHolder to if it should lose focus
     * @return {@link FocusRequestResult} result of focus interaction
     */
    public @FocusRequestResult int evaluateRequest(@AudioContext int requestedContext,
            FocusEntry focusHolder, List<FocusEntry> focusLosers, boolean allowDucking,
            boolean allowsDelayedFocus) {
        @AudioContext int holderContext = focusHolder.getAudioContext();
        Preconditions.checkArgumentInRange(holderContext, 0, mInteractionMatrix.length - 1,
                "holderContext");
        synchronized (mLock) {
            int[] holderRow = mInteractionMatrix[holderContext];
            Preconditions.checkArgumentInRange(requestedContext, 0, holderRow.length - 1,
                    "requestedContext");

            switch (holderRow[requestedContext]) {
                case INTERACTION_REJECT:
                    if (allowsDelayedFocus) {
                        return AudioManager.AUDIOFOCUS_REQUEST_DELAYED;
                    }
                    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
                case INTERACTION_EXCLUSIVE:
                    focusLosers.add(focusHolder);
                    return AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
                case INTERACTION_CONCURRENT:
                    // If ducking isn't allowed by the focus requester, then everybody else
                    // must get a LOSS.
                    // If a focus holder has set the AUDIOFOCUS_FLAG_PAUSES_ON_DUCKABLE_LOSS flag,
                    // they must get a LOSS message even if ducking would otherwise be allowed.
                    // If a focus holder holds the RECEIVE_CAR_AUDIO_DUCKING_EVENTS permission,
                    // they must receive all audio focus losses.
                    if (!allowDucking
                            || focusHolder.wantsPauseInsteadOfDucking()
                            || focusHolder.receivesDuckEvents()) {
                        focusLosers.add(focusHolder);
                    }
                    return AudioManager.AUDIOFOCUS_REQUEST_GRANTED;
                default:
                    Log.e(TAG, String.format("Unsupported CarAudioContext %d - rejecting request",
                            holderRow[requestedContext]));
                    return AudioManager.AUDIOFOCUS_REQUEST_FAILED;
            }
        }
    }

    /**
     * Sets userId for interaction focus settings
     */
    void setUserIdForSettings(@UserIdInt int userId) {
        synchronized (mLock) {
            mUserId = userId;
            if (mContentObserver != null) {
                mCarAudioFocusSettings.getContentResolver()
                        .unregisterContentObserver(mContentObserver);
                mContentObserver = null;
            }
            if (mUserId == UserHandle.USER_NULL) {
                setRejectNavigationOnCallLocked(false);
                return;
            }
            mContentObserver = new ContentObserver(new Handler(Looper.getMainLooper())) {
                @Override
                public void onChange(boolean selfChange, Uri uri) {
                    if (uri.equals(AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL_URI)) {
                        navigationOnCallSettingChanged();
                    }
                }
            };
            mCarAudioFocusSettings.getContentResolver()
                    .registerContentObserver(AUDIO_FOCUS_NAVIGATION_REJECTED_DURING_CALL_URI,
                            false, mContentObserver, userId);
            setRejectNavigationOnCallLocked(isRejectNavigationOnCallEnabledInSettings(mUserId));
        }
    }

    private boolean isRejectNavigationOnCallEnabledInSettings(@UserIdInt int userId) {
        return mCarAudioFocusSettings.isRejectNavigationOnCallEnabledInSettings(userId);
    }

    @VisibleForTesting
    int[][] getInteractionMatrix() {
        return cloneInteractionMatrix(mInteractionMatrix);
    }

    private static int[][] cloneInteractionMatrix(int[][] matrixToClone) {
        int[][] interactionMatrixClone =
                new int[matrixToClone.length][matrixToClone.length];
        for (int audioContext = 0; audioContext < matrixToClone.length; audioContext++) {
            System.arraycopy(matrixToClone[audioContext], 0,
                    interactionMatrixClone[audioContext], 0, matrixToClone.length);
        }
        return interactionMatrixClone;
    }

    public void dump(String indent, PrintWriter writer) {
        boolean rejectNavigationOnCall =
                mInteractionMatrix[CarAudioContext.CALL][CarAudioContext.NAVIGATION]
                == INTERACTION_REJECT;
        writer.printf("%sReject Navigation on Call: %b\n", indent, rejectNavigationOnCall);
    }
}
