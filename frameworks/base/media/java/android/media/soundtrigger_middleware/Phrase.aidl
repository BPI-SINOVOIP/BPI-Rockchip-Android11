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

/**
 * Key phrase descriptor.
 *
 * {@hide}
 */
parcelable Phrase {
    /** Unique keyphrase ID assigned at enrollment time. */
    int id;
    /** Recognition modes supported by this key phrase (bitfield of RecognitionMode enum). */
    int recognitionModes;
    /** List of users IDs associated with this key phrase. */
    int[] users;
    /** Locale - Java Locale style (e.g. en_US). */
    String locale;
    /** Phrase text. */
    String text;
}
