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
package android.media.soundtrigger_middleware;

import android.media.audio.common.AudioConfig;
import android.media.soundtrigger_middleware.RecognitionStatus;
import android.media.soundtrigger_middleware.SoundModelType;

/**
 * An event that gets sent to indicate a recognition (or aborting of the recognition process).
 * {@hide}
 */
parcelable RecognitionEvent {
    /** Recognition status. */
    RecognitionStatus status;
    /** Event type, same as sound model type. */
    SoundModelType type;
    /** Is it possible to capture audio from this utterance buffered by the implementation. */
    boolean captureAvailable;
    /* Audio session ID. framework use. */
    int captureSession;
    /**
     * Delay in ms between end of model detection and start of audio available for capture.
     * A negative value is possible (e.g. if key phrase is also available for Capture.
     */
    int captureDelayMs;
    /** Duration in ms of audio captured before the start of the trigger. 0 if none. */
    int capturePreambleMs;
    /** If true, the 'data' field below contains the capture of the trigger sound. */
    boolean triggerInData;
    /**
     * Audio format of either the trigger in event data or to use for capture of the rest of the
     * utterance. May be null when no audio is available for this event type.
     */
    @nullable AudioConfig audioConfig;
    /** Additional data. */
    byte[] data;
}
