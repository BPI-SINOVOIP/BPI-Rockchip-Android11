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

import android.media.soundtrigger_middleware.ConfidenceLevel;

/**
 * Specialized recognition event for key phrase detection.
 * {@hide}
 */
parcelable PhraseRecognitionExtra {
    // TODO(ytai): Constants / enums.

    /** keyphrase ID */
    int id;
    /** recognition modes used for this keyphrase */
    int recognitionModes;
    /** confidence level for mode RECOGNITION_MODE_VOICE_TRIGGER */
    int confidenceLevel;
    /** number of user confidence levels */
    ConfidenceLevel[] levels;
}
